FROM fedora:41

RUN dnf install -y \
    gcc-c++ \
    make \
    pkg-config \
    SDL2-devel \
    SDL2_ttf-devel \
    vlc-devel \
    libX11-devel \
    wayland-devel \
    mesa-dri-drivers \
    mesa-va-drivers \
    intel-media-driver-free \
    libva \
    alsa-lib-devel \
    && dnf clean all

WORKDIR /app

COPY main.cpp .

RUN g++ main.cpp -o streamimdb \
    $(sdl2-config --cflags --libs) \
    -lSDL2_ttf \
    $(pkg-config --cflags --libs libvlc)

CMD ["./streamimdb"]