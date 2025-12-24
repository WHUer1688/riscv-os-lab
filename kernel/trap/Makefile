# kernel/trap/Makefile

# 头文件
INLCUDES := ../../include

OBJS := trap.o trap_kernel.o trampoline.o trap_user.o

.PHONY: clean build

# 定义了一些通用操作和参数
include ../../common.mk

%.o: %.S
	$(CC) $(CFLAGS) -I $(INLCUDES) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -I $(INLCUDES) -c $< -o $@

build: $(OBJS)

clean:
	rm -f $(OBJS) *.d
