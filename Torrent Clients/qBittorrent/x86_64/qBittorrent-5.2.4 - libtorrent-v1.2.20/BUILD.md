# qBittorrent 5.2.4 魔改构建记录

本包为 Linux x86_64 的静态 PIE `qbittorrent-nox`，沿用本仓库 5.2.2 的 qBittorrent 与 libtorrent 调优补丁。旧版 5.2.2 产物保留。

构建日期：2026-10-17。二进制大小：35,705,480 字节。

二进制 SHA-256：`86d691ca4920b0c7f1d22c680b2f14e377d17a50e26fd41470639ce4b0aa1b7b`。

## 源码与依赖

| 项目 | 版本 |
| --- | --- |
| qBittorrent | [release-5.2.4](https://github.com/qbittorrent/qBittorrent/releases/tag/release-5.2.4)，commit `a1e4649cb8ae581925acc161dbab5cbc6584c542` |
| libtorrent-rasterbar | 1.2.20，含本仓库调优与 OpenSSL 4 兼容补丁 |
| Qt | 6.11.2 |
| Boost | 1.86.0，含 OpenSSL 4 兼容补丁 |
| OpenSSL | 4.0.2 |
| zlib | 1.3.2，含 errno 编译修复 |
| 构建系统 | Debian 13 / WSL2，GCC 14.2.0，CMake 3.31.6 |
| 构建工具 | [qbittorrent-nox-static](https://github.com/userdocs/qbittorrent-nox-static/tree/b37a8d02f16ad7457950de85ebb653bf836ce46b)，含本仓库 static-pie 修复 |

qBittorrent 源码归档 SHA-256：`226a6ad1aa7810fcbfa58b0d28b14feb8e207726cbe774d4d267ce769c19dd6c`。

[5.2.4 补丁](../../../../patches/qbittorrent/5.2.4/patch)与 5.2.2 补丁内容一致，SHA-256 为 `168c080c1a3f6277a45f9d91f7bbfa8f7f48ac30ab3c63372a3f754a8e553faa`。全部补丁均在本仓库 [patches](../../../../patches/) 目录内。

## 本次编译

复用已成功构建的 5.2.2 依赖目录。将本仓库的 `patches/qbittorrent/5.2.4/patch` 放入构建目录同名补丁路径后，在原构建工具目录运行：

```bash
CMAKE_BUILD_PARALLEL_LEVEL=6 LANG=C.UTF-8 ./qbt-nox-static.bash -ma x86_64 -qt release-5.2.4 -n qbittorrent
```

这个命令要求已有完整依赖目录与原 `.qbt_env`；它只重新编译 qBittorrent，不是从空环境开始的部署命令。实际编译使用 `-O3`、C++23、LTO 与 `-static-pie`，未指定 `-march=native`。

## 验证

运行仓库内的隔离 WebUI 测试：

```bash
python3 tests/qb_build_smoke_test.py "Torrent Clients/qBittorrent/x86_64/qBittorrent-5.2.4 - libtorrent-v1.2.20/qbittorrent-nox"
```

测试会创建临时配置，仅监听 localhost，验证 qBittorrent/libtorrent 版本和 17 项魔改默认值；不会连接现有 qB 实例。测试不代表生产负载下的速度保证。

本次测试通过：`qBittorrent v5.2.4`，API 返回 `libtorrent 1.2.20.0`；`file` 为 x86_64 static-pie，`ldd` 为 statically linked。安装器的菜单、版本组合和架构保护检查也已通过。

沿用的魔改项包括 WebUI CSRF 防护默认关闭，可在 WebUI 设置中启用。已有配置中显式写入的设置优先于源码默认值。
