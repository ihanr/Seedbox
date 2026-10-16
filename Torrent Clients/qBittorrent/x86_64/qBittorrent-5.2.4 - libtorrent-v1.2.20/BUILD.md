# qBittorrent 5.2.4 魔改构建记录

本包为 Linux x86_64 的静态 PIE `qbittorrent-nox`，沿用本仓库 5.2.2 的 qBittorrent 与 libtorrent 调优补丁，并在 libtorrent 中新增可选 PT 连接策略。旧版 5.2.2 产物保留。

构建日期：2026-10-17。二进制大小：35,705,192 字节。

二进制 SHA-256：`97472e84093ae76aed26a58a420263902b2ad25308cc56a1af203afcd114ab59`。

加入 PT 档位前的 5.2.4 二进制保留在 Git 历史提交 `e24842d11ff1f4c853f7f5e90bd133c3e093cd3a` 中；切回 `race` 只恢复原两项参数，不会降级二进制。

## 源码与依赖

| 项目 | 版本 |
| --- | --- |
| qBittorrent | [release-5.2.4](https://github.com/qbittorrent/qBittorrent/releases/tag/release-5.2.4)，commit `a1e4649cb8ae581925acc161dbab5cbc6584c542` |
| libtorrent-rasterbar | 1.2.20，含本仓库调优、OpenSSL 4 兼容与 PT 档位补丁 |
| Qt | 6.11.2 |
| Boost | 1.86.0，含 OpenSSL 4 兼容补丁 |
| OpenSSL | 4.0.2 |
| zlib | 1.3.2，含 errno 编译修复 |
| 构建系统 | Debian 13 / WSL2，GCC 14.2.0，CMake 3.31.6 |
| 构建工具 | [qbittorrent-nox-static](https://github.com/userdocs/qbittorrent-nox-static/tree/b37a8d02f16ad7457950de85ebb653bf836ce46b)，含本仓库 static-pie 修复 |

qBittorrent 源码归档 SHA-256：`226a6ad1aa7810fcbfa58b0d28b14feb8e207726cbe774d4d267ce769c19dd6c`。

[5.2.4 补丁](../../../../patches/qbittorrent/5.2.4/patch)与 5.2.2 补丁内容一致，SHA-256 为 `168c080c1a3f6277a45f9d91f7bbfa8f7f48ac30ab3c63372a3f754a8e553faa`。全部补丁均在本仓库 [patches](../../../../patches/) 目录内。

## 本次编译

复用已成功构建的依赖目录。将本仓库 `patches/qbittorrent/5.2.4/patch` 和 `patches/libtorrent/1.2.20/` 下的 `patch`、`openssl-4.patch`、`pt-profile.patch` 放入构建目录同名补丁路径后，在原构建工具目录运行：

```bash
CMAKE_BUILD_PARALLEL_LEVEL=6 LANG=C.UTF-8 ./qbt-nox-static.bash -ma x86_64 -qt release-5.2.4 -n libtorrent qbittorrent
```

这个命令要求已有完整依赖目录与原 `.qbt_env`；它重新编译 libtorrent 并重新链接 qBittorrent，不是从空环境开始的部署命令。实际编译使用 `-O3`、C++23、LTO 与 `-static-pie`，未指定 `-march=native`。

重建时修复了原 `openssl-4.patch` 末尾缺失的 3 行上下文；兼容修复的代码行为不变。三份 libtorrent 补丁已按顺序在官方 1.2.20 release 归档源码上检查应用。

## PT 档位

进程启动前设置 `SEEDBOX_PT_PROFILE`：`race` 为 `50/100`，`balanced` 为 `10/100`，`race200` 为 `50/200`，依次对应 `connect_seed_every_n_download` / `torrent_connect_boost`。未设置、空值和未知值均保持原来的 `race`。只有这两项可随档位变化；其他 qB 与 LT 魔改默认值不变。

本补丁针对本仓库 qB 5.2.4 产物；环境变量在 LT 默认表的进程启动初始化阶段读取，不支持运行中切换。qB 5.2.4 不覆盖这两项设置。systemd 每实例配置与性能边界见[根目录说明](../../../../README.md#pt-连接策略档位)。

## 验证

运行仓库内的隔离 WebUI 测试：

```bash
python3 tests/qb_build_smoke_test.py "Torrent Clients/qBittorrent/x86_64/qBittorrent-5.2.4 - libtorrent-v1.2.20/qbittorrent-nox"
```

测试会创建临时配置，仅监听 localhost，验证 qBittorrent/libtorrent 版本和 17 项魔改默认值；不会连接现有 qB 实例。测试不代表生产负载下的速度保证。

还可将探针链接到同一静态 libtorrent 库，检查实际默认值（在仓库根目录运行，按实际依赖目录修改 `prefix`）：

```bash
prefix=/home/shoo/qbt-static/qbt-build-cross
g++ -std=c++17 $(PKG_CONFIG_PATH="$prefix/lib/pkgconfig" pkg-config --cflags libtorrent-rasterbar) \
  tests/libtorrent_pt_profile_probe.cpp -L"$prefix/lib" -static-pie \
  -Wl,--start-group -ltorrent-rasterbar -lssl -lcrypto -liconv -lz -Wl,--end-group \
  -pthread -ldl -lrt -o /tmp/seedbox-pt-profile-probe
bash tests/libtorrent_pt_profile_test.sh /tmp/seedbox-pt-profile-probe
```

探针在独立进程中检查三档及未设置、空值、未知值，共 6 种情况；同时检查 `default_settings()` 与 `default_int_value()`，并验证补丁格式。它验证参数值，不是上传性能测试。

本机测试环境为 Debian 13 / WSL2，不代表所有发行版、tracker DNS/TLS 或生产负载都已通过验证。静态 glibc 链接仍会提示部分名称解析功能的运行环境兼容性风险；部署到不同发行版后应单独检查 tracker 连通性。

本次测试通过：6 种 libtorrent 默认值探针检查；完整 qB 在 `race`、`balanced`、`race200` 和未设置变量时均通过 WebUI 与 17 项默认值检查。程序报告 `qBittorrent v5.2.4`，API 返回 `libtorrent 1.2.20.0`；`file` 为 x86_64 static-pie，`ldd` 为 statically linked。安装器的菜单、版本组合、架构保护与系统支持检查也已通过。

沿用的魔改项包括 WebUI CSRF 防护默认关闭，可在 WebUI 设置中启用。已有配置中显式写入的设置优先于源码默认值。
