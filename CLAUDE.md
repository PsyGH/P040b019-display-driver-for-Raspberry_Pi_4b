# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
cd src && make                        # Build for running kernel
make KDIR=/path/to/kernel             # Build against specific kernel source
make CROSS=aarch64-linux-gnu- ARCH=arm64 KDIR=...  # Cross-compile
make install                          # Install .ko for current kernel
make clean                            # Kernel module clean
```

Full install on RPi4: `sudo ./scripts/install.sh` (builds module, deploys DT overlays, merges DTB, configures auto-load). Remove: `sudo ./scripts/uninstall.sh`.

**Prerequisites:** `linux-headers-$(uname -r) build-essential device-tree-compiler`  
**Kernel config:** `CONFIG_DRM_PANEL=y`, `CONFIG_DRM_MIPI_DSI=y`, `CONFIG_DRM_VC4=y`, `CONFIG_TOUCHSCREEN_EDT_FT5X06=m`

## Troubleshooting

```bash
dmesg | grep -i "rpi_dsi_display\|p040b019"
cat /sys/class/drm/card*-DSI-1/status
sudo i2cdetect -y 10        # Look for UU at 0x38 (touch)
sudo evtest                  # Test touch input
```

## Code Architecture

### Multi-Panel Descriptor Pattern

The single `panel-rpi-dsi-display.ko` module supports three panels via descriptors, selected by DT compatible string:

| Panel | Resolution | Lanes | DTS compatible |
|-------|-----------|-------|----------------|
| P040B019 (ST7701P) | 480×800 | 2 | `boe,p040b019` |
| w280bf036i | 480×640 | 1 | `wlk,w280bf036i` |
| tdo_qhd0500d5 | 540×960 | 2 | `truly,tdo-qhd0500d5` |

Adding a new panel means adding one `drm_display_mode`, one `power_on_timing`, one init sequence function, and one `rpi_dsi_display_desc` — no structural code changes.

### Probe flow

`rpi_dsi_display_probe()` → reads `of_device_get_match_data` for descriptor → acquires reset GPIO → registers DRM panel → tries DT backlight (pwm-backlight) first, falls back to DCS-based backlight → attaches MIPI DSI device.

### Panel lifecycle (DRM callbacks)

```
prepare → (HW reset → soft reset → init sequence → sleep out → TE on)
enable  → display on
disable → display off
unprepare → sleep in → de-assert reset
```

### Key structures

- `rpi_dsi_display_desc` — static panel metadata (mode, lanes, flags, format, init sequence, power timing)
- `rpi_dsi_display` — per-instance state (panel, dsi device, reset gpio, orientation)
- `power_on_timing` — reset/sleep pulse durations in ms

### File layout

```
src/panel-rpi-dsi-display.c   # Kernel module — all panel logic
src/Makefile                   # Kernel module build, supports cross-compile
overlay/p040b019-display.dts   # DT overlay: DSI1 + PWM backlight (GPIO13) + RESET (GPIO17)
overlay/st7701p-touch.dts      # DT overlay: FT6336U touch on I2C BSC0 (GPIO44/45)
scripts/install.sh             # Automated build + DT merge + install
scripts/uninstall.sh           # Remove module, restore original DTB
```

### Porting to other SoCs

The C driver uses standard DRM/MIPI panel APIs only — **no code changes needed**. Port via DT overlay: change `target = <&dsi1>` to the target SoC's DSI controller node, adjust GPIO/regulator references. Tested on BCM2711 (RPi4), designed for any SoC with MIPI DSI.
