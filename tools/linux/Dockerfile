# Build environment for the Linux dedicated server, for hosts that aren't x86-64 Linux.
#   docker build --platform linux/amd64 -t cod2-build-linux tools/linux
FROM ubuntu:24.04
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      ca-certificates git python3 build-essential gcc-14 g++-14 cmake pkg-config zlib1g-dev libgl-dev libcurl4-openssl-dev \
 && rm -rf /var/lib/apt/lists/*
