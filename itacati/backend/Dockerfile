# syntax=docker/dockerfile:1

FROM debian:stable-slim AS build
ARG PJ_VER=2.14.1
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git curl ca-certificates pkg-config \
    libssl-dev libasound2-dev libavcodec-dev libavformat-dev libswscale-dev \
    && rm -rf /var/lib/apt/lists/*

# build pjproject
WORKDIR /opt
RUN curl -L -o pjproject.tar.bz2 https://github.com/pjsip/pjproject/archive/refs/tags/${PJ_VER}.tar.gz || true \
 || (echo "Falling back to git clone" && git clone --depth 1 --branch ${PJ_VER} https://github.com/pjsip/pjproject.git pjproject)
RUN if [ -f pjproject.tar.bz2 ]; then mkdir pjproject && tar -xzf pjproject.tar.bz2 -C pjproject --strip-components=1; fi
WORKDIR /opt/pjproject
RUN ./configure --disable-sound --disable-video --with-ssl && make -j && make install

# build app
WORKDIR /app
COPY backend /app
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j

FROM debian:stable-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 libasound2 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /usr/local/lib /usr/local/lib
COPY --from=build /usr/local/include /usr/local/include
COPY --from=build /app/build/itacati-core /usr/local/bin/itacati-core
ENV LD_LIBRARY_PATH=/usr/local/lib
EXPOSE 5060/udp 5060/tcp 5061/tcp
CMD ["itacati-core"]