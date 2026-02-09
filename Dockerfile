# Normitri build environment: Conan 2, CMake, GCC (C++23)
# Use this to build without installing Conan/OpenCV on your host.
#
# Build image:   docker build -t normitri-builder .
# Run build:     docker run --rm -v "$(pwd)":/src -w /src normitri-builder ./scripts/build_nomitri.sh all
# Or shell:      docker run --rm -it -v "$(pwd)":/src -w /src normitri-builder bash

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# System deps for Conan packages (e.g. OpenCV -> vaapi/vdpau/system)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    python3-venv \
    libva-dev \
    libvdpau-dev \
    libxkbcommon-dev \
    libwayland-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxfixes-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libegl1-mesa-dev \
    libgles2-mesa-dev \
    libdrm-dev \
    libpulse-dev \
    libsndio-dev \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --break-system-packages conan

# Ensure conan is on PATH (pip may install to ~/.local)
ENV PATH="/root/.local/bin:${PATH}"

# Create default Conan 2 profile so 'conan install' works without user interaction
RUN conan profile detect --force

WORKDIR /src

# Copy source so CI/act can run build+test without mounting (workflow uses image /src)
COPY . /src
RUN chmod +x /src/scripts/build_nomitri.sh

# Default: run full build (override with docker run ... bash for a shell)
CMD ["./scripts/build_nomitri.sh", "all"]
