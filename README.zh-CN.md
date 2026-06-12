# P040B019 MIPI DSI 面板驱动 (ST7701P)

基于 **P040B019-MIPI-CTP** 480×800 MIPI DSI 显示屏的便携式 Linux 内核驱动，含 FT6336U 触摸，适配 Raspberry Pi 4B，可移植至任何带 MIPI DSI 控制器的 SoC。

## 硬件参数

| 参数 | 值 |
|------|-----|
| 面板 | P040B019 (BOE 4.0" IPS) |
| 分辨率 | 480×800 |
| 驱动 IC | Sitronix ST7701P |
| 接口 | MIPI DSI 2-lane |
| 触摸 | FT6336U (I2C 0x38, DSI SDA/SCL) |
| 背光 | PWM (GPIO12/13) |

## 快速安装 (Raspberry Pi 4B)

```bash
sudo ./scripts/install.sh
sudo reboot
```

## 系统要求

- Linux 内核 **≥ 4.11** (DRM panel API)
- `CONFIG_DRM_PANEL=y`, `CONFIG_DRM_MIPI_DSI=y`
- `CONFIG_TOUCHSCREEN_EDT_FT5X06=m` (触摸驱动)
- 当前运行内核对应的内核头文件

Raspberry Pi OS / Armbian 上安装依赖：

```bash
sudo apt-get install linux-headers-$(uname -r) build-essential device-tree-compiler
```

## 编译方法

### 目标设备上本地编译

```bash
cd src
make
sudo make install
```

### 交叉编译 (x86 → arm64)

```bash
cd src
make CROSS=aarch64-linux-gnu- ARCH=arm64 KDIR=/path/to/arm64-kernel-source
```

### 指定内核版本编译

```bash
cd src
make KDIR=/lib/modules/6.6.0-rpi/build
```

## 设备树

DT overlay 配置内容：

| Overlay | 内容 |
|---------|------|
| `p040b019-display.dts` | DSI1 面板节点 + PWM 背光 (GPIO13) + RESET (GPIO17) |
| `st7701p-touch.dts` | I2C 总线 10 上的 FT6336U 触摸控制器 |

### 手动部署 DT

```bash
# 1. 编译 overlay
cpp -nostdinc -I /usr/src/linux-headers-$(uname -r)/include \
  overlay/p040b019-display.dts > /tmp/panel.dts
dtc -@ -I dts -O dtb -o /tmp/panel.dtbo /tmp/panel.dts

# 2. 合并到基础 DTB（RPi 固件拒绝自定义 overlay）
cd /boot/firmware
sudo cp bcm2711-rpi-4-b.dtb bcm2711-rpi-4-b.dtb.orig
sudo fdtoverlay -i bcm2711-rpi-4-b.dtb.orig -o bcm2711-rpi-4-b.dtb \
  /tmp/panel.dtbo
```

## 移植到其他 SoC

驱动源代码是 **SoC 无关的** — 使用标准 DRM/MIPI panel API。移植步骤：

1. **修改 DT overlay** — 将 `target = <&dsi1>` 改为目标 SoC 的 DSI 节点
2. **调整 overlay 中的 GPIO/稳压器引用**
3. **C 驱动无需任何代码修改**

Rockchip RK3588 示例：

```dts
fragment@2 {
    target = <&dsi>;  // 原来是 &dsi1 (BCM2711)
    __overlay__ { ... };
};
```

## 内核配置清单

```ini
CONFIG_DRM=y
CONFIG_DRM_PANEL=y
CONFIG_DRM_MIPI_DSI=y
CONFIG_PWM=y
CONFIG_DRM_VC4=y              # 仅 RPi4
CONFIG_TOUCHSCREEN_EDT_FT5X06=m
```

## API 兼容性

| 内核 API | 最低版本 | 稳定性 |
|----------|---------|--------|
| `drm_panel_init/add/remove` | 4.11 | 稳定 |
| `mipi_dsi_dcs_write_buffer` | 4.2 | 稳定 |
| `devm_gpiod_get_optional` | 4.5 | 稳定 |
| `gpiod_set_value` | 4.2 | 稳定 |

所有 API 均已成熟 — 小版本内核升级（6.6→6.12）**无需代码修改**。

## 故障排查

```
# 检查面板是否加载
dmesg | grep -i "rpi_dsi_display\|p040b019"

# 检查 DRM 连接器状态
cat /sys/class/drm/card*-DSI-1/status

# 检查背光
ls /sys/class/backlight/
cat /sys/class/backlight/backlight/max_brightness

# 检查触摸
sudo i2cdetect -y 10        # 查找 0x38 处显示 UU
sudo evtest                  # 选择 EP0110M09，触摸测试
```

## 文件结构

```
p040b019-display-driver/
├── README.md                # 英文文档
├── README.zh-CN.md          # 中文文档
├── CLAUDE.md                # Claude Code 辅助文件
├── src/
│   ├── Makefile             # 内核模块编译（支持交叉编译）
│   └── panel-rpi-dsi-display.c   # 面板驱动 (480×800, ST7701P, 2-lane)
├── overlay/
│   ├── p040b019-display.dts # 显示 + 背光
│   └── st7701p-touch.dts    # FT6336U 触摸
└── scripts/
    ├── install.sh           # 一键安装
    └── uninstall.sh         # 完整卸载
```

## 开源协议

GPL-2.0-only — 与 Linux 内核相同。
