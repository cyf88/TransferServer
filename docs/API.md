# TransferServer HTTP 接口文档

TransferServer 提供基于 HTTP 的转发控制接口，用于下发「解密 / 转码」流水线配置，支持多路并发转发。

## 通用约定

- **端口**：默认 `8080`。优先级为 `config.ini` 的 `http_port` > 启动参数 > 默认值，例如 `./TransferServer 9000`。
- **监听地址**：`0.0.0.0`。
- **Content-Type**：请求体与响应体均为 `application/json`（`charset=utf-8`）。
- **多路标识**：每次成功启动会返回自增 `id`，后续 `stop` / `status` 用 `id` 定位具体某一路。
- **参数来源**：`/transfer/start` 既支持 query 参数也支持 JSON body，两者同时存在时 **JSON body 覆盖 query**。
- **对象生命周期**：调用 `/transfer/stop` 后该路立即从服务中移除；因 `idle_timeout` 或**输出保护**自动停止的路仍可查询，最多保留 60 秒后由后台线程回收，之后按不存在处理（404）。
- **输出保护**：输出连续发送失败达到 `send_fail_frames` 帧或 `send_fail_seconds` 秒（如上级端口不可达、TCP 断链）时，该路自动停止，`status` 变为 `stopped`；两者均配置为 `0` 时关闭该保护。日志分别为 `[TransferServer] 输出连续发送失败 ...` 与 `[HTTP] transfer id=N 输出连续发送失败，自动停止`。注意 UDP 输出通常不会立即感知对端不可达，该保护主要对 TCP 输出生效。

### 配置文件 config.ini

程序以工作目录下的 `config.ini` 为配置来源，格式为 `key = value`，`#` 开头为注释。

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `http_port` | `8080` | HTTP 控制端口；未配置时使用启动参数或默认 8080 |
| `idle_timeout` | `0` | 收流空闲超时（秒），超过该时间没收到数据自动停止；`0` 表示不启用 |
| `send_fail_frames` | `100` | 输出保护：连续发送失败帧数阈值，`0` 表示不启用该项 |
| `send_fail_seconds` | `5` | 输出保护：连续发送失败持续秒数阈值，`0` 表示不启用该项 |
| `cert_dir` | `./certs` | 验签证书目录，证书文件名为 `{cert_id}_sign.cer` |

### 响应格式

| 场景 | HTTP 状态码 | 响应体 |
|------|-------------|--------|
| 成功 | `200` | `{"ok":true, ...}` |
| 参数错误 | `400` | `{"ok":false,"error":"<原因>"}` |
| 资源不存在 | `404` | `{"ok":false,"error":"transfer not found"}` |
| 内部错误 | `500` | `{"ok":false,"error":"<具体原因>"}`，如 `load verify cert failed: ./certs/xxx_sign.cer` |

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
| `ssrc` | uint | 否 | `0x12345678` | 输出 RTP SSRC |
| `verify` | bool | 否 | `false` | 是否软验签（GB/T 35114），仅 SVAC 流生效 |
| `channel_id` | string | 否 | - | 通道 ID（用于记录/日志） |
| `cert_id` | string | 否 | - | 验签证书 ID，对应 `{cert_dir}/{cert_id}_sign.cer`；未传或为空时回退使用 `channel_id` |

> 说明：`verify=true` 时必须提供 `cert_id` 或 `channel_id`，否则返回 `400`：`missing channel_id/cert_id when verify enabled`；证书文件不存在时返回 `500`：`load verify cert failed: <路径>`。

### 请求示例

完整参数（TCP 收流 → 解密 + 转码 + 验签 → TCP 推送）：

```bash
curl -X POST http://127.0.0.1:8080/transfer/start \
  -H "Content-Type: application/json" \
  -d '{
        "src_protocol": "tcp",
        "dst_ip": "192.168.1.1",
        "dst_port": 554,
        "out_protocol": "tcp",
        "decrypt": true,
        "transcode": true,
        "transcode_format": "h264",
        "vkek": "AQIDBAUGBwgJCgsMDQ4PEA==",
        "vkek_version": "1.0",
        "fps": 25,
        "bitrate": 2000000,
        "ssrc": "0x12345678",
        "verify": true,
        "channel_id": "13100000001182000223",
        "cert_id": "13100000001182000223"
      }'
```

最简形式（UDP 收流、不转码、直接透传）：

```bash
curl -X POST "http://127.0.0.1:8080/transfer/start?src_protocol=udp&dst_ip=192.168.1.1&dst_port=554"
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
# 停止指定路
curl -X POST "http://127.0.0.1:8080/transfer/stop?id=1"

# 停止全部
curl -X POST http://127.0.0.1:8080/transfer/stop
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

查询转发状态。不带 `id` 返回全部，带 `id` 返回单路。因空闲超时自动停止、尚未被回收的路仍会返回，`status` 为 `stopped`。

### 请求参数（query）

| 参数 | 类型 | 必填 | 说明 |
|------|------|:----:|------|
| `id` | uint | 否 | 目标路 id；省略则返回全部 |

### 请求示例

```bash
# 查询全部
curl http://127.0.0.1:8080/transfer/status

# 查询指定路
curl "http://127.0.0.1:8080/transfer/status?id=1"
```

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

### 请求示例

```bash
curl http://127.0.0.1:8080/transfer/transcode
```

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
      "encrypted": true,
      "signed": true,
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
| `encrypted` | 原始码流是否加密：`true` / `false`；非 SVAC 或尚未解析出 NALU 时为 `null` |
| `signed` | 原始码流是否带签名：`true` / `false`；非 SVAC 或尚未解析出 NALU 时为 `null` |
| `decrypt` | 是否开启解密 |
| `protocol` | 输入传输协议（`tcp` / `udp`） |
| `recv_ip` / `recv_port` | 输入监听地址与实际收流端口 |
| `out_protocol` | 输出传输协议（`tcp` / `udp`） |
| `dst_ip` / `dst_port` | 输出目标地址与端口 |
| `fps` / `bitrate` | 转码参数 |
| `width` / `height` | 实际输入流分辨率（转码输出与输入一致） |

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
| `fps` / `bitrate` | int | 否 | 同 `/transfer/start` | 转码参数 |
| `verify` | bool | 否 | `false` | 是否软验签（GB/T 35114） |
| `channel_id` | string | 否 | - | 通道 ID（用于记录/日志） |
| `cert_id` | string | 否 | - | 验签证书 ID，对应 `{cert_dir}/{cert_id}_sign.cer`；未传或为空时回退使用 `channel_id` |

### 请求示例

保存接收流与转码输出：

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
        "vkek_version": "1.0",
        "fps": 25,
        "bitrate": 2000000,
        "verify": true,
        "channel_id": "13100000001182000223",
        "cert_id": "13100000001182000223"
      }'
```

只保存接收到的原始 PS：

```bash
curl -X POST "http://127.0.0.1:8080/transfer/save?src_protocol=udp&recv_file=/tmp/recv.ps&out_file=/tmp/out.es"
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

### 纯验签示例（不解密、不转码、不落盘）

```bash
curl -X POST http://127.0.0.1:8080/transfer/save \
  -d 'src_protocol=tcp&recv_file=/dev/null&out_file=/dev/null&verify=true&channel_id=13100000001182000223&cert_id=13100000001182000223'
```

结果通过 `GET /transfer/verify?id=N` 查询。

---

## 6. 查询验签结果

```
GET /transfer/verify
GET /transfer/verify?id=N
```

查询开启验签（`verify=true`）的转发路及其验签统计。不带 `id` 返回全部开启验签的路，带 `id` 返回指定路。

### 请求示例

```bash
# 全部验签路
curl http://127.0.0.1:8080/transfer/verify

# 指定路
curl "http://127.0.0.1:8080/transfer/verify?id=1"
```

### 响应（不带 id）

```json
{
  "ok": true,
  "count": 1,
  "transfers": [
    {
      "id": 1,
      "channel_id": "13100000001182000223",
      "verify": {
        "enabled": true,
        "result": "success",
        "detail": "verify success",
        "success": 12,
        "fail": 0,
        "last_time": "2026-11-20T13:58:57"
      }
    }
  ]
}
```

### 响应（带 id）

```json
{"ok": true, "id": 1, "verify": {...}}
```

| 字段 | 说明 |
|------|------|
| `enabled` | 该路是否开启验签 |
| `result` | 最近一次验签结果：`success` / `fail`；尚未验到 AUTH 单元时为 `null` |
| `detail` | 最近一次的详细结果文本 |
| `success` / `fail` | 累计验签成功 / 失败次数 |
| `last_time` | 最近一次验签时间（本地时间） |

指定 `id` 不存在或已停止时返回 HTTP 404。

---

## 7. 查询验签证书

```
GET /transfer/certs
```

列出 `cert_dir` 下所有 `*_sign.cer` 的证书 ID（去掉 `_sign.cer` 后缀）。

### 请求示例

```bash
curl http://127.0.0.1:8080/transfer/certs
```

### 响应

```json
{"ok": true, "count": 2, "certs": ["13100000001181000223", "13100000001182000223"]}
```

---

## 典型流程

```bash
# 1. 启动一路转码（完整参数见第 1 节）
curl -X POST http://127.0.0.1:8080/transfer/start \
  -H "Content-Type: application/json" \
  -d '{"src_protocol":"tcp","dst_ip":"192.168.1.1","dst_port":554,"transcode":true,"transcode_format":"h264"}'
# => {"ok":true,"status":"started","id":1,"recv_port":5000}

# 2. 查询全部状态
curl http://127.0.0.1:8080/transfer/status

# 3. 查询转码路详情
curl http://127.0.0.1:8080/transfer/transcode

# 4. 停止指定路
curl -X POST "http://127.0.0.1:8080/transfer/stop?id=1"
```

### 纯验签流程（不转码、不推送）

```bash
# 1. 查看可用证书
curl http://127.0.0.1:8080/transfer/certs

# 2. 启动验签（只收流验签，不落盘）
curl -X POST http://127.0.0.1:8080/transfer/save \
  -d 'src_protocol=tcp&recv_file=/dev/null&out_file=/dev/null&verify=true&channel_id=13100000001182000223&cert_id=13100000001182000223'

# 3. 查询验签结果
curl "http://127.0.0.1:8080/transfer/verify?id=2"

# 4. 停止
curl -X POST "http://127.0.0.1:8080/transfer/stop?id=2"
```
