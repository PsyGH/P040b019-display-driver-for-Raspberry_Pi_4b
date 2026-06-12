# P040B019 MIPI DSI Panel Driver (ST7701P)

Portable Linux kernel driver for the **P040B019-MIPI-CTP** 480×800 MIPI DSI panel with FT6336U touch, designed for Raspberry Pi 4B but portable to any SoC with a MIPI DSI controller.

## Hardware

| Parameter | Value |
|------|-----|
| Panel | P040B019 (BOE 4.0" IPS) |
| Resolution | 480×800 |
| Driver IC | Sitronix ST7701P |
| Interface | MIPI DSI 2-lane |
| Touch | FT6336U (I2C 0x38, DSI SDA/SCL) |
| Backlight | PWM (GPIO12/13) |

## Quick Install (Raspberry Pi 4B)

```bash
sudo ./scripts/install.sh
sudo reboot
```

## Requirements

- Linux kernel **≥ 4.11** (DRM panel API)
- `CONFIG_DRM_PANEL=y`, `CONFIG_DRM_MIPI_DSI=y`
- `CONFIG_TOUCHSCREEN_EDT_FT5X06=m` (touch driver)
- Kernel headers for your running kernel

On Raspberry Pi OS / Armbian:

```bash
sudo apt-get install linux-headers-$(uname -r) build-essential device-tree-compiler
```

## Build from Source

### Native build (on the target device)

```bash
cd src
make
sudo make install
```

### Cross-compile (from x86 to arm64)

```bash
cd src
make CROSS=aarch64-linux-gnu- ARCH=arm64 KDIR=/path/to/arm64-kernel-source
```

### Build against a specific kernel

```bash
cd src
make KDIR=/lib/modules/6.6.0-rpi/build
```

## Device Tree

The DT overlays configure:

| Overlay | Content |
|---------|---------|
| `p040b019-display.dts` | DSI1 panel node + PWM backlight (GPIO13) + RESET (GPIO17) |
| `st7701p-touch.dts` | FT6336U touch controller on I2C bus 10 |

### Manual DT deployment

```bash
# 1. Compile overlay
cpp -nostdinc -I /usr/src/linux-headers-$(uname -r)/include \
  overlay/p040b019-display.dts > /tmp/panel.dts
dtc -@ -I dts -O dtb -o /tmp/panel.dtbo /tmp/panel.dts

# 2. Merge into base DTB (RPi firmware rejects custom overlays)
cd /boot/firmware
sudo cp bcm2711-rpi-4-b.dtb bcm2711-rpi-4-b.dtb.orig
sudo fdtoverlay -i bcm2711-rpi-4-b.dtb.orig -o bcm2711-rpi-4-b.dtb \
  /tmp/panel.dtbo
```

## Porting to Other SoCs

The driver source is **SoC-agnostic** — it uses standard DRM/MIPI panel APIs. To port:

1. **Modify the DT overlay** — change `target = <&dsi1>` to your SoC's DSI node
2. **Adjust GPIO/regulator references** in the overlay
3. The C driver requires **zero code changes**

Example for Rockchip RK3588:

```dts
fragment@2 {
    target = <&dsi>;  // was &dsi1 on BCM2711
    __overlay__ { ... };
};
```

## Kernel Config Checklist

```ini
CONFIG_DRM=y
CONFIG_DRM_PANEL=y
CONFIG_DRM_MIPI_DSI=y
CONFIG_PWM=y
CONFIG_DRM_VC4=y              # RPi4 only
CONFIG_TOUCHSCREEN_EDT_FT5X06=m
```

## API Compatibility

| Kernel API | Min Version | Stability |
|-----------|-------------|-----------|
| `drm_panel_init/add/remove` | 4.11 | Stable |
| `mipi_dsi_dcs_write_buffer` | 4.2 | Stable |
| `devm_gpiod_get_optional` | 4.5 | Stable |
| `gpiod_set_value` | 4.2 | Stable |

All APIs used are mature — minor kernel version bumps (6.6→6.12) require **no code changes**.

## Troubleshooting

```
# Check if panel loaded
dmesg | grep -i "rpi_dsi_display\|p040b019"

# Check DRM connector status
cat /sys/class/drm/card*-DSI-1/status

# Check backlight
ls /sys/class/backlight/
cat /sys/class/backlight/backlight/max_brightness

# Check touch
sudo i2cdetect -y 10        # Look for UU at 0x38
sudo evtest                  # Select EP0110M09, touch screen
```

## Files

```
p040b019-display-driver/
├── README.md
├── src/
│   ├── Makefile                       # Portable kernel module build
│   └── panel-rpi-dsi-display.c        # Panel driver (480×800, ST7701P, 2-lane)
├── overlay/
│   ├── p040b019-display.dts           # Display + backlight
│   └── st7701p-touch.dts             # FT6336U touch
└── scripts/
    ├── install.sh                     # One-command installer
    └── uninstall.sh                   # Clean removal
```

## License

GPL-2.0-only — same as Linux kernel.
