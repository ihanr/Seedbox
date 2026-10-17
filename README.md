# Seedbox 安装说明

这是一个 Debian/Ubuntu 的 Seedbox 部署脚本仓库。它会安装 qBittorrent，并可选安装 BBR、autobrr、Vertex 和 autoremove-torrents，同时写入相应的系统调优配置。

> 请在新服务器或已做好备份的服务器上运行。安装 qBittorrent 时会创建/使用指定 Linux 用户，并覆盖该用户的 qBittorrent 配置。

## 支持的系统

- Debian 10、11、12、13
- Ubuntu 20.04、22.04、23.04、24.04

请使用 `root` 运行。脚本会先更新系统软件包。

Ubuntu 可安装 qBittorrent、autobrr、Vertex 和 autoremove-torrents；自定义 BBRx、BBRy、BBRz（`-x`、`-y`、`-z`）目前仅支持 Debian，请勿在 Ubuntu 上使用这些参数。

## 安装 qBittorrent 5.2.4

推荐组合为 qBittorrent `5.2.4` 与 libtorrent `v1.2.20`，提供 x86_64 静态二进制。它沿用本仓库 5.2.2 的魔改补丁，原 5.2.2 安装包仍保留供回退。5.2.4 目前只提供这一种架构和 libtorrent 组合。

下面的命令不会把密码写入 shell 历史；运行后脚本会提示输入密码：

```bash
bash <(wget -qO- https://raw.githubusercontent.com/ihanr/Seedbox/main/Install.sh) -u shoo -c 4096 -q 5.2.4 -l v1.2.20 -x
```

参数说明：

| 参数 | 含义 |
| --- | --- |
| `-u shoo` | qBittorrent 运行所用的 Linux 用户名，按需替换 |
| `-c 4096` | qBittorrent 磁盘缓存，单位 MiB；按内存情况调整 |
| `-q 5.2.4` | qBittorrent 版本 |
| `-l v1.2.20` | libtorrent 版本；5.2.4 请选择这一项 |
| `-x` | 安装 BBRx |

安装完成后，默认 WebUI 地址为 `http://服务器IP:9090`，默认入站端口为 `36666`。如需自定义端口，在命令末尾加入 `-o`；脚本会交互式询问 WebUI 和入站端口。

```bash
bash <(wget -qO- https://raw.githubusercontent.com/ihanr/Seedbox/main/Install.sh) -u shoo -c 4096 -q 5.2.4 -l v1.2.20 -x -o
```

BBR 安装完成后按脚本提示重启，再开始测速或使用。

## PT 连接策略档位

本仓库 2026-10-17 新构建的 `5.2.4 + libtorrent 1.2.20` 支持启动时选择档位。旧二进制、5.2.2 和其他组合不支持；仅设置环境变量不会升级程序。改动位于 [libtorrent 补丁](patches/libtorrent/1.2.20/pt-profile.patch)，没有改变 qBittorrent 版本号，也没有新增安装器参数。

是否为新包，需将 `sha256sum /usr/bin/qbittorrent-nox` 的结果与[构建记录](<Torrent Clients/qBittorrent/x86_64/qBittorrent-5.2.4 - libtorrent-v1.2.20/BUILD.md>)比对；不能仅凭 `--version` 区分。

| `SEEDBOX_PT_PROFILE` | `connect_seed_every_n_download` | `torrent_connect_boost` | 用途 |
| --- | --- | --- | --- |
| `race`（默认） | 50 | 100 | 保持原魔改策略，较偏向本机正在下载的任务 |
| `balanced` | 10 | 100 | 下载、做种任务并存时，增加本机已完成任务获得主动连接尝试的机会 |
| `race200`（实验） | 50 | 200 | 保持原调度比例，提高首次 tracker 响应后的立即连接尝试上限 |

这两个数字不是上传带宽比例，也不是保证能连接的 peer 数。`race200` 不会让 tracker 返回更多 peer；peer 不足、总连接数已满或磁盘繁忙时，可能没有收益，甚至增加负载。其余魔改默认值保持不变，尚无生产环境对比数据证明某个档位更快。

### 为 systemd 实例选择档位

先确认已经使用上述新二进制。以主实例为例：

```bash
systemctl edit qbittorrent-nox@shoo.service
```

在编辑器中加入以下内容并保存：

```ini
[Service]
Environment="SEEDBOX_PT_PROFILE=balanced"
```

然后重启该实例，重启期间会暂时中断任务：

```bash
systemctl daemon-reload
systemctl restart qbittorrent-nox@shoo.service
systemctl show qbittorrent-nox@shoo.service -p Environment
```

最后一条命令仅检查服务配置的环境变量。`qb2.service`、`qb3.service` 如需切换，分别执行同样操作并替换服务名；三个实例可以选择不同档位。直接在 SSH 中 `export` 后执行 `systemctl restart` 不会把变量传入服务。

需要回到原策略时，在同一处改为 `Environment="SEEDBOX_PT_PROFILE=race"`，再重新加载、重启。变量未设置、为空或值不识别时也回退到 `race`。档位只在进程启动时读取，不支持热切换，不会改写配置、种子或下载目录。

测试时一次只改一个实例、一个档位，对比相近新种的前 10 分钟上传量、连接建立时间，以及磁盘延迟、CPU 和内存；不同种子的人气、发布时间与线路差异会干扰结论。

## 上传槽分配（新配置默认 RateBased）

2026-10-18 起的新构建仅对 `5.2.4 + libtorrent 1.2.20` 调整一项 qB 默认值：上传槽模式由 `FixedSlots` 改为 `RateBased`。这是启用 LT 已有的按上传速率分配槽位功能，不是新增调度算法，也没有写死槽位数量。`FastestUpload` 做种排序、上述三个连接档位、资源保护开关、缓存和缓冲参数均保持不变；5.0.4、5.2.2 和其他安装包不变。

| 配置状态 | 启动后的上传槽模式 |
| --- | --- |
| 新配置，或未保存 `Session\ChokingAlgorithm` | `RateBased`，WebUI/API 值为 `1` |
| 已保存 `Session\ChokingAlgorithm=FixedSlots` | 保留 `FixedSlots`，WebUI/API 值为 `0` |
| 已保存 `Session\ChokingAlgorithm=RateBased` | 保留 `RateBased`，WebUI/API 值为 `1` |

配置文件中的值是枚举名字，不是 API 的数字。仅替换程序不会强制覆盖旧配置；先到 WebUI 的“设置 → 高级 → 上传槽位行为（Upload slots behavior）”检查，按需选择“基于上传速率（Upload rate based）”并保存。每个实例单独设置。

注意：上述旧配置优先是指直接替换二进制。原一键安装脚本仍会重写用户配置，本次没有改变这一行为；更新前先备份，不能用整套重装来验证旧配置是否保留。

全局和每种最大上传槽的默认配置值仍为 `-1`。但在 RateBased 模式下，LT 会按实际上传速率计算会话槽位，固定的全局槽位数不作为硬上限，不能把界面的 `-1` 理解成始终不限槽。它不感知 SSD/NVMe、内存压力或其他实例，也不会自动给三个实例公平分配带宽。

需要回到此前的不限槽行为时，将“上传槽位行为”切回“固定槽位（Fixed slots）”，并确认“连接”页的全局和每种最大上传槽限制均未勾选；保存即可，无需降级程序。请按自己的原配置恢复，不要覆盖有意设置的限制。

这是可回退的对照测试起点，不是已验证的竞速提速方案。槽位过少可能让新 peer 等待，甚至减少上传；没有资源争抢时也可能无收益。保持其他参数不变，对比相近负载下的新种前 10 分钟累计上传量及整机总上传，不能只看单个 peer 的速度。

## 实验性内存压力保护（默认关闭）

此功能只随本仓库新构建的 `5.2.4 + libtorrent 1.2.20` 提供。默认关闭，未启用时保持原有设置，也不采集压力指标。它不是上传加速器或进程内存硬限制，没有生产 PT 提速保证。

仅在需要对照测试的实例上设置：

```bash
systemctl edit qbittorrent-nox@shoo.service
```

```ini
[Service]
Environment="SEEDBOX_RESOURCE_GUARD=1"
```

保存后执行 `systemctl daemon-reload` 和 `systemctl restart qbittorrent-nox@shoo.service`。变量只有值 `1` 启用；未设置、空值、`0` 或无效值均关闭，无效值会记录告警。关闭时改为 `0` 并重启。`qb2`、`qb3` 需分别设置，不会自动跟随主实例启用。

### 它会做什么

- 使用 qB 已有的实际 payload 上传统计，不扫描全部 peer，也不按 peer 数均分内存。
- 每 10 秒读取 Linux 可用内存与 memory/io PSI。可用内存不超过 10%、memory `some avg10` 不低于 1%，连续 30 秒且有实际上传、IO `some avg10` 低于 10% 时，才开始收紧发送缓冲上限。
- 最多两级：用户上限的 75% 和 50%；两次收紧至少相隔 60 秒。在不提高原上限的前提下，不降到用户原低水位或 1 MiB 以下；原上限过小、非正或低水位不小于高水位时保持原值。
- 可用内存至少 20%、memory `some avg10` 低于 0.1%、IO `some avg10` 低于 5%，持续 120 秒后恢复一级。中间态、缺失指标或超过 30 秒的采样空档不会算作持续压力或持续健康。

以上是实验性初始保护策略，不是官方推荐阈值或通用最优值。IO 压力高时不继续收紧，避免更小的发送预读窗口加重供数等待。已分配的发送数据不会被强制剪掉，因此不保证立即降低内存。

### 不会做什么

- 不断开 peer，不改变连接数、上传槽、上传限速、磁盘缓存、I/O 线程、超时、重连或三个连接档位。
- 不修改 qB 配置文件。WebUI 显示的仍是用户基线，运行中改设置会使用最新基线计算；关闭并重启后恢复最新用户设置。
- 不修改系统、BBR 或 cgroup，也不宣称多个实例会自动公平分配资源。多个已启用实例分别观察整机压力，只调整自己的发送缓冲。
- 首版不自动处理已识别的容器、cgroup v1 或可见祖先中有限的 `memory.max` / `memory.high`。指标不完整时仅观察，已收紧的上限保持不变；不猜测容器配额，也不进一步收紧。

容器识别仅覆盖常见标记，不保证识别所有隔离环境或隐藏祖先配额；本功能不适用于容器限额管理。

需要可读的 `/proc/pressure/memory`、`/proc/pressure/io` 及 cgroup v2 信息；不满足时会记录指标不可用，不自动收紧。

在 WebUI 日志中搜索 `[resource-guard]`：启动会显示 `mode=off/on`；启用后记录指标可用性、保护级别及从 LT 回读的 `effective_high_kib`。用户原上限为 `baseline_high_kib`；数值单位均为 KiB。日志不记录密码、Cookie、tracker 地址或 passkey。

建议先只启用一个实例做对照。以整机累计上传量、新种前 10 分钟上传量、内存等待和磁盘等待一起评估；内存更低不等于上传更快。发现退化时关闭并重启即可回到用户基线。

## 其他可选组件

这些参数可与 qBittorrent 参数组合使用：

| 参数 | 功能 |
| --- | --- |
| `-r` | 安装 autoremove-torrents |
| `-b` | 安装 autobrr |
| `-v` | 安装 Vertex |
| `-x` | 安装 BBRx |
| `-y` | 安装 BBRy |
| `-z` | 安装 BBRz |
| `-3` | 安装 BBRv3 |
| `-o` | 交互式设置 qBittorrent、autobrr、Vertex 端口 |
| `-h` | 显示脚本帮助 |

示例：同时安装 qBittorrent、BBRx、autobrr 与 Vertex：

```bash
bash <(wget -qO- https://raw.githubusercontent.com/ihanr/Seedbox/main/Install.sh) -u shoo -c 4096 -q 5.2.4 -l v1.2.20 -x -b -v
```

## 安装后检查

```bash
qbittorrent-nox --version
systemctl status qbittorrent-nox@shoo
sysctl net.ipv4.tcp_congestion_control
```

将命令中的 `shoo` 改为实际用户名。若使用 BBRx，重启后可再执行：

```bash
lsmod | grep bbrx
sysctl net.ipv4.tcp_congestion_control
```

## 安全提示

- 不要把 WebUI 直接暴露到公网；至少使用防火墙或反向代理限制访问来源。
- 魔改补丁沿用 5.2.2 的 WebUI CSRF 防护默认关闭设置；可在 WebUI 设置中启用防护。此项不是性能优化。
- 不要在聊天记录、截图或 Git 仓库中保存 WebUI 密码、tracker passkey、Cookie 或 SSH 私钥。
- 更新或重装 qBittorrent 前，先备份 `/home/用户名/.config/qBittorrent/`。

## 致谢

- qBittorrent 静态构建：[userdocs/qbittorrent-nox-static](https://github.com/userdocs/qbittorrent-nox-static)
- qBittorrent 密码工具：[libqbpasswd](https://github.com/KozakaiAya/libqbpasswd)
- autoremove-torrents：[jerrymakesjelly/autoremove-torrents](https://github.com/jerrymakesjelly/autoremove-torrents)
- BBR：基于 [KozakaiAya/TCP_BBR](https://github.com/KozakaiAya/TCP_BBR)
