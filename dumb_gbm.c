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
 *
 * This is intended as a fallback backend, if nothing else works.
 * As such, where possible, it tries to fall back to something else
 * when it encounters an error, and it tries to be as permissive as possible.
 *
 * Based on https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/38646
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/mman.h>

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

static void*
gbm_bo_map_dumb(struct gbm_dumb_bo *bo)
{
    struct drm_mode_map_dumb map_arg;
    int ret;

    if (bo->map) {
        return bo->map;
    }

    memset(&map_arg, 0, sizeof(map_arg));
    map_arg.handle = bo->base.v0.handle.u32;

    ret = drmIoctl(bo->base.gbm->v0.fd, DRM_IOCTL_MODE_MAP_DUMB, &map_arg);
    if (ret) {
        return NULL;
    }

    /* We allow reading from gpu memory, but it is very slow and not recomended */
    bo->map = mmap(NULL, bo->size, PROT_WRITE | PROT_READ,
                   MAP_SHARED, bo->base.gbm->v0.fd, map_arg.offset);
    if (bo->map == MAP_FAILED) {
       bo->map = NULL;
       return NULL;
    }

    return bo->map;
}

static struct gbm_bo *
dumb_bo_from_fd(struct gbm_device *gbm,
                struct gbm_import_fd_data *fd_data)
{
    struct gbm_dumb_bo *bo;
    int ret;
    uint32_t handle;

    bo = calloc(1, sizeof *bo);
    if (bo == NULL) {
       errno = ENOMEM;
       return NULL;
    }

    ret = drmPrimeFDToHandle(gbm->v0.fd, fd_data->fd, &handle);
    if (ret) {
        free(bo);
        return NULL;
    }

    bo->base.gbm = gbm;
    bo->base.v0.width = fd_data->width;
    bo->base.v0.height = fd_data->height;
    bo->base.v0.stride = fd_data->stride;
    bo->base.v0.format = fd_data->format;
    bo->base.v0.handle.u32 = handle;
    bo->size = fd_data->stride * fd_data->height;

    return &bo->base;
}

/* ^^^ Headers and helpers ^^^ */

static void
dumb_destroy(struct gbm_device *gbm)
{
    free(gbm);
}

static int
dumb_is_format_supported(struct gbm_device *gbm,
                         uint32_t format,
                         uint32_t usage)
{
    /* No need to reject formats with dumb buffers */
    return !((usage & GBM_BO_USE_CURSOR) && (usage & GBM_BO_USE_RENDERING));
}

static int
dumb_get_format_modifier_plane_count(struct gbm_device *device,
                                     uint32_t format,
                                     uint64_t modifier)
{
    /* dumb buffers support neither modifiers nor planes */
    return -1;
}

/* This function ignores modifiers */
static struct gbm_bo*
dumb_bo_create(struct gbm_device *gbm,
               uint32_t width, uint32_t height,
               uint32_t format,
               uint32_t usage,
               const uint64_t *modifiers,
               const unsigned int count)
{
    struct drm_mode_create_dumb create_arg;
    struct gbm_dumb_bo *bo;

    int bpp;
    int ret;

#ifdef STRICT
    if (modifiers || count) {
        errno = EINVAL;
        return NULL;
    }
#endif

    format = dumb_format_canonicalize(format);

    /* Hack to not have to reimplement gbm_bo_get_bpp */
    struct gbm_bo bpp_bo = {.v0.format = format};
    bpp = gbm_bo_get_bpp(&bpp_bo);
    if (!bpp) {
        errno = EINVAL;
        return NULL;
    }

#ifdef STRICT
    if (!(usage & GBM_BO_USE_CURSOR) &&
        !(usage & GBM_BO_USE_SCANOUT) {
        errno = EINVAL;
        return NULL;
    }
#endif

    bo = calloc(1, sizeof *bo);
    if (!bo) {
        errno = ENOMEM;
        return NULL;
    }

    memset(&create_arg, 0, sizeof(create_arg));
    create_arg.bpp = bpp;
    create_arg.width = width;
    create_arg.height = height;

    ret = drmIoctl(bo->base.gbm->v0.fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_arg);
    if (ret) {
        free(bo);
        return NULL;
    }

    bo->base.gbm = gbm;
    bo->base.v0.width = width;
    bo->base.v0.height = height;
    bo->base.v0.stride = create_arg.pitch;
    bo->base.v0.format = format;
    bo->base.v0.handle.u32 = create_arg.handle;
    bo->size = create_arg.size;

    if (!gbm_bo_map_dumb(bo)) {
        struct drm_mode_destroy_dumb destroy_arg;

        memset(&destroy_arg, 0, sizeof destroy_arg);
        destroy_arg.handle = create_arg.handle;
        drmIoctl(bo->base.gbm->v0.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);

        free(bo);
        return NULL;
    }

    return &bo->base;
}

static struct gbm_bo*
dumb_bo_import(struct gbm_device *gbm, uint32_t type,
               void *buffer, uint32_t usage)
{
    struct gbm_dumb_device *dumb = (struct gbm_dumb_device*)gbm;

    if (!dumb->has_dmabuf_import) {
        errno = ENOSYS;
        return NULL;
    }

    switch (type) {
    case GBM_BO_IMPORT_WL_BUFFER:
    case GBM_BO_IMPORT_EGL_IMAGE:
    case GBM_BO_IMPORT_FD_MODIFIER:
        errno = ENOSYS;
        return NULL;
    case GBM_BO_IMPORT_FD:
        return dumb_bo_from_fd(gbm, buffer);
    default:
        errno = EINVAL;
        return NULL;
    }
}

/* vvv Loader stuff vvv */
static void
dumb_device_create_v0(struct gbm_device_v0 *dumb)
{
    #define SET_PROC(x) dumb->x = dumb_##x

    SET_PROC(destroy);
    SET_PROC(is_format_supported);
    SET_PROC(get_format_modifier_plane_count);
    SET_PROC(bo_create);
    SET_PROC(bo_import);

    #undef SET_PROC
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
