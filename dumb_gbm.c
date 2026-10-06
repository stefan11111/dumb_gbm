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

#include <sys/mman.h>

#include <drm.h>
#include <drm_fourcc.h> /* for DRM_FORMAT_MOD_{LINEAR,INVALID} */
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "dumb_gbm.h"

#define DUMB_BACKEND_ABI_VERSION 1
#define DUMB_BACKEND_NAME "dumb"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#ifndef CHAR_BIT
#define CHAR_BIT (64 / sizeof(uint64_t))
#endif

static const struct gbm_core *core;

static inline uint32_t
dumb_format_canonicalize(uint32_t gbm_format)
{
    return core->v0.format_canonicalize(gbm_format);
}

static void*
gbm_bo_map_dumb(struct gbm_dumb_bo *bo)
{
    uint64_t offset = 0;
    int fd;
    uint32_t handle;

    if (bo->map) {
        return bo->map;
    }

    fd = bo->base.gbm->v0.fd;
    handle = bo->base.v0.handle.u32;

    if (drmModeMapDumbBuffer(fd, handle, &offset)) {
        return NULL;
    }

    /* We allow reading from gpu memory, but it is very slow and not recomended */
    bo->map = mmap(NULL, bo->size, PROT_WRITE | PROT_READ,
                   MAP_SHARED, fd, offset);
    if (bo->map == MAP_FAILED) {
        bo->map = NULL;
        return NULL;
    }

    return bo->map;
}

static struct gbm_dumb_bo*
dumb_bo_create_from_handles(struct gbm_device *gbm,
                            uint32_t width, uint32_t height,
                            uint32_t format,
                            int num_planes,
                            const uint32_t *handles,
                            const int *strides,
                            const int *offsets,
                            uint64_t modifier,
                            uint64_t size,
                            uint32_t usage)
{
    struct gbm_dumb_bo *bo = NULL;
    int bpp;

    if (usage & GBM_BO_USE_WRITE) {
        for (int i = 1; i < num_planes; i++) {
            if (handles[i]) {
                errno = EINVAL;
                return NULL;
            }
        }
    }

    format = dumb_format_canonicalize(format);
    bpp = dumb_get_bpp_for_format(format);
    if (!bpp) {
        errno = EINVAL;
        return NULL;
    }

    bo = calloc(1, sizeof(*bo));
    if (!bo) {
        errno = ENOMEM;
        return NULL;
    }

    bo->base.gbm = gbm;
    bo->num_planes = num_planes;
    bo->base.v0.width = width;
    bo->base.v0.height = height;
    bo->base.v0.stride = strides[0];
    bo->base.v0.format = format;
    bo->base.v0.handle.u32 = handles[0];

    bo->num_planes = num_planes;
    for (int i = 0; i < num_planes; i++) {
        bo->handles[i] = handles[i];
        bo->strides[i] = strides[i];
        bo->offsets[i] = offsets[i];
    }
    bo->modifier = modifier;
    bo->size = size ? size : (height * strides[0]);

    /* XXX Compat with mesa XXX
     * Mesa maps dumb buffers without requiring a call to gbm_bo_map
     */
    if (usage & GBM_BO_USE_WRITE) {
        if (!gbm_bo_map_dumb(bo)) {
            free(bo);
            return NULL;
        }
    }

    return bo;
}

static struct gbm_bo*
dumb_bo_from_fds(struct gbm_device *gbm,
                 struct gbm_import_fd_modifier_data *data,
                 uint32_t usage)
{
    struct gbm_dumb_bo *bo;
    uint32_t handles[GBM_MAX_PLANES];

    for (unsigned i = 0; i < data->num_fds; i++) {
        handles[i] = 0;
        if (drmPrimeFDToHandle(gbm->v0.fd, data->fds[0], &handles[i])) {
            return NULL;
        }
    }

    bo = dumb_bo_create_from_handles(gbm,
                                     data->width, data->height,
                                     data->format,
                                     data->num_fds,
                                     handles,
                                     data->strides,
                                     data->offsets,
                                     data->modifier,
                                     0 /* size, guessed */,
                                     usage);
    if (!bo) {
        return NULL;
    }

    bo->is_imported = 1;
    return &bo->base;
}

static struct gbm_bo*
dumb_bo_from_fd(struct gbm_device *gbm,
                struct gbm_import_fd_data *fd_data,
                uint32_t usage)
{
    struct gbm_import_fd_modifier_data data =
    {
        .width = fd_data->width,
        .height = fd_data->height,
        .format = fd_data->format,
        .num_fds = 1,
        .fds[0] = fd_data->fd,
        .strides[0] = fd_data->stride,
        .offsets[0] = 0,
        .modifier = DRM_FORMAT_MOD_INVALID,
    };

    return dumb_bo_from_fds(gbm, &data, usage);
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
    return 1;
}

static int
dumb_get_format_modifier_plane_count(struct gbm_device *device,
                                     uint32_t format,
                                     uint64_t modifier)
{
    /* TODO: Return this from a table */
    return 1;
}

static struct gbm_bo*
dumb_bo_create(struct gbm_device *gbm,
               uint32_t width, uint32_t height,
               uint32_t format,
               uint32_t usage,
               const uint64_t *modifiers,
               const unsigned int count)
{
    struct gbm_dumb_bo *bo;
    int bpp;

    uint32_t handle = 0;
    uint32_t stride = 0;
    uint64_t size = 0;

    /**
     * We diverge from mesa's dri backend here.
     *
     * The dri backend ignores modifiers when creating dumb buffers.
     *
     * Here, we don't create a bo if no modifier is supported.
     */
    if (count && modifiers) {
        int found = 0;
        for (unsigned i = 0; !found && i < count; i++) {
            switch (modifiers[i]) {
            case DRM_FORMAT_MOD_LINEAR:
            case DRM_FORMAT_MOD_INVALID:
                found = 1;
            }
        }
        if (!found) {
            errno = ENOTSUP;
            return NULL;
        }
    }

    format = dumb_format_canonicalize(format);
    bpp = dumb_get_bpp_for_format(format);

    if (drmModeCreateDumbBuffer(gbm->v0.fd, width, height, bpp, 0 /* flags */,
                                &handle, &stride, &size)) {
        return NULL;
    }

    bo = dumb_bo_create_from_handles(gbm,
                                     width, height,
                                     format,
                                     1 /* num_planes */,
                                     &handle,
                                     &(const int){stride},
                                     &(const int){0} /* offsets */,
                                     DRM_FORMAT_MOD_LINEAR /* modifier */,
                                     size,
                                     usage | GBM_BO_USE_WRITE);

    if (!bo) {
        drmModeDestroyDumbBuffer(gbm->v0.fd, handle);
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
        return dumb_bo_from_fd(gbm, buffer, usage);
    case GBM_BO_IMPORT_FD_MODIFIER:
        return dumb_bo_from_fds(gbm, buffer, usage);
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
    int bpp = dumb_get_bpp_for_format(_bo->v0.format);

    int cpp = (bpp + CHAR_BIT - 1) / CHAR_BIT;

    if (bo->map) {
        *map_data = (char *)bo->map + (bo->base.v0.stride * y) + (x * cpp);
        *stride = bo->base.v0.stride;
        return *map_data;
    }

    return NULL;
}

static void
dumb_bo_unmap(struct gbm_bo *_bo, void *map_data)
{
}

static int
dumb_bo_write(struct gbm_bo *_bo, const void *buf, size_t data)
{
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;

    if (!bo->map) {
        errno = EINVAL;
        return -1;
    }

    memcpy(bo->map, buf, data);
    return 0;
}

static int
dumb_bo_get_planes(struct gbm_bo *_bo)
{
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;
    return bo->num_planes;
}

static union gbm_bo_handle
dumb_bo_get_handle(struct gbm_bo *_bo, int plane)
{
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;
    union gbm_bo_handle handle = {.u64 = 0};

    if (plane >= bo->num_planes) {
        errno = EINVAL;
        return handle;
    }

    handle.u32 = bo->handles[plane];
    return handle;
}

static int
dumb_bo_get_plane_fd(struct gbm_bo *_bo, int plane)
{
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;
    struct gbm_dumb_device *dumb = (struct gbm_dumb_device*)_bo->gbm;
    int prime_fd = -1;

    if (plane >= bo->num_planes) {
        errno = EINVAL;
        return -1;
    }

    if (!dumb->has_dmabuf_export) {
        errno = ENOSYS;
        return -1;
    }

    return drmPrimeHandleToFD(dumb->base.v0.fd, bo->handles[plane], DRM_RDWR, &prime_fd) ? -1 : prime_fd;
}

static int
dumb_bo_get_fd(struct gbm_bo *bo)
{
    return dumb_bo_get_plane_fd(bo, 0 /* plane */);
}

static uint32_t
dumb_bo_get_stride(struct gbm_bo *_bo, int plane)
{
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;

    if (plane >= bo->num_planes) {
        errno = EINVAL;
        return 0;
    }

    return bo->strides[plane];
}

static uint32_t
dumb_bo_get_offset(struct gbm_bo *_bo, int plane)
{
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;

    if (plane >= bo->num_planes) {
        errno = EINVAL;
        return 0;
    }

    return bo->offsets[plane];
}

static uint64_t
dumb_bo_get_modifier(struct gbm_bo *_bo)
{
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;

    return bo->modifier;
}

static void
dumb_bo_destroy(struct gbm_bo *_bo)
{
    struct gbm_device *gbm = _bo->gbm;
    struct gbm_dumb_bo *bo = (struct gbm_dumb_bo*)_bo;

    if (bo->map) {
        munmap(bo->map, bo->size);
        bo->map = NULL;
    }

    /* Handles are not reference-counted, do not destroy them if imported */
    if (!bo->is_imported) {
        drmModeDestroyDumbBuffer(gbm->v0.fd, bo->base.v0.handle.u32);
    }
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

    /* stubs */
    SET_PROC(surface_create);
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
