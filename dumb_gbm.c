/*
 * Copyright © 2025 stefan11111
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
#include <fcntl.h> /* for O_RDWR, which DRM_RDWR is defined as */

#ifdef STRICT
#include <assert.h>
#endif

#include <sys/mman.h>

#include <drm.h>
#include <drm_fourcc.h> /* for DRM_FORMAT_MOD_{LINEAR,INVALID} */
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

static struct gbm_bo*
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
    bo->bpp = dumb_get_bpp_for_format(fd_data->format);

    return &bo->base;
}

static struct gbm_bo*
dumb_bo_from_fds(struct gbm_device *gbm,
                 struct gbm_import_fd_modifier_data *fd_modifier_data)
{
    if (fd_modifier_data->num_fds != 1) {
        errno = EINVAL;
        return NULL;
    }

    struct gbm_import_fd_data fd_data =
    {
     .fd = fd_modifier_data->fds[0],
     .width = fd_modifier_data->width,
     .height = fd_modifier_data->height,
     .stride = fd_modifier_data->strides[0],
     .format = fd_modifier_data->format,
    };

    return dumb_bo_from_fd(gbm, &fd_data);
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
    switch (modifier) {
    case DRM_FORMAT_MOD_LINEAR:
    case DRM_FORMAT_MOD_INVALID:
       /* dumb buffers are single-plane only */
       return 1;
    default:
        /* dumb buffers don't support modifiers */
        errno = EINVAL;
        return -1;
    }
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

    bpp = dumb_get_bpp_for_format(format);
    if (!bpp) {
        errno = EINVAL;
        return NULL;
    }

#ifdef STRICT
    if (!(usage & GBM_BO_USE_CURSOR) &&
        !(usage & GBM_BO_USE_SCANOUT)) {
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

    ret = drmIoctl(gbm->v0.fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_arg);
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
    bo->bpp = bpp;

    /**
     * Sadly, we have to map the buffer now, for gbm_bo_write to work.
     * Since we have to do it for some buffers, do it for all buffers.
     */
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
        errno = ENOSYS;
        return NULL;
    case GBM_BO_IMPORT_FD:
        return dumb_bo_from_fd(gbm, buffer);
    case GBM_BO_IMPORT_FD_MODIFIER:
        return dumb_bo_from_fds(gbm, buffer);
    default:
        errno = EINVAL;
        return NULL;
    }
}

static void*
dumb_bo_map(struct gbm_bo *_bo,
            uint32_t x, uint32_t y,
            uint32_t width, uint32_t height,
            uint32_t flags, uint32_t *stride, void **map_data)
{
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;

    /* This probably breaks if CHAR_BIT != 8 */
    int cpp = (bo->bpp + 7) / 8;

    if (bo->map) {
        *map_data = (char *)bo->map + (bo->base.v0.stride * y) + (x * cpp);
        *stride = bo->base.v0.stride;
        return *map_data;
    }

    /* This really shouldn't happen */
    return NULL;
}

static void
dumb_bo_unmap(struct gbm_bo *_bo, void *map_data)
{
#ifdef STRICT
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;

    assert(map_data >= bo->map);
    assert((char*)map_data < ((char*)bo->map + bo->size));
#endif
}

static int
dumb_bo_write(struct gbm_bo *_bo, const void *buf, size_t data)
{
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;

    if (!bo->map) {
        /* This really shouldn't happen */
        return -1;
    }

    memcpy(bo->map, buf, data);
    return 0;
}

static int
dumb_bo_get_fd(struct gbm_bo *bo)
{
    int ret;
    int prime_fd = -1;

    struct gbm_dumb_device *dumb = (struct gbm_dumb_device*)bo->gbm;

    if (!dumb->has_dmabuf_export) {
        errno = ENOSYS;
        return -1;
    }

    ret = drmPrimeHandleToFD(dumb->base.v0.fd, bo->v0.handle.u32, DRM_RDWR, &prime_fd);
    if (!ret) {
        return prime_fd;
    }
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
    /* Dumb buffers are single-plane only. */
    if (plane != 0) {
        union gbm_bo_handle ret = {.s64 = -1};

        errno = EINVAL;
        return ret;
    }

    return bo->v0.handle;
}

static int
dumb_bo_get_plane_fd(struct gbm_bo *bo, int plane)
{
    /* Dumb buffers are single-plane only. */
    if (plane != 0) {
        errno = EINVAL;
        return -1;
    }

    return dumb_bo_get_fd(bo);
}

static uint32_t
dumb_bo_get_stride(struct gbm_bo *bo, int plane)
{
    /* Dumb buffers are single-plane only. */
    if (plane != 0) {
        errno = EINVAL;
        return 0;
    }

    return bo->v0.stride;
}

static uint32_t
dumb_bo_get_offset(struct gbm_bo *bo, int plane)
{
    /* Dumb buffers are single-plane only. */
    if (plane != 0) {
        errno = EINVAL;
    }

    /* Dumb buffers have no offset */
    return 0;
}

static uint64_t
dumb_bo_get_modifier(struct gbm_bo *bo)
{
    /* Dumb buffers are linear */
    return DRM_FORMAT_MOD_LINEAR;
}

static void
dumb_bo_destroy(struct gbm_bo *_bo)
{
    struct gbm_device *gbm = _bo->gbm;
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;
    struct drm_mode_destroy_dumb arg;

    munmap(bo->map, bo->size);
    bo->map = NULL;

    memset(&arg, 0, sizeof(arg));
    arg.handle = bo->base.v0.handle.u32;
    drmIoctl(gbm->v0.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &arg);

    free(bo);
}

/* surface procs are implemented as stubs */
static struct gbm_surface*
dumb_surface_create(struct gbm_device *gbm,
                    uint32_t width, uint32_t height,
                    uint32_t format, uint32_t flags,
                    const uint64_t *modifiers,
                    const unsigned count)
{
    errno = ENOSYS;
    return NULL;
}

static struct gbm_bo*
dumb_surface_lock_front_buffer(struct gbm_surface *surface)
{
    errno = ENOSYS;
    return NULL;
}

static void
dumb_surface_release_buffer(struct gbm_surface *surface,
                            struct gbm_bo *bo)
{
    errno = ENOSYS;
}

static int
dumb_surface_has_free_buffers(struct gbm_surface *surface)
{
    errno = ENOSYS;
    return 0;
}

static void
dumb_surface_destroy(struct gbm_surface *surface)
{
    /* free(surface) */

    errno = ENOSYS;
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
    SET_PROC(bo_map);
    SET_PROC(bo_unmap);
    SET_PROC(bo_write);
    SET_PROC(bo_get_fd);
    SET_PROC(bo_get_planes);
    SET_PROC(bo_get_handle);
    SET_PROC(bo_get_plane_fd);
    SET_PROC(bo_get_stride);
    SET_PROC(bo_get_offset);
    SET_PROC(bo_get_modifier);
    SET_PROC(bo_destroy);
    SET_PROC(surface_create);

    /**
     * For some reason, the dri libgbm backend
     * from mesa doesn't implement these three
     */
    SET_PROC(surface_lock_front_buffer);
    SET_PROC(surface_release_buffer);
    SET_PROC(surface_has_free_buffers);

    SET_PROC(surface_destroy);

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
