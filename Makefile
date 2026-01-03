include common.mk

KERN = kernel
KERNEL_ELF = kernel-qemu
CPUNUM = 3
FS_IMG = fs.img
INTERVAL ?= 1000000

.PHONY: clean $(KERN)

$(KERN):
	$(MAKE) build INTERVAL=$(INTERVAL) --directory=$@

# QEMU相关配置
QEMU     =  qemu-system-riscv64
QEMUOPTS =  -machine virt -bios none -kernel $(KERNEL_ELF) 
QEMUOPTS += -m 128M -smp $(CPUNUM) -nographic
QEMUOPTS += -drive file=$(FS_IMG),if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

# 调试
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

build: $(KERN)

# 创建空的文件系统映像（用于测试）
$(FS_IMG):
	dd if=/dev/zero of=$(FS_IMG) bs=1M count=10 2>/dev/null || \
	dd if=/dev/zero of=$(FS_IMG) bs=1024 count=10240 2>/dev/null || \
	touch $(FS_IMG) && truncate -s 10M $(FS_IMG) 2>/dev/null || \
	(echo "Warning: Could not create fs.img, creating empty file" && touch $(FS_IMG))

# qemu运行
qemu: $(KERN) $(FS_IMG)
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $(KERN) .gdbinit
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

clean:
	$(MAKE) --directory=$(KERN) clean
	rm -f $(KERNEL_ELF) .gdbinit $(FS_IMG)