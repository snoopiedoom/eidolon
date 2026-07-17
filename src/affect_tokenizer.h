#ifndef EIDOLON_AFFECT_TOKENIZER_H
#define EIDOLON_AFFECT_TOKENIZER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct EidolonAffectTokenizer EidolonAffectTokenizer;

EidolonAffectTokenizer *eidolon_affect_tokenizer_create(const char *vocab_path,
                                                        const char *merges_path, char *error,
                                                        size_t error_capacity);
void eidolon_affect_tokenizer_destroy(EidolonAffectTokenizer *tokenizer);

/* Writes a RoBERTa sequence including <s> and </s>. Returns zero on failure. */
size_t eidolon_affect_tokenizer_encode(EidolonAffectTokenizer *tokenizer, const char *text,
                                       int64_t *token_ids, int64_t *attention_mask, size_t capacity,
                                       char *error, size_t error_capacity);

#endif
