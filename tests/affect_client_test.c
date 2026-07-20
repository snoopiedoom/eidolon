#include "affect_client.h"

#include <SDL3/SDL.h>

#include <assert.h>

#ifndef EIDOLON_TEST_AFFECT_WORKER
#error EIDOLON_TEST_AFFECT_WORKER is required
#endif

int main(void) {
    assert(SDL_Init(0));
    EidolonAffectClient *client = eidolon_affect_client_create(EIDOLON_TEST_AFFECT_WORKER);
    assert(client != NULL);
    const uint64_t deadline = SDL_GetTicks() + 10000U;
    while (!eidolon_affect_client_available(client) && SDL_GetTicks() < deadline) {
        SDL_Delay(5U);
    }
    assert(eidolon_affect_client_available(client));
    assert(eidolon_affect_client_submit(client, 42U, "I love this. You did a wonderful job."));
    assert(eidolon_affect_client_submit(client, 43U, "What is that? I need to know."));

    uint64_t sequence = 0U;
    float probabilities[EIDOLON_GOEMOTIONS_COUNT] = {0};
    while (!eidolon_affect_client_poll(client, &sequence, probabilities) &&
           SDL_GetTicks() < deadline) {
        SDL_Delay(5U);
    }
    assert(sequence == 42U);
    assert(probabilities[EIDOLON_EMOTION_ADMIRATION] > 0.80F);
    assert(probabilities[EIDOLON_EMOTION_LOVE] > 0.60F);
    while (!eidolon_affect_client_poll(client, &sequence, probabilities) &&
           SDL_GetTicks() < deadline) {
        SDL_Delay(5U);
    }
    assert(sequence == 43U);
    assert(probabilities[EIDOLON_EMOTION_CURIOSITY] > 0.40F);
    eidolon_affect_client_destroy(client);
    SDL_Quit();
    return 0;
}
