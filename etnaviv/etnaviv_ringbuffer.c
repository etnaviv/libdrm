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

#include <assert.h>

#include "etnaviv_drmif.h"
#include "etnaviv_priv.h"
#include "etnaviv_ringbuffer.h"

static void *grow(void *ptr, uint32_t nr, uint32_t *max, uint32_t sz)
{
	if ((nr + 1) > *max) {
		if ((*max * 2) < (nr + 1))
			*max = nr + 5;
		else
			*max = *max * 2;
		ptr = realloc(ptr, *max * sz);
	}
	return ptr;
}

#define APPEND(x, name) ({ \
	(x)->name = grow((x)->name, (x)->nr_ ## name, &(x)->max_ ## name, sizeof((x)->name[0])); \
	(x)->nr_ ## name ++; \
})

struct etna_ringbuffer * etna_ringbuffer_new(struct etna_pipe *pipe,
		uint32_t size)
{
	struct etna_ringbuffer *ring;

	ring = calloc(1, sizeof(*ring));
	if (!ring) {
		ERROR_MSG("allocation failed");
		goto fail;
	}

	ring->ring_bo = etna_bo_new(pipe->dev, size, 0);
	if (!ring->ring_bo) {
		ERROR_MSG("ringbuffer allocation failed");
		goto fail;
	}

	ring->size = size;
	ring->pipe = pipe;
	ring->start = etna_bo_map(ring->ring_bo);
	ring->end = &(ring->start[size/4]);

	ring->cur = ring->last_start = ring->start;

	return ring;

fail:
	if (ring)
		etna_ringbuffer_del(ring);
	return NULL;
}

void etna_ringbuffer_del(struct etna_ringbuffer *ring)
{
	if (ring->ring_bo)
		etna_bo_del(ring->ring_bo);
	free(ring);
}

void etna_ringbuffer_reset(struct etna_ringbuffer *ring)
{
	uint32_t *start = ring->start;
	ring->cur = ring->last_start = start;
}

int etna_ringbuffer_flush(struct etna_ringbuffer *ring)
{
	/* TODO */
	return 0;
}

uint32_t etna_ringbuffer_timestamp(struct etna_ringbuffer *ring)
{
	return ring->last_timestamp;
}

void etna_ringbuffer_reloc(struct etna_ringbuffer *ring, const struct etna_reloc *reloc)
{
	/* TODO */
}
