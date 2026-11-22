# Minimal Linux x86_64 build image for QNX SDP 8.0 cross-builds. It contains NO QNX software: the SDP is installed
# separately under your myQNX licence and mounted at run time (see scripts/qnx-container.sh).
FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential ninja-build pkg-config rsync git wget ca-certificates curl unzip xz-utils file \
      python3 python3-venv locales sudo \
    && locale-gen en_US.UTF-8 && rm -rf /var/lib/apt/lists/*
ENV LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
# CMake 3.22 (Ubuntu 20.04 ships 3.16, too old for whisper.cpp)
RUN wget -q https://cmake.org/files/v3.22/cmake-3.22.0-linux-x86_64.sh -O /tmp/cmake.sh \
    && mkdir -p /opt/cmake && sh /tmp/cmake.sh --prefix=/opt/cmake --skip-license \
    && ln -s /opt/cmake/bin/cmake /usr/local/bin/cmake && rm /tmp/cmake.sh
ARG USER_NAME=dev
ARG GROUP_NAME=dev
ARG USER_ID=1000
ARG GROUP_ID=1000
ARG QNX_SDP_VERSION=qnx800
ENV QNX_SDP_VERSION=$QNX_SDP_VERSION
RUN (getent group ${GROUP_NAME} || groupadd --gid ${GROUP_ID} ${GROUP_NAME}) \
    && useradd --uid ${USER_ID} --gid ${GROUP_ID} --no-log-init --create-home ${USER_NAME}
USER ${USER_NAME}
WORKDIR /home/${USER_NAME}
CMD ["/bin/bash"]
