#!/usr/bin/env bash
# Uninstall P040B019 display driver
set -e

echo "==> 卸载 panel-rpi-dsi-display"

rm -f /lib/modules/$(uname -r)/kernel/drivers/gpu/drm/panel/panel-rpi-dsi-display.ko
rm -f /etc/modules-load.d/p040b019.conf
depmod -a

# Restore original DTB
for d in /boot/firmware /boot; do
  if [ -f "$d/bcm2711-rpi-4-b.dtb.orig" ]; then
    cp "$d/bcm2711-rpi-4-b.dtb.orig" "$d/bcm2711-rpi-4-b.dtb"
    echo "  ✓ 恢复原始 DTB"
    break
  fi
done

echo "卸载完成。重启生效: sudo reboot"
