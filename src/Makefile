# P040B019 ST7701P MIPI DSI Panel Driver
# Portable kernel module build — works on any kernel >= 4.11
#
# Usage:
#   make                           Build .ko for running kernel
#   make KDIR=/path/to/kernel      Build against specific kernel source
#   make CROSS=aarch64-linux-gnu-  Cross-compile
#   make install                   Install module for current kernel
#   make clean

MODULE  := panel-rpi-dsi-display
obj-m   += $(MODULE).o

# Kernel build directory (default: running kernel)
KDIR    ?= /lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd)

# Cross-compiler prefix (optional)
ifdef CROSS
  CROSS_COMPILE := $(CROSS)
  ARCH          ?= arm64
endif

all: modules

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

install: modules_install

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.* *.symvers *.order Module*

.PHONY: all modules modules_install install clean
