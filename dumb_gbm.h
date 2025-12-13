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

#ifndef DUMB_GBM_H
#define DUMB_GBM_H

#include "gbm_backend_abi.h"

struct gbm_dumb_bo {
   struct gbm_bo base; /* Needs to be the first field */

   uint64_t size;
   int bpp;
   void *map;
};

struct gbm_dumb_device {
    struct gbm_device base; /* Needs to be the first field */

    int has_dmabuf_import;
    int has_dmabuf_export;
};

/* Copied from mesa's gbm_bo_get_bpp */
static inline uint32_t
dumb_get_bpp_for_format(uint32_t format)
{
   switch (format) {
      default:
         return 0;
      case GBM_FORMAT_C8:
      case GBM_FORMAT_R8:
      case GBM_FORMAT_RGB332:
      case GBM_FORMAT_BGR233:
         return 8;
      case GBM_FORMAT_R16:
      case GBM_FORMAT_GR88:
      case GBM_FORMAT_XRGB4444:
      case GBM_FORMAT_XBGR4444:
      case GBM_FORMAT_RGBX4444:
      case GBM_FORMAT_BGRX4444:
      case GBM_FORMAT_ARGB4444:
      case GBM_FORMAT_ABGR4444:
      case GBM_FORMAT_RGBA4444:
      case GBM_FORMAT_BGRA4444:
      case GBM_FORMAT_XRGB1555:
      case GBM_FORMAT_XBGR1555:
      case GBM_FORMAT_RGBX5551:
      case GBM_FORMAT_BGRX5551:
      case GBM_FORMAT_ARGB1555:
      case GBM_FORMAT_ABGR1555:
      case GBM_FORMAT_RGBA5551:
      case GBM_FORMAT_BGRA5551:
      case GBM_FORMAT_RGB565:
      case GBM_FORMAT_BGR565:
         return 16;
      case GBM_FORMAT_RGB888:
      case GBM_FORMAT_BGR888:
         return 24;
      case GBM_FORMAT_RG1616:
      case GBM_FORMAT_GR1616:
      case GBM_FORMAT_XRGB8888:
      case GBM_FORMAT_XBGR8888:
      case GBM_FORMAT_RGBX8888:
      case GBM_FORMAT_BGRX8888:
      case GBM_FORMAT_ARGB8888:
      case GBM_FORMAT_ABGR8888:
      case GBM_FORMAT_RGBA8888:
      case GBM_FORMAT_BGRA8888:
      case GBM_FORMAT_XRGB2101010:
      case GBM_FORMAT_XBGR2101010:
      case GBM_FORMAT_RGBX1010102:
      case GBM_FORMAT_BGRX1010102:
      case GBM_FORMAT_ARGB2101010:
      case GBM_FORMAT_ABGR2101010:
      case GBM_FORMAT_RGBA1010102:
      case GBM_FORMAT_BGRA1010102:
         return 32;
      case GBM_FORMAT_XBGR16161616:
      case GBM_FORMAT_ABGR16161616:
      case GBM_FORMAT_XBGR16161616F:
      case GBM_FORMAT_ABGR16161616F:
         return 64;
   }
}

#endif /* DUMB_GBM_H */
