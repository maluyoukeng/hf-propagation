# hf-propagation · 编译、测试与烧录

[English](BUILD.en.md) · [返回 README](../README.md)

---

## 1. 环境要求

| 项目 | 要求 |
| --- | --- |
| 目标芯片 | ESP32-C3 |
| ESP-IDF | **5.5.3** |
| Python | 3.10 或更高 |
| CMake/Ninja | ESP-IDF 自带工具链即可 |
| 宿主机 C 编译器 | 仅运行宿主测试时需要（`cc`/GCC/Clang） |
| Node.js + npx | 仅重新生成字体时需要 |

ESP-IDF 安装与激活：
<https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/get-started/>

本项目的依赖通过 ESP-IDF Component Manager 管理，版本锁定在
`dependencies.lock`：

- `lvgl/lvgl` 9.5.0
- `espressif/esp_lvgl_port` 2.9.0
- `espressif/button` 4.2.0
- `espressif/esp_codec_dev` 1.6.2

`components/bsp/idf_component.yml` 声明了 BSP 依赖；首次构建时 IDF 会下载
`managed_components/`，该目录不需要提交到 Git。

## 2. 编译

在仓库根目录执行：

```bash
idf.py set-target esp32c3
idf.py build
```

生成文件：

- `build/hf-propagation.bin`：应用镜像
- `build/hf-propagation.elf`：调试符号
- `build/hf-propagation.map`：链接映射
- `build/bootloader/bootloader.bin`
- `build/partition_table/partition-table.bin`

生成合并镜像（推荐用于完整刷写）：

```bash
idf.py merge-bin -o build/hf-propagation-full.bin
```

合并镜像为 `build/hf-propagation-full.bin`，从 `0x0` 烧录。
ESP-IDF 5.5 的 `idf.py merge-bin` 默认输出 `build/merged-binary.bin`；
使用 `-o` 可以指定文件名。

## 3. 分区布局

`partitions.csv`：

| 名称 | 类型 | 偏移 | 大小 |
| --- | --- | --- | --- |
| `nvs` | data/nvs | `0x9000` | 24 KB |
| `phy_init` | data/phy | `0xf000` | 4 KB |
| `otadata` | data/ota | `0x10000` | 8 KB |
| `ota_0` | app/ota_0 | `0x20000` | 4032 KB |
| `ota_1` | app/ota_1 | `0x410000` | 4032 KB |

应用使用双 OTA 分区仅用于安全的 app-only 刷写；本应用**不提供**软件 OTA
升级入口。8 MB Flash 的剩余空间保留未使用。

## 4. 烧录

### 4.1 完整镜像（干净安装）

```bash
python -m esptool --chip esp32c3 -p COMx -b 460800 \
  --before default_reset --after hard_reset \
  write_flash 0x0 build/hf-propagation-full.bin
```

或直接：

```bash
idf.py -p COMx flash
```

注意：

- 完整刷写会写入 bootloader、分区表和 OTA 数据；NVS 是否保留取决于实际
  写入范围，不能保证保留 WiFi、参数和定位。
- 不要用于不同分区布局的设备，除非你确认布局兼容。

### 4.2 只更新应用（保留 NVS）

设备当前分区布局必须与本仓库一致：

```bash
python -m esptool --chip esp32c3 -p COMx -b 460800 \
  --before default_reset --after hard_reset \
  write_flash 0x20000 build/hf-propagation.bin
```

这种方式通常保留 NVS 中的 WiFi、SFI/Kp、站点和定位坐标。

### 4.3 串口

- 使用 ESP32-C3 原生 USB Serial/JTAG。
- Windows 下端口通常显示为 `COMx`；Linux 下通常为 `/dev/ttyACM0`。
- 打不开串口时，检查是否被串口监视器占用、USB 线是否支持数据、设备是否
  进入下载模式。

## 5. 宿主测试

```bash
bash tools/validate.sh
```

如果 `cc` 不在 PATH 中：

```bash
CC=/path/to/gcc bash tools/validate.sh
```

测试内容：

- `test_prop_model.c`：日历、太阳几何、MUF/LUF、自动定位坐标、波段评分
- `test_app_settings.c`：参数校验、站点循环、NVS 坐标持久化
- `test_app_spaceweather.c`：NOAA WWV 文本解析
- `test_app_location.c`：多种 IP 定位 JSON 的解析与失败处理
- `test_hf_font_coverage.py`：UI 文本与四个字库的字形覆盖

## 6. 字体生成

提交的 `assets/fonts/hf_font_*.c` 已经包含当前 UI 所需字形。修改
`main/hf_app_ui.c` 或 `main/prop_model.c` 中的中文文案后，需要重新生成：

```bash
python3 tools/gen_hf_font.py --font /path/to/NotoSansSC-Regular.otf
python3 tests/test_hf_font_coverage.py
```

脚本使用 `npx lv_font_conv@1.5.2`，需要 Node.js。字体源文件不随仓库提交；
请使用有合法授权的 Noto Sans SC / Source Han Sans SC 字体。

## 7. 常见构建问题

### 7.1 组件下载失败

检查能否访问 `https://components.espressif.com`，或使用本地镜像/代理。
删除 `managed_components/` 后重新执行 `idf.py build`。

### 7.2 分区表或 Flash 大小不匹配

确认 `sdkconfig.defaults` 中 `CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y`，并使用
本仓库的 `partitions.csv`。设备必须是 8 MB Flash。

### 7.3 屏幕卡死或看门狗复位

项目将 LVGL 内存池设为 64 KB（`CONFIG_LV_MEM_SIZE_KILOBYTES=64`）。
如果加入更多 UI 资源后出现绘制卡死，可适当调大；ESP32-C3 无 PSRAM，
需同时关注内部 RAM。

### 7.4 编译通过但设备不显示

编译成功不代表硬件验收。检查屏幕排线、背光、USB 供电和串口日志；固件
启动后应打印 `hf_prop: ready: station=... target=... sfi=... kp=...`。
