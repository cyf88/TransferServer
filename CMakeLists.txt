cmake_minimum_required(VERSION 3.10)
project(TransferServer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/inc)
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
# 统一引用 media-server 工程内的第三方库
set(LIBMPEG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/media-server/libmpeg)
set(LIBRTP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/media-server/librtp)

# 核心处理流水线源文件
set(TRANSFER_SOURCES
    ${SOURCE_DIR}/Decrypter.cpp
    ${SOURCE_DIR}/FFmpegDecoder.cpp
    ${SOURCE_DIR}/FramePool.cpp
    ${SOURCE_DIR}/logger.cpp
    ${SOURCE_DIR}/NaluSpliter.cpp
    ${SOURCE_DIR}/NetworkReceiver.cpp
    ${SOURCE_DIR}/PsDemuxer.cpp
    ${SOURCE_DIR}/PSSender.cpp
    ${SOURCE_DIR}/RtpReceiver.cpp
    ${SOURCE_DIR}/SMCryptor.cpp
    ${SOURCE_DIR}/svac.cpp
    ${SOURCE_DIR}/SVACDecoder.cpp
    ${SOURCE_DIR}/TransferServer.cpp
    ${SOURCE_DIR}/util.cpp
    ${SOURCE_DIR}/YUVEncoder.cpp
    ${SOURCE_DIR}/server.cpp
)

# media-server libmpeg 源文件（PS 解析与封装）
set(LIBMPEG_SOURCES
    ${LIBMPEG_DIR}/source/mpeg-crc32.c
    ${LIBMPEG_DIR}/source/mpeg-element-descriptor.c
    ${LIBMPEG_DIR}/source/mpeg-muxer.c
    ${LIBMPEG_DIR}/source/mpeg-pack-header.c
    ${LIBMPEG_DIR}/source/mpeg-packet.c
    ${LIBMPEG_DIR}/source/mpeg-pat.c
    ${LIBMPEG_DIR}/source/mpeg-pes.c
    ${LIBMPEG_DIR}/source/mpeg-pmt.c
    ${LIBMPEG_DIR}/source/mpeg-ps-dec.c
    ${LIBMPEG_DIR}/source/mpeg-ps-enc.c
    ${LIBMPEG_DIR}/source/mpeg-psd.c
    ${LIBMPEG_DIR}/source/mpeg-psm.c
    ${LIBMPEG_DIR}/source/mpeg-sdt.c
    ${LIBMPEG_DIR}/source/mpeg-system-header.c
    ${LIBMPEG_DIR}/source/mpeg-ts-dec.c
    ${LIBMPEG_DIR}/source/mpeg-ts-enc.c
    ${LIBMPEG_DIR}/source/mpeg-ts-h264.c
    ${LIBMPEG_DIR}/source/mpeg-ts-h265.c
    ${LIBMPEG_DIR}/source/mpeg-ts-h266.c
    ${LIBMPEG_DIR}/source/mpeg-util.c
)

# media-server librtp 源文件（RTP 负载解码 MP2P）
# payload 目录下所有 pack/unpack 源文件一起编译（rtp-payload 分发依赖全部编解码器符号）
file(GLOB LIBRTP_PAYLOAD_SOURCES ${LIBRTP_DIR}/payload/*.c)
set(LIBRTP_SOURCES
    ${LIBRTP_PAYLOAD_SOURCES}
    ${LIBRTP_DIR}/source/rtp-packet.c
    ${LIBRTP_DIR}/source/rtp-profile.c
)

add_executable(${PROJECT_NAME} ${TRANSFER_SOURCES} ${LIBMPEG_SOURCES} ${LIBRTP_SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${INCLUDE_DIR}
    ${THIRD_PARTY_DIR}
    ${LIBMPEG_DIR}/include
    ${LIBMPEG_DIR}/source
    ${LIBRTP_DIR}/include
    ${LIBRTP_DIR}/payload
)

# 与 media-server libmpeg 默认构建保持一致
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    MPEG_H26X_VERIFY
    MPEG_ZERO_PAYLOAD_LENGTH
    MPEG_DAHUA_AAC_FROM_G711
    MPEG_KEDA_H265_FROM_H264
)

# 依赖库：FFmpeg 编解码、x264/x265、国密 OpenSSL(libcrypto)
# 依赖库路径按部署环境调整（此处沿用 SVACTransfer2 的链接方式）
target_link_libraries(${PROJECT_NAME}
    libavformat.a
    libavcodec.a
    libavutil.a
    libswscale.a
    libx264.a
    libx265.a
    libswresample.a
    libcrypto.a
    libpthread.so
    rt
    libdl.so
    libz.so
    ${CMAKE_THREAD_LIBS_INIT}
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
