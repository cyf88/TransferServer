#!/bin/bash
set -euo pipefail

# 工程源码通过 -v 挂载到 /workspace，本脚本在容器内编译。
# 产物输出到 /workspace/build（即宿主机上的 build 目录，已在 .gitignore 中忽略）。

cmake -S /workspace -B /workspace/build -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"

cmake --build /workspace/build -j"$(nproc)"

echo "Build done: /workspace/build/TransferServer"
