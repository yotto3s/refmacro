# Multi-stage build for refmacro — C++26 compile-time AST metaprogramming framework
#
# Usage:
#   docker build -t refmacro .
#   docker build -t refmacro --build-arg GCC_COMMIT=abc123 .
#   docker build -t refmacro --build-arg GCC_JOBS=4 .
#   docker build -t refmacro --build-arg REFMACRO_BUILD_EXAMPLES=ON .
#   docker run refmacro

# ---------- Stage 1: Build GCC with reflection support ----------
FROM ubuntu:24.04 AS gcc-builder

ARG GCC_REPO=https://github.com/gcc-mirror/gcc.git
ARG GCC_BRANCH=master
ARG GCC_COMMIT=
ARG GCC_JOBS=

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential git flex bison texinfo \
        libgmp-dev libmpfr-dev libmpc-dev libisl-dev zlib1g-dev \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY scripts/build_gcc.sh /tmp/build_gcc.sh

RUN bash /tmp/build_gcc.sh \
        --gcc-repo "$GCC_REPO" \
        --gcc-branch "$GCC_BRANCH" \
        ${GCC_COMMIT:+--gcc-commit "$GCC_COMMIT"} \
        ${GCC_JOBS:+--jobs "$GCC_JOBS"} \
        --install-prefix /opt/gcc \
    && rm -rf /opt/gcc-build /opt/gcc-src

# ---------- Stage 2: Build and test refmacro ----------
FROM ubuntu:24.04 AS refmacro

RUN apt-get update && apt-get install -y --no-install-recommends \
        make git ca-certificates gpg wget \
        libc6-dev libgmp10 libmpfr6 libmpc3 libisl23 zlib1g \
        binutils \
    && wget -qO- https://apt.kitware.com/keys/kitware-archive-latest.asc \
        | gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main" \
        > /etc/apt/sources.list.d/kitware.list \
    && apt-get update && apt-get install -y --no-install-recommends cmake \
    && rm -rf /var/lib/apt/lists/*

COPY --from=gcc-builder /opt/gcc /opt/gcc
ENV PATH="/opt/gcc/bin:$PATH"
ENV LD_LIBRARY_PATH="/opt/gcc/lib64"

ARG REFMACRO_BUILD_EXAMPLES=OFF

WORKDIR /refmacro
COPY . .

RUN cmake -B build \
        -DCMAKE_CXX_COMPILER=/opt/gcc/bin/g++ \
        -DREFMACRO_BUILD_EXAMPLES=${REFMACRO_BUILD_EXAMPLES} \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

CMD ["ctest", "--test-dir", "build", "--output-on-failure"]
