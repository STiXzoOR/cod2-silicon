# MinGW cross-compilers for the Windows build, and Wine to run what they produce.
#   docker build --platform linux/amd64 -t cod2-build-win tools/windows
FROM ubuntu:24.04
RUN dpkg --add-architecture i386 \
 && apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      ca-certificates curl unzip git python3 make ninja-build cmake pkg-config \
      gcc-mingw-w64-i686 g++-mingw-w64-i686 binutils-mingw-w64-i686 \
      gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64 \
      libz-mingw-w64-dev \
      wine wine32:i386 wine64 \
 && rm -rf /var/lib/apt/lists/*
