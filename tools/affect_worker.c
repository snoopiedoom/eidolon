#include "affect_protocol.h"
#include "affect_tokenizer.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#include <onnxruntime_c_api.h>

#ifndef EIDOLON_AFFECT_MODEL_PATH
#error EIDOLON_AFFECT_MODEL_PATH is required
#endif
#ifndef EIDOLON_AFFECT_VOCAB_PATH
#error EIDOLON_AFFECT_VOCAB_PATH is required
#endif
#ifndef EIDOLON_AFFECT_MERGES_PATH
#error EIDOLON_AFFECT_MERGES_PATH is required
#endif

#define TOKEN_CAPACITY 256U
#define GOEMOTIONS_COUNT 28U

static const char *const emotion_names[GOEMOTIONS_COUNT] = {
    "admiration",    "amusement",   "anger",    "annoyance",      "approval",    "caring",
    "confusion",     "curiosity",   "desire",   "disappointment", "disapproval", "disgust",
    "embarrassment", "excitement",  "fear",     "gratitude",      "grief",       "joy",
    "love",          "nervousness", "optimism", "pride",          "realization", "relief",
    "remorse",       "sadness",     "surprise", "neutral",
};

typedef struct Worker {
    const OrtApi *ort;
    OrtEnv *environment;
    OrtSessionOptions *options;
    OrtSession *session;
    OrtMemoryInfo *memory_info;
    EidolonAffectTokenizer *tokenizer;
} Worker;

static bool ort_ok(const OrtApi *ort, OrtStatus *status, const char *operation) {
    if (status == NULL) {
        return true;
    }
    fprintf(stderr, "affect worker: %s: %s\n", operation, ort->GetErrorMessage(status));
    ort->ReleaseStatus(status);
    return false;
}

static void worker_destroy(Worker *worker) {
    if (worker->ort != NULL) {
        if (worker->memory_info != NULL) {
            worker->ort->ReleaseMemoryInfo(worker->memory_info);
        }
        if (worker->session != NULL) {
            worker->ort->ReleaseSession(worker->session);
        }
        if (worker->options != NULL) {
            worker->ort->ReleaseSessionOptions(worker->options);
        }
        if (worker->environment != NULL) {
            worker->ort->ReleaseEnv(worker->environment);
        }
    }
    eidolon_affect_tokenizer_destroy(worker->tokenizer);
    memset(worker, 0, sizeof(*worker));
}

static bool worker_create(Worker *worker) {
    memset(worker, 0, sizeof(*worker));
    worker->ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (worker->ort == NULL) {
        fprintf(stderr, "affect worker: incompatible ONNX Runtime API\n");
        return false;
    }
    if (!ort_ok(worker->ort,
                worker->ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "eidolon-affect",
                                       &worker->environment),
                "CreateEnv") ||
        !ort_ok(worker->ort, worker->ort->CreateSessionOptions(&worker->options),
                "CreateSessionOptions") ||
        !ort_ok(worker->ort, worker->ort->SetIntraOpNumThreads(worker->options, 1),
                "SetIntraOpNumThreads") ||
        !ort_ok(worker->ort,
                worker->ort->SetSessionGraphOptimizationLevel(worker->options, ORT_ENABLE_ALL),
                "SetSessionGraphOptimizationLevel")) {
        worker_destroy(worker);
        return false;
    }
#if defined(_WIN32)
    const wchar_t model_path[] = EIDOLON_AFFECT_MODEL_PATH;
#else
    const char *model_path = EIDOLON_AFFECT_MODEL_PATH;
#endif
    if (!ort_ok(worker->ort,
                worker->ort->CreateSession(worker->environment, model_path, worker->options,
                                           &worker->session),
                "CreateSession") ||
        !ort_ok(worker->ort,
                worker->ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault,
                                                 &worker->memory_info),
                "CreateCpuMemoryInfo")) {
        worker_destroy(worker);
        return false;
    }
    char tokenizer_error[256] = {0};
    worker->tokenizer =
        eidolon_affect_tokenizer_create(EIDOLON_AFFECT_VOCAB_PATH, EIDOLON_AFFECT_MERGES_PATH,
                                        tokenizer_error, sizeof(tokenizer_error));
    if (worker->tokenizer == NULL) {
        fprintf(stderr, "affect worker: %s\n", tokenizer_error);
        worker_destroy(worker);
        return false;
    }
    return true;
}

static bool worker_infer(Worker *worker, const char *text, float probabilities[GOEMOTIONS_COUNT]) {
    int64_t token_ids[TOKEN_CAPACITY] = {0};
    int64_t attention_mask[TOKEN_CAPACITY] = {0};
    char tokenizer_error[256] = {0};
    const size_t token_count =
        eidolon_affect_tokenizer_encode(worker->tokenizer, text, token_ids, attention_mask,
                                        TOKEN_CAPACITY, tokenizer_error, sizeof(tokenizer_error));
    if (token_count == 0U) {
        fprintf(stderr, "affect worker: %s\n", tokenizer_error);
        return false;
    }
    const int64_t shape[2] = {1, (int64_t)token_count};
    OrtValue *inputs[2] = {NULL, NULL};
    const char *input_names[2] = {"input_ids", "attention_mask"};
    const char *output_names[1] = {"logits"};
    OrtValue *output = NULL;
    bool success =
        ort_ok(worker->ort,
               worker->ort->CreateTensorWithDataAsOrtValue(
                   worker->memory_info, token_ids, token_count * sizeof(token_ids[0]), shape, 2U,
                   ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &inputs[0]),
               "CreateTensor(input_ids)") &&
        ort_ok(worker->ort,
               worker->ort->CreateTensorWithDataAsOrtValue(
                   worker->memory_info, attention_mask, token_count * sizeof(attention_mask[0]),
                   shape, 2U, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &inputs[1]),
               "CreateTensor(attention_mask)") &&
        ort_ok(worker->ort,
               worker->ort->Run(worker->session, NULL, input_names, (const OrtValue *const *)inputs,
                                2U, output_names, 1U, &output),
               "Run");
    float *logits = NULL;
    if (success) {
        success = ort_ok(worker->ort, worker->ort->GetTensorMutableData(output, (void **)&logits),
                         "GetTensorMutableData");
    }
    if (success) {
        for (size_t index = 0U; index < GOEMOTIONS_COUNT; ++index) {
            probabilities[index] = 1.0F / (1.0F + expf(-logits[index]));
        }
    }
    if (output != NULL) {
        worker->ort->ReleaseValue(output);
    }
    if (inputs[1] != NULL) {
        worker->ort->ReleaseValue(inputs[1]);
    }
    if (inputs[0] != NULL) {
        worker->ort->ReleaseValue(inputs[0]);
    }
    return success;
}

static int compare_probabilities(const void *left, const void *right) {
    const float left_value = ((const float *)left)[1];
    const float right_value = ((const float *)right)[1];
    return left_value < right_value ? 1 : left_value > right_value ? -1 : 0;
}

static int run_once(Worker *worker, const char *text) {
    float probabilities[GOEMOTIONS_COUNT] = {0};
    if (!worker_infer(worker, text, probabilities)) {
        return 1;
    }
    float ranked[GOEMOTIONS_COUNT][2];
    for (size_t index = 0U; index < GOEMOTIONS_COUNT; ++index) {
        ranked[index][0] = (float)index;
        ranked[index][1] = probabilities[index];
    }
    qsort(ranked, GOEMOTIONS_COUNT, sizeof(ranked[0]), compare_probabilities);
    for (size_t rank = 0U; rank < 8U; ++rank) {
        const size_t index = (size_t)ranked[rank][0];
        printf("%-16s %.5f\n", emotion_names[index], probabilities[index]);
    }
    return 0;
}

static bool read_exact(void *data, size_t length) {
    return fread(data, 1U, length, stdin) == length;
}

static int run_protocol(Worker *worker) {
#if defined(_WIN32)
    (void)_setmode(_fileno(stdin), _O_BINARY);
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    for (;;) {
        EidolonAffectRequestHeader request;
        if (!read_exact(&request, sizeof(request))) {
            return feof(stdin) ? 0 : 1;
        }
        if (request.magic != EIDOLON_AFFECT_PROTOCOL_MAGIC ||
            request.version != EIDOLON_AFFECT_PROTOCOL_VERSION ||
            request.text_length > EIDOLON_AFFECT_TEXT_CAPACITY) {
            return 2;
        }
        char text[EIDOLON_AFFECT_TEXT_CAPACITY + 1U];
        if (!read_exact(text, request.text_length)) {
            return 1;
        }
        text[request.text_length] = '\0';
        EidolonAffectResponse response = {EIDOLON_AFFECT_PROTOCOL_MAGIC,
                                          EIDOLON_AFFECT_PROTOCOL_VERSION,
                                          request.sequence,
                                          1U,
                                          {0}};
        if (worker_infer(worker, text, response.probabilities)) {
            response.status = 0U;
        }
        if (fwrite(&response, 1U, sizeof(response), stdout) != sizeof(response) ||
            fflush(stdout) != 0) {
            return 1;
        }
    }
}

int main(int argc, char **argv) {
    Worker worker;
    if (!worker_create(&worker)) {
        return 1;
    }
    const int result = argc == 3 && strcmp(argv[1], "--text") == 0 ? run_once(&worker, argv[2])
                       : argc == 1                                 ? run_protocol(&worker)
                                                                   : 2;
    if (result == 2) {
        fprintf(stderr, "usage: eidolon-affect-worker [--text TEXT]\n");
    }
    worker_destroy(&worker);
    return result;
}
