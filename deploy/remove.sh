#!/bin/bash
# ============================================================================
# TransferServer 移除脚本：停止并删除容器，可选择删除部署目录与镜像
#
# 用法（在本目录下执行）：
#   bash remove.sh
#
# 注意：部署目录要与 deploy.sh 顶部的 RUN_DIR 保持一致。
# ============================================================================
set -e
cd "$(dirname "$0")"

# 部署路径（宿主机绝对路径），需与 deploy.sh 中的 RUN_DIR 一致
RUN_DIR="$(pwd)/run"

docker rm -f transferserver >/dev/null 2>&1 || true
echo "[remove] 容器 transferserver 已删除"

read -r -p "删除部署目录 $RUN_DIR（含配置、证书、SDK、日志）？[y/N] " ans || ans=n
case "$ans" in
y | Y)
    rm -rf "$RUN_DIR"
    echo "[remove] 已删除 $RUN_DIR"
    ;;
*)
    echo "[remove] 保留 $RUN_DIR"
    ;;
esac

read -r -p "删除本地镜像 transferserver:latest？[y/N] " ans || ans=n
case "$ans" in
y | Y)
    docker rmi -f transferserver:latest >/dev/null 2>&1 || true
    echo "[remove] 镜像 transferserver:latest 已删除"
    ;;
*)
    echo "[remove] 保留镜像"
    ;;
esac

echo "[remove] 完成"
