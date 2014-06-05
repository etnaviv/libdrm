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

#ifndef ETNAVIV_DRMIF_H_
#define ETNAVIV_DRMIF_H_

#include <xf86drm.h>
#include <stdint.h>

struct etna_bo;
struct etna_pipe;
struct etna_device;

enum etna_pipe_id {
	ETNA_PIPE_3D = 1,
	ETNA_PIPE_2D = 2,
	ETNA_PIPE_VG = 3,
	ETNA_PIPE_MAX
};

enum etna_param_id {
	FD_DEVICE_ID,
	FD_GMEM_SIZE,
	FD_GPU_ID,
};

/* bo flags: */
#define DRM_FREEDRENO_GEM_TYPE_SMI        0x00000001
#define DRM_FREEDRENO_GEM_TYPE_KMEM       0x00000002
#define DRM_FREEDRENO_GEM_TYPE_MEM_MASK   0x0000000f
#define DRM_FREEDRENO_GEM_CACHE_NONE      0x00000000
#define DRM_FREEDRENO_GEM_CACHE_WCOMBINE  0x00100000
#define DRM_FREEDRENO_GEM_CACHE_WTHROUGH  0x00200000
#define DRM_FREEDRENO_GEM_CACHE_WBACK     0x00400000
#define DRM_FREEDRENO_GEM_CACHE_WBACKWA   0x00800000
#define DRM_FREEDRENO_GEM_CACHE_MASK      0x00f00000
#define DRM_FREEDRENO_GEM_GPUREADONLY     0x01000000

/* bo access flags: (keep aligned to MSM_PREP_x) */
#define DRM_FREEDRENO_PREP_READ           0x01
#define DRM_FREEDRENO_PREP_WRITE          0x02
#define DRM_FREEDRENO_PREP_NOSYNC         0x04

/* device functions:
 */

struct etna_device * etna_device_new(int fd);
struct etna_device * etna_device_new_dup(int fd);
struct etna_device * etna_device_ref(struct etna_device *dev);
void etna_device_del(struct etna_device *dev);


/* pipe functions:
 */

struct etna_pipe * etna_pipe_new(struct etna_device *dev, enum etna_pipe_id id);
void etna_pipe_del(struct etna_pipe *pipe);
int etna_pipe_get_param(struct etna_pipe *pipe, enum etna_param_id param,
		uint64_t *value);
int etna_pipe_wait(struct etna_pipe *pipe, uint32_t timestamp);


/* buffer-object functions:
 */

struct etna_bo * etna_bo_new(struct etna_device *dev,
		uint32_t size, uint32_t flags);
#if 0
struct etna_bo * fd_bo_from_fbdev(struct fd_pipe *pipe,
		int fbfd, uint32_t size);
#endif
struct etna_bo *etna_bo_from_handle(struct etna_device *dev,
		uint32_t handle, uint32_t size);
struct etna_bo * etna_bo_from_name(struct etna_device *dev, uint32_t name);
struct etna_bo * etna_bo_from_dmabuf(struct etna_device *dev, int fd);
struct etna_bo * etna_bo_ref(struct etna_bo *bo);
void etna_bo_del(struct etna_bo *bo);
int etna_bo_get_name(struct etna_bo *bo, uint32_t *name);
uint32_t etna_bo_handle(struct etna_bo *bo);
int etna_bo_dmabuf(struct etna_bo *bo);
uint32_t etna_bo_size(struct etna_bo *bo);
void * etna_bo_map(struct etna_bo *bo);
int etna_bo_cpu_prep(struct etna_bo *bo, uint32_t op);
void etna_bo_cpu_fini(struct etna_bo *bo);

#endif /* FREEDRENO_DRMIF_H_ */
