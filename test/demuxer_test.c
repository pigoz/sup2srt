#include <assert.h>
#include "../src/log.h"
#include "../src/demuxer.h"

static void assertei(char *msg, int a, int b) {
    if (a != b) {
        err("test='%s': got '%d' expected '%d'\n", msg, a, b);
        abort();
    }
}

int main(int argc, char **argv) {
    int limit = 1;
    struct demuxer *d = demuxer_init(NULL, "../fixtures/bgum1.sup");
    struct sub **subs = demuxer_decode_subs(d, &limit);
    assertei("limit", limit, 1);
    assertei("start", subs[0]->start * 1000, 170629);
    assertei("end", subs[0]->end * 1000, 176385);
    talloc_free(d);
    return 0;
}
