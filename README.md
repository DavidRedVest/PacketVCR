# PacketVCR

一个跨平台的 UDP 流"录像机"：像 VCR 录像/回放视频一样，忠实录制一路 UDP 流（单播或组播）到标准
`.pcap` 文件，并按原始包间时序精确回放出去。面向音视频流媒体开发测试场景，例如：

- 录制一路组播测试流，用于自动化回归测试的"标准素材"
- 反复回放已录制的真实流量，对播放器/解码端做压力测试或问题复现
- 在不同网络环境下复现某次实际抓包的时序问题

## 特性

- **原始包级保真**：不解析、不关心 payload 编码格式（分辨率/帧率/编码方式均未知），只保证每个
  UDP 数据报的字节内容和到达时间被如实记录，数据报边界不丢失。
- **标准 pcap 格式**：录制文件是标准 libpcap 格式（非 pcapng），可直接用 Wireshark 打开分析，
  无需额外插件。真实的源/目的 IP、端口均被保留；仅以太网 MAC 地址和 IPv4 TTL 是应用层 UDP
  socket 拿不到的字段，用占位值填充。
- **无漂移回放**：回放采用"绝对截止时间调度"，长时间播放不会产生累积时间漂移；支持暂停/恢复
  （暂停期间整体平移后续时序，不影响包间相对间隔）、循环播放、变速播放、TTL/网卡选择。
- **无需抓包驱动**：不依赖 libpcap/Npcap，pcap 帧的合成与解析全部手工实现，避免在 Windows 上
  需要管理员权限安装驱动。
- **原生 socket，非 Qt 网络层**：所有网络 I/O 走原生 BSD socket / Winsock2，不使用
  `QUdpSocket`（曾在实测中出现丢包/延迟问题）；Qt 仅用于 GUI 界面层。

## 下载即用（预编译版）

GitHub Actions 会在 Windows / macOS / Linux 三个平台上原生编译并打包，产出解压/挂载即可运行的
可执行文件（GUI 已用 `windeployqt`/`macdeployqt`/`linuxdeployqt` 打包好 Qt 运行时，无需额外安装
Qt）。

- 打 tag（形如 `v1.0.0`）推送后，会自动在 [Releases](../../releases) 页面发布**一个** `PacketVCR.zip`，
  解压后是这样的结构，三个平台的可执行文件分别放在各自子文件夹里：

  ```
  PacketVCR/
    windows/   PacketVCR.exe  recorder_cli.exe  player_cli.exe  (+ 依赖的 Qt DLL)
    macos/     PacketVCR.app  recorder_cli      player_cli
    linux/     PacketVCR      recorder_cli      player_cli      (+ bundle 的 .so)
  ```

  只需要把对应平台的子文件夹拷到自己电脑上，双击 `PacketVCR`（Windows/macOS 是
  `PacketVCR.exe`/`PacketVCR.app`）即可运行 GUI，或在终端里运行 `recorder_cli`/`player_cli`。
- 每次 push / PR 也会触发同一套构建（不发布 Release，仅做 CI 验证），可在
  [Actions](../../actions) 对应 workflow run 里下载名为 `PacketVCR-all-platforms` 的 Artifact
  试跑，内容和 Release 里的 `PacketVCR.zip` 一致。

> Linux 的 Qt 运行时打包（`linuxdeployqt`）跨发行版兼容性不如 Windows/macOS 稳定，如果在某个
> 发行版上启动报"找不到 Qt platform plugin xcb"之类的错误，通常是缺少 `libxcb-cursor0` 等系统库，
> 装上对应发行版的包即可。

> **macOS 提示"PacketVCR 已损坏，无法打开，你应该将它移到废纸篓"**：这不是文件真的损坏了，
> 是 macOS Gatekeeper 的限制——`PacketVCR.app` 目前没有经过付费的 Apple 开发者证书签名/公证，
> 从浏览器下载解压后会被打上"下载隔离"标记，未签名的 App 在较新 macOS 上会直接显示这种更吓人
> 的提示，而不是可以选择"仍要打开"的提示。解决办法：打开终端执行
> ```sh
> xattr -cr /path/到/PacketVCR.app
> ```
> 清掉隔离标记后即可正常双击打开，这一步对每台第一次运行的 Mac 只需要做一次。

## 构建

依赖 Qt6（Core / Network / Widgets / Test 组件）与 CMake ≥ 3.21。若 Qt6 未安装在默认搜索路径，
需要通过 `-DCMAKE_PREFIX_PATH` 显式指定：

```sh
cmake -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/macos
cmake --build build
```

构建产物：

| 目标 | 说明 |
|---|---|
| `recorder_cli` | 命令行录制工具 |
| `player_cli` | 命令行回放工具 |
| `video_tools_gui` | Qt6 图形界面（含录制/回放两个面板） |

## 使用

### 录制

```sh
recorder_cli <group-or-bind-ip> <port> <output.pcap> [local-interface-ip]

# 示例：加入组播组 224.1.1.4:6010，录制到 capture.pcap
recorder_cli 224.1.1.4 6010 capture.pcap
```

录制过程中按 `Ctrl+C` 停止，终端会实时打印已收包数与字节数。

### 回放

```sh
player_cli <input.pcap> <dest-ip> <dest-port> [speed] [loop:0|1] [ttl]

# 示例：以原速将 capture.pcap 回放到 224.1.1.4:6010，不循环，TTL=1
player_cli capture.pcap 224.1.1.4 6010 1.0 0 1
```

`speed` 为播放速率倍数（如 `2.0` 表示两倍速），`loop` 为 `1` 时循环播放。

### 图形界面

直接运行 `video_tools_gui`，在"录制"与"回放"两个标签页中分别配置网卡、地址、端口等参数，
并可实时查看进度与日志。

## 测试

```sh
ctest --test-dir build                          # 运行全部测试
ctest --test-dir build -R test_pcap_roundtrip    # 运行指定测试
```

包含 pcap 格式往返测试、校验和测试，以及基于真实 loopback socket 的回放时序测试（验证无漂移
调度、暂停/恢复、变速播放的实际行为）。

## 架构

```
src/pcap  →  src/net  →  src/core  →  src/cli / src/gui
```

- `src/pcap`：libpcap 文件格式读写，合成/解析 Ethernet + IPv4 + UDP 帧头，Qt-free。
- `src/net`：跨平台 UDP socket 封装、网卡枚举、IPv4 地址工具，Qt-free。
- `src/core`：`Recorder` / `Player` 核心状态机，CLI 与 GUI 共用，Qt-free。
- `src/cli`：命令行工具，薄封装。
- `src/gui`：Qt6 Widgets 图形界面。

更详细的开发约定见 [CLAUDE.md](CLAUDE.md)。
