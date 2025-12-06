/*
 * Copyright © 2025 Stefan11111
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
 *     stefan11111 <stefan11111@shitposting.expert>
 *
 */

#include <errno.h>

#include "dumb_gbm.h"

static void
dumb_bo_destroy(struct gbm_device *gbm)
{
    errno = ENOSYS;
}

static int
dumb_is_format_supported(struct gbm_device *gbm, uint32_t format, uint32_t usage)
{
    errno = ENOSYS;
    return 0;
}

static int
dumb_get_format_modifier_plane_count(struct gbm_device *device,
                                uint32_t format,
                                uint64_t modifier)
{
    errno = ENOSYS;
    return -1;
}

static struct gbm_bo*
dumb_bo_create(struct gbm_device *gbm,
               uint32_t width, uint32_t height,
               uint32_t format,
               uint32_t usage,
               const uint64_t *modifiers,
               const unsigned int count)
{
    errno = ENOSYS;
    return NULL;
}

static struct gbm_bo*
dumb_bo_import(struct gbm_device *gbm, uint32_t type,
               void *buffer, uint32_t usage)
{
    errno = ENOSYS;
    return NULL;
}

static void*
dumb_bo_map(struct gbm_bo *bo,
            uint32_t x, uint32_t y,
            uint32_t width, uint32_t height,
            uint32_t flags, uint32_t *stride,
            void **map_data)
{
    errno = ENOSYS;
    return NULL;
}

static void
dumb_bo_unmap(struct gbm_bo *bo, void *map_data)
{
    errno = ENOSYS;
}

static int
dumb_bo_write(struct gbm_bo *bo, const void *buf, size_t data)
{
    errno = ENOSYS;
    return -1;
}

static int
dumb_bo_get_fd(struct gbm_bo *bo)
{
    errno = ENOSYS;
    return -1;
}

static int
dumb_bo_get_planes(struct gbm_bo *bo)
{
    /* Dumb buffers are single-plane only. */
    return 1;
}

static union gbm_bo_handle
dumb_bo_get_handle(struct gbm_bo *bo, int plane)
{
    union gbm_bo_handle ret = {.s64 = -1};
    return ret;
}

static int
dumb_bo_get_plane_fd(struct gbm_bo *bo, int plane)
{
    /* dumb BOs can only utilize non-planar formats */
    return -1;
}

static uint32_t
dumb_bo_get_stride(struct gbm_bo *bo, int plane)
{
    errno = ENOSYS;
    return 0;
}



static struct gbm_device *
dumb_device_create(int fd, uint32_t gbm_backend_version)
{
    struct gbm_device *gbm_dev = malloc(sizeof(*gbm_dev));
    if (!gbm_dev) {
        return dev;
    }

    gbm->dev->dummy = NULL;

    struct gbm_device_v0 *dumb = &gbm_dev->v0;
    dumb->fd = fd;
    dumb->backend_version = gbm_backend_version;
    dumb->name = "dumb";
    dumb->destroy = dumb_bo_destroy;
    dumb->is_format_supported = dumb_is_format_supported;
    dumb->get_format_modifier_plane_count = dumb_get_format_modifier_plane_count;
    dumb->bo_create = dumb_bo_create;
    dumb->bo_import = dumb_bo_import;
    dumb->bo_map = dumb_bo_map;
    dumb->bo_unmap = dumb_bo_unmap;
    dumb->bo_write = dumb_bo_write;
    dumb->bo_get_fd = dumb_bo_get_fd;
    dumb->bo_get_planes = dumb_bo_get_planes;
    dumb->bo_get_handle = dumb_bo_get_handle;
    dumb->bo_get_plane_fd = dumb_bo_get_plane_fd;
    dumb->bo_get_stride = dumb_bo_get_stride;
}
