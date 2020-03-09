#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <png.h>
#include "./ta/ta.h"
#include "./ta/ta_talloc.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define log(...) av_log(NULL, AV_LOG_INFO, __VA_ARGS__)
#define warn(...) av_log(NULL, AV_LOG_WARNING, __VA_ARGS__)
#define err(...) av_log(NULL, AV_LOG_ERROR, __VA_ARGS__)

struct sub {
    int index;
    AVSubtitle avsub;
    uint64_t pts;
    uint64_t endpts;
    double start;
    double end;
};

static void screenshot(struct sub *sub);

void free_sub(struct sub *s) {
    avsubtitle_free(&s->avsub);
    s->pts = AV_NOPTS_VALUE;
    s->endpts = AV_NOPTS_VALUE;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        err("You need to specify a media file.\n");
        return -1;
    }

    char *fname = argv[1];

    AVFormatContext *formatctx = avformat_alloc_context();
    if (!formatctx) {
        err("could not allocate memory for fmtctx\n");
        return -1;
    }

    if (avformat_open_input(&formatctx, fname, NULL, NULL) != 0) {
        err("could not open the file\n");
        return -1;
    }

    log("opened '%s' container (%s)\n", formatctx->iformat->name, fname);

    if (avformat_find_stream_info(formatctx,  NULL) < 0) {
        log("could not get the stream info\n");
        return -1;
    }

    if (formatctx->nb_streams != 1) {
        err("stream size not 1 (%d)\n", formatctx->nb_streams);
        return -1;
    }

    AVStream *stream = formatctx->streams[0];
    AVCodecParameters *codecpar = stream->codecpar;
    AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);

    if (codec == NULL) {
        err("unsupported codec!\n");
        return -1;
    }

    AVCodecContext *codecctx = avcodec_alloc_context3(codec);
    if (!codecctx) {
        err("failed to allocated memory for AVCodecContext\n");
        return -1;
    }

    if (avcodec_parameters_to_context(codecctx, codecpar) < 0) {
        err("failed to copy codec params to codec context\n");
        return -1;
    }

    if (avcodec_open2(codecctx, codec, NULL) < 0) {
        err("failed to open codec through avcodec_open2\n");
        return -1;
    }

    char *envlimit = getenv("FRAME_LIMIT");
    int limit = envlimit ? atoi(envlimit) : -1;
    AVSubtitle avsub;
    AVPacket packet;
    int got_sub;
    bool waiting = false;
    struct sub *sub = talloc_zero(NULL, struct sub);

    while (1) {
        int res = av_read_frame(formatctx, &packet);

        if (res == AVERROR_EOF) {
            log("done.\n");
            break;
        }

        if (res < 0) {
            err("read frame error: %s\n", av_err2str(res));
            break;
        }

        res = avcodec_decode_subtitle2(codecctx, &avsub, &got_sub, &packet);

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
        if (!waiting && avsub.num_rects) {
            sub->avsub = avsub;
            sub->pts = packet.pts;
            waiting = true;
            goto cleanup;
        }

        // we are waiting for a new sub, which can be either full, or empty
        // just to signal the end of the event
        if (waiting) {
            sub->endpts = packet.pts;
            AVRational tb = stream->time_base;
            sub->start = sub->pts * av_q2d(tb);
            sub->end = sub->endpts * av_q2d(tb);
            screenshot(sub);
            sub->index++;
            limit--;

            if (avsub.num_rects) {
                sub->avsub = avsub;
                sub->pts = packet.pts;
            } else {
                waiting = false;
            }
            goto cleanup;
        }


        cleanup:

        if (limit == 0) {
            warn("reached FRAME_LIMIT\n");
            break;
        }

        av_packet_unref(&packet);
    }

    return 0;
}

static void postprocess(uint8_t *bgra, uint32_t *pal) {
    uint8_t limit = 255 / 2;
    uint8_t gray = (bgra[0] + bgra[1] + bgra[2]) / 3;
    // TODO maybe preserve some anti aliasing
    bgra[0] = 0;
    bgra[1] = 0;
    bgra[2] = 0;
    // removes the border
    bgra[3] = bgra[3] > 0 && gray > limit ? 255 : 0;
}

static void screenshot(struct sub *sub) {
    log("[%04d] %f => %f\n", sub->index, sub->start, sub->end);
    int channels = 4;
    AVSubtitle avsub = sub->avsub;

    assert(avsub.num_rects == 1);

    AVSubtitleRect *rect = avsub.rects[0];
    uint8_t **data = rect->data;
    assert(rect->nb_colors > 0);
    assert(rect->nb_colors <= 256);

    uint32_t pal[256] = {0};
    memcpy(pal, data[1], rect->nb_colors * channels);

    int lsize = rect->linesize[0];
    int width = rect->w;
    int height = rect->h;
    uint32_t *pict = talloc_array(sub, uint32_t, width * height);

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            int idx = y * lsize + x;
            int color = data[0][idx];
            memcpy(pict + idx, pal + color, channels);
        }
    }

    png_structp png = png_create_write_struct(
        PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);

    png_set_IHDR(png, info, width, height, 8,
        PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_BASE);

    png_byte **rows = png_malloc(png, height * sizeof(png_byte *));
    for (int y = 0; y < height; y++) {
        png_byte *row = png_malloc(png, sizeof(uint8_t) * width * channels);
        rows[y] = row;
        for (int x = 0; x < width; x++) {
            int idx = y * lsize + x;
            uint8_t bgra[channels];
            memcpy(bgra, pict + idx, channels);
            postprocess(bgra, pal);
            *row++ = bgra[2];
            *row++ = bgra[1];
            *row++ = bgra[0];
            *row++ = bgra[3];
        }
    }

    char filename[72];
    mkdir("supdata", 0700);
    sprintf(filename, "supdata/frame-%05d.png", sub->index);
    FILE *fp = fopen(filename, "wb");
    png_init_io(png, fp);
    png_set_rows(png, info, rows);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);

    char xattrv[32];
    sprintf(xattrv, "%f", sub->start);
    fsetxattr(fileno(fp), "start-time", xattrv, strlen(xattrv), 0, 0);
    sprintf(xattrv, "%f", sub->end);
    fsetxattr(fileno(fp), "end-time", xattrv, strlen(xattrv), 0, 0);
    fclose(fp);

    for (int y = 0; y < height; y++) {
        png_free(png, rows[y]);
    }

    png_free(png, rows);
}
