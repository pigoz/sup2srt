#include "tqueue.h"

#if __APPLE__
#include <dispatch/dispatch.h>

void tqueue_dispatch(
    void (*task)(void *),
    void **args,
    int len
) {
    dispatch_queue_t queue =
        dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0);
    dispatch_group_t group = dispatch_group_create();
    for (int j = 0; j < len; j++) {
        void *arg = args[j];
        dispatch_group_async_f(group, queue, arg, task);
    }
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    dispatch_release(group);
    dispatch_release(queue);
}

#else

#include <libavutil/avutil.h>
#include <pthread.h>

void tqueue_dispatch(
    void (*task)(void *),
    void **args,
    int len
) {
    const int max_workers = 32;
    int iterations = (len / max_workers) + 1;
    pthread_t threads[max_workers];

    for (int i = 0; i < iterations; i++) {
        int lsize = FFMIN(max_workers, len - max_workers * i);

        for (int j = 0; j < lsize; j++) {
            void *arg = args[i * max_workers + j];
            pthread_create(&threads[j], NULL, (void *)task, arg);
        }

        for (int j = 0; j < lsize; j++) {
            pthread_join(threads[j], NULL);
        }
    }
}

#endif
