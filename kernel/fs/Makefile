# kernel/fs/Makefile

# 头文件
INLCUDES := ../../include

OBJS := buf.o fs.o bitmap.o inode.o dir.o file.o

.PHONY: clean build

# 定义了一些通用操作和参数
include ../../common.mk

%.o: %.c
	$(CC) $(CFLAGS) -I $(INLCUDES) -c $< -o $@

build: $(OBJS)

clean:
	rm -f $(OBJS) *.d

