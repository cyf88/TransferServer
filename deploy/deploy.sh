#!/bin/bash
# ============================================================================
# TransferServer 一键部署脚本（Docker + docker run）
#
# 用法（在本目录下执行）：
#   bash deploy.sh            # 部署并启动
#   bash deploy.sh up
#   bash deploy.sh restart
#   bash deploy.sh status
#   bash deploy.sh logs
#   bash deploy.sh down
#
# 本目录自包含，整个目录拷到目标机即可部署：
#   transferserver.tar     Docker 离线镜像（本地没有该镜像时自动导入）
#   TransferServer         运行二进制
#   config.ini             默认配置
#   certs/                 验签证书
#   sdk/                   SVAC 解码库 + license.dat + svacplus
#   remove.sh              移除服务（删除容器，可选删除 run 目录与镜像）
#
# 部署目录由脚本顶部 RUN_DIR 变量指定（默认本目录下的 run），并挂载到容器的
# /opt/transferserver：TransferServer / config.ini / certs/ / sdk/ / license.dat /
# TransferServer.log
#
# 转发媒体端口由 /transfer/start 动态分配，无法预映射，所以用 --network host。
# ============================================================================
set -e
cd "$(dirname "$0")"

# 部署路径（宿主机绝对路径）：二进制、配置、证书、SDK、日志都放在这里
RUN_DIR="$(pwd)/run"

# HTTP 端口以 config.ini 的 http_port 为准（仅用于启动参数与健康检查）
HTTP_PORT=$(grep -E '^[[:space:]]*http_port' "$RUN_DIR/config.ini" 2>/dev/null | head -n1 | sed 's/.*=[[:space:]]*//')
if [ -z "$HTTP_PORT" ]; then
    HTTP_PORT=$(grep -E '^[[:space:]]*http_port' config.ini | head -n1 | sed 's/.*=[[:space:]]*//')
fi
if [ -z "$HTTP_PORT" ]; then
    HTTP_PORT=8080
fi

case "${1:-up}" in
up)
    # 1. 镜像：本地没有就从离线包导入，不构建
    if ! docker image inspect transferserver:latest >/dev/null 2>&1; then
        echo "[deploy] 导入离线镜像 transferserver.tar"
        docker load -i transferserver.tar
    fi

    # 2. 准备部署目录
    mkdir -p "$RUN_DIR/certs" "$RUN_DIR/sdk"
    if [ -f TransferServer ]; then
        cp -f TransferServer "$RUN_DIR/TransferServer"
    else
        echo "[deploy] 从镜像提取二进制"
        TMP=$(docker create --entrypoint /bin/true transferserver:latest)
        docker cp "$TMP:/opt/transferserver/TransferServer" "$RUN_DIR/TransferServer"
        docker rm -f "$TMP" >/dev/null
    fi
    chmod +x "$RUN_DIR/TransferServer"
    if [ ! -f "$RUN_DIR/config.ini" ]; then
        cp config.ini "$RUN_DIR/config.ini"
    fi
    cp -n certs/*_sign.cer "$RUN_DIR/certs/" 2>/dev/null || true
    cp -f sdk/*.so* "$RUN_DIR/sdk/"
    cp -f sdk/svacplus "$RUN_DIR/sdk/"
    chmod +x "$RUN_DIR/sdk/svacplus"
    cp -f sdk/license.dat "$RUN_DIR/license.dat"
    cp -f sdk/license.dat "$RUN_DIR/sdk/license.dat"

    # 3. 启动容器
    docker rm -f transferserver >/dev/null 2>&1 || true
    docker run -d \
        --name transferserver \
        --network host \
        --restart unless-stopped \
        --entrypoint /opt/transferserver/TransferServer \
        -e LD_LIBRARY_PATH=/opt/transferserver/sdk \
        -v "$RUN_DIR":/opt/transferserver \
        -w /opt/transferserver \
        transferserver:latest "$HTTP_PORT"

    sleep 2
    # 启动失败（如端口被占用、配置错误）时打印日志并退出，避免误报部署成功
    if [ "$(docker inspect -f '{{.State.Status}}' transferserver 2>/dev/null)" != "running" ]; then
        echo "[deploy] 容器未正常运行，最近日志："
        docker logs --tail 30 transferserver || true
        echo "[deploy] 请检查 $RUN_DIR/config.ini（如 http_port 是否被占用）后重试"
        exit 1
    fi
    docker ps --filter "name=^transferserver$" --format '{{.Names}}  {{.Status}}'
    curl -s "http://127.0.0.1:$HTTP_PORT/transfer/status" || true
    echo
    echo "[deploy] 完成"
    ;;
down)
    docker rm -f transferserver
    echo "[deploy] 已停止并删除容器，部署目录保留：$RUN_DIR"
    ;;
restart)
    docker restart transferserver
    echo "[deploy] 已重启"
    ;;
status)
    docker ps -a --filter "name=^transferserver$"
    curl -s "http://127.0.0.1:$HTTP_PORT/transfer/status" || true
    echo
    ;;
logs)
    docker logs -f --tail 100 transferserver
    ;;
*)
    echo "用法: bash deploy.sh [up|down|restart|status|logs]"
    exit 1
    ;;
esac
