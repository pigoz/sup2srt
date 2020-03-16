#include "./image.h"
#include "./demuxer.h"
#include "./tqueue.h"
#include "./log.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        err("You need to specify a media file.\n");
        return -1;
    }

    char *fname = argv[1];

    char *envlimit = getenv("FRAME_LIMIT");
    int limit = envlimit ? atoi(envlimit) : -1;

    void *talloc_ctx = talloc_new(NULL);

    struct demuxer *d = demuxer_init(talloc_ctx, fname);
    struct sub **subs = demuxer_decode_subs(d, &limit);

    tqueue_dispatch((void *)sub_to_image, (void **)subs, limit);

    talloc_free(talloc_ctx);

    return 0;
}
