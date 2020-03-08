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
    bool complete;
    AVSubtitle avsub;
    uint64_t pts;
    uint64_t endpts;
    double start;
    double end;
};

static void screenshot(struct sub *sub);

void free_sub(struct sub *s) {
    s->complete = false;
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
    AVSubtitle sub;
    AVPacket packet;
    int got_sub;
    struct sub *prev = talloc_zero(NULL, struct sub);

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

        res = avcodec_decode_subtitle2(codecctx, &sub, &got_sub, &packet);

        if (res < 0) {
            err("decode sub error: %s", av_err2str(res));
            continue;
        }

        if (!got_sub) {
            continue;
        }

        if (!prev->complete && !sub.num_rects) {
            prev->complete = true;
            prev->endpts = packet.pts;
            AVRational tb = stream->time_base;
            prev->start = prev->pts * av_q2d(tb);
            prev->end = prev->endpts * av_q2d(tb);
            screenshot(prev);
            prev->index++;
            limit--;
        } else {
            prev->complete = false;
            prev->pts = packet.pts;
            prev->avsub = sub;
        }

        if (limit == 0) {
            warn("reached FRAME_LIMIT\n");
            break;
        }

        av_packet_unref(&packet);


    }

    return 0;
}

static void screenshot(struct sub *sub) {
    log("[%04d] %f => %f\n", sub->index, sub->start, sub->end);
    int pixel_size = 4;
    int depth = 8;
    AVSubtitle avsub = sub->avsub;
    int width = 1920;
    int height = 1080;
    int lsize = 0;
    uint8_t *pict;

    for (int i = 0; i < avsub.num_rects; i++) {
        AVSubtitleRect *rect = avsub.rects[i];
        uint8_t **data = rect->data;
        assert(rect->nb_colors > 0);
        assert(rect->nb_colors <= 256);
        uint32_t pal[256] = {0};
        memcpy(pal, data[1], rect->nb_colors * 4);
        lsize = rect->linesize[0];
        width = rect->w;
        height = rect->h;
        pict = talloc_array(NULL, uint8_t, 4 * width * height);

        for(int y = 0; y < rect->h; y++) {
            for(int x = 0; x < rect->w; x++) {
                int idx = y * lsize + x;
                int color = data[0][idx];
                memcpy(&pict[idx], &pal[color], 4);
                /*
                uint8_t rgba[4];
                continue;
                log("%d %d => %d-%d-%d-%d \n",
                    x, y, rgba[0], rgba[1], rgba[2], rgba[3]);
                */
            }
        }
    }

    png_structp png = png_create_write_struct(
        PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);

    png_set_IHDR(png, info, width, height, depth, PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);

    png_byte **rows = png_malloc(png, height * sizeof (png_byte *));
    for (int y = 0; y < height; y++) {
        png_byte *row = png_malloc(png, sizeof(uint8_t) * width * pixel_size);
        rows[y] = row;
        for (int x = 0; x < width; x++) {
            uint8_t *rgba = &pict[y * lsize + x];
            int l = 80;
            *row++ = rgba[0] > l ? 0 : 255;
            *row++ = rgba[0] > l ? 0 : 255;
            *row++ = rgba[0] > l ? 0 : 255;
            *row++ = rgba[0] <= 112 ? 0 : 255;
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
