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

#define DUMB_BACKEND_ABI_VERSION 1
#define MIN(a,b) ((a) < (b) ? (a) : (b))

static const struct gbm_core *core;

static struct gbm_device*
dumb_device_create(int fd, uint32_t gbm_backend_version)
{
    return NULL;
}

static struct gbm_backend gbm_dumb_backend = {
   .v0.backend_version = GBM_BACKEND_ABI_VERSION,
   .v0.backend_name = "dumb",
   .v0.create_device = dumb_device_create,
};

/* only exported symbol */
struct gbm_backend*
GBM_GET_BACKEND_PROC(const struct gbm_core *gbm_core)
{
    core = gbm_core;
    return &gbm_dumb_backend;
}
