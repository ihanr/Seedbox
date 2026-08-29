# Seedbox 安装说明

这是一个 Debian/Ubuntu 的 Seedbox 部署脚本仓库。它会安装 qBittorrent，并可选安装 BBR、autobrr、Vertex 和 autoremove-torrents，同时写入相应的系统调优配置。

> 请在新服务器或已做好备份的服务器上运行。安装 qBittorrent 时会创建/使用指定 Linux 用户，并覆盖该用户的 qBittorrent 配置。

## 支持的系统

- Debian 10、11、12、13
- Ubuntu 20.04、22.04、23.04、24.04

请使用 `root` 运行。脚本会先更新系统软件包。

Ubuntu 可安装 qBittorrent、autobrr、Vertex 和 autoremove-torrents；自定义 BBRx、BBRy、BBRz（`-x`、`-y`、`-z`）目前仅支持 Debian，请勿在 Ubuntu 上使用这些参数。

## 安装 qBittorrent 5.2.2

推荐组合为 qBittorrent `5.2.2` 与 libtorrent `v1.2.20`。该版本已包含面向 PT/Seedbox 的高并发默认参数，并提供 x86_64 静态二进制。

下面的命令不会把密码写入 shell 历史；运行后脚本会提示输入密码：

```bash
bash <(wget -qO- https://raw.githubusercontent.com/ihanr/Seedbox/main/Install.sh) -u shoo -c 4096 -q 5.2.2 -l v1.2.20 -x
```

参数说明：

| 参数 | 含义 |
| --- | --- |
| `-u shoo` | qBittorrent 运行所用的 Linux 用户名，按需替换 |
| `-c 4096` | qBittorrent 磁盘缓存，单位 MiB；按内存情况调整 |
| `-q 5.2.2` | qBittorrent 版本 |
| `-l v1.2.20` | libtorrent 版本；5.2.2 请选择这一项 |
| `-x` | 安装 BBRx |

安装完成后，默认 WebUI 地址为 `http://服务器IP:9090`，默认入站端口为 `36666`。如需自定义端口，在命令末尾加入 `-o`；脚本会交互式询问 WebUI 和入站端口。

```bash
bash <(wget -qO- https://raw.githubusercontent.com/ihanr/Seedbox/main/Install.sh) -u shoo -c 4096 -q 5.2.2 -l v1.2.20 -x -o
```

BBR 安装完成后按脚本提示重启，再开始测速或使用。

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
bash <(wget -qO- https://raw.githubusercontent.com/ihanr/Seedbox/main/Install.sh) -u shoo -c 4096 -q 5.2.2 -l v1.2.20 -x -b -v
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
- 不要在聊天记录、截图或 Git 仓库中保存 WebUI 密码、tracker passkey、Cookie 或 SSH 私钥。
- 更新或重装 qBittorrent 前，先备份 `/home/用户名/.config/qBittorrent/`。

## 致谢

- qBittorrent 静态构建：[userdocs/qbittorrent-nox-static](https://github.com/userdocs/qbittorrent-nox-static)
- qBittorrent 密码工具：[libqbpasswd](https://github.com/KozakaiAya/libqbpasswd)
- autoremove-torrents：[jerrymakesjelly/autoremove-torrents](https://github.com/jerrymakesjelly/autoremove-torrents)
- BBR：基于 [KozakaiAya/TCP_BBR](https://github.com/KozakaiAya/TCP_BBR)
