#include "hook_output.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    FILE *input = NULL;
#ifdef _WIN32
    assert(tmpfile_s(&input) == 0);
#else
    input = tmpfile();
#endif
    assert(input != NULL);
    assert(fprintf(input, "{\"transcript_path\":\"%s\"}", EIDOLON_TEST_TRANSCRIPT) > 0);
    rewind(input);

    char output[128];
    assert(eidolon_hook_read_agent_output(input, output, sizeof(output)));
    assert(strcmp(output, "hello\nworld") == 0);
    fclose(input);
    return 0;
}
