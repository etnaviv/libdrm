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

struct etna_ringbuffer_internal  {
	struct etna_ringbuffer base;
	struct etna_bo *ring_bo;

	struct list_head submit_list;

	/* bo's table: */
	struct drm_vivante_gem_submit_bo *bos;
	uint32_t nr_bos, max_bos;

	/* cmd's table: */
	struct drm_vivante_gem_submit_cmd *cmds;
	uint32_t nr_cmds, max_cmds;
	struct etna_ringbuffer **rings;
	uint32_t nr_rings, max_rings;

	/* reloc's table: */
	struct drm_vivante_gem_submit_reloc *relocs;
	uint32_t nr_relocs, max_relocs;
};

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

static inline struct etna_ringbuffer_internal * to_etna_ringbuffer_internal(struct etna_ringbuffer *x)
{
	return (struct etna_ringbuffer_internal *)x;
}

struct etna_ringbuffer * etna_ringbuffer_new(struct etna_pipe *pipe,
		uint32_t size)
{
	struct etna_ringbuffer_internal *ring_internal;
	struct etna_ringbuffer *ring = NULL;

	ring_internal = calloc(1, sizeof(*ring_internal));
	if (!ring_internal) {
		ERROR_MSG("allocation failed");
		goto fail;
	}

	ring = &ring_internal->base;

	list_inithead(&ring_internal->submit_list);

	ring_internal->ring_bo = etna_bo_new(pipe->dev, size, 0);
	if (!ring_internal->ring_bo) {
		ERROR_MSG("ringbuffer allocation failed");
		goto fail;
	}

	ring->size = size;
	ring->pipe = pipe;
	ring->start = etna_bo_map(ring_internal->ring_bo);
	ring->end = &(ring->start[size/4]);

	ring->cur = ring->last_start = ring->start;

	return ring;

fail:
	if (ring)
		etna_ringbuffer_del(ring);
	return NULL;
}

/* add (if needed) bo, return idx: */
static uint32_t bo2idx(struct etna_ringbuffer *ring, struct etna_bo *bo, uint32_t flags)
{
	struct etna_ringbuffer_internal *ring_internal = to_etna_ringbuffer_internal(ring);
	int id = ring->pipe->id;
	uint32_t idx;
	if (!bo->indexp1[id]) {
		struct list_head *list = &bo->list[id];
		idx = APPEND(ring_internal, bos);
		ring_internal->bos[idx].flags = 0;
		ring_internal->bos[idx].handle = bo->handle;
		ring_internal->bos[idx].presumed = bo->presumed;
		bo->indexp1[id] = idx + 1;

		assert(LIST_IS_EMPTY(list));
		etna_bo_ref(bo);
		list_addtail(list, &ring_internal->submit_list);
	} else {
		idx = bo->indexp1[id] - 1;
	}
	if (flags & ETNA_RELOC_READ)
		ring_internal->bos[idx].flags |= ETNA_SUBMIT_BO_READ;
	if (flags & ETNA_RELOC_WRITE)
		ring_internal->bos[idx].flags |= ETNA_SUBMIT_BO_WRITE;
	return idx;
}

static int check_cmd_bo(struct etna_ringbuffer *ring,
		struct drm_vivante_gem_submit_cmd *cmd, struct etna_bo *bo)
{
	struct etna_ringbuffer_internal *ring_internal = to_etna_ringbuffer_internal(ring);
	return ring_internal->bos[cmd->submit_idx].handle == bo->handle;
}

static uint32_t offset_bytes(void *end, void *start)
{
	return ((char *)end) - ((char *)start);
}

static struct drm_vivante_gem_submit_cmd * get_cmd(struct etna_ringbuffer *ring,
		struct etna_ringbuffer *target_ring, struct etna_bo *target_bo,
		uint32_t submit_offset, uint32_t size, uint32_t type)
{
	struct etna_ringbuffer_internal *ring_internal = to_etna_ringbuffer_internal(ring);
	struct drm_vivante_gem_submit_cmd *cmd = NULL;
	uint32_t i;

	/* figure out if we already have a cmd buf: */
	for (i = 0; i < ring_internal->nr_cmds; i++) {
		cmd = &ring_internal->cmds[i];
		if ((cmd->submit_offset == submit_offset) &&
				(cmd->size == size) &&
				(cmd->type == type) &&
				check_cmd_bo(ring, cmd, target_bo))
			break;
		cmd = NULL;
	}

	/* create cmd buf if not: */
	if (!cmd) {
		uint32_t idx = APPEND(ring_internal, cmds);
		APPEND(ring_internal, rings);
		ring_internal->rings[idx] = target_ring;
		cmd = &ring_internal->cmds[idx];
		cmd->type = type;
		cmd->submit_idx = bo2idx(ring, target_bo, ETNA_RELOC_READ);
		cmd->submit_offset = submit_offset;
		cmd->size = size;
		cmd->pad = 0;
	}

	return cmd;
}

static uint32_t find_next_reloc_idx(struct etna_ringbuffer_internal *ring_internal,
		uint32_t start, uint32_t offset)
{
	uint32_t i;

	/* a binary search would be more clever.. */
	for (i = start; i < ring_internal->nr_relocs; i++) {
		struct drm_vivante_gem_submit_reloc *reloc = &ring_internal->relocs[i];
		if (reloc->submit_offset >= offset)
			return i;
	}

	return i;
}

void etna_ringbuffer_del(struct etna_ringbuffer *ring)
{
	struct etna_ringbuffer_internal *ring_internal = to_etna_ringbuffer_internal(ring);
	if (ring_internal->ring_bo)
		etna_bo_del(ring_internal->ring_bo);
	free(ring_internal);
}

void etna_ringbuffer_reset(struct etna_ringbuffer *ring)
{
	struct etna_ringbuffer_internal *ring_internal = to_etna_ringbuffer_internal(ring);
	unsigned i;
	uint32_t *start = ring->start;
	ring->cur = ring->last_start = start;

	/* for each of the cmd buffers, clear their reloc's: */
	for (i = 0; i < ring_internal->nr_cmds; i++) {
		struct etna_ringbuffer_internal *target_ring = to_etna_ringbuffer_internal(ring_internal->rings[i]);
		target_ring->nr_relocs = 0;
	}

	ring_internal->nr_relocs = 0;
	ring_internal->nr_cmds = 0;
	ring_internal->nr_bos = 0;
}

int etna_ringbuffer_flush(struct etna_ringbuffer *ring)
{
	struct etna_ringbuffer_internal *ring_internal = to_etna_ringbuffer_internal(ring);
	struct etna_bo *ring_bo = ring_internal->ring_bo;
	struct drm_vivante_gem_submit req = {
			.pipe = ring->pipe->id,
	};
	struct etna_bo *etna_bo = NULL, *tmp;

	uint32_t i, submit_offset, size;
	int ret, id = ring->pipe->id;
	uint32_t *last_start = ring->last_start;

	submit_offset = offset_bytes(last_start, ring->start);
	size = offset_bytes(ring->cur, last_start);

	get_cmd(ring, ring, ring_bo, submit_offset, size, ETNA_SUBMIT_CMD_BUF);

	/* needs to be after get_cmd() as that could create bos/cmds table: */
	req.bos = VOID2U64(ring_internal->bos),
	req.nr_bos = ring_internal->nr_bos;
	req.cmds = VOID2U64(ring_internal->cmds),
	req.nr_cmds = ring_internal->nr_cmds;

	/* for each of the cmd's fix up their reloc's: */
	for (i = 0; i < ring_internal->nr_cmds; i++) {
		struct drm_vivante_gem_submit_cmd *cmd = &ring_internal->cmds[i];
		struct etna_ringbuffer_internal *target_ring = to_etna_ringbuffer_internal(ring_internal->rings[i]);
		uint32_t a = find_next_reloc_idx(target_ring, 0, cmd->submit_offset);
		uint32_t b = find_next_reloc_idx(target_ring, a, cmd->submit_offset + cmd->size);
		cmd->relocs = VOID2U64(&target_ring->relocs[a]);
		cmd->nr_relocs = (b > a) ? b - a : 0;
	}

	DEBUG_MSG("nr_cmds=%u, nr_bos=%u\n", req.nr_cmds, req.nr_bos);

	ret = drmCommandWriteRead(ring->pipe->dev->fd, DRM_VIVANTE_GEM_SUBMIT,
			&req, sizeof(req));
	if (ret) {
		ERROR_MSG("submit failed: %d (%s)", ret, strerror(errno));
	} else {
		/* update timestamp on all rings associated with submit: */
		for (i = 0; i < ring_internal->nr_cmds; i++) {
			struct etna_ringbuffer *target_ring = ring_internal->rings[i];
			if (!ret)
				target_ring->last_timestamp = req.fence;
		}
	}

	LIST_FOR_EACH_ENTRY_SAFE(etna_bo, tmp, &ring_internal->submit_list, list[id]) {
		struct list_head *list = &etna_bo->list[id];
		list_delinit(list);
		etna_bo->indexp1[id] = 0;
		etna_bo_del(etna_bo);
	}

	etna_ringbuffer_reset(ring);

	return 0;
}

uint32_t etna_ringbuffer_timestamp(struct etna_ringbuffer *ring)
{
	return ring->last_timestamp;
}

void etna_ringbuffer_reloc(struct etna_ringbuffer *ring, const struct etna_reloc *r)
{
	struct etna_ringbuffer_internal *ring_internal = to_etna_ringbuffer_internal(ring);
	struct drm_vivante_gem_submit_reloc *reloc;
	struct etna_ringbuffer *parent = ring->parent ? ring->parent : ring;
	uint32_t idx = APPEND(ring_internal, relocs);
	uint32_t addr;

	reloc = &ring_internal->relocs[idx];

	reloc->reloc_idx = bo2idx(parent, r->bo, r->flags);
	reloc->reloc_offset = r->offset;
	reloc->or = r->or;
	reloc->shift = r->shift;
	reloc->submit_offset = offset_bytes(ring->cur, ring->start);

	addr = r->bo->presumed;
	if (r->shift < 0)
		addr >>= -r->shift;
	else
		addr <<= r->shift;
	(*ring->cur++) = addr | r->or;
}
