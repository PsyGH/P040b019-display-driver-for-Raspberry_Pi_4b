#!/usr/bin/env bash
# P040B019 MIPI DSI + FT6336U Touch driver installer
# Works on Raspberry Pi 4B with any Linux >= 4.11
# Usage: sudo ./install.sh
set -e

cd "$(dirname "$0")/.."
PROJECT_DIR="$(pwd)"

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "${CYAN}==>${NC} $*"; }
ok()    { echo -e "${GREEN}  ✓${NC} $*"; }
err()   { echo -e "${RED}  ✗${NC} $*"; exit 1; }

[ "$(id -u)" -eq 0 ] || err "need root — run: sudo ./install.sh"

# ── 1. Build kernel module ──
info "1/4 Building kernel module"
if [ ! -d /lib/modules/$(uname -r)/build ]; then
  info "  Installing kernel headers..."
  apt-get update -qq && apt-get install -y linux-headers-$(uname -r) build-essential
fi
cd "$PROJECT_DIR/src"
make clean 2>/dev/null || true
make || err "Build failed, check kernel version compatibility"
ok "panel-rpi-dsi-display.ko"

# ── 2. Install module ──
info "2/4 Installing module"
MODDIR=/lib/modules/$(uname -r)/kernel/drivers/gpu/drm/panel
mkdir -p "$MODDIR"
cp panel-rpi-dsi-display.ko "$MODDIR/"
depmod -a
# Auto-load on boot
echo "panel-rpi-dsi-display" > /etc/modules-load.d/p040b019.conf
ok "Module installed and configured for auto-load on boot"

# ── 3. Deploy DT overlays ──
info "3/4 Deploying device tree"
KINC=$(find /lib/modules/$(uname -r)/build/include -maxdepth 0 2>/dev/null || echo "")
[ -z "$KINC" ] && KINC="/usr/src/linux-headers-$(uname -r)/include"
cd "$PROJECT_DIR/overlay"

# Compile DTBOs
for name in p040b019-display st7701p-touch; do
  cpp -nostdinc -undef -x assembler-with-cpp -I "$KINC" \
    "${name}.dts" > "/tmp/${name}.dts.preprocessed"
  dtc -@ -I dts -O dtb -o "/tmp/${name}.dtbo" "/tmp/${name}.dts.preprocessed"
  ok "Compiled ${name}.dtbo"
done

# Merge into base DTB
FIRMWARE_DIR=""
[ -d /boot/firmware ] && FIRMWARE_DIR=/boot/firmware
[ -d /boot ] && [ ! -d /boot/firmware ] && FIRMWARE_DIR=/boot
DTB="bcm2711-rpi-4-b.dtb"

if [ -f "$FIRMWARE_DIR/$DTB" ]; then
  # Backup original
  if [ ! -f "$FIRMWARE_DIR/${DTB}.orig" ]; then
    cp "$FIRMWARE_DIR/$DTB" "$FIRMWARE_DIR/${DTB}.orig"
    ok "Backed up original DTB → ${DTB}.orig"
  fi
  # Merge overlays
  fdtoverlay -i "$FIRMWARE_DIR/${DTB}.orig" -o "$FIRMWARE_DIR/$DTB" \
    /tmp/p040b019-display.dtbo /tmp/st7701p-touch.dtbo
  ok "DT overlay merged into ${DTB}"
else
  info "${DTB} not found, DTBOs compiled to /tmp/ — merge manually"
fi

# ── 4. Verify ──
info "4/4 Verifying"
if lsmod | grep -q panel_rpi_dsi_display; then
  ok "Module loaded"
else
  modprobe panel-rpi-dsi-display 2>/dev/null && ok "Module loaded successfully" || info "Module will load after reboot"
fi

echo ""
echo -e "${GREEN}Installation complete!${NC}"
echo "  Reboot to activate DTB: sudo reboot"
echo "  Check display: dmesg | grep rpi_dsi"
echo "  Set brightness: echo 128 > /sys/class/backlight/backlight/brightness"
