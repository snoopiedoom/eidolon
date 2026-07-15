#include "model.h"

#include "log.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#define MODEL_TARGET_WIDTH 256
#define MODEL_TARGET_HEIGHT 256

typedef struct EidolonModelVertex {
    float position[3];
    float texcoord[2];
} EidolonModelVertex;

typedef struct EidolonModelDraw {
    Uint32 first_index;
    Uint32 index_count;
    Uint32 texture_index;
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
    SDL_GPUDevice *device;
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    SDL_GPUTexture **textures;
    size_t texture_count;
    SDL_GPUSampler *sampler;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUTexture *color_target;
    SDL_GPUTexture *depth_target;
    SDL_Texture *renderer_texture;
    EidolonModelDraw *draws;
    size_t draw_count;
};

static const SDL_GPUTextureFormat MODEL_COLOR_FORMAT =
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
static const SDL_GPUTextureFormat MODEL_DEPTH_FORMAT = SDL_GPU_TEXTUREFORMAT_D16_UNORM;

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

static const cgltf_accessor *primitive_attribute(const cgltf_primitive *primitive,
                                                 cgltf_attribute_type type, cgltf_int index) {
    return cgltf_find_accessor(primitive, type, index);
}

static bool primitive_is_supported(const cgltf_primitive *primitive) {
    return primitive->type == cgltf_primitive_type_triangles &&
           primitive_attribute(primitive, cgltf_attribute_type_position, 0) != NULL;
}

static bool count_geometry(const cgltf_data *data, EidolonCpuGeometry *geometry) {
    for (cgltf_size node_index = 0; node_index < data->nodes_count; ++node_index) {
        const cgltf_node *node = &data->nodes[node_index];
        if (node->mesh == NULL) {
            continue;
        }

        for (cgltf_size primitive_index = 0;
             primitive_index < node->mesh->primitives_count; ++primitive_index) {
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

    if (geometry->vertex_count == 0 || geometry->index_count == 0 ||
        geometry->draw_count == 0) {
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

static Uint32 texture_index_for_material(const cgltf_data *data,
                                         const cgltf_material *material) {
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

static void transform_position(const cgltf_float matrix[16], const cgltf_float source[3],
                               float destination[3]) {
    destination[0] = (float)(matrix[0] * source[0] + matrix[4] * source[1] +
                             matrix[8] * source[2] + matrix[12]);
    destination[1] = (float)(matrix[1] * source[0] + matrix[5] * source[1] +
                             matrix[9] * source[2] + matrix[13]);
    destination[2] = (float)(matrix[2] * source[0] + matrix[6] * source[1] +
                             matrix[10] * source[2] + matrix[14]);
}

static bool fill_geometry(const cgltf_data *data, EidolonCpuGeometry *geometry) {
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
                1.0F, 0.0F, 0.0F, 0.0F,
                0.0F, 1.0F, 0.0F, 0.0F,
                0.0F, 0.0F, 1.0F, 0.0F,
                0.0F, 0.0F, 0.0F, 1.0F,
            };
            SDL_memcpy(world, identity, sizeof(world));
        } else {
            cgltf_node_transform_world(node, world);
        }

        for (cgltf_size primitive_index = 0;
             primitive_index < node->mesh->primitives_count; ++primitive_index) {
            const cgltf_primitive *primitive = &node->mesh->primitives[primitive_index];
            if (!primitive_is_supported(primitive)) {
                continue;
            }

            const cgltf_accessor *positions =
                primitive_attribute(primitive, cgltf_attribute_type_position, 0);
            const cgltf_accessor *texcoords =
                primitive_attribute(primitive, cgltf_attribute_type_texcoord, 0);
            const size_t primitive_vertex_count = (size_t)positions->count;
            const size_t primitive_index_count = primitive->indices != NULL
                                                     ? (size_t)primitive->indices->count
                                                     : primitive_vertex_count;

            for (size_t vertex_index = 0; vertex_index < primitive_vertex_count;
                 ++vertex_index) {
                cgltf_float position[3];
                if (!cgltf_accessor_read_float(positions, (cgltf_size)vertex_index, position,
                                               SDL_arraysize(position))) {
                    SDL_SetError("could not read model position accessor");
                    return false;
                }
                transform_position(world, position,
                                   geometry->vertices[vertex_cursor + vertex_index].position);

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
            }

            for (size_t primitive_index_cursor = 0;
                 primitive_index_cursor < primitive_index_count; ++primitive_index_cursor) {
                const size_t local_index =
                    primitive->indices != NULL
                        ? (size_t)cgltf_accessor_read_index(
                              primitive->indices, (cgltf_size)primitive_index_cursor)
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
            };
            vertex_cursor += primitive_vertex_count;
            index_cursor += primitive_index_count;
            draw_cursor += 1;
        }
    }
    return true;
}

static bool build_cpu_geometry(const cgltf_data *data, EidolonCpuGeometry *geometry) {
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

    if (!fill_geometry(data, geometry)) {
        cpu_geometry_destroy(geometry);
        return false;
    }
    return true;
}

static bool upload_geometry(EidolonModelRenderer *model,
                            const EidolonCpuGeometry *geometry) {
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

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        model->device, &(SDL_GPUTransferBufferCreateInfo){
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
    SDL_UploadToGPUBuffer(copy,
                          &(SDL_GPUTransferBufferLocation){.transfer_buffer = transfer},
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
                                           int width, int height, size_t pitch,
                                           const char *name) {
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

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(
        model->device,
        &(SDL_GPUTextureCreateInfo){
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

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        model->device, &(SDL_GPUTransferBufferCreateInfo){
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
    SDL_UploadToGPUTexture(copy,
                           &(SDL_GPUTextureTransferInfo){.transfer_buffer = transfer},
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
        model->textures[(size_t)image_index + 1U] =
            upload_rgba_texture(model, surface->pixels, surface->w, surface->h,
                                (size_t)surface->pitch, image->name);
        SDL_DestroySurface(surface);
        if (model->textures[(size_t)image_index + 1U] == NULL) {
            return false;
        }
    }
    return true;
}

static SDL_GPUShader *load_shader(EidolonModelRenderer *model, const char *shader_directory,
                                  const char *name, SDL_GPUShaderStage stage,
                                  Uint32 sampler_count, Uint32 uniform_count) {
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
    SDL_GPUShader *shader = SDL_CreateGPUShader(
        model->device,
        &(SDL_GPUShaderCreateInfo){
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
        load_shader(model, shader_directory, "model.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    if (vertex_shader == NULL) {
        return false;
    }
    SDL_GPUShader *fragment_shader = load_shader(model, shader_directory, "model.frag",
                                                 SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);
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
    };
    const SDL_GPUColorTargetDescription color_target = {.format = MODEL_COLOR_FORMAT};
    const SDL_GPUGraphicsPipelineCreateInfo create_info = {
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
    SDL_ReleaseGPUShader(model->device, fragment_shader);
    SDL_ReleaseGPUShader(model->device, vertex_shader);
    return model->pipeline != NULL;
}

static bool create_targets(EidolonModelRenderer *model, SDL_Renderer *renderer) {
    model->color_target = SDL_CreateGPUTexture(
        model->device,
        &(SDL_GPUTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = MODEL_COLOR_FORMAT,
            .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
            .width = MODEL_TARGET_WIDTH,
            .height = MODEL_TARGET_HEIGHT,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
        });
    model->depth_target = SDL_CreateGPUTexture(
        model->device,
        &(SDL_GPUTextureCreateInfo){
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
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC,
                          MODEL_TARGET_WIDTH, MODEL_TARGET_HEIGHT);
    if (model->renderer_texture == NULL) {
        return false;
    }
    SDL_SetTextureBlendMode(model->renderer_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(model->renderer_texture, SDL_SCALEMODE_LINEAR);
    return true;
}

static bool create_sampler(EidolonModelRenderer *model) {
    model->sampler = SDL_CreateGPUSampler(
        model->device,
        &(SDL_GPUSamplerCreateInfo){
            .min_filter = SDL_GPU_FILTER_LINEAR,
            .mag_filter = SDL_GPU_FILTER_LINEAR,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        });
    return model->sampler != NULL;
}

static bool render_static_model(EidolonModelRenderer *model) {
    static const float model_view_projection[16] = {
        1.65F, 0.0F,  0.0F, 0.0F,
        0.0F,  1.65F, 0.0F, 0.0F,
        0.0F,  0.0F, -1.5F, 0.0F,
        0.0F, -0.92F, 0.35F, 1.0F,
    };

    const Uint32 row_bytes = MODEL_TARGET_WIDTH * 4U;
    const Uint32 image_bytes = row_bytes * MODEL_TARGET_HEIGHT;
    SDL_GPUTransferBuffer *download = SDL_CreateGPUTransferBuffer(
        model->device, &(SDL_GPUTransferBufferCreateInfo){
                           .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                           .size = image_bytes,
                       });
    if (download == NULL) {
        return false;
    }

    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(model->device);
    if (commands == NULL) {
        SDL_ReleaseGPUTransferBuffer(model->device, download);
        return false;
    }
    SDL_PushGPUVertexUniformData(commands, 0, model_view_projection,
                                 sizeof(model_view_projection));

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
        SDL_ReleaseGPUTransferBuffer(model->device, download);
        return false;
    }

    SDL_BindGPUGraphicsPipeline(render_pass, model->pipeline);
    SDL_BindGPUVertexBuffers(render_pass, 0,
                             &(SDL_GPUBufferBinding){.buffer = model->vertex_buffer}, 1);
    SDL_BindGPUIndexBuffer(render_pass,
                           &(SDL_GPUBufferBinding){.buffer = model->index_buffer},
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);
    for (size_t draw_index = 0; draw_index < model->draw_count; ++draw_index) {
        const EidolonModelDraw *draw = &model->draws[draw_index];
        const size_t texture_index = draw->texture_index < model->texture_count
                                         ? (size_t)draw->texture_index
                                         : 0;
        const SDL_GPUTextureSamplerBinding binding = {
            .texture = model->textures[texture_index],
            .sampler = model->sampler,
        };
        SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
        SDL_DrawGPUIndexedPrimitives(render_pass, draw->index_count, 1, draw->first_index, 0,
                                     0);
    }
    SDL_EndGPURenderPass(render_pass);

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    if (copy == NULL) {
        SDL_CancelGPUCommandBuffer(commands);
        SDL_ReleaseGPUTransferBuffer(model->device, download);
        return false;
    }
    SDL_DownloadFromGPUTexture(
        copy,
        &(SDL_GPUTextureRegion){
            .texture = model->color_target,
            .w = MODEL_TARGET_WIDTH,
            .h = MODEL_TARGET_HEIGHT,
            .d = 1,
        },
        &(SDL_GPUTextureTransferInfo){
            .transfer_buffer = download,
            .pixels_per_row = MODEL_TARGET_WIDTH,
            .rows_per_layer = MODEL_TARGET_HEIGHT,
        });
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (fence == NULL) {
        SDL_ReleaseGPUTransferBuffer(model->device, download);
        return false;
    }
    SDL_GPUFence *fences[] = {fence};
    if (!SDL_WaitForGPUFences(model->device, true, fences, SDL_arraysize(fences))) {
        SDL_ReleaseGPUFence(model->device, fence);
        SDL_ReleaseGPUTransferBuffer(model->device, download);
        return false;
    }

    const void *pixels = SDL_MapGPUTransferBuffer(model->device, download, false);
    if (pixels == NULL) {
        SDL_ReleaseGPUFence(model->device, fence);
        SDL_ReleaseGPUTransferBuffer(model->device, download);
        return false;
    }
    const bool updated =
        SDL_UpdateTexture(model->renderer_texture, NULL, pixels, (int)row_bytes);
    SDL_UnmapGPUTransferBuffer(model->device, download);
    SDL_ReleaseGPUFence(model->device, fence);
    SDL_ReleaseGPUTransferBuffer(model->device, download);
    return updated;
}

EidolonModelRenderer *eidolon_model_create(SDL_Renderer *renderer, const char *model_path,
                                            const char *shader_directory) {
    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, true, NULL);
    if (device == NULL) {
        return NULL;
    }

    EidolonModelRenderer *model = SDL_calloc(1, sizeof(*model));
    if (model == NULL) {
        SDL_SetError("out of memory while creating model renderer");
        return NULL;
    }
    model->device = device;

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

    EidolonCpuGeometry geometry;
    if (!build_cpu_geometry(data, &geometry)) {
        cgltf_free(data);
        eidolon_model_destroy(model);
        return NULL;
    }
    const bool uploaded = upload_geometry(model, &geometry) && upload_textures(model, data);
    if (uploaded) {
        model->draws = geometry.draws;
        model->draw_count = geometry.draw_count;
        geometry.draws = NULL;
    }
    cpu_geometry_destroy(&geometry);
    cgltf_free(data);
    if (!uploaded || !create_sampler(model) || !create_pipeline(model, shader_directory) ||
        !create_targets(model, renderer) || !render_static_model(model)) {
        eidolon_model_destroy(model);
        return NULL;
    }

    eidolon_log_write("model", "loaded GLB draws=%zu textures=%zu gpu=%s", model->draw_count,
                      model->texture_count, SDL_GetGPUDeviceDriver(model->device));
    return model;
}

void eidolon_model_destroy(EidolonModelRenderer *model) {
    if (model == NULL) {
        return;
    }
    if (model->renderer_texture != NULL) {
        SDL_DestroyTexture(model->renderer_texture);
    }
    if (model->device != NULL && model->depth_target != NULL) {
        SDL_ReleaseGPUTexture(model->device, model->depth_target);
    }
    if (model->device != NULL && model->color_target != NULL) {
        SDL_ReleaseGPUTexture(model->device, model->color_target);
    }
    if (model->device != NULL && model->pipeline != NULL) {
        SDL_ReleaseGPUGraphicsPipeline(model->device, model->pipeline);
    }
    if (model->device != NULL && model->sampler != NULL) {
        SDL_ReleaseGPUSampler(model->device, model->sampler);
    }
    if (model->device != NULL && model->textures != NULL) {
        for (size_t texture_index = 0; texture_index < model->texture_count; ++texture_index) {
            if (model->textures[texture_index] != NULL) {
                SDL_ReleaseGPUTexture(model->device, model->textures[texture_index]);
            }
        }
    }
    if (model->device != NULL && model->index_buffer != NULL) {
        SDL_ReleaseGPUBuffer(model->device, model->index_buffer);
    }
    if (model->device != NULL && model->vertex_buffer != NULL) {
        SDL_ReleaseGPUBuffer(model->device, model->vertex_buffer);
    }
    SDL_free(model->textures);
    SDL_free(model->draws);
    if (model->device != NULL) {
        SDL_DestroyGPUDevice(model->device);
    }
    SDL_free(model);
}

SDL_Texture *eidolon_model_texture(const EidolonModelRenderer *model) {
    return model != NULL ? model->renderer_texture : NULL;
}
