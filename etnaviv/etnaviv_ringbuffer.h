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

#ifndef ETNAVIV_RINGBUFFER_H_
#define ETNAVIV_RINGBUFFER_H_

struct etna_ringbuffer {
	int size;
	uint32_t *cur, *end, *start, *last_start;
	struct etna_pipe *pipe;
	uint32_t last_timestamp;

	struct etna_bo *ring_bo;
};

struct etna_ringbuffer * etna_ringbuffer_new(struct etna_pipe *pipe,
		uint32_t size);
void etna_ringbuffer_del(struct etna_ringbuffer *ring);
void etna_ringbuffer_reset(struct etna_ringbuffer *ring);

int etna_ringbuffer_flush(struct etna_ringbuffer *ring);
uint32_t etna_ringbuffer_timestamp(struct etna_ringbuffer *ring);

static inline void etna_ringbuffer_emit(struct etna_ringbuffer *ring,
		uint32_t data)
{
	(*ring->cur++) = data;
}

struct etna_reloc {
	struct etna_bo *bo;
#define ETNA_RELOC_READ             0x0001
#define ETNA_RELOC_WRITE            0x0002
	uint32_t flags;
	uint32_t offset;
	uint32_t or;
	int32_t  shift;
};

void etna_ringbuffer_reloc(struct etna_ringbuffer *ring, const struct etna_reloc *reloc);

#endif /* ETNAVIV_RINGBUFFER_H_ */
