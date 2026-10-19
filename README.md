# hf-propagation

**Shortwave propagation predictor for the FoloToy AI Passport.**
**运行在 FoloToy AI Passport 上的离线短波传播预测应用。**

项目主页：<https://github.com/maluyoukeng/hf-propagation>

[English](README.en.md) · [使用说明](docs/USER_GUIDE.md) · [编译与烧录](docs/BUILD.md) · [硬件说明](docs/HARDWARE.md)

---

## 项目简介

这是一个面向业余无线电短波通联规划的便携应用。设备根据太阳通量
`SFI`、地磁指数 `Kp`、昼夜路径和收发地点距离，估算 `MUF`、`LUF` 以及
80m–10m 八个业余波段的可用性，并给出最佳波段和 24 小时最佳时段。

设备可离线使用，也可以通过 2.4 GHz WiFi 连接路由器，从 NOAA SWPC
获取实时 `SFI`、`Kp` 和 `A` 指数。站点除内置城市外，还支持
**自动定位**：联网更新时根据公网 IP 获取城市级经纬度，直接用于传播模型。

> 本项目是 FoloToy AI Passport 的社区衍生应用，不是 FoloToy 官方固件。
> 预测结果仅供业余无线电通联规划参考，不替代专业传播软件或实测结果。

## 主要功能

- 八个业余波段：80m、40m、30m、20m、17m、15m、12m、10m
- 估算 `MUF`、`LUF`、路径距离、方位角和昼间路径比例
- 最佳波段、各波段 0–100 评分与四级可用性评级
- 24 小时最佳通联时段
- 8 个内置站点城市（北京、上海、广州、成都、乌鲁木齐、哈尔滨、拉萨、海口）以及 **自动定位**
- 6 个目标区域：东亚、东南亚、欧洲、北美、大洋洲、南美
- NOAA SWPC 实时空间天气数据更新
- WiFi 热点配网：手机/电脑连接设备热点后，在网页中扫描并选择附近 WiFi
- 参数、时区、站点和定位坐标保存在 NVS，掉电不丢失
- 中文界面、三个实体按键操作、240 × 320 屏幕

## 硬件要求

| 项目 | 规格 |
| --- | --- |
| 设备 | FoloToy AI Passport |
| 主控 | ESP32-C3，8 MB Flash，无 PSRAM |
| 显示 | ST7789P3，240 × 320 竖屏 RGB565 |
| 按键 | `UP` / `DOWN` / `OK` 三键，ADC 电阻分压，共用 GPIO0 |
| 无线 | 2.4 GHz WiFi |
| 音频/电池 | ES8311、CW2017（本应用只使用电池读数） |
| 烧录/日志 | ESP32-C3 原生 USB Serial/JTAG |

完整引脚表见 [docs/HARDWARE.md](docs/HARDWARE.md)。

## 快速开始

### 1. 准备开发环境

- ESP-IDF **5.5.3**
- Python 3
- 可用的 2.4 GHz WiFi 网络（仅数据更新和自动定位需要）

首次构建时，ESP-IDF Component Manager 会根据 `dependencies.lock` 下载：
LVGL 9.5.0、`esp_lvgl_port` 2.9.0、`button` 4.2.0、`esp_codec_dev` 1.6.2。

### 2. 编译

```bash
idf.py set-target esp32c3
idf.py build
```

构建完成后会生成：

- `build/hf-propagation.bin`：应用镜像
- `build/hf-propagation-full.bin`：包含 bootloader、分区表和 OTA
  数据的合并镜像（运行
  `idf.py merge-bin -o build/hf-propagation-full.bin` 后生成）

> ESP-IDF 5.5 的 `idf.py merge-bin` 默认输出 `build/merged-binary.bin`；
> 使用 `-o` 可以指定 `hf-propagation-full.bin` 这个文件名。

### 3. 烧录

完整刷写（干净安装；NVS 中的 WiFi、参数和定位可能受影响）：

```bash
idf.py -p COMx flash
```

只更新应用、保留 NVS 配置（需要与设备当前分区布局一致）：

```bash
python -m esptool --chip esp32c3 -p COMx -b 460800 \
  write_flash 0x20000 build/hf-propagation.bin
```

更详细的构建、分区和刷写说明见 [docs/BUILD.md](docs/BUILD.md)。

## 首次使用

1. 开机后进入主界面，短按 `UP` / `DOWN` 切换波段，短按 `OK` 查看详情，
   长按 `OK` 进入菜单。
2. 进入 `参数设置`，选择站点、目标区域、时区和时间；也可以手动填写
   `SFI` 与 `Kp`。
3. 进入 `WiFi 设置`，按 `OK` 开启设备热点。
4. 手机或电脑连接热点：
   - SSID：`AI-PASSPORT-XXXXXX`（设备 MAC 后 6 位）
   - 密码：`aipassport`
   - 网页：<http://192.168.4.1>
5. 在网页中点击“扫描附近 WiFi”，选择自己的 2.4 GHz 网络并输入密码，
   保存。
6. 回到设备，进入 `数据更新`，按 `OK`。设备连接路由器后：
   - 如果站点为“自动定位”，先获取网络位置；
   - 再从 NOAA 获取 `SFI`、`Kp`、`A` 指数；
   - 自动写入参数并重新计算预测。

## 按键操作

| 操作 | 功能 |
| --- | --- |
| `UP` / `DOWN` 短按 | 菜单中移动选择；总览中切换波段；设置中调整数值 |
| `OK` 短按 | 进入、确认、开始更新 |
| `OK` 长按 | 返回上级；在参数设置中保存并返回 |
| `OK` 长按（编辑中） | 取消本次编辑 |

## 页面说明

| 页面 | 内容 |
| --- | --- |
| 总览 | 当前波段、MUF/LUF、评分、最佳波段、简明日夜与距离信息 |
| 波段详情 | 评分、最佳时段、昼间路径、距离、方位角 |
| 参数设置 | 站点、目标、时区、SFI、Kp、UTC 时间、日期、定位状态 |
| WiFi 设置 | 热点信息、网页地址、路由器 WiFi 配置状态 |
| 数据更新 | WiFi、NOAA 指数、发布时间、网络位置和更新状态 |
| 使用说明 | 模型说明与按键提示 |

## 自动定位

在 `参数设置 → 站点` 中选择 **自动定位**。之后每次进入 `数据更新` 并联网时，
设备会依次尝试多个 IP 定位服务，获取城市级经纬度并保存到 NVS。

- 使用公网 IP，**不是 GPS**，也不是扫描附近 WiFi AP 的三角定位。
- 精度通常为城市级；VPN、代理、运营商出口或企业网络可能导致偏差。
- 定位失败时保留上一次坐标；从未定位过时按北京坐标估算。
- 内置固定城市始终可手动选择，便于在定位不准确时覆盖。
- 定位服务会收到设备的公网 IP。设备只在需要定位时发起请求。

## 数据来源与隐私

- 空间天气：NOAA SWPC WWV 文本
  <https://services.swpc.noaa.gov/text/wwv.txt>
- 网络定位：`ip-api.com`、`ipapi.co`、`ipwho.is`、`freeipapi.com`
- 设备不保存第三方账号、密钥或个人身份信息；WiFi 密码保存在本地 NVS。
- 本项目不会上传设备数据到自建服务器，也没有云端账号体系。

## 目录结构

```text
main/                应用逻辑、UI、WiFi、NOAA、定位与传播模型
components/bsp/      FoloToy AI Passport 板级支持
assets/fonts/        生成的 LVGL 中文字体子集与字形清单
docs/                使用说明、编译说明、硬件说明
tests/               可脱离硬件运行的宿主测试
tools/               验证与字体生成脚本
CMakeLists.txt       项目入口
partitions.csv       NVS + PHY + 双 OTA 分区布局
sdkconfig.defaults   ESP32-C3、USB 控制台、LVGL 等默认配置
dependencies.lock    ESP-IDF 组件版本锁定
```

## 测试

宿主机测试（需要 C 编译器和 Python 3）：

```bash
bash tools/validate.sh
```

覆盖传播模型、设置持久化、NOAA 解析、IP 定位解析和字体覆盖。
编译通过不代表真机通过；屏幕、按键、WiFi 和定位精度仍需在设备上验收。

## 许可证与致谢

- 本项目基于 [FoloToy AI Passport](https://github.com/FoloToy/ai-passport)
  的 BSP 和基线工程，遵循 MIT License。
- 上游版权：Copyright (c) 2026 FoloToy，详见 [LICENSE](LICENSE)。
- 字体子集来自 Noto Sans SC / Source Han Sans SC，SIL Open Font License 1.1。
- NOAA SWPC 提供公开空间天气数据。
- 感谢 FoloToy AI Passport 社区提供的开放硬件与开发文档。

第三方组件、字体和数据源的完整说明见 [NOTICE.md](NOTICE.md)。
