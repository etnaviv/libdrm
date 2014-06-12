/*
 * Copyright (C) 2014 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "xf86drm.h"
#include "etnaviv_drmif.h"
#include "etnaviv_ringbuffer.h"

#include "state.xml.h"
#include "state_2d.xml.h"
#include "cmdstream.xml.h"

#include "write_bmp.h"

/* Queue load state command header (queues one word) */
static inline void etna_emit_load_state(struct etna_ringbuffer *ring,
		const uint16_t offset, const uint16_t count)
{
	uint32_t v;

	v = 	(VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE | VIV_FE_LOAD_STATE_HEADER_OFFSET(offset) |
			(VIV_FE_LOAD_STATE_HEADER_COUNT(count) & VIV_FE_LOAD_STATE_HEADER_COUNT__MASK));

	etna_ringbuffer_emit(ring, v);
}

static inline void etna_set_state(struct etna_ringbuffer *ring, uint32_t address, uint32_t value)
{
	etna_emit_load_state(ring, address >> 2, 1);
	etna_ringbuffer_emit(ring, value);
}

static inline void etna_set_state_from_bo(struct etna_ringbuffer *ring,
		uint32_t address, struct etna_bo *bo)
{
	etna_emit_load_state(ring, address >> 2, 1);

	etna_ringbuffer_reloc(ring, &(struct etna_reloc){
		.bo = bo,
		.flags = ETNA_RELOC_READ,
		.offset = 0,
		.or = 0,
		.shift = 0,
	});
}

static void gen_cmd_stream(struct etna_ringbuffer *rb, struct etna_bo *bmp, const int width, const int height)
{
	int rec;
	static int num_rects = 256;

    etna_set_state(rb, VIVS_DE_SRC_ADDRESS, 0);
    etna_set_state(rb, VIVS_DE_SRC_STRIDE, width*4);
    etna_set_state(rb, VIVS_DE_SRC_ROTATION_CONFIG, 0);
    etna_set_state(rb, VIVS_DE_SRC_CONFIG,
            VIVS_DE_SRC_CONFIG_UNK16 |
            VIVS_DE_SRC_CONFIG_SOURCE_FORMAT(DE_FORMAT_MONOCHROME) |
            VIVS_DE_SRC_CONFIG_LOCATION_MEMORY |
            VIVS_DE_SRC_CONFIG_PACK_PACKED8 |
            VIVS_DE_SRC_CONFIG_PE10_SOURCE_FORMAT(DE_FORMAT_MONOCHROME));
    etna_set_state(rb, VIVS_DE_SRC_ORIGIN, 0);
    etna_set_state(rb, VIVS_DE_SRC_SIZE, 0);
    etna_set_state(rb, VIVS_DE_SRC_COLOR_BG, 0xff44ff44);
    etna_set_state(rb, VIVS_DE_SRC_COLOR_FG, 0xff44ff44);
    etna_set_state(rb, VIVS_DE_STRETCH_FACTOR_LOW, 0);
    etna_set_state(rb, VIVS_DE_STRETCH_FACTOR_HIGH, 0);
    etna_set_state_from_bo(rb, VIVS_DE_DEST_ADDRESS, bmp);
    etna_set_state(rb, VIVS_DE_DEST_STRIDE, width*4);
    etna_set_state(rb, VIVS_DE_DEST_ROTATION_CONFIG, 0);
    etna_set_state(rb, VIVS_DE_DEST_CONFIG,
            VIVS_DE_DEST_CONFIG_FORMAT(DE_FORMAT_A8R8G8B8) |
            VIVS_DE_DEST_CONFIG_COMMAND_LINE |
            VIVS_DE_DEST_CONFIG_SWIZZLE(DE_SWIZZLE_ARGB) |
            VIVS_DE_DEST_CONFIG_TILED_DISABLE |
            VIVS_DE_DEST_CONFIG_MINOR_TILED_DISABLE
            );
    etna_set_state(rb, VIVS_DE_ROP,
            VIVS_DE_ROP_ROP_FG(0xcc) | VIVS_DE_ROP_ROP_BG(0xcc) | VIVS_DE_ROP_TYPE_ROP4);
    etna_set_state(rb, VIVS_DE_CLIP_TOP_LEFT,
            VIVS_DE_CLIP_TOP_LEFT_X(0) |
            VIVS_DE_CLIP_TOP_LEFT_Y(0)
            );
    etna_set_state(rb, VIVS_DE_CLIP_BOTTOM_RIGHT,
            VIVS_DE_CLIP_BOTTOM_RIGHT_X(width) |
            VIVS_DE_CLIP_BOTTOM_RIGHT_Y(height)
            );
    etna_set_state(rb, VIVS_DE_CONFIG, 0); /* TODO */
    etna_set_state(rb, VIVS_DE_SRC_ORIGIN_FRACTION, 0);
    etna_set_state(rb, VIVS_DE_ALPHA_CONTROL, 0);
    etna_set_state(rb, VIVS_DE_ALPHA_MODES, 0);
    etna_set_state(rb, VIVS_DE_DEST_ROTATION_HEIGHT, 0);
    etna_set_state(rb, VIVS_DE_SRC_ROTATION_HEIGHT, 0);
    etna_set_state(rb, VIVS_DE_ROT_ANGLE, 0);

    /* Clear color PE20 */
    etna_set_state(rb, VIVS_DE_CLEAR_PIXEL_VALUE32, 0xff40ff40);
    /* Clear color PE10 */
    etna_set_state(rb, VIVS_DE_CLEAR_BYTE_MASK, 0xff);
    etna_set_state(rb, VIVS_DE_CLEAR_PIXEL_VALUE_LOW, 0xff40ff40);
    etna_set_state(rb, VIVS_DE_CLEAR_PIXEL_VALUE_HIGH, 0xff40ff40);

    etna_set_state(rb, VIVS_DE_DEST_COLOR_KEY, 0);
    etna_set_state(rb, VIVS_DE_GLOBAL_SRC_COLOR, 0);
    etna_set_state(rb, VIVS_DE_GLOBAL_DEST_COLOR, 0);
    etna_set_state(rb, VIVS_DE_COLOR_MULTIPLY_MODES, 0);
    etna_set_state(rb, VIVS_DE_PE_TRANSPARENCY, 0);
    etna_set_state(rb, VIVS_DE_PE_CONTROL, 0);
    etna_set_state(rb, VIVS_DE_PE_DITHER_LOW, 0xffffffff);
    etna_set_state(rb, VIVS_DE_PE_DITHER_HIGH, 0xffffffff);

    /* Queue DE command */
    etna_ringbuffer_emit(rb, VIV_FE_DRAW_2D_HEADER_OP_DRAW_2D | VIV_FE_DRAW_2D_HEADER_COUNT(num_rects));

    rb->cur++; /* rectangles start aligned */
    for(rec = 0; rec < num_rects; rec++)
    {
        int x1 = 0;
        int y1 = rec;
        int x2 = 256;
        int y2 = rec;

        etna_ringbuffer_emit(rb, VIV_FE_DRAW_2D_TOP_LEFT_X(x1) |
                                      VIV_FE_DRAW_2D_TOP_LEFT_Y(y1));
        etna_ringbuffer_emit(rb, VIV_FE_DRAW_2D_BOTTOM_RIGHT_X(x2) |
                                      VIV_FE_DRAW_2D_BOTTOM_RIGHT_Y(y2));
    }
    etna_set_state(rb, 1, 0);
    etna_set_state(rb, 1, 0);
    etna_set_state(rb, 1, 0);

    etna_set_state(rb, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_PE2D);
}

int main(int argc, char *argv[])
{
	const int width = 256;
	const int height = 256;
	const size_t bmp_size = width * height * 4;

	struct etna_device *dev;
	struct etna_pipe *pipe;
	struct etna_bo *bmp;
	struct etna_ringbuffer *rb;

	drmVersionPtr version;
	int fd, ret = 0;

	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		return 1;

	version = drmGetVersion(fd);
	if (version) {
		printf("Version: %d.%d.%d\n", version->version_major,
		       version->version_minor, version->version_patchlevel);
		printf("  Name: %s\n", version->name);
		printf("  Date: %s\n", version->date);
		printf("  Description: %s\n", version->desc);
		drmFreeVersion(version);
	}

	dev = etna_device_new(fd);
	if (!dev) {
		ret = 2;
		goto fail;
	}

	pipe = etna_pipe_new(dev, ETNA_PIPE_2D);
	if (!pipe) {
		ret = 3;
		goto fail;
	}

	bmp = etna_bo_new(dev, bmp_size, 0);
	if (!bmp) {
		ret = 4;
		goto fail;
	}
	memset(etna_bo_map(bmp), 0, bmp_size);


	rb = etna_ringbuffer_new(pipe, 1024);
	if (!rb) {
		ret = 5;
		goto fail;
	}

	/* generate command sequence */
	gen_cmd_stream(rb, bmp, width, height);

	etna_ringbuffer_flush(rb);

	bmp_dump32(etna_bo_map(bmp), width, height, false, "/tmp/etna.bmp");

fail:
	if (rb)
		etna_ringbuffer_del(rb);

	if (pipe)
		etna_pipe_del(pipe);

	if (dev)
		etna_device_del(dev);

	close(fd);

	return ret;
}
