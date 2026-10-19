# hf-propagation · 硬件说明

[English](HARDWARE.en.md) · [返回 README](../README.md)

本应用面向 **FoloToy AI Passport**。硬件事实以
`components/bsp/include/bsp_pins.h` 为单一来源；本文档是便于阅读的摘要。

## 1. 平台规格

| 项目 | 规格 |
| --- | --- |
| 主控 | ESP32-C3，RISC-V 单核，160 MHz |
| Flash | 8 MB，无 PSRAM |
| 显示 | ST7789P3，240 × 320，竖屏，RGB565，SPI2 |
| 输入 | `UP` / `DOWN` / `OK` 三键，ADC 电阻分压，共用 GPIO0 |
| 音频 | ES8311，I2S0 全双工 |
| 电池 | CW2017 电量计，I2C0 |
| 无线 | 2.4 GHz WiFi（ESP32-C3 不支持 5 GHz） |
| 控制台 | 原生 USB Serial/JTAG |

本应用使用显示、按键、电池读数和 WiFi；不启用音频播放/录音。

## 2. 引脚

### 2.1 显示

| 信号 | GPIO |
| --- | --- |
| SPI MOSI | 9 |
| SPI SCLK | 8 |
| CS | 1 |
| DC | 20 |
| RST | 未接 MCU（`-1`，软复位） |
| 背光 | 21（LEDC PWM） |

### 2.2 按键

| 按键 | ADC 电压窗口 |
| --- | --- |
| `UP` | 0–150 mV |
| `DOWN` | 150–447 mV |
| `OK` | 447–1900 mV |
| 松开 | ≥ 1900 mV（约 3300 mV） |

三键共用 `ADC1_CH0` / GPIO0。不要改用内部上拉；内部上拉的阻值和精度会
导致三档电压重叠。

### 2.3 I2C

| 信号 | GPIO |
| --- | --- |
| SDA | 10 |
| SCL | 7 |

| 设备 | 7 位地址 |
| --- | --- |
| ES8311 | `0x18` |
| CW2017 | `0x63` |

### 2.4 I2S / 音频

| 信号 | GPIO |
| --- | --- |
| MCLK | 6 |
| BCLK | 5 |
| WS | 3 |
| DOUT（MCU → codec） | 2 |
| DIN（codec → MCU） | 4 |
| PA_CTRL | 未接 MCU（`-1`） |

### 2.5 USB

GPIO18/19 保留给 ESP32-C3 原生 USB Serial/JTAG。UART0 默认 TX 为 GPIO21，
与背光冲突，因此项目使用 USB Serial/JTAG 作为控制台。

## 3. 电源与启动

- 设备通过 USB-C 供电/充电；具体充电和电池保护以硬件版本为准。
- 本应用不主动进入 deep sleep，开机后保持 UI 和按键响应。
- 电池百分比来自 CW2017，精度取决于电芯和 profile，不是校准仪表。

## 4. 无线

- WiFi 仅支持 2.4 GHz。
- 配网时设备开启 SoftAP，默认 SSID 前缀 `AI-PASSPORT-`，密码
  `aipassport`，网页 `http://192.168.4.1`。
- SoftAP 与 STA 在配网扫描时使用 APSTA 模式；扫描完成后热点保持开启。
- 数据更新时设备作为 STA 连接路由器，更新完成后断开。

## 5. 修改硬件

更换屏幕、按键分压或外设时，先修改
`components/bsp/include/bsp_pins.h`，再检查：

- `BSP_LCD_*` 与显示初始化参数
- `BSP_BTN_MV_TABLE` 与按键窗口
- `BSP_I2C_*` 地址与共享总线
- `BSP_I2S_*` 音频引脚

应用层不应复制这些常量。修改后需要重新进行真机显示、按键、WiFi 和电池
验收。
