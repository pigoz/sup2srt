#include <assert.h>
#include "./log.h"
#include "./demuxer.h"

void destroy_sub(void *p) {
    struct sub *sub = (struct sub *)p;
    av_packet_unref(&sub->avpacket);
    avsubtitle_free(&sub->avsub);
}

struct sub *create_sub(void *talloc_ctx, AVPacket avpacket, AVSubtitle avsub) {
    struct sub *sub = talloc_zero(talloc_ctx, struct sub);
    talloc_set_destructor(sub, destroy_sub);
    sub->avpacket = avpacket;
    sub->avsub = avsub;
    sub->pts = avpacket.pts;
    return sub;
}


struct demuxer *demuxer_init(void *talloc_ctx, char *fname) {
    AVFormatContext *formatctx = avformat_alloc_context();
    if (!formatctx) {
        err("could not allocate memory for fmtctx\n");
        return NULL;
    }

    if (avformat_open_input(&formatctx, fname, NULL, NULL) != 0) {
        err("could not open the file\n");
        return NULL;
    }

    log("opened '%s' container (%s)\n", formatctx->iformat->name, fname);

    if (avformat_find_stream_info(formatctx,  NULL) < 0) {
        log("could not get the stream info\n");
        return NULL;
    }

    if (formatctx->nb_streams != 1) {
        err("stream size not 1 (%d)\n", formatctx->nb_streams);
        return NULL;
    }

    AVStream *stream = formatctx->streams[0];
    AVCodecParameters *codecpar = stream->codecpar;
    AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);

    if (codec == NULL) {
        err("unsupported codec!\n");
        return NULL;
    }

    AVCodecContext *codecctx = avcodec_alloc_context3(codec);
    if (!codecctx) {
        err("failed to allocate memory for AVCodecContext\n");
        return NULL;
    }

    if (avcodec_parameters_to_context(codecctx, codecpar) < 0) {
        err("failed to copy codec params to codec context\n");
        return NULL;
    }

    if (avcodec_open2(codecctx, codec, NULL) < 0) {
        err("failed to open codec through avcodec_open2\n");
        return NULL;
    }

    struct demuxer *d = talloc_ptrtype(talloc_ctx, d);
    d->stream = stream;
    d->codecctx = codecctx;
    d->formatctx = formatctx;

    return d;
}

struct sub **demuxer_decode_subs(struct demuxer *demuxer, int *limit) {
    struct sub **list = talloc_array(demuxer, struct sub *, 0);
    int list_num = 0;

    AVSubtitle avsub;
    AVPacket packet;
    int got_sub;
    int index = 0;
    bool waiting = false;

    while (1) {
        int res = av_read_frame(demuxer->formatctx, &packet);

        if (res == AVERROR_EOF) {
            log("eof.\n");
            break;
        }

        if (res < 0) {
            err("read frame error: %s\n", av_err2str(res));
            break;
        }

        res = avcodec_decode_subtitle2(
            demuxer->codecctx, &avsub, &got_sub, &packet);

        if (res < 0) {
            err("decode sub error: %s", av_err2str(res));
            continue;
        }

        if (!got_sub) {
            continue;
        }

        assert(avsub.num_rects < 2);

        // a new sub will come in and we wait for the next, which can be full
        // or empty
        if (!waiting && avsub.num_rects > 0) {
            struct sub *sub = create_sub(list, packet, avsub);
            MP_TARRAY_APPEND(demuxer, list, list_num, sub);
            waiting = true;
            goto cleanup;
        }

        // we are waiting for a new sub, which can be either full, or empty
        // just to signal the end of the event
        if (waiting) {
            struct sub *sub = list[list_num - 1];
            sub->endpts = packet.pts;
            AVRational tb = demuxer->stream->time_base;
            sub->start = sub->pts * av_q2d(tb);
            sub->end = sub->endpts * av_q2d(tb);
            sub->index = index++;
            (*limit)--;

            if (avsub.num_rects) {
                struct sub *sub = create_sub(list, packet, avsub);
                MP_TARRAY_APPEND(demuxer, list, list_num, sub);
            } else {
                av_packet_unref(&packet);
                waiting = false;
            }
            goto cleanup;
        }

        cleanup:

        if (*limit == 0) {
            warn("reached limit\n");
            break;
        }
    }

    *limit = index;

    return list;
}
