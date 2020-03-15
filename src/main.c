#include <sys/xattr.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "./image.h"
#include "./demuxer.h"
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

    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        err("getrlimit error %s\n", strerror(errno));
    }

    int maxfiles = ((long) rl.rlim_cur) * 0.80;
    int iterations = (limit / maxfiles) + 1;
    pthread_t threads[maxfiles];

    for (int i = 0; i < iterations; i++) {
        int lsize = FFMIN(maxfiles, limit - maxfiles * i);

        for (int j = 0; j < lsize; j++) {
            struct sub *sub = subs[i * maxfiles + j];
            log("[%04d] %f => %f\n", sub->index, sub->start, sub->end);
            pthread_create(&threads[j], NULL, (void *)sub_to_image, sub);
        }

        for (int j = 0; j < lsize; j++) {
            pthread_join(threads[j], NULL);
        }
    }

    talloc_free(talloc_ctx);

    return 0;
}

