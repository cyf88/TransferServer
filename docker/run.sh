#!/bin/bash
set -euo pipefail

# 运行已编译的可执行文件（需先执行 /opt/build.sh 完成编译）。
# 转码所需 libZXSvacDec.so 默认放在 /workspace/sdk 下（对应配置默认 ./sdk/libZXSvacDec.so）。

export LD_LIBRARY_PATH="/workspace/sdk:${LD_LIBRARY_PATH:-}"

cd /workspace
exec ./build/TransferServer "${HTTP_PORT:-8080}"
