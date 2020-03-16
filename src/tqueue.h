#pragma once

void tqueue_dispatch(
    void (*task)(void *),
    void **args,
    int len);
