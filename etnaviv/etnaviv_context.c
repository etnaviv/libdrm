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

	list_inithead(&ctx->submit_list);

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
	ctx->nr_bos = 0;
	ctx->nr_cmds = 0;
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

uint32_t etna_context_timestamp(struct etna_context *ctx)
{
	return ctx->last_timestamp;
}

/* add (if needed) bo, return idx: */
static uint32_t bo2idx(struct etna_context *ctx, struct etna_bo *bo, uint32_t flags)
{
	int id = ctx->pipe->id;
	uint32_t idx;
	if (!bo->indexp1[id]) {
		struct list_head *list = &bo->list[id];
		idx = APPEND(ctx, bos);
		ctx->bos[idx].flags = 0;
		ctx->bos[idx].handle = bo->handle;
		ctx->bos[idx].presumed = bo->presumed;
		bo->indexp1[id] = idx + 1;

		assert(LIST_IS_EMPTY(list));
		etna_bo_ref(bo);
		list_addtail(list, &ctx->submit_list);
	} else {
		idx = bo->indexp1[id] - 1;
	}
	if (flags & ETNA_RELOC_READ)
		ctx->bos[idx].flags |= ETNA_SUBMIT_BO_READ;
	if (flags & ETNA_RELOC_WRITE)
		ctx->bos[idx].flags |= ETNA_SUBMIT_BO_WRITE;
	return idx;
}

void etna_context_flush(struct etna_context *ctx)
{
	int ret, idx, id = ctx->pipe->id;
	struct etna_bo *etna_bo = NULL, *tmp;
	struct drm_vivante_gem_submit_cmd *cmd = NULL;

	struct drm_vivante_gem_submit req = {
			.pipe = ctx->pipe->id,
	};

	/* TODO: we only support _ONE_ cmd per submit ioctl for now. This makes
	 *       things simpler. */
	idx = APPEND(ctx, cmds);
	cmd = &ctx->cmds[idx];
	cmd->type = ETNA_SUBMIT_CMD_BUF;
	cmd->submit_idx = bo2idx(ctx, ctx->cmd_stream[ctx->current_stream], ETNA_RELOC_READ);
	cmd->submit_offset = 0;
	cmd->size = ctx->offset;
	cmd->pad = 0;

	req.cmds = VOID2U64(cmd);
	req.nr_cmds = ctx->nr_cmds;
	req.bos = VOID2U64(ctx->bos);
	req.nr_bos = ctx->nr_bos;

	ret = drmCommandWriteRead(ctx->pipe->dev->fd, DRM_VIVANTE_GEM_SUBMIT,
			&req, sizeof(req));

	if (ret) {
		ERROR_MSG("submit failed: %d (%s)", ret, strerror(errno));
	} else {
		ctx->last_timestamp = req.fence;
	}

	LIST_FOR_EACH_ENTRY_SAFE(etna_bo, tmp, &ctx->submit_list, list[id]) {
		struct list_head *list = &etna_bo->list[id];
		list_delinit(list);
		etna_bo->indexp1[id] = 0;
	}
}

void etna_context_finish(struct etna_context *ctx)
{
	etna_context_flush(ctx);
	etna_pipe_wait(ctx->pipe, ctx->last_timestamp);
}

void etna_context_reloc(struct etna_context *ctx, const struct etna_reloc *r)
{
	struct drm_vivante_gem_submit_reloc *reloc;
	uint32_t idx = APPEND(ctx, relocs);
	uint32_t addr;

	reloc = &ctx->relocs[idx];

	reloc->reloc_idx = bo2idx(ctx, r->bo, r->flags);
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
