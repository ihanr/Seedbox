# qBittorrent 5.2.4 魔改构建记录

本包为 Linux x86_64 的静态 PIE `qbittorrent-nox`，沿用本仓库 5.2.2 的 qBittorrent 与 libtorrent 调优补丁，包含可选 PT 连接策略、默认关闭的实验性内存压力保护，以及新配置默认启用的 RateBased 上传槽模式。旧版 5.2.2 产物保留。

构建日期：2026-10-18。二进制大小：35,770,760 字节。

二进制 SHA-256：`7c195f02d470267f869b65248e61320ea940a7601ff3e306923ab846e0f4126f`。

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

[5.2.4 基础补丁](../../../../patches/qbittorrent/5.2.4/patch)与 5.2.2 补丁内容一致，SHA-256 为 `168c080c1a3f6277a45f9d91f7bbfa8f7f48ac30ab3c63372a3f754a8e553faa`。上传槽改动使用独立补丁，不修改基础补丁或共享 LT 补丁。全部补丁均在本仓库 [patches](../../../../patches/) 目录内。

## 本次编译

复用已成功构建且含 PT 档位补丁的依赖目录。将本仓库 `patches/qbittorrent/5.2.4/` 下的 `patch`、`resource-guard.patch` 与 `upload-slots.patch` 放入构建目录同名补丁路径后，在原构建工具目录运行：

```bash
CMAKE_BUILD_PARALLEL_LEVEL=6 LANG=C.UTF-8 ./qbt-nox-static.bash -ma x86_64 -qt release-5.2.4 -n qbittorrent
```

这个命令要求已有完整依赖目录与原 `.qbt_env`，并以构建工具要求的 root（或可用 sudo）身份运行；本次只重新编译 qBittorrent，libtorrent 静态库与依赖不变，不是从空环境开始的部署命令。实际编译使用 `-O3`、C++23、LTO 与 `-static-pie`，未指定 `-march=native`。

依赖沿用上次构建的 `patches/libtorrent/1.2.20/` 下 `patch`、`openssl-4.patch` 与 `pt-profile.patch`；其中 OpenSSL 兼容补丁的末尾上下文在此前构建时已修复。三份补丁未因本次上传槽默认值调整而修改。

本次链接使用的 `libtorrent-rasterbar.a` SHA-256：`7eedc30e66db431abf84606058c36d13d11097ed9204e9ab64c176e94a9cfc7b`。

与此前构建缓存的源码清单逐项比对后，`src/` 下的所有 `.cpp` / `.h` 中仅 `sessionimpl.cpp` 有差异，且 diff 仅为上传槽默认枚举的一行修改；资源保护 header 及依赖目录中的静态库清单哈希均保持不变。

## PT 档位

进程启动前设置 `SEEDBOX_PT_PROFILE`：`race` 为 `50/100`，`balanced` 为 `10/100`，`race200` 为 `50/200`，依次对应 `connect_seed_every_n_download` / `torrent_connect_boost`。未设置、空值和未知值均保持原来的 `race`。只有这两项可随档位变化；其他 qB 与 LT 魔改默认值不变。

本补丁针对本仓库 qB 5.2.4 产物；环境变量在 LT 默认表的进程启动初始化阶段读取，不支持运行中切换。qB 5.2.4 不覆盖这两项设置。systemd 每实例配置与性能边界见[根目录说明](../../../../README.md#pt-连接策略档位)。

## 上传槽默认值

独立的 [upload-slots.patch](../../../../patches/qbittorrent/5.2.4/upload-slots.patch) 在基础补丁和资源保护补丁之后应用，仅将 qB 会话的 `ChokingAlgorithm` 默认值从 `FixedSlots` 改为 `RateBased`，不修改 LT 原生算法。补丁 SHA-256：`a789ecf288143463d56414cf7b3a8dcd510c4885a2504b160286009e54322d3e`。

qB 将 `RateBased` 映射为 LT 1.2.20 的 `rate_based_choker`；WebUI/API 的 `upload_slots_behavior` 为 `1`，配置文件使用名字 `RateBased`，不是数字 `1`。仅替换程序时，已有显式配置优先，包括 `FixedSlots`，不强制迁移；原一键安装脚本仍会重写用户配置，本次不修改安装器。WebUI 可切回固定槽位，保存并重启后仍保留选择。

`FastestUpload`、全局与每种最大上传槽的默认配置值 `-1`、三个 PT 档位和资源保护行为均不变。RateBased 模式按上传速率计算会话槽位，不把固定全局槽位数当硬上限；它不读取磁盘类型或内存压力，也不协调多个实例。使用及回退说明见[根目录说明](../../../../README.md#上传槽分配新配置默认-ratebased)。

此前默认固定槽位的 5.2.4 程序保留在 Git 历史提交 `acecd041a27d25ff5fff69833908f0d1150f5dea` 中，SHA-256 为 `5ee25af84876f52940473234dc211f49c1c452072328763d2bb244a6c36f8f15`。切回固定槽位设置不需要降级二进制。

## 实验性内存压力保护

额外的 [resource-guard.patch](../../../../patches/qbittorrent/5.2.4/resource-guard.patch) 在原 qB 魔改补丁之后应用；内含 [resourceguard.h](../../../../patches/qbittorrent/5.2.4/resourceguard.h) 与两处 qB 会话文件的集成。单元测试直接使用这份 header，重建时应核对补丁内 header 与独立文件一致。

资源保护补丁 SHA-256：`84182071366b152792fa51ec2916ef347e67f69da6bfddd73075a0b8e36887c9`；独立 header SHA-256：`176023237896952f9460769e5aae52c7e11e31e03c8253c2a3ac5377bfe8d6d6`。

仅进程启动前设置 `SEEDBOX_RESOURCE_GUARD=1` 时启用。未设置、空值、`0` 或无效值均关闭。启用后仅在持续内存压力、有实际上传且 IO 压力未过高时，调整 LT 的发送缓冲高水位；不改低水位、倍率、磁盘缓存、连接数、peer、超时或重连。运行时有效上限不写入配置，WebUI 原基线不变；阈值、systemd 每实例开关与局限见[根目录说明](../../../../README.md#实验性内存压力保护默认关闭)。

未加入保护的 5.2.4 二进制可从 Git 历史提交 `feff3f2dd618bca2d22a7b8ca6a4eacaf8074c93` 找回。关闭开关只停用保护，不会降级程序。

## 验证

核心策略与 Linux 文件解析测试，无需第三方测试框架：

```bash
g++ -std=c++17 -Wall -Wextra -Werror tests/resource_guard_test.cpp -o /tmp/seedbox-resource-guard-test
/tmp/seedbox-resource-guard-test
```

覆盖默认关闭、采样节流、两级收紧、上下限、最新用户基线、上传与 IO 门槛、迟滞恢复、数据中断、异常指标、cgroup/容器识别；有可用逗号小数 locale 时也检查 PSI 的固定点号解析，否则明确显示跳过该项。

运行仓库内的隔离 WebUI 测试：

```bash
python3 tests/qb_build_smoke_test.py "Torrent Clients/qBittorrent/x86_64/qBittorrent-5.2.4 - libtorrent-v1.2.20/qbittorrent-nox"
```

测试会创建临时配置，仅监听 localhost，验证 qBittorrent/libtorrent 版本、17 项原魔改默认值、上传槽模式、做种排序及两个最大上传槽配置值；不会连接现有 qB 实例。还会通过 API 依次选择固定槽位与 RateBased，每次检查读取结果并正常退出、重新启动，以验证持久化。测试不代表生产负载下的速度保证。另设 `SEEDBOX_EXPECT_RESOURCE_GUARD=off` 可检查关闭状态；同时设置 `SEEDBOX_RESOURCE_GUARD=1 SEEDBOX_EXPECT_RESOURCE_GUARD=on` 时，还验证初始 LT 有效上限和 WebUI 修改基线后的实际 LT 回读。后者是测试断言变量，不是运行功能的开关。

旧配置优先测试可分别设置 `SEEDBOX_TEST_CHOKING_ALGORITHM=FixedSlots` 和 `SEEDBOX_TEST_CHOKING_ALGORITHM=RateBased` 再运行同一测试。这个变量仅供测试脚本生成临时配置使用，不是 qB 运行开关。测试旧版 5.2.2 安装包时，在二进制路径后传入 `5.2.2`，其新配置仍应为固定槽位。

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

压力级别 1/2 的策略使用模拟指标验证；真实完整 qB 的回读及 WebUI 基线修改检查是在级别 0 进行，尚未实测级别 1/2 下的 WebUI 修改、退出持久化与生产压力闭环。容器识别只是常见标记检查，不能保证识别隐藏祖先配额。这些测试不证明多实例公平分配、内存降幅或上传改善。

本次测试通过：核心策略与 Linux 文件解析（包括实际德语小数 locale）；AddressSanitizer/UndefinedBehaviorSanitizer 检查；重新链接至未改动 LT 静态库的 6 种默认值探针；10 个 5.2.4 隔离 WebUI 场景（含三个并发实例），每个场景检查 17 项原魔改默认值及 4 项上传槽相关配置，并验证 API 切换到 `0`、`1` 后分别正常退出和重启保存。覆盖新配置、已保存 `FixedSlots`、已保存 `RateBased`；另以未改动的 5.2.2 安装包验证新配置仍为固定槽位。

资源保护关闭检查覆盖未设置、`0`、空值和无效值；开启检查包含实际 LT 缓冲上限回读与 WebUI 基线修改。WSL 环境下程序报告 `metrics=unavailable` 并保持仅观察，因此这些完整 qB 测试没有触发真实压力收紧。复制到仓库后的 5.2.4 安装包另做了一次默认关闭、模式切换和重启保存检查，且与构建产物逐字节一致。以上测试均为空任务的隔离会话，不验证真实 peer 选择、RateBased 在竞速负载下的槽位数或上传收益。

程序报告 `qBittorrent v5.2.4`，API 返回 `libtorrent 1.2.20.0`；`file` 为 x86_64 static-pie，`ldd` 为 statically linked。安装器的菜单、版本组合、架构保护与系统支持检查也已通过。

沿用的魔改项包括 WebUI CSRF 防护默认关闭，可在 WebUI 设置中启用。已有配置中显式写入的设置优先于源码默认值。
