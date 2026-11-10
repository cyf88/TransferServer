# syntax=docker/dockerfile:1

# ============================================================================
# 编译环境镜像：CentOS 7 + devtoolset-7，内置 FFmpeg / x264 / x265 / GmSSL。
# 工程源码通过 -v 挂载到 /workspace，外部修改代码后 docker exec 进容器编译即可。
# ============================================================================
FROM centos:7

ARG FFMPEG_VERSION=4.4
ARG X265_VERSION=3.5
ARG NASM_VERSION=2.15.05
ARG YASM_VERSION=1.3.0
ARG GMSSL_URL=https://github.com/guanzhi/GmSSL/archive/refs/heads/GmSSL-v2.zip

# CentOS 7 已停止维护，官方 vault 源也已失效(403)，改用阿里云镜像
RUN set -eux; \
    sed -i \
        -e 's|^mirrorlist=|#mirrorlist=|g' \
        -e 's|^metalink=|#metalink=|g' \
        -e 's|^# *baseurl=http://mirror.centos.org|baseurl=http://mirrors.aliyun.com|g' \
        -e 's|^baseurl=http://mirror.centos.org|baseurl=http://mirrors.aliyun.com|g' \
        /etc/yum.repos.d/CentOS-Base.repo; \
    yum -y install centos-release-scl epel-release; \
    sed -i \
        -e 's|^mirrorlist=|#mirrorlist=|g' \
        -e 's|^metalink=|#metalink=|g' \
        -e 's|^# *baseurl=http://mirror.centos.org|baseurl=http://mirrors.aliyun.com|g' \
        -e 's|^baseurl=http://mirror.centos.org|baseurl=http://mirrors.aliyun.com|g' \
        /etc/yum.repos.d/CentOS-SCLo-*.repo; \
    sed -i \
        -e 's|^mirrorlist=|#mirrorlist=|g' \
        -e 's|^metalink=|#metalink=|g' \
        -e 's|^# *baseurl=http://download.fedoraproject.org/pub/epel|baseurl=https://mirrors.aliyun.com/epel|g' \
        -e 's|^baseurl=http://download.fedoraproject.org/pub/epel|baseurl=https://mirrors.aliyun.com/epel|g' \
        /etc/yum.repos.d/epel.repo; \
    yum -y install \
        devtoolset-7-gcc \
        devtoolset-7-gcc-c++ \
        devtoolset-7-binutils \
        devtoolset-7-libstdc++-devel \
        cmake3 \
        make git wget unzip which file perl bzip2 \
        glibc-devel zlib-devel; \
    yum clean all

# CentOS 7 自带 cmake 2.8 版本过低，用 EPEL 的 cmake3 并软链为 cmake
RUN ln -sf /usr/bin/cmake3 /usr/local/bin/cmake

# 编译所需环境变量（必须在编译依赖库之前生效，否则找不到 gcc）
ENV PATH=/opt/rh/devtoolset-7/root/usr/bin:${PATH} \
    CC=/opt/rh/devtoolset-7/root/usr/bin/gcc \
    CXX=/opt/rh/devtoolset-7/root/usr/bin/g++ \
    LD_LIBRARY_PATH=/opt/rh/devtoolset-7/root/usr/lib64:/opt/rh/devtoolset-7/root/usr/lib:/usr/local/lib:/usr/local/lib64 \
    CPATH=/usr/local/include \
    LIBRARY_PATH=/usr/local/lib:/usr/local/lib64 \
    PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig

# ---- yasm / nasm ----
RUN set -eux; \
    mkdir -p /tmp/pkg && cd /tmp/pkg; \
    curl -fsSL "http://www.tortall.net/projects/yasm/releases/yasm-${YASM_VERSION}.tar.gz" -o yasm.tar.gz; \
    tar xf yasm.tar.gz && cd "yasm-${YASM_VERSION}"; \
    ./configure --prefix=/usr/local --enable-pic && make -j"$(nproc)" && make install; \
    cd /tmp/pkg; \
    curl -fsSL "https://www.nasm.us/pub/nasm/releasebuilds/${NASM_VERSION}/nasm-${NASM_VERSION}.tar.gz" -o nasm.tar.gz; \
    tar xf nasm.tar.gz && cd "nasm-${NASM_VERSION}"; \
    ./configure --prefix=/usr/local && make -j"$(nproc)" && make install; \
    rm -rf /tmp/pkg

# ---- x264 ----
RUN set -eux; \
    mkdir -p /tmp/pkg && cd /tmp/pkg; \
    curl -fsSL "https://code.videolan.org/videolan/x264/-/archive/stable/x264-stable.tar.bz2" -o x264.tar.bz2; \
    tar xf x264.tar.bz2 && cd x264-stable; \
    ./configure --prefix=/usr/local --enable-pic --enable-shared --enable-static; \
    make -j"$(nproc)" && make install; \
    rm -rf /tmp/pkg

# ---- x265 ----
RUN set -eux; \
    mkdir -p /tmp/pkg && cd /tmp/pkg; \
    curl -fsSL "https://bitbucket.org/multicoreware/x265_git/downloads/x265_${X265_VERSION}.tar.gz" -o x265.tar.gz; \
    tar xf x265.tar.gz && cd "x265_${X265_VERSION}/build/linux"; \
    cmake -G "Unix Makefiles" ../../source \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DENABLE_SHARED=ON -DENABLE_STATIC=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON; \
    make -j"$(nproc)" && make install; \
    rm -rf /tmp/pkg

# ---- FFmpeg ----
RUN set -eux; \
    mkdir -p /tmp/pkg && cd /tmp/pkg; \
    curl -fsSL "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2" -o ffmpeg.tar.bz2; \
    tar xf ffmpeg.tar.bz2 && cd "ffmpeg-${FFMPEG_VERSION}"; \
    ./configure --prefix=/usr/local --enable-gpl --enable-shared --enable-static --enable-pic \
        --enable-libx264 --enable-libx265; \
    make -j"$(nproc)" && make install; \
    rm -rf /tmp/pkg

# ---- GmSSL（国密 OpenSSL：SM2/SM3/SM4）----
RUN set -eux; \
    mkdir -p /tmp/pkg && cd /tmp/pkg; \
    curl -fsSL "${GMSSL_URL}" -o gmssl.zip; \
    unzip -q gmssl.zip && cd GmSSL-GmSSL-v2; \
    ./config --prefix=/usr/local; \
    make -j"$(nproc)" && make install_sw; \
    rm -rf /tmp/pkg

# 编译 / 运行辅助脚本
COPY docker/build.sh /opt/build.sh
COPY docker/run.sh /opt/run.sh
RUN chmod +x /opt/build.sh /opt/run.sh

# 源码挂载目录
WORKDIR /workspace

# 容器常驻，方便 docker exec 进入编译
CMD ["sleep", "infinity"]
