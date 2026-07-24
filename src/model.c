#include "model.h"

#include "humanoid.h"
#include "log.h"
#include "motion.h"
#include "pose_solver.h"
#include "vrm_body.h"
#include "vrm_projection.h"

#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <d3d11.h>
#include <windows.h>
#endif

#include <cgltf.h>

#if !defined(_WIN32)
#define MODEL_TARGET_WIDTH 1536
#define MODEL_TARGET_HEIGHT 1536
#endif
#define MODEL_MAX_JOINTS 256
#if !defined(_WIN32)
#define MODEL_READBACK_COUNT 3
#endif
#define MODEL_FRAME_INTERVAL_MS 33U

typedef struct EidolonModelVertex {
    float position[3];
    float neutral_delta[3];
    float focused_delta[3];
    float texcoord[2];
    Uint16 joints[4];
    float weights[4];
} EidolonModelVertex;

typedef struct EidolonModelScene {
    float model_view_projection[16];
    float expression_weights[4];
} EidolonModelScene;

typedef struct EidolonModelMaterial {
    float base_color_factor[4];
    float alpha_cutoff;
    float alpha_mode;
    float padding[2];
} EidolonModelMaterial;

#if !defined(_WIN32)
typedef struct EidolonModelReadback {
    SDL_GPUTransferBuffer *download;
    SDL_GPUFence *fence;
    uint64_t frame_sequence;
    uint64_t transform_revision;
} EidolonModelReadback;
#endif

typedef struct EidolonModelDraw {
    Uint32 first_index;
    Uint32 index_count;
    Uint32 texture_index;
    EidolonModelMaterial material;
    bool alpha_blend;
} EidolonModelDraw;

typedef struct EidolonCpuGeometry {
    EidolonModelVertex *vertices;
    Uint32 *indices;
    EidolonModelDraw *draws;
    size_t vertex_count;
    size_t index_count;
    size_t draw_count;
} EidolonCpuGeometry;

struct EidolonModelRenderer {
#if defined(_WIN32)
    SDL_Renderer *renderer;
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    ID3D11Buffer *vertex_buffer;
    ID3D11Buffer *index_buffer;
    ID3D11ShaderResourceView **textures;
    ID3D11SamplerState *sampler;
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
    ID3D11InputLayout *input_layout;
    ID3D11Buffer *scene_buffer;
    ID3D11Buffer *bones_buffer;
    ID3D11Buffer *material_buffer;
    ID3D11RasterizerState *rasterizer;
    ID3D11DepthStencilState *depth_state;
    ID3D11DepthStencilState *depth_state_no_write;
    ID3D11BlendState *alpha_blend;
    ID3D11RenderTargetView *color_rtv;
    ID3D11Texture2D *depth_texture;
    ID3D11DepthStencilView *depth_dsv;
#else
    SDL_GPUDevice *device;
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    SDL_GPUTexture **textures;
    SDL_GPUSampler *sampler;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUGraphicsPipeline *alpha_pipeline;
    SDL_GPUTexture *color_target;
    SDL_GPUTexture *depth_target;
#endif
    size_t texture_count;
    SDL_Texture *renderer_texture;
    EidolonModelDraw *draws;
    size_t draw_count;
    EidolonMotionRig motion;
    EidolonHumanoidProfile humanoid;
    EidolonSemanticPose semantic_pose;
    EidolonVrmBody vrm_body;
    EidolonVrmProjection vrm_projection;
    EidolonEprBodyProfile body_profile;
    bool humanoid_ready;
    bool vrm_ready;
    bool semantic_pose_active;
    bool semantic_pose_failed;
    Uint16 joint_nodes[MODEL_MAX_JOINTS];
    float inverse_bind[MODEL_MAX_JOINTS][16];
    float joint_palette[MODEL_MAX_JOINTS][16];
    size_t joint_count;
#if !defined(_WIN32)
    EidolonModelReadback readbacks[MODEL_READBACK_COUNT];
    size_t next_readback;
    uint64_t next_frame_sequence;
#endif
    uint64_t last_submit_ms;
    uint64_t presented_frame_sequence;
    float yaw_radians;
    float pitch_radians;
    float roll_radians;
    float rotation_pivot[3];
    float view_scale;
    float view_depth_scale;
    bool fit_vrm_view;
    uint64_t transform_revision;
    uint64_t presented_transform_revision;
    int target_width;
    int target_height;
    bool failed;
};

#if !defined(_WIN32)
static const SDL_GPUTextureFormat MODEL_COLOR_FORMAT = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
static const SDL_GPUTextureFormat MODEL_DEPTH_FORMAT = SDL_GPU_TEXTUREFORMAT_D16_UNORM;

static SDL_GPUDevice *create_gpu_device(void) {
    const SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL;
    const char *requested = SDL_getenv("EIDOLON_GPU_DRIVER");
    if (requested != NULL && requested[0] != '\0') {
        return SDL_CreateGPUDevice(formats, true, requested);
    }
#if defined(_WIN32)
    SDL_GPUDevice *device = SDL_CreateGPUDevice(formats, true, "vulkan");
    if (device != NULL) {
        return device;
    }
    eidolon_log_write("model", "Vulkan unavailable; trying D3D12: %s", SDL_GetError());
    SDL_ClearError();
    return SDL_CreateGPUDevice(formats, true, "direct3d12");
#else
    return SDL_CreateGPUDevice(formats, true, NULL);
#endif
}
#endif

static void disable_failed_model(EidolonModelRenderer *model, const char *operation) {
    if (model->failed) {
        return;
    }
    eidolon_log_write("model", "%s failed; disabling 3D renderer: %s", operation, SDL_GetError());
    model->failed = true;
    model->transform_revision += 1U;
    SDL_ClearError();
}

static void matrix_identity(float matrix[16]) {
    SDL_memset(matrix, 0, sizeof(float) * 16U);
    matrix[0] = 1.0F;
    matrix[5] = 1.0F;
    matrix[10] = 1.0F;
    matrix[15] = 1.0F;
}

static void matrix_multiply(const float left[16], const float right[16], float result[16]) {
    float product[16];
    for (size_t column = 0; column < 4; ++column) {
        for (size_t row = 0; row < 4; ++row) {
            product[column * 4 + row] = left[0 * 4 + row] * right[column * 4 + 0] +
                                        left[1 * 4 + row] * right[column * 4 + 1] +
                                        left[2 * 4 + row] * right[column * 4 + 2] +
                                        left[3 * 4 + row] * right[column * 4 + 3];
        }
    }
    SDL_memcpy(result, product, sizeof(product));
}

static void matrix_rotation_y(float radians, float matrix[16]) {
    const float cosine = SDL_cosf(radians);
    const float sine = SDL_sinf(radians);
    matrix_identity(matrix);
    matrix[0] = cosine;
    matrix[2] = -sine;
    matrix[8] = sine;
    matrix[10] = cosine;
}

static void matrix_rotation_x(float radians, float matrix[16]) {
    const float cosine = SDL_cosf(radians);
    const float sine = SDL_sinf(radians);
    matrix_identity(matrix);
    matrix[5] = cosine;
    matrix[6] = sine;
    matrix[9] = -sine;
    matrix[10] = cosine;
}

static void matrix_rotation_z(float radians, float matrix[16]) {
    const float cosine = SDL_cosf(radians);
    const float sine = SDL_sinf(radians);
    matrix_identity(matrix);
    matrix[0] = cosine;
    matrix[1] = sine;
    matrix[4] = -sine;
    matrix[5] = cosine;
}

static void matrix_translation(float x, float y, float z, float matrix[16]) {
    matrix_identity(matrix);
    matrix[12] = x;
    matrix[13] = y;
    matrix[14] = z;
}

static bool size_to_u32(size_t value, Uint32 *result, const char *description) {
    if (value > UINT32_MAX) {
        SDL_SetError("%s is too large for SDL_GPU", description);
        return false;
    }
    *result = (Uint32)value;
    return true;
}

static void cpu_geometry_destroy(EidolonCpuGeometry *geometry) {
    SDL_free(geometry->vertices);
    SDL_free(geometry->indices);
    SDL_free(geometry->draws);
    SDL_zero(*geometry);
}

static void set_rotation_pivot(EidolonModelRenderer *model, const EidolonCpuGeometry *geometry) {
    float minimum[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float maximum[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (size_t vertex_index = 0; vertex_index < geometry->vertex_count; ++vertex_index) {
        for (size_t axis = 0; axis < 3; ++axis) {
            const float value = geometry->vertices[vertex_index].position[axis];
            minimum[axis] = SDL_min(minimum[axis], value);
            maximum[axis] = SDL_max(maximum[axis], value);
        }
    }
    for (size_t axis = 0; axis < 3; ++axis) {
        model->rotation_pivot[axis] = (minimum[axis] + maximum[axis]) * 0.5F;
    }
    if (model->vrm_ready) {
        const float width = maximum[0] - minimum[0];
        const float height = maximum[1] - minimum[1];
        const float depth = maximum[2] - minimum[2];
        if (height > 0.0001F) {
            model->view_scale = 1.70F / height;
            model->view_depth_scale = 0.80F / SDL_max(depth, 0.10F);
            model->fit_vrm_view = true;
            eidolon_log_write(
                "model",
                "VRM geometry fit width=%.3f height=%.3f depth=%.3f center=%.3f,%.3f,%.3f "
                "scale=%.3f",
                width, height, depth, model->rotation_pivot[0], model->rotation_pivot[1],
                model->rotation_pivot[2], model->view_scale);
        }
    }
}

static void model_projection(const EidolonModelRenderer *model, float projection[16]) {
    if (!model->fit_vrm_view) {
        static const float legacy[16] = {
            1.4F, 0.0F, 0.0F,   0.0F, 0.0F, 1.4F,    0.0F, 0.0F,
            0.0F, 0.0F, -0.55F, 0.0F, 0.0F, -0.784F, 0.5F, 1.0F,
        };
        SDL_memcpy(projection, legacy, sizeof(legacy));
        return;
    }
    matrix_identity(projection);
    projection[0] = model->view_scale;
    projection[5] = model->view_scale;
    projection[10] = -model->view_depth_scale;
    projection[12] = -model->rotation_pivot[0] * model->view_scale;
    projection[13] = -model->rotation_pivot[1] * model->view_scale;
    projection[14] = 0.5F + model->rotation_pivot[2] * model->view_depth_scale;
}

static const cgltf_accessor *primitive_attribute(const cgltf_primitive *primitive,
                                                 cgltf_attribute_type type, cgltf_int index) {
    return cgltf_find_accessor(primitive, type, index);
}

static bool data_has_extension(const cgltf_data *data, const char *name) {
    for (cgltf_size index = 0; index < data->data_extensions_count; ++index) {
        const cgltf_extension *extension = &data->data_extensions[index];
        if (extension->name != NULL && SDL_strcmp(extension->name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool primitive_is_supported(const cgltf_primitive *primitive) {
    return primitive->type == cgltf_primitive_type_triangles &&
           primitive_attribute(primitive, cgltf_attribute_type_position, 0) != NULL;
}

static bool skins_share_joint_palette(const cgltf_data *data) {
    const cgltf_skin *reference = &data->skins[0];
    for (cgltf_size skin_index = 1; skin_index < data->skins_count; ++skin_index) {
        const cgltf_skin *skin = &data->skins[skin_index];
        if (skin->joints_count != reference->joints_count || skin->inverse_bind_matrices == NULL) {
            return false;
        }
        for (cgltf_size joint = 0; joint < reference->joints_count; ++joint) {
            cgltf_float reference_matrix[16];
            cgltf_float matrix[16];
            if (skin->joints[joint] != reference->joints[joint] ||
                !cgltf_accessor_read_float(reference->inverse_bind_matrices, joint,
                                           reference_matrix, 16) ||
                !cgltf_accessor_read_float(skin->inverse_bind_matrices, joint, matrix, 16)) {
                return false;
            }
            for (size_t component = 0; component < 16U; ++component) {
                if (SDL_fabsf((float)reference_matrix[component] - (float)matrix[component]) >
                    0.00001F) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool build_motion_rig(EidolonModelRenderer *model, const cgltf_data *data, bool vrm) {
    if (data->skins_count == 0 || data->skins[0].joints_count == 0 ||
        data->skins[0].joints_count >= MODEL_MAX_JOINTS ||
        data->skins[0].inverse_bind_matrices == NULL || !skins_share_joint_palette(data)) {
        SDL_SetError("model skins must share one palette with fewer than %d joints",
                     MODEL_MAX_JOINTS);
        return false;
    }
    if (!eidolon_motion_init(&model->motion, (size_t)data->nodes_count)) {
        return false;
    }

    for (cgltf_size node_index = 0; node_index < data->nodes_count; ++node_index) {
        const cgltf_node *node = &data->nodes[node_index];
        const int parent = node->parent != NULL ? (int)(node->parent - data->nodes) : -1;
        if (!eidolon_motion_set_node(&model->motion, (size_t)node_index, node->name, parent,
                                     node->translation, node->rotation, node->scale)) {
            return false;
        }
    }

    const cgltf_skin *skin = &data->skins[0];
    model->joint_count = (size_t)skin->joints_count;
    for (size_t joint_index = 0; joint_index < MODEL_MAX_JOINTS; ++joint_index) {
        matrix_identity(model->joint_palette[joint_index]);
    }
    for (cgltf_size joint_index = 0; joint_index < skin->joints_count; ++joint_index) {
        const ptrdiff_t node_index = skin->joints[joint_index] - data->nodes;
        if (node_index < 0 || (size_t)node_index >= model->motion.node_count ||
            !cgltf_accessor_read_float(skin->inverse_bind_matrices, joint_index,
                                       model->inverse_bind[joint_index], 16)) {
            SDL_SetError("could not decode model skin joint %zu", (size_t)joint_index);
            return false;
        }
        model->joint_nodes[joint_index] = (Uint16)node_index;
    }
    if (vrm) {
        return eidolon_motion_rebuild_world(&model->motion);
    }
    if (!eidolon_motion_finalize(&model->motion)) {
        return false;
    }
    return true;
}

static bool count_geometry(const cgltf_data *data, EidolonCpuGeometry *geometry) {
    for (cgltf_size node_index = 0; node_index < data->nodes_count; ++node_index) {
        const cgltf_node *node = &data->nodes[node_index];
        if (node->mesh == NULL) {
            continue;
        }

        for (cgltf_size primitive_index = 0; primitive_index < node->mesh->primitives_count;
             ++primitive_index) {
            const cgltf_primitive *primitive = &node->mesh->primitives[primitive_index];
            if (!primitive_is_supported(primitive)) {
                continue;
            }

            const cgltf_accessor *positions =
                primitive_attribute(primitive, cgltf_attribute_type_position, 0);
            const size_t index_count = primitive->indices != NULL
                                           ? (size_t)primitive->indices->count
                                           : (size_t)positions->count;
            if (positions->count > SIZE_MAX - geometry->vertex_count ||
                index_count > SIZE_MAX - geometry->index_count ||
                geometry->draw_count == SIZE_MAX) {
                SDL_SetError("model geometry size overflow");
                return false;
            }
            geometry->vertex_count += (size_t)positions->count;
            geometry->index_count += index_count;
            geometry->draw_count += 1;
        }
    }

    if (geometry->vertex_count == 0 || geometry->index_count == 0 || geometry->draw_count == 0) {
        SDL_SetError("model contains no triangle geometry");
        return false;
    }
    if (geometry->vertex_count > UINT32_MAX || geometry->index_count > UINT32_MAX ||
        geometry->draw_count > UINT32_MAX) {
        SDL_SetError("model geometry exceeds 32-bit GPU limits");
        return false;
    }
    return true;
}

static Uint32 texture_index_for_material(const cgltf_data *data, const cgltf_material *material) {
    if (material == NULL || !material->has_pbr_metallic_roughness) {
        return 0;
    }

    const cgltf_texture *texture = material->pbr_metallic_roughness.base_color_texture.texture;
    if (texture == NULL || texture->image == NULL || data->images_count == 0) {
        return 0;
    }

    const ptrdiff_t image_index = texture->image - data->images;
    if (image_index < 0 || (size_t)image_index >= (size_t)data->images_count) {
        return 0;
    }
    return (Uint32)image_index + 1U;
}

static EidolonModelMaterial material_parameters(const cgltf_material *material) {
    EidolonModelMaterial parameters = {
        .base_color_factor = {1.0F, 1.0F, 1.0F, 1.0F},
        .alpha_cutoff = 0.5F,
        .alpha_mode = (float)cgltf_alpha_mode_opaque,
    };
    if (material == NULL) {
        return parameters;
    }
    if (material->has_pbr_metallic_roughness) {
        for (size_t component = 0; component < 4U; ++component) {
            parameters.base_color_factor[component] =
                (float)material->pbr_metallic_roughness.base_color_factor[component];
        }
    }
    parameters.alpha_mode = (float)material->alpha_mode;
    if (material->alpha_mode == cgltf_alpha_mode_mask) {
        parameters.alpha_cutoff = (float)material->alpha_cutoff;
    }
    return parameters;
}

static void transform_position(const cgltf_float matrix[16], const cgltf_float source[3],
                               float destination[3]) {
    destination[0] =
        (float)(matrix[0] * source[0] + matrix[4] * source[1] + matrix[8] * source[2] + matrix[12]);
    destination[1] =
        (float)(matrix[1] * source[0] + matrix[5] * source[1] + matrix[9] * source[2] + matrix[13]);
    destination[2] = (float)(matrix[2] * source[0] + matrix[6] * source[1] +
                             matrix[10] * source[2] + matrix[14]);
}

static void transform_direction(const cgltf_float matrix[16], const cgltf_float source[3],
                                float destination[3]) {
    destination[0] = (float)(matrix[0] * source[0] + matrix[4] * source[1] + matrix[8] * source[2]);
    destination[1] = (float)(matrix[1] * source[0] + matrix[5] * source[1] + matrix[9] * source[2]);
    destination[2] =
        (float)(matrix[2] * source[0] + matrix[6] * source[1] + matrix[10] * source[2]);
}

static const cgltf_accessor *morph_position_accessor(const cgltf_primitive *primitive,
                                                     size_t target) {
    if (target >= (size_t)primitive->targets_count) {
        return NULL;
    }
    const cgltf_morph_target *morph = &primitive->targets[target];
    for (cgltf_size index = 0; index < morph->attributes_count; ++index) {
        const cgltf_attribute *attribute = &morph->attributes[index];
        if (attribute->type == cgltf_attribute_type_position && attribute->index == 0) {
            return attribute->data;
        }
    }
    return NULL;
}

static bool accumulate_expression_delta(const cgltf_primitive *primitive, size_t node_index,
                                        size_t vertex_index, const cgltf_float world[16],
                                        const EidolonVrmExpressionBind *binds, size_t bind_count,
                                        float destination[3]) {
    for (size_t bind_index = 0; bind_index < bind_count; ++bind_index) {
        const EidolonVrmExpressionBind *bind = &binds[bind_index];
        if (bind->node != node_index || bind->weight <= 0.0F) {
            continue;
        }
        const cgltf_accessor *accessor = morph_position_accessor(primitive, bind->target);
        cgltf_float delta[3];
        float transformed[3];
        if (accessor == NULL || vertex_index >= (size_t)accessor->count ||
            !cgltf_accessor_read_float(accessor, (cgltf_size)vertex_index, delta,
                                       SDL_arraysize(delta))) {
            SDL_SetError("VRM expression morph target could not be decoded");
            return false;
        }
        transform_direction(world, delta, transformed);
        for (size_t axis = 0; axis < 3U; ++axis) {
            destination[axis] += transformed[axis] * bind->weight;
        }
    }
    return true;
}

static bool fill_geometry(const cgltf_data *data, size_t identity_joint,
                          const EidolonVrmBody *vrm_body, EidolonCpuGeometry *geometry) {
    size_t vertex_cursor = 0;
    size_t index_cursor = 0;
    size_t draw_cursor = 0;

    for (cgltf_size node_index = 0; node_index < data->nodes_count; ++node_index) {
        const cgltf_node *node = &data->nodes[node_index];
        if (node->mesh == NULL) {
            continue;
        }

        cgltf_float world[16];
        if (node->skin != NULL) {
            const cgltf_float identity[16] = {
                1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
            };
            SDL_memcpy(world, identity, sizeof(world));
        } else {
            cgltf_node_transform_world(node, world);
        }

        for (cgltf_size primitive_index = 0; primitive_index < node->mesh->primitives_count;
             ++primitive_index) {
            const cgltf_primitive *primitive = &node->mesh->primitives[primitive_index];
            if (!primitive_is_supported(primitive)) {
                continue;
            }

            const cgltf_accessor *positions =
                primitive_attribute(primitive, cgltf_attribute_type_position, 0);
            const cgltf_accessor *texcoords =
                primitive_attribute(primitive, cgltf_attribute_type_texcoord, 0);
            const cgltf_accessor *joints =
                primitive_attribute(primitive, cgltf_attribute_type_joints, 0);
            const cgltf_accessor *weights =
                primitive_attribute(primitive, cgltf_attribute_type_weights, 0);
            if (node->skin != NULL && (joints == NULL || weights == NULL)) {
                SDL_SetError("skinned model primitive has no joints or weights");
                return false;
            }
            const size_t primitive_vertex_count = (size_t)positions->count;
            const size_t primitive_index_count = primitive->indices != NULL
                                                     ? (size_t)primitive->indices->count
                                                     : primitive_vertex_count;

            for (size_t vertex_index = 0; vertex_index < primitive_vertex_count; ++vertex_index) {
                cgltf_float position[3];
                if (!cgltf_accessor_read_float(positions, (cgltf_size)vertex_index, position,
                                               SDL_arraysize(position))) {
                    SDL_SetError("could not read model position accessor");
                    return false;
                }
                transform_position(world, position,
                                   geometry->vertices[vertex_cursor + vertex_index].position);
                EidolonModelVertex *vertex = &geometry->vertices[vertex_cursor + vertex_index];
                if (vrm_body != NULL && vrm_body->has_expression &&
                    (!accumulate_expression_delta(primitive, (size_t)node_index, vertex_index,
                                                  world, vrm_body->neutral_binds,
                                                  vrm_body->neutral_bind_count,
                                                  vertex->neutral_delta) ||
                     !accumulate_expression_delta(primitive, (size_t)node_index, vertex_index,
                                                  world, vrm_body->focused_binds,
                                                  vrm_body->focused_bind_count,
                                                  vertex->focused_delta))) {
                    return false;
                }

                if (texcoords != NULL) {
                    cgltf_float texcoord[2];
                    if (!cgltf_accessor_read_float(texcoords, (cgltf_size)vertex_index, texcoord,
                                                   SDL_arraysize(texcoord))) {
                        SDL_SetError("could not read model texture-coordinate accessor");
                        return false;
                    }
                    geometry->vertices[vertex_cursor + vertex_index].texcoord[0] =
                        (float)texcoord[0];
                    geometry->vertices[vertex_cursor + vertex_index].texcoord[1] =
                        (float)texcoord[1];
                }

                if (node->skin != NULL) {
                    cgltf_uint joint_values[4];
                    cgltf_float weight_values[4];
                    if (!cgltf_accessor_read_uint(joints, (cgltf_size)vertex_index, joint_values,
                                                  SDL_arraysize(joint_values)) ||
                        !cgltf_accessor_read_float(weights, (cgltf_size)vertex_index, weight_values,
                                                   SDL_arraysize(weight_values))) {
                        SDL_SetError("could not read model skin attributes");
                        return false;
                    }
                    float weight_sum = 0.0F;
                    for (size_t influence = 0; influence < 4; ++influence) {
                        if ((size_t)joint_values[influence] >= identity_joint) {
                            SDL_SetError("model vertex references invalid skin joint");
                            return false;
                        }
                        vertex->joints[influence] = (Uint16)joint_values[influence];
                        vertex->weights[influence] = (float)weight_values[influence];
                        weight_sum += vertex->weights[influence];
                    }
                    if (weight_sum > 0.000001F) {
                        for (size_t influence = 0; influence < 4; ++influence) {
                            vertex->weights[influence] /= weight_sum;
                        }
                    } else {
                        vertex->joints[0] = (Uint16)identity_joint;
                        vertex->weights[0] = 1.0F;
                    }
                } else {
                    vertex->joints[0] = (Uint16)identity_joint;
                    vertex->weights[0] = 1.0F;
                }
            }

            for (size_t primitive_index_cursor = 0; primitive_index_cursor < primitive_index_count;
                 ++primitive_index_cursor) {
                const size_t local_index =
                    primitive->indices != NULL
                        ? (size_t)cgltf_accessor_read_index(primitive->indices,
                                                            (cgltf_size)primitive_index_cursor)
                        : primitive_index_cursor;
                if (local_index >= primitive_vertex_count) {
                    SDL_SetError("model index lies outside its vertex accessor");
                    return false;
                }
                geometry->indices[index_cursor + primitive_index_cursor] =
                    (Uint32)(vertex_cursor + local_index);
            }

            geometry->draws[draw_cursor] = (EidolonModelDraw){
                .first_index = (Uint32)index_cursor,
                .index_count = (Uint32)primitive_index_count,
                .texture_index = texture_index_for_material(data, primitive->material),
                .material = material_parameters(primitive->material),
                .alpha_blend = primitive->material != NULL &&
                               primitive->material->alpha_mode == cgltf_alpha_mode_blend,
            };
            vertex_cursor += primitive_vertex_count;
            index_cursor += primitive_index_count;
            draw_cursor += 1;
        }
    }
    return true;
}

static bool build_cpu_geometry(const cgltf_data *data, size_t identity_joint,
                               const EidolonVrmBody *vrm_body, EidolonCpuGeometry *geometry) {
    SDL_zero(*geometry);
    if (!count_geometry(data, geometry)) {
        return false;
    }

    geometry->vertices = SDL_calloc(geometry->vertex_count, sizeof(*geometry->vertices));
    geometry->indices = SDL_calloc(geometry->index_count, sizeof(*geometry->indices));
    geometry->draws = SDL_calloc(geometry->draw_count, sizeof(*geometry->draws));
    if (geometry->vertices == NULL || geometry->indices == NULL || geometry->draws == NULL) {
        SDL_SetError("out of memory while decoding model geometry");
        cpu_geometry_destroy(geometry);
        return false;
    }

    if (!fill_geometry(data, identity_joint, vrm_body, geometry)) {
        cpu_geometry_destroy(geometry);
        return false;
    }
    return true;
}

#if defined(_WIN32)
static bool d3d11_succeeded(HRESULT result, const char *operation) {
    if (SUCCEEDED(result)) {
        return true;
    }
    SDL_SetError("%s failed (HRESULT 0x%08lx)", operation, (unsigned long)result);
    return false;
}

static bool acquire_renderer_device(EidolonModelRenderer *model, SDL_Renderer *renderer) {
    const char *renderer_name = SDL_GetRendererName(renderer);
    if (renderer_name == NULL || SDL_strcmp(renderer_name, "direct3d11") != 0) {
        SDL_SetError("Rio requires SDL's direct3d11 renderer on Windows (active renderer: %s)",
                     renderer_name != NULL ? renderer_name : "unknown");
        return false;
    }

    model->renderer = renderer;
    model->device = SDL_GetPointerProperty(SDL_GetRendererProperties(renderer),
                                           SDL_PROP_RENDERER_D3D11_DEVICE_POINTER, NULL);
    if (model->device == NULL) {
        SDL_SetError("SDL direct3d11 renderer did not expose its D3D11 device");
        return false;
    }
    ID3D11Device_GetImmediateContext(model->device, &model->context);
    if (model->context == NULL) {
        SDL_SetError("could not acquire the SDL renderer's D3D11 immediate context");
        return false;
    }
    return true;
}

static bool upload_geometry(EidolonModelRenderer *model, const EidolonCpuGeometry *geometry) {
    const size_t vertex_bytes_size = geometry->vertex_count * sizeof(*geometry->vertices);
    const size_t index_bytes_size = geometry->index_count * sizeof(*geometry->indices);
    Uint32 vertex_bytes = 0;
    Uint32 index_bytes = 0;
    if (!size_to_u32(vertex_bytes_size, &vertex_bytes, "model vertex buffer") ||
        !size_to_u32(index_bytes_size, &index_bytes, "model index buffer")) {
        return false;
    }

    const D3D11_BUFFER_DESC vertex_description = {
        .ByteWidth = vertex_bytes,
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
    };
    const D3D11_SUBRESOURCE_DATA vertex_data = {.pSysMem = geometry->vertices};
    if (!d3d11_succeeded(ID3D11Device_CreateBuffer(model->device, &vertex_description, &vertex_data,
                                                   &model->vertex_buffer),
                         "creating D3D11 model vertex buffer")) {
        return false;
    }

    const D3D11_BUFFER_DESC index_description = {
        .ByteWidth = index_bytes,
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
    };
    const D3D11_SUBRESOURCE_DATA index_data = {.pSysMem = geometry->indices};
    return d3d11_succeeded(ID3D11Device_CreateBuffer(model->device, &index_description, &index_data,
                                                     &model->index_buffer),
                           "creating D3D11 model index buffer");
}

static ID3D11ShaderResourceView *upload_rgba_texture(EidolonModelRenderer *model,
                                                     const Uint8 *pixels, int width, int height,
                                                     size_t pitch, const char *name) {
    (void)name;
    if (width <= 0 || height <= 0 || pitch > UINT_MAX) {
        SDL_SetError("invalid model texture dimensions or pitch");
        return NULL;
    }

    const D3D11_TEXTURE2D_DESC description = {
        .Width = (UINT)width,
        .Height = (UINT)height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {.Count = 1},
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    };
    const D3D11_SUBRESOURCE_DATA initial_data = {
        .pSysMem = pixels,
        .SysMemPitch = (UINT)pitch,
    };
    ID3D11Texture2D *texture = NULL;
    if (!d3d11_succeeded(
            ID3D11Device_CreateTexture2D(model->device, &description, &initial_data, &texture),
            "creating D3D11 model texture")) {
        return NULL;
    }

    ID3D11ShaderResourceView *view = NULL;
    const HRESULT result = ID3D11Device_CreateShaderResourceView(
        model->device, (ID3D11Resource *)texture, NULL, &view);
    ID3D11Texture2D_Release(texture);
    if (!d3d11_succeeded(result, "creating D3D11 model texture view")) {
        return NULL;
    }
    return view;
}

static SDL_Surface *load_embedded_png(const cgltf_image *image) {
    if (image->buffer_view == NULL) {
        SDL_SetError("model image '%s' is not embedded in the GLB",
                     image->name != NULL ? image->name : "unnamed");
        return NULL;
    }
    const Uint8 *bytes = cgltf_buffer_view_data(image->buffer_view);
    if (bytes == NULL) {
        SDL_SetError("model image '%s' has no loaded buffer",
                     image->name != NULL ? image->name : "unnamed");
        return NULL;
    }

    SDL_IOStream *stream = SDL_IOFromConstMem(bytes, (size_t)image->buffer_view->size);
    if (stream == NULL) {
        return NULL;
    }
    SDL_Surface *surface = SDL_LoadPNG_IO(stream, true);
    if (surface == NULL) {
        return NULL;
    }
    if (surface->format == SDL_PIXELFORMAT_ABGR8888) {
        return surface;
    }

    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    return converted;
}

static bool upload_textures(EidolonModelRenderer *model, const cgltf_data *data) {
    if ((size_t)data->images_count >= UINT32_MAX) {
        SDL_SetError("model has too many images");
        return false;
    }
    model->texture_count = (size_t)data->images_count + 1U;
    model->textures = SDL_calloc(model->texture_count, sizeof(*model->textures));
    if (model->textures == NULL) {
        SDL_SetError("out of memory while loading model textures");
        return false;
    }

    const Uint8 white[] = {255, 255, 255, 255};
    model->textures[0] = upload_rgba_texture(model, white, 1, 1, sizeof(white), "white");
    if (model->textures[0] == NULL) {
        return false;
    }

    for (cgltf_size image_index = 0; image_index < data->images_count; ++image_index) {
        const cgltf_image *image = &data->images[image_index];
        SDL_Surface *surface = load_embedded_png(image);
        if (surface == NULL) {
            return false;
        }
        model->textures[(size_t)image_index + 1U] = upload_rgba_texture(
            model, surface->pixels, surface->w, surface->h, (size_t)surface->pitch, image->name);
        SDL_DestroySurface(surface);
        if (model->textures[(size_t)image_index + 1U] == NULL) {
            return false;
        }
    }
    return true;
}

static void *load_d3d11_shader(const char *shader_directory, const char *name, size_t *size) {
    char path[1024];
    const int path_length =
        SDL_snprintf(path, sizeof(path), "%s/DXBC/%s.cso", shader_directory, name);
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) {
        SDL_SetError("model shader path is too long");
        return NULL;
    }
    void *code = SDL_LoadFile(path, size);
    if (code == NULL) {
        SDL_SetError("could not load D3D11 model shader %s: %s", path, SDL_GetError());
    }
    return code;
}

static bool create_constant_buffer(EidolonModelRenderer *model, UINT bytes, ID3D11Buffer **buffer) {
    const D3D11_BUFFER_DESC description = {
        .ByteWidth = bytes,
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    return d3d11_succeeded(ID3D11Device_CreateBuffer(model->device, &description, NULL, buffer),
                           "creating D3D11 model constant buffer");
}

static bool create_pipeline(EidolonModelRenderer *model, const char *shader_directory) {
    size_t vertex_size = 0;
    size_t pixel_size = 0;
    void *vertex_code = load_d3d11_shader(shader_directory, "model.vert", &vertex_size);
    if (vertex_code == NULL) {
        return false;
    }
    void *pixel_code = load_d3d11_shader(shader_directory, "model.frag", &pixel_size);
    if (pixel_code == NULL) {
        SDL_free(vertex_code);
        return false;
    }

    bool created =
        d3d11_succeeded(ID3D11Device_CreateVertexShader(model->device, vertex_code, vertex_size,
                                                        NULL, &model->vertex_shader),
                        "creating D3D11 model vertex shader");
    if (created) {
        created =
            d3d11_succeeded(ID3D11Device_CreatePixelShader(model->device, pixel_code, pixel_size,
                                                           NULL, &model->pixel_shader),
                            "creating D3D11 model pixel shader");
    }

    const D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         (UINT)offsetof(EidolonModelVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         (UINT)offsetof(EidolonModelVertex, neutral_delta), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"POSITION", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         (UINT)offsetof(EidolonModelVertex, focused_delta), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof(EidolonModelVertex, texcoord),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0,
         (UINT)offsetof(EidolonModelVertex, joints), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         (UINT)offsetof(EidolonModelVertex, weights), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    if (created) {
        created = d3d11_succeeded(
            ID3D11Device_CreateInputLayout(model->device, elements, SDL_arraysize(elements),
                                           vertex_code, vertex_size, &model->input_layout),
            "creating D3D11 model input layout");
    }
    SDL_free(pixel_code);
    SDL_free(vertex_code);
    if (!created ||
        !create_constant_buffer(model, sizeof(EidolonModelScene), &model->scene_buffer) ||
        !create_constant_buffer(model, sizeof(EidolonModelMaterial), &model->material_buffer) ||
        !create_constant_buffer(model, sizeof(model->joint_palette), &model->bones_buffer)) {
        return false;
    }

    const D3D11_RASTERIZER_DESC rasterizer = {
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_NONE,
        .FrontCounterClockwise = TRUE,
        .DepthClipEnable = TRUE,
    };
    if (!d3d11_succeeded(
            ID3D11Device_CreateRasterizerState(model->device, &rasterizer, &model->rasterizer),
            "creating D3D11 model rasterizer")) {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depth = {
        .DepthEnable = TRUE,
        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D11_COMPARISON_LESS_EQUAL,
    };
    if (!d3d11_succeeded(
            ID3D11Device_CreateDepthStencilState(model->device, &depth, &model->depth_state),
            "creating D3D11 model depth state")) {
        return false;
    }
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    if (!d3d11_succeeded(ID3D11Device_CreateDepthStencilState(model->device, &depth,
                                                              &model->depth_state_no_write),
                         "creating D3D11 transparent model depth state")) {
        return false;
    }
    const D3D11_BLEND_DESC blend = {
        .RenderTarget = {{
            .BlendEnable = TRUE,
            .SrcBlend = D3D11_BLEND_SRC_ALPHA,
            .DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
            .BlendOp = D3D11_BLEND_OP_ADD,
            .SrcBlendAlpha = D3D11_BLEND_ONE,
            .DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
            .BlendOpAlpha = D3D11_BLEND_OP_ADD,
            .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
        }},
    };
    return d3d11_succeeded(
        ID3D11Device_CreateBlendState(model->device, &blend, &model->alpha_blend),
        "creating D3D11 transparent model blend state");
}

static bool create_color_target_view(EidolonModelRenderer *model, SDL_Texture *renderer_texture,
                                     ID3D11RenderTargetView **result) {
    ID3D11Texture2D *color_texture = SDL_GetPointerProperty(
        SDL_GetTextureProperties(renderer_texture), SDL_PROP_TEXTURE_D3D11_TEXTURE_POINTER, NULL);
    if (color_texture == NULL) {
        SDL_SetError("SDL target texture did not expose its D3D11 resource");
        return false;
    }

    return d3d11_succeeded(ID3D11Device_CreateRenderTargetView(
                               model->device, (ID3D11Resource *)color_texture, NULL, result),
                           "creating D3D11 model render target");
}

static bool replace_targets(EidolonModelRenderer *model, int side) {
    SDL_Texture *replacement_texture = SDL_CreateTexture(model->renderer, SDL_PIXELFORMAT_ABGR8888,
                                                         SDL_TEXTUREACCESS_TARGET, side, side);
    ID3D11RenderTargetView *replacement_rtv = NULL;
    ID3D11Texture2D *replacement_depth = NULL;
    ID3D11DepthStencilView *replacement_dsv = NULL;
    bool created = replacement_texture != NULL;
    if (created) {
        created = SDL_SetTextureBlendMode(replacement_texture, SDL_BLENDMODE_BLEND) &&
                  SDL_SetTextureScaleMode(replacement_texture, SDL_SCALEMODE_LINEAR) &&
                  create_color_target_view(model, replacement_texture, &replacement_rtv);
    }
    const D3D11_TEXTURE2D_DESC depth_description = {
        .Width = (UINT)side,
        .Height = (UINT)side,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_D16_UNORM,
        .SampleDesc = {.Count = 1},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_DEPTH_STENCIL,
    };
    if (created) {
        created = d3d11_succeeded(ID3D11Device_CreateTexture2D(model->device, &depth_description,
                                                               NULL, &replacement_depth),
                                  "creating D3D11 model depth texture");
    }
    if (created) {
        created = d3d11_succeeded(
            ID3D11Device_CreateDepthStencilView(model->device, (ID3D11Resource *)replacement_depth,
                                                NULL, &replacement_dsv),
            "creating D3D11 model depth view");
    }
    if (created) {
        created = SDL_FlushRenderer(model->renderer);
    }
    if (!created) {
        if (replacement_dsv != NULL) {
            ID3D11DepthStencilView_Release(replacement_dsv);
        }
        if (replacement_depth != NULL) {
            ID3D11Texture2D_Release(replacement_depth);
        }
        if (replacement_rtv != NULL) {
            ID3D11RenderTargetView_Release(replacement_rtv);
        }
        if (replacement_texture != NULL) {
            SDL_DestroyTexture(replacement_texture);
        }
        return false;
    }

    SDL_Texture *old_texture = model->renderer_texture;
    ID3D11RenderTargetView *old_rtv = model->color_rtv;
    ID3D11Texture2D *old_depth = model->depth_texture;
    ID3D11DepthStencilView *old_dsv = model->depth_dsv;
    model->renderer_texture = replacement_texture;
    model->color_rtv = replacement_rtv;
    model->depth_texture = replacement_depth;
    model->depth_dsv = replacement_dsv;
    model->target_width = side;
    model->target_height = side;

    if (old_dsv != NULL) {
        ID3D11DepthStencilView_Release(old_dsv);
    }
    if (old_depth != NULL) {
        ID3D11Texture2D_Release(old_depth);
    }
    if (old_rtv != NULL) {
        ID3D11RenderTargetView_Release(old_rtv);
    }
    if (old_texture != NULL) {
        SDL_DestroyTexture(old_texture);
    }
    return true;
}

static bool refresh_color_target_view(EidolonModelRenderer *model) {
    ID3D11RenderTargetView *replacement = NULL;
    if (!create_color_target_view(model, model->renderer_texture, &replacement)) {
        return false;
    }
    ID3D11RenderTargetView *old_rtv = model->color_rtv;
    model->color_rtv = replacement;
    if (old_rtv != NULL) {
        ID3D11RenderTargetView_Release(old_rtv);
    }
    return true;
}

static bool create_targets(EidolonModelRenderer *model, SDL_Renderer *renderer) {
    (void)renderer;
    return replace_targets(model, EIDOLON_MODEL_RENDER_RESOLUTION_DEFAULT);
}

static bool create_sampler(EidolonModelRenderer *model) {
    const D3D11_SAMPLER_DESC description = {
        .Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
        .ComparisonFunc = D3D11_COMPARISON_NEVER,
        .MaxLOD = FLT_MAX,
    };
    return d3d11_succeeded(
        ID3D11Device_CreateSamplerState(model->device, &description, &model->sampler),
        "creating D3D11 model sampler");
}

static bool update_joint_palette(EidolonModelRenderer *model) {
    for (size_t joint_index = 0; joint_index < model->joint_count; ++joint_index) {
        const float *world =
            eidolon_motion_world(&model->motion, (size_t)model->joint_nodes[joint_index]);
        if (world == NULL) {
            return false;
        }
        matrix_multiply(world, model->inverse_bind[joint_index], model->joint_palette[joint_index]);
    }
    matrix_identity(model->joint_palette[model->joint_count]);
    return true;
}

static bool write_constant_buffer(EidolonModelRenderer *model, ID3D11Buffer *buffer,
                                  const void *data, size_t bytes) {
    D3D11_MAPPED_SUBRESOURCE mapped;
    const HRESULT result = ID3D11DeviceContext_Map(model->context, (ID3D11Resource *)buffer, 0,
                                                   D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (!d3d11_succeeded(result, "mapping D3D11 model constant buffer")) {
        return false;
    }
    SDL_memcpy(mapped.pData, data, bytes);
    ID3D11DeviceContext_Unmap(model->context, (ID3D11Resource *)buffer, 0);
    return true;
}

typedef struct EidolonD3D11State {
    ID3D11RenderTargetView *render_target;
    ID3D11DepthStencilView *depth_view;
    ID3D11BlendState *blend_state;
    FLOAT blend_factor[4];
    UINT sample_mask;
    ID3D11DepthStencilState *depth_state;
    UINT stencil_reference;
    ID3D11RasterizerState *rasterizer;
    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    UINT viewport_count;
    ID3D11InputLayout *input_layout;
    D3D11_PRIMITIVE_TOPOLOGY topology;
    ID3D11Buffer *vertex_buffer;
    UINT vertex_stride;
    UINT vertex_offset;
    ID3D11Buffer *index_buffer;
    DXGI_FORMAT index_format;
    UINT index_offset;
    ID3D11VertexShader *vertex_shader;
    ID3D11Buffer *vertex_constant_buffers[2];
    ID3D11PixelShader *pixel_shader;
    ID3D11ShaderResourceView *pixel_resource;
    ID3D11SamplerState *pixel_sampler;
} EidolonD3D11State;

static void save_d3d11_state(ID3D11DeviceContext *context, EidolonD3D11State *state) {
    SDL_zero(*state);
    ID3D11DeviceContext_OMGetRenderTargets(context, 1, &state->render_target, &state->depth_view);
    ID3D11DeviceContext_OMGetBlendState(context, &state->blend_state, state->blend_factor,
                                        &state->sample_mask);
    ID3D11DeviceContext_OMGetDepthStencilState(context, &state->depth_state,
                                               &state->stencil_reference);
    ID3D11DeviceContext_RSGetState(context, &state->rasterizer);
    state->viewport_count = SDL_arraysize(state->viewports);
    ID3D11DeviceContext_RSGetViewports(context, &state->viewport_count, state->viewports);
    ID3D11DeviceContext_IAGetInputLayout(context, &state->input_layout);
    ID3D11DeviceContext_IAGetPrimitiveTopology(context, &state->topology);
    ID3D11DeviceContext_IAGetVertexBuffers(context, 0, 1, &state->vertex_buffer,
                                           &state->vertex_stride, &state->vertex_offset);
    ID3D11DeviceContext_IAGetIndexBuffer(context, &state->index_buffer, &state->index_format,
                                         &state->index_offset);
    UINT class_instance_count = 0;
    ID3D11DeviceContext_VSGetShader(context, &state->vertex_shader, NULL, &class_instance_count);
    ID3D11DeviceContext_VSGetConstantBuffers(
        context, 0, SDL_arraysize(state->vertex_constant_buffers), state->vertex_constant_buffers);
    class_instance_count = 0;
    ID3D11DeviceContext_PSGetShader(context, &state->pixel_shader, NULL, &class_instance_count);
    ID3D11DeviceContext_PSGetShaderResources(context, 0, 1, &state->pixel_resource);
    ID3D11DeviceContext_PSGetSamplers(context, 0, 1, &state->pixel_sampler);
}

static void restore_d3d11_state(ID3D11DeviceContext *context, EidolonD3D11State *state) {
    ID3D11DeviceContext_OMSetRenderTargets(context, 1, &state->render_target, state->depth_view);
    ID3D11DeviceContext_OMSetBlendState(context, state->blend_state, state->blend_factor,
                                        state->sample_mask);
    ID3D11DeviceContext_OMSetDepthStencilState(context, state->depth_state,
                                               state->stencil_reference);
    ID3D11DeviceContext_RSSetState(context, state->rasterizer);
    ID3D11DeviceContext_RSSetViewports(context, state->viewport_count, state->viewports);
    ID3D11DeviceContext_IASetInputLayout(context, state->input_layout);
    ID3D11DeviceContext_IASetPrimitiveTopology(context, state->topology);
    ID3D11DeviceContext_IASetVertexBuffers(context, 0, 1, &state->vertex_buffer,
                                           &state->vertex_stride, &state->vertex_offset);
    ID3D11DeviceContext_IASetIndexBuffer(context, state->index_buffer, state->index_format,
                                         state->index_offset);
    ID3D11DeviceContext_VSSetShader(context, state->vertex_shader, NULL, 0);
    ID3D11DeviceContext_VSSetConstantBuffers(
        context, 0, SDL_arraysize(state->vertex_constant_buffers), state->vertex_constant_buffers);
    ID3D11DeviceContext_PSSetShader(context, state->pixel_shader, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(context, 0, 1, &state->pixel_resource);
    ID3D11DeviceContext_PSSetSamplers(context, 0, 1, &state->pixel_sampler);

    if (state->pixel_sampler != NULL) {
        ID3D11SamplerState_Release(state->pixel_sampler);
    }
    if (state->pixel_resource != NULL) {
        ID3D11ShaderResourceView_Release(state->pixel_resource);
    }
    if (state->pixel_shader != NULL) {
        ID3D11PixelShader_Release(state->pixel_shader);
    }
    for (size_t index = 0; index < SDL_arraysize(state->vertex_constant_buffers); ++index) {
        if (state->vertex_constant_buffers[index] != NULL) {
            ID3D11Buffer_Release(state->vertex_constant_buffers[index]);
        }
    }
    if (state->vertex_shader != NULL) {
        ID3D11VertexShader_Release(state->vertex_shader);
    }
    if (state->index_buffer != NULL) {
        ID3D11Buffer_Release(state->index_buffer);
    }
    if (state->vertex_buffer != NULL) {
        ID3D11Buffer_Release(state->vertex_buffer);
    }
    if (state->input_layout != NULL) {
        ID3D11InputLayout_Release(state->input_layout);
    }
    if (state->rasterizer != NULL) {
        ID3D11RasterizerState_Release(state->rasterizer);
    }
    if (state->depth_state != NULL) {
        ID3D11DepthStencilState_Release(state->depth_state);
    }
    if (state->blend_state != NULL) {
        ID3D11BlendState_Release(state->blend_state);
    }
    if (state->depth_view != NULL) {
        ID3D11DepthStencilView_Release(state->depth_view);
    }
    if (state->render_target != NULL) {
        ID3D11RenderTargetView_Release(state->render_target);
    }
}

static bool submit_model_frame(EidolonModelRenderer *model, uint64_t now_ms) {
    float projection[16];
    float yaw_rotation[16];
    float pitch_rotation[16];
    float roll_rotation[16];
    float yaw_pitch_rotation[16];
    float model_rotation[16];
    float from_pivot[16];
    float to_pivot[16];
    float rotation_from_pivot[16];
    float centered_rotation[16];
    EidolonModelScene scene;
    model_projection(model, projection);
    matrix_rotation_y(model->yaw_radians, yaw_rotation);
    matrix_rotation_x(model->pitch_radians, pitch_rotation);
    matrix_rotation_z(model->roll_radians, roll_rotation);
    matrix_multiply(yaw_rotation, pitch_rotation, yaw_pitch_rotation);
    matrix_multiply(yaw_pitch_rotation, roll_rotation, model_rotation);
    matrix_translation(-model->rotation_pivot[0], -model->rotation_pivot[1],
                       -model->rotation_pivot[2], from_pivot);
    matrix_translation(model->rotation_pivot[0], model->rotation_pivot[1], model->rotation_pivot[2],
                       to_pivot);
    matrix_multiply(model_rotation, from_pivot, rotation_from_pivot);
    matrix_multiply(to_pivot, rotation_from_pivot, centered_rotation);
    matrix_multiply(projection, centered_rotation, scene.model_view_projection);
    scene.expression_weights[0] =
        model->vrm_ready ? 1.0F - model->vrm_projection.focused_expression_weight : 0.0F;
    scene.expression_weights[1] =
        model->vrm_ready ? model->vrm_projection.focused_expression_weight : 0.0F;
    scene.expression_weights[2] = 0.0F;
    scene.expression_weights[3] = 0.0F;

    if (!model->vrm_ready) {
        eidolon_motion_update_idle(&model->motion, now_ms);
        if (model->semantic_pose_active && model->humanoid_ready && !model->semantic_pose_failed &&
            !eidolon_pose_solve(&model->motion, &model->humanoid, &model->semantic_pose)) {
            eidolon_log_write("motion", "semantic pose disabled after solve failure: %s",
                              SDL_GetError());
            model->semantic_pose_failed = true;
            SDL_ClearError();
            eidolon_motion_update_idle(&model->motion, now_ms);
        }
    }
    if (!update_joint_palette(model) || !SDL_FlushRenderer(model->renderer) ||
        !write_constant_buffer(model, model->scene_buffer, &scene, sizeof(scene)) ||
        !write_constant_buffer(model, model->bones_buffer, model->joint_palette,
                               sizeof(model->joint_palette))) {
        return false;
    }

    EidolonD3D11State saved_state;
    save_d3d11_state(model->context, &saved_state);

    static const FLOAT clear_color[4] = {0.0F, 0.0F, 0.0F, 0.0F};
    ID3D11ShaderResourceView *null_view = NULL;
    ID3D11DeviceContext_PSSetShaderResources(model->context, 0, 1, &null_view);
    ID3D11DeviceContext_OMSetRenderTargets(model->context, 1, &model->color_rtv, model->depth_dsv);
    ID3D11DeviceContext_ClearRenderTargetView(model->context, model->color_rtv, clear_color);
    ID3D11DeviceContext_ClearDepthStencilView(model->context, model->depth_dsv, D3D11_CLEAR_DEPTH,
                                              1.0F, 0);

    const D3D11_VIEWPORT viewport = {
        .Width = (FLOAT)model->target_width,
        .Height = (FLOAT)model->target_height,
        .MaxDepth = 1.0F,
    };
    const UINT stride = sizeof(EidolonModelVertex);
    const UINT offset = 0;
    ID3D11DeviceContext_RSSetViewports(model->context, 1, &viewport);
    ID3D11DeviceContext_RSSetState(model->context, model->rasterizer);
    ID3D11DeviceContext_IASetInputLayout(model->context, model->input_layout);
    ID3D11DeviceContext_IASetPrimitiveTopology(model->context,
                                               D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_IASetVertexBuffers(model->context, 0, 1, &model->vertex_buffer, &stride,
                                           &offset);
    ID3D11DeviceContext_IASetIndexBuffer(model->context, model->index_buffer, DXGI_FORMAT_R32_UINT,
                                         0);
    ID3D11DeviceContext_VSSetShader(model->context, model->vertex_shader, NULL, 0);
    ID3D11DeviceContext_PSSetShader(model->context, model->pixel_shader, NULL, 0);
    ID3D11Buffer *constant_buffers[] = {model->scene_buffer, model->bones_buffer};
    ID3D11DeviceContext_VSSetConstantBuffers(model->context, 0, SDL_arraysize(constant_buffers),
                                             constant_buffers);
    ID3D11DeviceContext_PSSetSamplers(model->context, 0, 1, &model->sampler);
    ID3D11DeviceContext_PSSetConstantBuffers(model->context, 0, 1, &model->material_buffer);

    bool rendered = true;
    for (size_t draw_index = 0; draw_index < model->draw_count; ++draw_index) {
        const EidolonModelDraw *draw = &model->draws[draw_index];
        const size_t texture_index =
            draw->texture_index < model->texture_count ? (size_t)draw->texture_index : 0;
        if (!write_constant_buffer(model, model->material_buffer, &draw->material,
                                   sizeof(draw->material))) {
            rendered = false;
            break;
        }
        ID3D11DeviceContext_OMSetDepthStencilState(
            model->context, draw->alpha_blend ? model->depth_state_no_write : model->depth_state,
            0);
        ID3D11DeviceContext_OMSetBlendState(
            model->context, draw->alpha_blend ? model->alpha_blend : NULL, NULL, UINT_MAX);
        ID3D11DeviceContext_PSSetShaderResources(model->context, 0, 1,
                                                 &model->textures[texture_index]);
        ID3D11DeviceContext_DrawIndexed(model->context, draw->index_count, draw->first_index, 0);
    }

    ID3D11DeviceContext_PSSetShaderResources(model->context, 0, 1, &null_view);
    ID3D11DeviceContext_OMSetRenderTargets(model->context, 0, NULL, NULL);
    restore_d3d11_state(model->context, &saved_state);
    if (!rendered) {
        return false;
    }
    model->last_submit_ms = now_ms;
    model->presented_frame_sequence += 1U;
    model->presented_transform_revision = model->transform_revision;
    return true;
}

#else
static bool upload_geometry(EidolonModelRenderer *model, const EidolonCpuGeometry *geometry) {
    const size_t vertex_bytes_size = geometry->vertex_count * sizeof(*geometry->vertices);
    const size_t index_bytes_size = geometry->index_count * sizeof(*geometry->indices);
    if (vertex_bytes_size > SIZE_MAX - index_bytes_size) {
        SDL_SetError("model upload size overflow");
        return false;
    }

    Uint32 vertex_bytes = 0;
    Uint32 index_bytes = 0;
    Uint32 transfer_bytes = 0;
    if (!size_to_u32(vertex_bytes_size, &vertex_bytes, "model vertex buffer") ||
        !size_to_u32(index_bytes_size, &index_bytes, "model index buffer") ||
        !size_to_u32(vertex_bytes_size + index_bytes_size, &transfer_bytes,
                     "model transfer buffer")) {
        return false;
    }

    model->vertex_buffer = SDL_CreateGPUBuffer(
        model->device,
        &(SDL_GPUBufferCreateInfo){.usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = vertex_bytes});
    model->index_buffer = SDL_CreateGPUBuffer(
        model->device,
        &(SDL_GPUBufferCreateInfo){.usage = SDL_GPU_BUFFERUSAGE_INDEX, .size = index_bytes});
    if (model->vertex_buffer == NULL || model->index_buffer == NULL) {
        return false;
    }

    SDL_GPUTransferBuffer *transfer =
        SDL_CreateGPUTransferBuffer(model->device, &(SDL_GPUTransferBufferCreateInfo){
                                                       .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                       .size = transfer_bytes,
                                                   });
    if (transfer == NULL) {
        return false;
    }

    Uint8 *mapped = SDL_MapGPUTransferBuffer(model->device, transfer, false);
    if (mapped == NULL) {
        SDL_ReleaseGPUTransferBuffer(model->device, transfer);
        return false;
    }
    SDL_memcpy(mapped, geometry->vertices, vertex_bytes_size);
    SDL_memcpy(mapped + vertex_bytes_size, geometry->indices, index_bytes_size);
    SDL_UnmapGPUTransferBuffer(model->device, transfer);

    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(model->device);
    if (commands == NULL) {
        SDL_ReleaseGPUTransferBuffer(model->device, transfer);
        return false;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    if (copy == NULL) {
        SDL_CancelGPUCommandBuffer(commands);
        SDL_ReleaseGPUTransferBuffer(model->device, transfer);
        return false;
    }
    SDL_UploadToGPUBuffer(copy, &(SDL_GPUTransferBufferLocation){.transfer_buffer = transfer},
                          &(SDL_GPUBufferRegion){
                              .buffer = model->vertex_buffer,
                              .size = vertex_bytes,
                          },
                          false);
    SDL_UploadToGPUBuffer(copy,
                          &(SDL_GPUTransferBufferLocation){
                              .transfer_buffer = transfer,
                              .offset = vertex_bytes,
                          },
                          &(SDL_GPUBufferRegion){
                              .buffer = model->index_buffer,
                              .size = index_bytes,
                          },
                          false);
    SDL_EndGPUCopyPass(copy);
    const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(model->device, transfer);
    return submitted;
}

static SDL_GPUTexture *upload_rgba_texture(EidolonModelRenderer *model, const Uint8 *pixels,
                                           int width, int height, size_t pitch, const char *name) {
    if (width <= 0 || height <= 0) {
        SDL_SetError("invalid model texture dimensions");
        return NULL;
    }

    const size_t row_bytes = (size_t)width * 4U;
    const size_t image_bytes_size = row_bytes * (size_t)height;
    Uint32 image_bytes = 0;
    if (row_bytes / 4U != (size_t)width || image_bytes_size / row_bytes != (size_t)height ||
        !size_to_u32(image_bytes_size, &image_bytes, "model texture")) {
        SDL_SetError("model texture size overflow");
        return NULL;
    }

    SDL_GPUTexture *texture =
        SDL_CreateGPUTexture(model->device, &(SDL_GPUTextureCreateInfo){
                                                .type = SDL_GPU_TEXTURETYPE_2D,
                                                .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                                .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                                .width = (Uint32)width,
                                                .height = (Uint32)height,
                                                .layer_count_or_depth = 1,
                                                .num_levels = 1,
                                                .sample_count = SDL_GPU_SAMPLECOUNT_1,
                                            });
    if (texture == NULL) {
        return NULL;
    }
    if (name != NULL) {
        SDL_SetGPUTextureName(model->device, texture, name);
    }

    SDL_GPUTransferBuffer *transfer =
        SDL_CreateGPUTransferBuffer(model->device, &(SDL_GPUTransferBufferCreateInfo){
                                                       .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                       .size = image_bytes,
                                                   });
    if (transfer == NULL) {
        SDL_ReleaseGPUTexture(model->device, texture);
        return NULL;
    }
    Uint8 *mapped = SDL_MapGPUTransferBuffer(model->device, transfer, false);
    if (mapped == NULL) {
        SDL_ReleaseGPUTransferBuffer(model->device, transfer);
        SDL_ReleaseGPUTexture(model->device, texture);
        return NULL;
    }
    for (int row = 0; row < height; ++row) {
        SDL_memcpy(mapped + (size_t)row * row_bytes, pixels + (size_t)row * pitch, row_bytes);
    }
    SDL_UnmapGPUTransferBuffer(model->device, transfer);

    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(model->device);
    if (commands == NULL) {
        SDL_ReleaseGPUTransferBuffer(model->device, transfer);
        SDL_ReleaseGPUTexture(model->device, texture);
        return NULL;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    if (copy == NULL) {
        SDL_CancelGPUCommandBuffer(commands);
        SDL_ReleaseGPUTransferBuffer(model->device, transfer);
        SDL_ReleaseGPUTexture(model->device, texture);
        return NULL;
    }
    SDL_UploadToGPUTexture(copy, &(SDL_GPUTextureTransferInfo){.transfer_buffer = transfer},
                           &(SDL_GPUTextureRegion){
                               .texture = texture,
                               .w = (Uint32)width,
                               .h = (Uint32)height,
                               .d = 1,
                           },
                           false);
    SDL_EndGPUCopyPass(copy);
    const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(model->device, transfer);
    if (!submitted) {
        SDL_ReleaseGPUTexture(model->device, texture);
        return NULL;
    }
    return texture;
}

static SDL_Surface *load_embedded_png(const cgltf_image *image) {
    if (image->buffer_view == NULL) {
        SDL_SetError("model image '%s' is not embedded in the GLB",
                     image->name != NULL ? image->name : "unnamed");
        return NULL;
    }
    const Uint8 *bytes = cgltf_buffer_view_data(image->buffer_view);
    if (bytes == NULL) {
        SDL_SetError("model image '%s' has no loaded buffer",
                     image->name != NULL ? image->name : "unnamed");
        return NULL;
    }

    SDL_IOStream *stream = SDL_IOFromConstMem(bytes, (size_t)image->buffer_view->size);
    if (stream == NULL) {
        return NULL;
    }
    SDL_Surface *surface = SDL_LoadPNG_IO(stream, true);
    if (surface == NULL) {
        return NULL;
    }
    if (surface->format == SDL_PIXELFORMAT_ABGR8888) {
        return surface;
    }

    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    return converted;
}

static bool upload_textures(EidolonModelRenderer *model, const cgltf_data *data) {
    if ((size_t)data->images_count >= UINT32_MAX) {
        SDL_SetError("model has too many images");
        return false;
    }
    model->texture_count = (size_t)data->images_count + 1U;
    model->textures = SDL_calloc(model->texture_count, sizeof(*model->textures));
    if (model->textures == NULL) {
        SDL_SetError("out of memory while loading model textures");
        return false;
    }

    const Uint8 white[] = {255, 255, 255, 255};
    model->textures[0] = upload_rgba_texture(model, white, 1, 1, sizeof(white), "white");
    if (model->textures[0] == NULL) {
        return false;
    }

    for (cgltf_size image_index = 0; image_index < data->images_count; ++image_index) {
        const cgltf_image *image = &data->images[image_index];
        SDL_Surface *surface = load_embedded_png(image);
        if (surface == NULL) {
            return false;
        }
        model->textures[(size_t)image_index + 1U] = upload_rgba_texture(
            model, surface->pixels, surface->w, surface->h, (size_t)surface->pitch, image->name);
        SDL_DestroySurface(surface);
        if (model->textures[(size_t)image_index + 1U] == NULL) {
            return false;
        }
    }
    return true;
}

static SDL_GPUShader *load_shader(EidolonModelRenderer *model, const char *shader_directory,
                                  const char *name, SDL_GPUShaderStage stage, Uint32 sampler_count,
                                  Uint32 uniform_count) {
    const SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(model->device);
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    const char *subdirectory = NULL;
    const char *extension = NULL;
    if ((supported & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        subdirectory = "SPIRV";
        extension = "spv";
    } else if ((supported & SDL_GPU_SHADERFORMAT_DXIL) != 0) {
        format = SDL_GPU_SHADERFORMAT_DXIL;
        subdirectory = "DXIL";
        extension = "dxil";
    } else {
        SDL_SetError("SDL_GPU selected a shader format Eidolon has not baked");
        return NULL;
    }

    char path[1024];
    const int path_length = SDL_snprintf(path, sizeof(path), "%s/%s/%s.%s", shader_directory,
                                         subdirectory, name, extension);
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) {
        SDL_SetError("model shader path is too long");
        return NULL;
    }

    size_t code_size = 0;
    void *code = SDL_LoadFile(path, &code_size);
    if (code == NULL) {
        SDL_SetError("could not load model shader %s: %s", path, SDL_GetError());
        return NULL;
    }
    SDL_GPUShader *shader =
        SDL_CreateGPUShader(model->device, &(SDL_GPUShaderCreateInfo){
                                               .code_size = code_size,
                                               .code = code,
                                               .entrypoint = "main",
                                               .format = format,
                                               .stage = stage,
                                               .num_samplers = sampler_count,
                                               .num_uniform_buffers = uniform_count,
                                           });
    SDL_free(code);
    return shader;
}

static bool create_pipeline(EidolonModelRenderer *model, const char *shader_directory) {
    SDL_GPUShader *vertex_shader =
        load_shader(model, shader_directory, "model.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0, 2);
    if (vertex_shader == NULL) {
        return false;
    }
    SDL_GPUShader *fragment_shader =
        load_shader(model, shader_directory, "model.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    if (fragment_shader == NULL) {
        SDL_ReleaseGPUShader(model->device, vertex_shader);
        return false;
    }

    const SDL_GPUVertexBufferDescription vertex_buffer_description = {
        .slot = 0,
        .pitch = sizeof(EidolonModelVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };
    const SDL_GPUVertexAttribute vertex_attributes[] = {
        {
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = offsetof(EidolonModelVertex, position),
        },
        {
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = offsetof(EidolonModelVertex, texcoord),
        },
        {
            .location = 2,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_USHORT4,
            .offset = offsetof(EidolonModelVertex, joints),
        },
        {
            .location = 3,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
            .offset = offsetof(EidolonModelVertex, weights),
        },
        {
            .location = 4,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = offsetof(EidolonModelVertex, neutral_delta),
        },
        {
            .location = 5,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = offsetof(EidolonModelVertex, focused_delta),
        },
    };
    SDL_GPUColorTargetDescription color_target = {.format = MODEL_COLOR_FORMAT};
    SDL_GPUGraphicsPipelineCreateInfo create_info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .vertex_input_state =
            {
                .vertex_buffer_descriptions = &vertex_buffer_description,
                .num_vertex_buffers = 1,
                .vertex_attributes = vertex_attributes,
                .num_vertex_attributes = SDL_arraysize(vertex_attributes),
            },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state =
            {
                .fill_mode = SDL_GPU_FILLMODE_FILL,
                .cull_mode = SDL_GPU_CULLMODE_NONE,
                .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
                .enable_depth_clip = true,
            },
        .multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
        .depth_stencil_state =
            {
                .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                .enable_depth_test = true,
                .enable_depth_write = true,
            },
        .target_info =
            {
                .color_target_descriptions = &color_target,
                .num_color_targets = 1,
                .depth_stencil_format = MODEL_DEPTH_FORMAT,
                .has_depth_stencil_target = true,
            },
    };
    model->pipeline = SDL_CreateGPUGraphicsPipeline(model->device, &create_info);
    color_target.blend_state = (SDL_GPUColorTargetBlendState){
        .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .color_blend_op = SDL_GPU_BLENDOP_ADD,
        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        .enable_blend = true,
    };
    create_info.depth_stencil_state.enable_depth_write = false;
    model->alpha_pipeline = SDL_CreateGPUGraphicsPipeline(model->device, &create_info);
    SDL_ReleaseGPUShader(model->device, fragment_shader);
    SDL_ReleaseGPUShader(model->device, vertex_shader);
    return model->pipeline != NULL && model->alpha_pipeline != NULL;
}

static bool create_targets(EidolonModelRenderer *model, SDL_Renderer *renderer) {
    model->color_target =
        SDL_CreateGPUTexture(model->device, &(SDL_GPUTextureCreateInfo){
                                                .type = SDL_GPU_TEXTURETYPE_2D,
                                                .format = MODEL_COLOR_FORMAT,
                                                .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                                                         SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                                .width = MODEL_TARGET_WIDTH,
                                                .height = MODEL_TARGET_HEIGHT,
                                                .layer_count_or_depth = 1,
                                                .num_levels = 1,
                                                .sample_count = SDL_GPU_SAMPLECOUNT_1,
                                            });
    model->depth_target =
        SDL_CreateGPUTexture(model->device, &(SDL_GPUTextureCreateInfo){
                                                .type = SDL_GPU_TEXTURETYPE_2D,
                                                .format = MODEL_DEPTH_FORMAT,
                                                .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
                                                .width = MODEL_TARGET_WIDTH,
                                                .height = MODEL_TARGET_HEIGHT,
                                                .layer_count_or_depth = 1,
                                                .num_levels = 1,
                                                .sample_count = SDL_GPU_SAMPLECOUNT_1,
                                            });
    if (model->color_target == NULL || model->depth_target == NULL) {
        return false;
    }

    model->renderer_texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                          MODEL_TARGET_WIDTH, MODEL_TARGET_HEIGHT);
    if (model->renderer_texture == NULL) {
        return false;
    }
    SDL_SetTextureBlendMode(model->renderer_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(model->renderer_texture, SDL_SCALEMODE_LINEAR);
    const Uint32 image_bytes = MODEL_TARGET_WIDTH * MODEL_TARGET_HEIGHT * 4U;
    for (size_t readback_index = 0; readback_index < MODEL_READBACK_COUNT; ++readback_index) {
        model->readbacks[readback_index].download = SDL_CreateGPUTransferBuffer(
            model->device, &(SDL_GPUTransferBufferCreateInfo){
                               .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                               .size = image_bytes,
                           });
        if (model->readbacks[readback_index].download == NULL) {
            return false;
        }
    }
    return true;
}

static bool create_sampler(EidolonModelRenderer *model) {
    model->sampler = SDL_CreateGPUSampler(
        model->device, &(SDL_GPUSamplerCreateInfo){
                           .min_filter = SDL_GPU_FILTER_LINEAR,
                           .mag_filter = SDL_GPU_FILTER_LINEAR,
                           .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
                           .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                           .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                           .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                       });
    return model->sampler != NULL;
}

static bool update_joint_palette(EidolonModelRenderer *model) {
    for (size_t joint_index = 0; joint_index < model->joint_count; ++joint_index) {
        const float *world =
            eidolon_motion_world(&model->motion, (size_t)model->joint_nodes[joint_index]);
        if (world == NULL) {
            return false;
        }
        matrix_multiply(world, model->inverse_bind[joint_index], model->joint_palette[joint_index]);
    }
    matrix_identity(model->joint_palette[model->joint_count]);
    return true;
}

static bool consume_readback(EidolonModelRenderer *model, EidolonModelReadback *readback,
                             bool wait) {
    if (readback->fence == NULL) {
        return true;
    }
    if (wait) {
        SDL_GPUFence *fences[] = {readback->fence};
        if (!SDL_WaitForGPUFences(model->device, true, fences, SDL_arraysize(fences))) {
            return false;
        }
    } else if (!SDL_QueryGPUFence(model->device, readback->fence)) {
        return true;
    }

    const void *pixels = SDL_MapGPUTransferBuffer(model->device, readback->download, false);
    if (pixels == NULL) {
        return false;
    }
    bool updated = true;
    if (readback->frame_sequence > model->presented_frame_sequence) {
        updated = SDL_UpdateTexture(model->renderer_texture, NULL, pixels, MODEL_TARGET_WIDTH * 4);
        if (updated) {
            model->presented_frame_sequence = readback->frame_sequence;
            model->presented_transform_revision = readback->transform_revision;
        }
    }
    SDL_UnmapGPUTransferBuffer(model->device, readback->download);
    SDL_ReleaseGPUFence(model->device, readback->fence);
    readback->fence = NULL;
    return updated;
}

static bool submit_model_frame(EidolonModelRenderer *model, uint64_t now_ms) {
    float projection[16];
    float yaw_rotation[16];
    float pitch_rotation[16];
    float roll_rotation[16];
    float yaw_pitch_rotation[16];
    float model_rotation[16];
    float from_pivot[16];
    float to_pivot[16];
    float rotation_from_pivot[16];
    float centered_rotation[16];
    EidolonModelScene scene;
    model_projection(model, projection);
    matrix_rotation_y(model->yaw_radians, yaw_rotation);
    matrix_rotation_x(model->pitch_radians, pitch_rotation);
    matrix_rotation_z(model->roll_radians, roll_rotation);
    matrix_multiply(yaw_rotation, pitch_rotation, yaw_pitch_rotation);
    matrix_multiply(yaw_pitch_rotation, roll_rotation, model_rotation);
    matrix_translation(-model->rotation_pivot[0], -model->rotation_pivot[1],
                       -model->rotation_pivot[2], from_pivot);
    matrix_translation(model->rotation_pivot[0], model->rotation_pivot[1], model->rotation_pivot[2],
                       to_pivot);
    matrix_multiply(model_rotation, from_pivot, rotation_from_pivot);
    matrix_multiply(to_pivot, rotation_from_pivot, centered_rotation);
    matrix_multiply(projection, centered_rotation, scene.model_view_projection);
    scene.expression_weights[0] =
        model->vrm_ready ? 1.0F - model->vrm_projection.focused_expression_weight : 0.0F;
    scene.expression_weights[1] =
        model->vrm_ready ? model->vrm_projection.focused_expression_weight : 0.0F;
    scene.expression_weights[2] = 0.0F;
    scene.expression_weights[3] = 0.0F;

    EidolonModelReadback *readback = NULL;
    for (size_t offset = 0; offset < MODEL_READBACK_COUNT; ++offset) {
        const size_t index = (model->next_readback + offset) % MODEL_READBACK_COUNT;
        if (model->readbacks[index].fence == NULL) {
            readback = &model->readbacks[index];
            model->next_readback = (index + 1U) % MODEL_READBACK_COUNT;
            break;
        }
    }
    if (readback == NULL) {
        return true;
    }
    readback->frame_sequence = ++model->next_frame_sequence;
    readback->transform_revision = model->transform_revision;

    if (!model->vrm_ready) {
        eidolon_motion_update_idle(&model->motion, now_ms);
        if (model->semantic_pose_active && model->humanoid_ready && !model->semantic_pose_failed &&
            !eidolon_pose_solve(&model->motion, &model->humanoid, &model->semantic_pose)) {
            eidolon_log_write("motion", "semantic pose disabled after solve failure: %s",
                              SDL_GetError());
            model->semantic_pose_failed = true;
            SDL_ClearError();
            eidolon_motion_update_idle(&model->motion, now_ms);
        }
    }
    if (!update_joint_palette(model)) {
        return false;
    }

    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(model->device);
    if (commands == NULL) {
        return false;
    }
    SDL_PushGPUVertexUniformData(commands, 0, &scene, sizeof(scene));
    SDL_PushGPUVertexUniformData(commands, 1, model->joint_palette, sizeof(model->joint_palette));

    const SDL_GPUColorTargetInfo color_target = {
        .texture = model->color_target,
        .clear_color = {0.0F, 0.0F, 0.0F, 0.0F},
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    const SDL_GPUDepthStencilTargetInfo depth_target = {
        .texture = model->depth_target,
        .clear_depth = 1.0F,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_DONT_CARE,
        .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
        .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
    };
    SDL_GPURenderPass *render_pass =
        SDL_BeginGPURenderPass(commands, &color_target, 1, &depth_target);
    if (render_pass == NULL) {
        SDL_CancelGPUCommandBuffer(commands);
        return false;
    }

    SDL_BindGPUVertexBuffers(render_pass, 0,
                             &(SDL_GPUBufferBinding){.buffer = model->vertex_buffer}, 1);
    SDL_BindGPUIndexBuffer(render_pass, &(SDL_GPUBufferBinding){.buffer = model->index_buffer},
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);
    for (size_t draw_index = 0; draw_index < model->draw_count; ++draw_index) {
        const EidolonModelDraw *draw = &model->draws[draw_index];
        const size_t texture_index =
            draw->texture_index < model->texture_count ? (size_t)draw->texture_index : 0;
        const SDL_GPUTextureSamplerBinding binding = {
            .texture = model->textures[texture_index],
            .sampler = model->sampler,
        };
        SDL_PushGPUFragmentUniformData(commands, 0, &draw->material,
                                       (Uint32)sizeof(draw->material));
        SDL_BindGPUGraphicsPipeline(render_pass,
                                    draw->alpha_blend ? model->alpha_pipeline : model->pipeline);
        SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
        SDL_DrawGPUIndexedPrimitives(render_pass, draw->index_count, 1, draw->first_index, 0, 0);
    }
    SDL_EndGPURenderPass(render_pass);

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    if (copy == NULL) {
        SDL_CancelGPUCommandBuffer(commands);
        return false;
    }
    SDL_DownloadFromGPUTexture(copy,
                               &(SDL_GPUTextureRegion){
                                   .texture = model->color_target,
                                   .w = MODEL_TARGET_WIDTH,
                                   .h = MODEL_TARGET_HEIGHT,
                                   .d = 1,
                               },
                               &(SDL_GPUTextureTransferInfo){
                                   .transfer_buffer = readback->download,
                                   .pixels_per_row = MODEL_TARGET_WIDTH,
                                   .rows_per_layer = MODEL_TARGET_HEIGHT,
                               });
    SDL_EndGPUCopyPass(copy);

    readback->fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (readback->fence == NULL) {
        return false;
    }
    model->last_submit_ms = now_ms;
    return true;
}
#endif

void eidolon_model_set_rotation(EidolonModelRenderer *model, float yaw_radians, float pitch_radians,
                                float roll_radians) {
    if (model != NULL &&
        (model->yaw_radians != yaw_radians || model->pitch_radians != pitch_radians ||
         model->roll_radians != roll_radians)) {
        model->yaw_radians = yaw_radians;
        model->pitch_radians = pitch_radians;
        model->roll_radians = roll_radians;
        model->transform_revision += 1U;
    }
}

void eidolon_model_set_neutral_pose(EidolonModelRenderer *model, float arm_lower_radians,
                                    float elbow_bend_add_radians) {
    if (model == NULL) {
        return;
    }
    const EidolonNeutralPose current = eidolon_motion_neutral_pose(&model->motion);
    if (current.shoulder_lower_radians != arm_lower_radians ||
        current.elbow_bend_add_radians != elbow_bend_add_radians) {
        eidolon_motion_set_neutral_pose(&model->motion,
                                        (EidolonNeutralPose){
                                            .shoulder_lower_radians = arm_lower_radians,
                                            .elbow_bend_add_radians = elbow_bend_add_radians,
                                        });
        model->transform_revision += 1U;
    }
}

void eidolon_model_set_semantic_pose(EidolonModelRenderer *model, const EidolonSemanticPose *pose) {
    if (model == NULL || pose == NULL || !model->humanoid_ready) {
        return;
    }
    model->semantic_pose = *pose;
    model->semantic_pose_active = true;
    model->semantic_pose_failed = false;
    model->transform_revision += 1U;
}

void eidolon_model_clear_semantic_pose(EidolonModelRenderer *model) {
    if (model == NULL || !model->semantic_pose_active) {
        return;
    }
    model->semantic_pose_active = false;
    model->semantic_pose_failed = false;
    model->transform_revision += 1U;
}

void eidolon_model_set_idle_tuning(EidolonModelRenderer *model, EidolonIdleTuning tuning) {
    if (model == NULL) {
        return;
    }
    const EidolonIdleTuning current = eidolon_motion_idle_tuning(&model->motion);
    if (current.breath_period_seconds != tuning.breath_period_seconds ||
        current.breath_chest_radians != tuning.breath_chest_radians ||
        current.breath_neck_counter_radians != tuning.breath_neck_counter_radians ||
        current.sway_period_seconds != tuning.sway_period_seconds ||
        current.sway_spine_radians != tuning.sway_spine_radians ||
        current.sway_chest_counter_radians != tuning.sway_chest_counter_radians ||
        current.sway_head_radians != tuning.sway_head_radians) {
        eidolon_motion_set_idle_tuning(&model->motion, tuning);
        model->transform_revision += 1U;
    }
}

bool eidolon_model_set_render_resolution(EidolonModelRenderer *model, int side) {
    if (model == NULL || model->failed) {
        return SDL_SetError("cannot resize an unavailable model renderer");
    }
    if (side < EIDOLON_MODEL_RENDER_RESOLUTION_MIN || side > EIDOLON_MODEL_RENDER_RESOLUTION_MAX) {
        return SDL_SetError("model render resolution must be between %d and %d",
                            EIDOLON_MODEL_RENDER_RESOLUTION_MIN,
                            EIDOLON_MODEL_RENDER_RESOLUTION_MAX);
    }
    if (side == model->target_width && side == model->target_height) {
        return true;
    }
#if defined(_WIN32)
    if (!replace_targets(model, side)) {
        return false;
    }
    model->last_submit_ms = 0;
    model->transform_revision += 1U;
    eidolon_log_write("model", "render target resized to %dx%d", side, side);
    return true;
#else
    return SDL_SetError("runtime model render resolution is only supported on Windows");
#endif
}

int eidolon_model_render_resolution(const EidolonModelRenderer *model) {
    return model != NULL ? model->target_width : 0;
}

uint64_t eidolon_model_presented_transform_revision(const EidolonModelRenderer *model) {
    if (model == NULL) {
        return 0U;
    }
    return model->failed ? UINT64_MAX : model->presented_transform_revision;
}

uint64_t eidolon_model_presented_frame_sequence(const EidolonModelRenderer *model) {
    if (model == NULL || model->failed) {
        return 0U;
    }
    return model->presented_frame_sequence;
}

EidolonModelRenderer *eidolon_model_create(SDL_Renderer *renderer, const char *model_path,
                                           const char *shader_directory,
                                           EidolonNeutralPose neutral_pose,
                                           EidolonIdleTuning idle_tuning) {
    EidolonModelRenderer *model = SDL_calloc(1, sizeof(*model));
    if (model == NULL) {
        SDL_SetError("out of memory while creating model renderer");
        return NULL;
    }
#if defined(_WIN32)
    model->target_width = EIDOLON_MODEL_RENDER_RESOLUTION_DEFAULT;
    model->target_height = EIDOLON_MODEL_RENDER_RESOLUTION_DEFAULT;
    if (!acquire_renderer_device(model, renderer)) {
        eidolon_model_destroy(model);
        return NULL;
    }
#else
    model->target_width = MODEL_TARGET_WIDTH;
    model->target_height = MODEL_TARGET_HEIGHT;
    model->device = create_gpu_device();
    if (model->device == NULL) {
        eidolon_model_destroy(model);
        return NULL;
    }
#endif

    cgltf_options options = {0};
    cgltf_data *data = NULL;
    cgltf_result result = cgltf_parse_file(&options, model_path, &data);
    if (result != cgltf_result_success) {
        SDL_SetError("could not parse model GLB (cgltf result %d)", (int)result);
        eidolon_model_destroy(model);
        return NULL;
    }
    result = cgltf_load_buffers(&options, data, model_path);
    if (result != cgltf_result_success) {
        SDL_SetError("could not load model GLB buffers (cgltf result %d)", (int)result);
        cgltf_free(data);
        eidolon_model_destroy(model);
        return NULL;
    }
    result = cgltf_validate(data);
    if (result != cgltf_result_success) {
        SDL_SetError("model GLB validation failed (cgltf result %d)", (int)result);
        cgltf_free(data);
        eidolon_model_destroy(model);
        return NULL;
    }

    const bool is_vrm = data_has_extension(data, "VRMC_vrm");
    if (!is_vrm && data_has_extension(data, "VRM")) {
        SDL_SetError("legacy VRM 0.x is not supported by the VRM 1.0 body path");
        cgltf_free(data);
        eidolon_model_destroy(model);
        return NULL;
    }
    if (is_vrm) {
        char error[256];
        if (!eidolon_vrm_body_parse(data, &model->vrm_body, error, sizeof(error)) ||
            !eidolon_vrm_body_make_profile(data, &model->vrm_body, &model->body_profile, error,
                                           sizeof(error))) {
            SDL_SetError("VRM 1.0 body validation failed: %s", error);
            cgltf_free(data);
            eidolon_model_destroy(model);
            return NULL;
        }
    }

    EidolonCpuGeometry geometry;
    if (!build_motion_rig(model, data, is_vrm) ||
        !build_cpu_geometry(data, model->joint_count, is_vrm ? &model->vrm_body : NULL,
                            &geometry)) {
        cgltf_free(data);
        eidolon_model_destroy(model);
        return NULL;
    }
    if (is_vrm) {
        model->vrm_ready =
            eidolon_vrm_projection_init(&model->vrm_projection, &model->vrm_body, &model->motion);
        if (!model->vrm_ready) {
            SDL_SetError("could not initialize VRM 1.0 control projection");
            cpu_geometry_destroy(&geometry);
            cgltf_free(data);
            eidolon_model_destroy(model);
            return NULL;
        }
    } else {
        model->humanoid_ready = eidolon_humanoid_profile_init(&model->humanoid, &model->motion);
        if (!model->humanoid_ready) {
            eidolon_log_write("motion",
                              "humanoid profile unavailable; target-space poses disabled: %s",
                              SDL_GetError());
            SDL_ClearError();
        }
    }
    eidolon_motion_set_neutral_pose(&model->motion, neutral_pose);
    eidolon_motion_set_idle_tuning(&model->motion, idle_tuning);
    set_rotation_pivot(model, &geometry);
    const bool uploaded = upload_geometry(model, &geometry) && upload_textures(model, data);
    if (uploaded) {
        model->draws = geometry.draws;
        model->draw_count = geometry.draw_count;
        geometry.draws = NULL;
    }
    cpu_geometry_destroy(&geometry);
    cgltf_free(data);
    if (!uploaded || !create_sampler(model) || !create_pipeline(model, shader_directory) ||
        !create_targets(model, renderer) || !submit_model_frame(model, 0)) {
        eidolon_model_destroy(model);
        return NULL;
    }
#if !defined(_WIN32)
    for (size_t readback_index = 0; readback_index < MODEL_READBACK_COUNT; ++readback_index) {
        if (!consume_readback(model, &model->readbacks[readback_index], true)) {
            eidolon_model_destroy(model);
            return NULL;
        }
    }
#endif

#if defined(_WIN32)
    const char *gpu_driver = SDL_GetRendererName(renderer);
    const char *frame_transfer = "shared-texture";
#else
    const char *gpu_driver = SDL_GetGPUDeviceDriver(model->device);
    const char *frame_transfer = "asynchronous-readback";
#endif
    eidolon_log_write("model",
                      "loaded %s draws=%zu textures=%zu joints=%zu target=%dx%d gpu=%s humanoid=%s "
                      "frame-transfer=%s",
                      model->vrm_ready ? "VRM 1.0" : "GLB", model->draw_count, model->texture_count,
                      model->joint_count, model->target_width, model->target_height,
                      gpu_driver != NULL ? gpu_driver : "unknown",
                      (model->humanoid_ready || model->vrm_ready) ? "ready" : "unavailable",
                      frame_transfer);
    if (model->vrm_ready) {
        eidolon_log_write("model",
                          "VRM body '%s' by %s look-at=%s expression=%s spring=%s "
                          "license=%s",
                          model->vrm_body.name, model->vrm_body.author,
                          model->vrm_body.has_look_at ? "yes" : "head-only",
                          model->vrm_body.has_expression ? "neutral+relaxed" : "neutral-only",
                          model->vrm_body.has_spring_bones ? "declared-not-simulated" : "absent",
                          model->vrm_body.license_url);
    }
    if (model->humanoid_ready) {
        eidolon_log_write(
            "motion", "humanoid metrics shoulders=%.4f arms=%.4f/%.4f torso=%.4f",
            model->humanoid.shoulder_width, model->humanoid.arm_length[EIDOLON_HUMANOID_LEFT],
            model->humanoid.arm_length[EIDOLON_HUMANOID_RIGHT], model->humanoid.torso_length);
    }
    return model;
}

bool eidolon_model_body_profile(const EidolonModelRenderer *model, EidolonEprBodyProfile *profile) {
    if (model == NULL || profile == NULL || !model->vrm_ready || model->failed) {
        return false;
    }
    *profile = model->body_profile;
    return true;
}

bool eidolon_model_apply_control(EidolonModelRenderer *model,
                                 const EidolonCanonicalControl *control) {
    if (model == NULL || control == NULL || !model->vrm_ready || model->failed) {
        return SDL_SetError("VRM control projection is unavailable");
    }
    if (!eidolon_vrm_projection_apply(&model->vrm_projection, &model->motion, control)) {
        return SDL_SetError("VRM control revision %llu was stale or invalid",
                            (unsigned long long)control->revision);
    }
    model->transform_revision += 1U;
    return true;
}

const char *eidolon_model_body_name(const EidolonModelRenderer *model) {
    if (model != NULL && model->vrm_ready && model->vrm_body.name[0] != '\0') {
        return model->vrm_body.name;
    }
    return "Rio (Battle)";
}

void eidolon_model_update(EidolonModelRenderer *model, uint64_t now_ms) {
    if (model == NULL || model->failed) {
        return;
    }
#if !defined(_WIN32)
    for (size_t readback_index = 0; readback_index < MODEL_READBACK_COUNT; ++readback_index) {
        if (!consume_readback(model, &model->readbacks[readback_index], false)) {
            disable_failed_model(model, "animated readback");
            return;
        }
    }
#endif
    if (now_ms - model->last_submit_ms < MODEL_FRAME_INTERVAL_MS) {
        return;
    }
    if (!submit_model_frame(model, now_ms)) {
        disable_failed_model(model, "animated frame submission");
    }
}

void eidolon_model_request_redraw(EidolonModelRenderer *model) {
    if (model != NULL && !model->failed) {
#if defined(_WIN32)
        if (!refresh_color_target_view(model)) {
            disable_failed_model(model, "refreshing reset D3D11 model target");
            return;
        }
#endif
        model->last_submit_ms = 0;
        model->transform_revision += 1U;
    }
}

void eidolon_model_destroy(EidolonModelRenderer *model) {
    if (model == NULL) {
        return;
    }
#if defined(_WIN32)
    if (model->color_rtv != NULL) {
        ID3D11RenderTargetView_Release(model->color_rtv);
    }
    if (model->depth_dsv != NULL) {
        ID3D11DepthStencilView_Release(model->depth_dsv);
    }
    if (model->depth_texture != NULL) {
        ID3D11Texture2D_Release(model->depth_texture);
    }
    if (model->depth_state != NULL) {
        ID3D11DepthStencilState_Release(model->depth_state);
    }
    if (model->depth_state_no_write != NULL) {
        ID3D11DepthStencilState_Release(model->depth_state_no_write);
    }
    if (model->alpha_blend != NULL) {
        ID3D11BlendState_Release(model->alpha_blend);
    }
    if (model->rasterizer != NULL) {
        ID3D11RasterizerState_Release(model->rasterizer);
    }
    if (model->bones_buffer != NULL) {
        ID3D11Buffer_Release(model->bones_buffer);
    }
    if (model->scene_buffer != NULL) {
        ID3D11Buffer_Release(model->scene_buffer);
    }
    if (model->material_buffer != NULL) {
        ID3D11Buffer_Release(model->material_buffer);
    }
    if (model->input_layout != NULL) {
        ID3D11InputLayout_Release(model->input_layout);
    }
    if (model->pixel_shader != NULL) {
        ID3D11PixelShader_Release(model->pixel_shader);
    }
    if (model->vertex_shader != NULL) {
        ID3D11VertexShader_Release(model->vertex_shader);
    }
    if (model->sampler != NULL) {
        ID3D11SamplerState_Release(model->sampler);
    }
    if (model->textures != NULL) {
        for (size_t texture_index = 0; texture_index < model->texture_count; ++texture_index) {
            if (model->textures[texture_index] != NULL) {
                ID3D11ShaderResourceView_Release(model->textures[texture_index]);
            }
        }
    }
    if (model->index_buffer != NULL) {
        ID3D11Buffer_Release(model->index_buffer);
    }
    if (model->vertex_buffer != NULL) {
        ID3D11Buffer_Release(model->vertex_buffer);
    }
    if (model->renderer_texture != NULL) {
        SDL_DestroyTexture(model->renderer_texture);
    }
    if (model->context != NULL) {
        ID3D11DeviceContext_Release(model->context);
    }
#else
    /* A lost device is deliberately abandoned; some D3D12 drivers fault during teardown. */
    const bool release_gpu = model->device != NULL && !model->failed;
    if (release_gpu) {
        (void)SDL_WaitForGPUIdle(model->device);
    }
    for (size_t readback_index = 0; readback_index < MODEL_READBACK_COUNT; ++readback_index) {
        if (release_gpu && model->readbacks[readback_index].fence != NULL) {
            SDL_ReleaseGPUFence(model->device, model->readbacks[readback_index].fence);
        }
        if (release_gpu && model->readbacks[readback_index].download != NULL) {
            SDL_ReleaseGPUTransferBuffer(model->device, model->readbacks[readback_index].download);
        }
    }
    if (model->renderer_texture != NULL) {
        SDL_DestroyTexture(model->renderer_texture);
    }
    if (release_gpu && model->depth_target != NULL) {
        SDL_ReleaseGPUTexture(model->device, model->depth_target);
    }
    if (release_gpu && model->color_target != NULL) {
        SDL_ReleaseGPUTexture(model->device, model->color_target);
    }
    if (release_gpu && model->pipeline != NULL) {
        SDL_ReleaseGPUGraphicsPipeline(model->device, model->pipeline);
    }
    if (release_gpu && model->alpha_pipeline != NULL) {
        SDL_ReleaseGPUGraphicsPipeline(model->device, model->alpha_pipeline);
    }
    if (release_gpu && model->sampler != NULL) {
        SDL_ReleaseGPUSampler(model->device, model->sampler);
    }
    if (release_gpu && model->textures != NULL) {
        for (size_t texture_index = 0; texture_index < model->texture_count; ++texture_index) {
            if (model->textures[texture_index] != NULL) {
                SDL_ReleaseGPUTexture(model->device, model->textures[texture_index]);
            }
        }
    }
    if (release_gpu && model->index_buffer != NULL) {
        SDL_ReleaseGPUBuffer(model->device, model->index_buffer);
    }
    if (release_gpu && model->vertex_buffer != NULL) {
        SDL_ReleaseGPUBuffer(model->device, model->vertex_buffer);
    }
    if (release_gpu) {
        SDL_DestroyGPUDevice(model->device);
    }
#endif
    SDL_free(model->textures);
    SDL_free(model->draws);
    eidolon_vrm_projection_destroy(&model->vrm_projection);
    eidolon_motion_destroy(&model->motion);
    SDL_free(model);
}

SDL_Texture *eidolon_model_texture(const EidolonModelRenderer *model) {
    return model != NULL && !model->failed ? model->renderer_texture : NULL;
}
