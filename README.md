# Dumb gbm
Dumb gbm is a generic gbm backend that can only allocate dumb buffers.

Dumb buffers are very well supported by drivers, and are cpu-renderable.
In some cases (notably not nvidia), dumb buffers can also be rendered to with the gpu.

Dumb buffers are useful both for cursor buffers and for
probe/fallback buffers for front rendering.

With this, dumb buffer users don't have to implement
two code paths for gbm buffers and dumb buffers,
they can just use the gbm path for both.

On some cards there is no official libgbm support,
so this is the best there is.
This project makes it so that these cards have gbm buffer support,
and that support doesn't have mesa as a dependency.

Notably, the nvidia drivers prior to the 5xx series do not support gbm
buffer allocation.
As of writing this, without any patches to mesa,
there is no gbm support for cards not supported by the 5xx drivers.
This is the only way to get libgbm working on these cards.

While this backend can only create dumb buffers,
it can import tiled buffers (even planar) created by other means
(e.g. exported GL textures).
