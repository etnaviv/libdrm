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

#include "etnaviv_priv.h"

int etna_pipe_get_param(struct etna_pipe *pipe,
		enum etna_param_id param, uint64_t *value)
{
#if 0
	switch(param) {
	case FD_DEVICE_ID: // XXX probably get rid of this..
	case FD_GPU_ID:
		*value = msm_pipe->gpu_id;
		return 0;
	case FD_GMEM_SIZE:
		*value = msm_pipe->gmem;
		return 0;
	default:
		ERROR_MSG("invalid param id: %d", param);
		return -1;
	}

#endif
	*value = 0xdeadbeef;
	return 0;
}

int etna_pipe_wait(struct etna_pipe *pipe, uint32_t timestamp)
{
	struct etna_device *dev = pipe->dev;
	struct drm_vivante_wait_fence req = {
			.fence = timestamp,
	};
	int ret;

	get_abs_timeout(&req.timeout, 5000);

	ret = drmCommandWrite(dev->fd, DRM_MSM_WAIT_FENCE, &req, sizeof(req));
	if (ret) {
		ERROR_MSG("wait-fence failed! %d (%s)", ret, strerror(errno));
		return ret;
	}

	return 0;
}

void etna_pipe_del(struct etna_pipe *pipe)
{
	free(pipe);
}

static uint64_t get_param(struct etna_device *dev, uint32_t pipe, uint32_t param)
{
	struct drm_vivante_param req = {
			.pipe = pipe,
			.param = param,
	};
	int ret;

	ret = drmCommandWriteRead(dev->fd, DRM_VIVANTE_GET_PARAM, &req, sizeof(req));
	if (ret) {
		ERROR_MSG("get-param failed! %d (%s)", ret, strerror(errno));
		return 0;
	}

	return req.value;
}

struct etna_pipe * etna_pipe_new(struct etna_device *dev, enum etna_pipe_id id)
{
	static const uint32_t pipe_id[] = {
			[ETNA_PIPE_3D] = VIVANTE_PIPE_3D,
			[ETNA_PIPE_2D] = VIVANTE_PIPE_2D,
			[ETNA_PIPE_VG] = VIVANTE_PIPE_VG,
	};
	struct etna_pipe *pipe = NULL;

	pipe = calloc(1, sizeof(*pipe));
	if (!pipe) {
		ERROR_MSG("allocation failed");
		goto fail;
	}

	pipe->id = pipe_id[id];
	pipe->dev = dev;
	pipe->specs.model    = get_param(dev, pipe_id[id], VIVANTE_PARAM_GPU_MODEL);
	pipe->specs.revision = get_param(dev, pipe_id[id], VIVANTE_PARAM_GPU_REVISION);

	if (!pipe->specs.model)
		goto fail;

	INFO_MSG("Pipe Info:");
	INFO_MSG(" GPU model:          0x%x (rev %x)", pipe->specs.model, pipe->specs.revision);

	return pipe;
fail:
	if (pipe)
		etna_pipe_del(pipe);
	return NULL;
}
