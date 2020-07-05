#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <png.h>
#include "./log.h"
#include "./image.h"

static void subpp(uint8_t *bgra, uint32_t *pal) {
    uint8_t limit = 255 / 2.2;
    uint8_t gray = (bgra[0] + bgra[1] + bgra[2]) / 3;
    // TODO maybe preserve some anti aliasing
    bgra[0] = 0;
    bgra[1] = 0;
    bgra[2] = 0;
    // removes the border
    bgra[3] = bgra[3] > 0 && gray > limit ? 255 : 0;
}

void rect_to_image(struct sub *sub, int rect_idx) {
    int channels = 4;
    AVSubtitle avsub = sub->avsub;

    AVSubtitleRect *rect = avsub.rects[rect_idx];
    uint8_t **data = rect->data;
    assert(rect->nb_colors >= 0);
    assert(rect->nb_colors <= 256);

    uint32_t pal[256] = {0};
    memcpy(pal, data[1], rect->nb_colors * channels);

    int lsize = rect->linesize[0];
    int width = rect->w;
    int height = rect->h;

    if (width == 0 || height == 0) {
        return;
    }

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
            subpp(bgra, pal);
            *row++ = bgra[2];
            *row++ = bgra[1];
            *row++ = bgra[0];
            *row++ = bgra[3];
        }
    }

    char filename[72];
    mkdir("supdata", 0700);
    sprintf(filename, "supdata/frame-%05d-%05d.png", sub->index, rect_idx);
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        err("failed to open file '%s' (%s)\n", filename, strerror(errno));
        goto cleanup;
    }
    png_init_io(png, fp);
    png_set_rows(png, info, rows);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);

    char xattrv[32];
    sprintf(xattrv, "%f", sub->start);
    fsetxattr(fileno(fp), "start-time", xattrv, strlen(xattrv), 0, 0);
    sprintf(xattrv, "%f", sub->end);
    fsetxattr(fileno(fp), "end-time", xattrv, strlen(xattrv), 0, 0);
    fclose(fp);

cleanup:

    for (int y = 0; y < height; y++) {
        png_free(png, rows[y]);
    }

    png_free(png, rows);
}

void sub_to_image(struct sub *sub) {
    log("[%04d] %f => %f\n", sub->index, sub->start, sub->end);

    AVSubtitle avsub = sub->avsub;
    for (int i = 0; i < avsub.num_rects; i++) {
        rect_to_image(sub, i);
    }
}
