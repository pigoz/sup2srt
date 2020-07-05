#pragma once
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdbool.h>
#include "./ta/ta.h"
#include "./ta/ta_talloc.h"

struct demuxer {
    AVStream *stream;
    AVFormatContext *formatctx;
    AVCodecContext *codecctx;
};

struct sub {
    int index;
    AVPacket avpacket;
    AVSubtitle avsub;
    uint64_t pts;
    uint64_t endpts;
    double start;
    double end;
};

struct demuxer *demuxer_init(void *talloc_ctx, char *fname);
struct sub **demuxer_decode_subs(struct demuxer *demuxer, int *limit);
struct sub **demuxer_coalesce_subs(struct sub **, int *limit);
