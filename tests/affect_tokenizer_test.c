#include "affect_tokenizer.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#ifndef EIDOLON_TEST_AFFECT_VOCAB
#error EIDOLON_TEST_AFFECT_VOCAB is required
#endif
#ifndef EIDOLON_TEST_AFFECT_MERGES
#error EIDOLON_TEST_AFFECT_MERGES is required
#endif

int main(void) {
    char error[256] = {0};
    EidolonAffectTokenizer *tokenizer = eidolon_affect_tokenizer_create(
        EIDOLON_TEST_AFFECT_VOCAB, EIDOLON_TEST_AFFECT_MERGES, error, sizeof(error));
    assert(tokenizer != NULL);

    int64_t ids[8] = {0};
    int64_t mask[8] = {0};
    size_t count =
        eidolon_affect_tokenizer_encode(tokenizer, "hello", ids, mask, 8U, error, sizeof(error));
    assert(count == 3U);
    assert(ids[0] == 0 && ids[1] == 11 && ids[2] == 2 && ids[3] == 1);
    assert(mask[0] == 1 && mask[2] == 1 && mask[3] == 0);

    count =
        eidolon_affect_tokenizer_encode(tokenizer, " hello", ids, mask, 8U, error, sizeof(error));
    assert(count == 3U);
    assert(ids[0] == 0 && ids[1] == 13 && ids[2] == 2);

    eidolon_affect_tokenizer_destroy(tokenizer);
    return 0;
}
