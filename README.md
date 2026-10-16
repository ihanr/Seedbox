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
