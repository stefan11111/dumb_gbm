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

/**
 * gbm backend implementation with dumb buffers only and minimal dependencies
 * Only dependencies are libdrm, a libgbm loader, a C99 compiler and make.
 * Based on https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/38646
 */

#include <stdlib.h>
#include <errno.h>

#include <drm.h>
#include <xf86drm.h>

#include "dumb_gbm.h"

#define DUMB_BACKEND_ABI_VERSION 1
#define DUMB_BACKEND_NAME "dumb"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

static const struct gbm_core *core;

static inline uint32_t
dumb_format_canonicalize(uint32_t gbm_format)
{
    return core->v0.format_canonicalize(gbm_format);
}

/* ^^^ Headers and helpers ^^^ */



/* vvv Loader stuff vvv */
static void
dumb_device_create_v0(struct gbm_device_v0 *dumb)
{
}

static struct gbm_device*
dumb_device_create(int fd, uint32_t gbm_backend_version)
{
    struct gbm_dumb_device *dumb_gbm = NULL;

    int ret;
    uint64_t value = 0;

    ret = drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &value);
    if (ret > 0 || value == 0) {
        /* No dumb buffer support */
        errno = ENOSYS;
        return NULL;
    }

    dumb_gbm = calloc(1, sizeof(*dumb_gbm));
    if (!dumb_gbm) {
        errno = ENOMEM;
        return NULL;
    }

    ret = drmGetCap(fd, DRM_CAP_PRIME, &value);
    if (ret == 0) {
        dumb_gbm->has_dmabuf_import = !!(value & DRM_PRIME_CAP_IMPORT);
        dumb_gbm->has_dmabuf_export = !!(value & DRM_PRIME_CAP_EXPORT);
    }

    /* We don't touch the backend_desc field, the loader sets and uses it */
    /* dumb->backend_desc = NULL; */

    /* Loader already gives us the min(backend_version, loader_version) */
    dumb_gbm->base.v0.backend_version = gbm_backend_version;
    dumb_gbm->base.v0.fd = fd;
    dumb_gbm->base.v0.name = DUMB_BACKEND_NAME;

    dumb_device_create_v0(&dumb_gbm->base.v0);

    return &dumb_gbm->base;
}

static struct gbm_backend gbm_dumb_backend = {
   .v0.backend_version = DUMB_BACKEND_ABI_VERSION,
   .v0.backend_name = DUMB_BACKEND_NAME,
   .v0.create_device = dumb_device_create,
};

/* only exported symbol */
struct gbm_backend*
GBM_GET_BACKEND_PROC(const struct gbm_core *gbm_core)
{
    core = gbm_core;
    return &gbm_dumb_backend;
}
