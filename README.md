# Dumb gbm
Dumb gbm is a gbm backend for dumb buffers.
These are linear gpu buffers, with no modifier or egl support.

However, they don't require many things to work, and are useful
for unaccelerated kernel modesetting and cursor buffers for hw cursors.

On some cards, there is no tiled buffer support, so this is the best there is.
This project makes it so that these cards have gbm buffer support,
and that support doesn't have mesa as a dependency.

Notably, the nvidia drivers prior to the 5xx series do not support gbm
buffer allocation.
As of writing this, without any patches to mesa,
there is no gbm support for cards not supported by the 5xx drivers.
This is the only way to get libgbm working on these cards.
