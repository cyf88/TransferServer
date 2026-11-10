# TransferServer HTTP 接口文档

TransferServer 提供基于 HTTP 的转发控制接口，用于下发「解密 / 转码」流水线配置，支持多路并发转发。

## 通用约定

- **端口**：默认 `8080`，可通过启动参数指定，如 `./TransferServer 9000`。
- **监听地址**：`0.0.0.0`。
- **Content-Type**：请求体与响应体均为 `application/json`（`charset=utf-8`）。
- **多路标识**：每次成功启动会返回自增 `id`，后续 `stop` / `status` 用 `id` 定位具体某一路。
- **参数来源**：`/transfer/start` 既支持 query 参数也支持 JSON body，两者同时存在时 **JSON body 覆盖 query**。

### 响应格式

| 场景 | HTTP 状态码 | 响应体 |
|------|-------------|--------|
| 成功 | `200` | `{"ok":true, ...}` |
| 参数错误 | `400` | `{"ok":false,"error":"<原因>"}` |
| 资源不存在 | `404` | `{"ok":false,"error":"transfer not found"}` |
| 内部错误 | `500` | `{"ok":false,"error":"transfer start failed"}` |

---

## 1. 启动转发

```
POST /transfer/start
GET  /transfer/start
```

启动一路转发，返回该路的 `id` 与实际的收流端口。

### 请求参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|:----:|--------|------|
| `src_protocol` | string | 是 | - | 输入传输协议：`tcp` / `udp`（收流端口始终自动分配） |
| `dst_ip` | string | 是 | - | 输出目标 IP |
| `dst_port` | int | 是 | - | 输出目标端口 |
| `out_protocol` | string | 否 | `tcp` | 输出传输协议：`tcp` / `udp` |
| `decrypt` | bool | 否 | `false` | 是否解密（仅 SVAC 视频） |
| `transcode` | bool | 否 | `false` | 是否转码（SVAC → H.264/H.265） |
| `transcode_format` | string | 否 | `h264` | 转码输出格式：`h264` / `h265` |
| `vkek` | string | 否 | - | 解密密钥：16 字节密钥的 base64 编码（24 字符，以 `==` 结尾） |
| `vkek_version` | string | 否 | `1.0` | 密钥版本 |
| `fps` | int | 否 | `25` | 转码帧率 |
| `bitrate` | int | 否 | `2000000` | 转码码率（bps） |
| `width` | int | 否 | `1920` | 转码目标宽度 |
| `height` | int | 否 | `1080` | 转码目标高度 |
| `decoder_lib_path` | string | 否 | `./sdk/libZXSvacDec.so` | SVAC 解码库路径 |
| `ssrc` | uint | 否 | `0x12345678` | 输出 RTP SSRC |

### 请求示例

```bash
curl -X POST http://127.0.0.1:8080/transfer/start \
  -H "Content-Type: application/json" \
  -d '{
        "src_protocol": "tcp",
        "dst_ip": "192.168.1.1",
        "dst_port": 554,
        "decrypt": true,
        "transcode": true,
        "transcode_format": "h264",
        "vkek": "AQIDBAUGBwgJCgsMDQ4PEA==",
        "vkek_version": "1.0"
      }'
```

### 成功响应

```json
{"ok": true, "status": "started", "id": 1, "recv_port": 5000}
```

| 字段 | 说明 |
|------|------|
| `ok` | 固定 `true` |
| `status` | 固定 `started` |
| `id` | 本路转发的唯一标识，供 `stop` / `status` 使用 |
| `recv_port` | 实际收流端口（由服务自动分配） |

---

## 2. 停止转发

```
POST /transfer/stop
GET  /transfer/stop
```

停止转发。**不带 `id` 时停止全部**，带 `id` 时停止指定路。

### 请求参数（query）

| 参数 | 类型 | 必填 | 说明 |
|------|------|:----:|------|
| `id` | uint | 否 | 目标路 id；省略则停止全部 |

### 请求示例

```bash
curl "http://127.0.0.1:8080/transfer/stop?id=1"
```

### 响应

- 成功（单路或全部）：

```json
{"ok": true, "status": "stopped"}
```

- 指定 id 不存在（HTTP 404）：

```json
{"ok": false, "error": "transfer not found"}
```

---

## 3. 查询状态

```
GET /transfer/status
```

查询转发状态。不带 `id` 返回全部，带 `id` 返回单路。

### 请求参数（query）

| 参数 | 类型 | 必填 | 说明 |
|------|------|:----:|------|
| `id` | uint | 否 | 目标路 id；省略则返回全部 |

### 响应

- 不带 `id`（返回全部）：

```json
{
  "ok": true,
  "count": 2,
  "transfers": [
    {"id": 1, "status": "running", "recv_port": 5000},
    {"id": 2, "status": "stopped", "recv_port": 5001}
  ]
}
```

- 带 `id`（返回单路）：

```json
{"ok": true, "status": "running"}
```

- 指定 id 不存在（HTTP 404）：

```json
{"ok": false, "error": "transfer not found"}
```

---

## 4. 查询转码路

```
GET /transfer/transcode
```

查询当前**正在运行**且开启转码（`transcode=true`）的路，含完整转码配置。仅 `GET`。

### 响应

```json
{
  "ok": true,
  "count": 1,
  "transfers": [
    {
      "id": 1,
      "transcode_format": "h264",
      "input_format": "svac",
      "decrypt": true,
      "protocol": "tcp",
      "recv_ip": "127.0.0.1",
      "recv_port": 5000,
      "out_protocol": "tcp",
      "dst_ip": "192.168.1.1",
      "dst_port": 554,
      "fps": 25,
      "bitrate": 2000000,
      "width": 1920,
      "height": 1080
    }
  ]
}
```

| 字段 | 说明 |
|------|------|
| `id` | 路唯一标识 |
| `transcode_format` | 转码输出格式（`h264` / `h265`） |
| `input_format` | 实际输入编码：`svac` / `h264` / `h265` / `unknown` |
| `decrypt` | 是否开启解密 |
| `protocol` | 输入传输协议（`tcp` / `udp`） |
| `recv_ip` / `recv_port` | 输入监听地址与实际收流端口 |
| `out_protocol` | 输出传输协议（`tcp` / `udp`） |
| `dst_ip` / `dst_port` | 输出目标地址与端口 |
| `fps` / `bitrate` / `width` / `height` | 转码参数 |

---

## 5. 保存到文件

```
POST /transfer/save
GET  /transfer/save
```

接收输入流，将**接收流（PS）**与**解密/转码后的裸流（ES）**分别保存到文件，**不对外推送**。返回该路 `id` 与实际收流端口。

### 请求参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|:----:|--------|------|
| `src_protocol` | string | 是 | - | 输入传输协议：`tcp` / `udp`（收流端口始终自动分配） |
| `recv_file` | string | 是 | - | 接收流（重组后的 PS）保存路径 |
| `out_file` | string | 是 | - | 解密/转码后裸流（ES）保存路径 |
| `decrypt` | bool | 否 | `false` | 是否解密（仅 SVAC 视频） |
| `transcode` | bool | 否 | `false` | 是否转码（SVAC → H.264/H.265） |
| `transcode_format` | string | 否 | `h264` | 转码输出格式：`h264` / `h265` |
| `vkek` | string | 否 | - | 解密密钥（同 `/transfer/start`） |
| `vkek_version` | string | 否 | `1.0` | 密钥版本 |
| `fps` / `bitrate` / `width` / `height` | int | 否 | 同 `/transfer/start` | 转码参数 |
| `decoder_lib_path` | string | 否 | `./sdk/libZXSvacDec.so` | SVAC 解码库路径 |

### 请求示例

```bash
curl -X POST http://127.0.0.1:8080/transfer/save \
  -H "Content-Type: application/json" \
  -d '{
        "src_protocol": "tcp",
        "recv_file": "/tmp/recv.ps",
        "out_file": "/tmp/out.h264",
        "decrypt": true,
        "transcode": true,
        "transcode_format": "h264",
        "vkek": "AQIDBAUGBwgJCgsMDQ4PEA==",
        "vkek_version": "1.0"
      }'
```

### 成功响应

```json
{"ok": true, "status": "started", "id": 1, "recv_port": 5000, "recv_file": "/tmp/recv.ps", "out_file": "/tmp/out.h264"}
```

| 字段 | 说明 |
|------|------|
| `ok` / `status` | 同 `/transfer/start` |
| `id` | 本路唯一标识，供 `stop` / `status` 使用 |
| `recv_port` | 实际收流端口 |
| `recv_file` / `out_file` | 实际保存路径 |

> 说明：`out_file` 保存的是裸 ES 流——仅解密时为解密后的 SVAC 裸流，转码时为 H.264/H.265 裸流。

---

## 典型流程

```bash
# 1. 启动一路转码
curl -X POST http://127.0.0.1:8080/transfer/start \
  -H "Content-Type: application/json" \
  -d '{"src_protocol":"tcp","dst_ip":"192.168.1.1","dst_port":554,"transcode":true,"transcode_format":"h264"}'
# => {"ok":true,"status":"started","id":1,"recv_port":5000}

# 2. 查询全部状态
curl http://127.0.0.1:8080/transfer/status

# 3. 查询转码路详情
curl http://127.0.0.1:8080/transfer/transcode

# 4. 停止指定路
curl "http://127.0.0.1:8080/transfer/stop?id=1"
```
