# 本地 IPC 协议

主控进程与 Host 进程使用 Windows 本地命名管道，由 `QLocalServer` 和 `QLocalSocket` 提供。服务名由主控进程每次启动时生成随机 UUID，并通过进程参数传递给对应 Host；服务端设置为仅当前用户可访问。

每帧由 4 字节 little-endian 无符号长度和 CBOR map payload 组成。控制帧上限为 16 MiB，超限、CBOR 非 map、未知协议版本、重复 `requestId` 或连接异常都会产生明确错误并断开连接。图像、Mask 和特征预览不得放入控制帧，后续必须以 `QSharedMemory` 描述符传输。

每条消息必须包含：

- `protocolVersion`：当前固定为 `1`。
- `requestId`：连接内唯一字符串。
- `jobId`：任务字符串；进程级握手可为空字符串。
- `type`：消息类型。
- `timestampUtc`：UTC ISO-8601 时间。

失败响应额外必须具有非空 `errorCode`、非空 `errorMessage` 和 `recoverable`。Phase 1 已实现 `hello`、`helloAck`、`heartbeat`、`shutdown`、`completed` 和 `error`。Host 每秒发送一次心跳；UI 只把收到 `completed` 并且进程以零退出码结束视为正常结束。
