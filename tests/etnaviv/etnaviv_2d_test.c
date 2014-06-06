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
#include <unistd.h>

#include "xf86drm.h"
#include "etnaviv_drmif.h"
#include "etnaviv_ringbuffer.h"

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


	rb = etna_ringbuffer_new(pipe, 1024);
	if (!rb) {
		ret = 5;
		goto fail;
	}

	/* TODO: generate command sequence */


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
