#!/bin/bash

# Run with full host environment preservation, explicit Wayland socket bindings,
# comprehensive GPU mapping, and generic IPC sockets.
sudo -E docker run -it --rm \
    --user 1000:1000 \
    --group-add video \
    --group-add render \
    --ipc=host \
    -e SDL_VIDEODRIVER=wayland \
    -e GDK_BACKEND=wayland \
    -e QT_QPA_PLATFORM=wayland \
    -e WAYLAND_DISPLAY=wayland-0 \
    -e XDG_RUNTIME_DIR=/tmp \
    -v /run/user/1000/wayland-0:/tmp/wayland-0:ro \
    --device /dev/dri:/dev/dri \
    --device /dev/snd \
    streamimdb-app