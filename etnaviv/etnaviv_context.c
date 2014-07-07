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

#define CMD_STREAM_SIZE				0x8000
#define CMD_STREAM_END_CLEARANCE	24		/* LINK op code */

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

struct etna_context * etna_context_new(struct etna_pipe *pipe)
{
	struct etna_context *ctx;
	int i;

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		ERROR_MSG("allocation failed");
		goto fail;
	}

	for (i = 0; i < NUM_CMD_STREAMS; i++) {
		void *tmp;
		ctx->cmd_stream[i] = etna_bo_new(pipe->dev, CMD_STREAM_SIZE, ETNA_BO_CMDSTREAM);

		if (!ctx->cmd_stream[i]) {
			ERROR_MSG("allocation failed");
			goto fail;
		}

		tmp = etna_bo_map(ctx->cmd_stream[i]);
		if (!tmp) {
			ERROR_MSG("mmap failed");
			goto fail;
		}
	}

	ctx->cmd = ctx->cmd_stream[0]->map;
	ctx->pipe = pipe;

	return ctx;

fail:
	if (ctx)
		etna_context_del(ctx);
	return NULL;
}

void etna_context_del(struct etna_context *ctx)
{
	int i;

	for (i = 0; i < NUM_CMD_STREAMS; i++) {
		etna_bo_del(ctx->cmd_stream[i]);
	}

	free(ctx->relocs);
	free(ctx);
}

static void switch_cmd_stream(struct etna_context *ctx)
{
	int cmd_steam_idx = (ctx->current_stream + 1) % NUM_CMD_STREAMS;

	etna_context_flush(ctx);

	ctx->current_stream = cmd_steam_idx;
	ctx->offset = 0;
	ctx->cmd = ctx->cmd_stream[cmd_steam_idx]->map;
	ctx->nr_relocs = 0;
}

void etna_context_reserve(struct etna_context *ctx, size_t n)
{
	if ((ctx->offset + n) * 4 + CMD_STREAM_END_CLEARANCE <= CMD_STREAM_SIZE)
	{
		return;
	}

	switch_cmd_stream(ctx);
}

void etna_context_emit(struct etna_context *ctx, uint32_t data)
{
	ctx->cmd[ctx->offset++] = data;
}

void etna_context_flush(struct etna_context *ctx)
{
	/* TODO*/
}

void etna_context_finish(struct etna_context *ctx)
{
	etna_context_flush(ctx);

	/* TODO */
}

void etna_context_reloc(struct etna_context *ctx, const struct etna_reloc *r)
{
	struct drm_vivante_gem_submit_reloc *reloc;
	uint32_t idx = APPEND(ctx, relocs);
	uint32_t addr;

	reloc = &ctx->relocs[idx];

	reloc->reloc_idx = 0; /* TODO */
	reloc->reloc_offset = r->offset;
	reloc->or = r->or;
	reloc->shift = r->shift;
	reloc->submit_offset = 0;	/* TODO */

	addr = r->bo->presumed;
	if (r->shift < 0)
		addr >>= -r->shift;
	else
		addr <<= r->shift;
	etna_context_emit(ctx, addr | r->or);
}
