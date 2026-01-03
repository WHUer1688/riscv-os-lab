#include "bitmap.h"
#include "buf.h"
#include "fs.h"
#include "lib/print.h"

// 获取bitmap块号（数据bitmap）
static uint32 bmap_block(void) {
    return sb.bmapstart;
}

// 获取bitmap块号（inode bitmap）
static uint32 imap_block(void) {
    return sb.bmapstart - 1;  // inode bitmap在数据bitmap之前
}

// 在bitmap中查找第一个空闲位
static uint32 bitmap_alloc(uint32 bitmap_blockno) {
    struct buf *b = buf_read(bitmap_blockno);
    if (b == NULL) {
        panic("bitmap_alloc: failed to read bitmap");
    }
    
    uint8 *bits = b->data;
    // 每个block有1024字节 = 8192位
    for (uint32 i = 0; i < 1024; i++) {
        if (bits[i] != 0xFF) {  // 这个字节不是全1
            // 在这个字节中查找第一个0位
            for (int j = 0; j < 8; j++) {
                if ((bits[i] & (1 << j)) == 0) {
                    // 找到空闲位
                    bits[i] |= (1 << j);
                    buf_write(b);  // 标记为dirty
                    buf_release(b);
                    return i * 8 + j;
                }
            }
        }
    }
    
    buf_release(b);
    return 0;  // 没有空闲位
}

// 在bitmap中释放位
static void bitmap_free(uint32 bitmap_blockno, uint32 bit) {
    struct buf *b = buf_read(bitmap_blockno);
    if (b == NULL) {
        panic("bitmap_free: failed to read bitmap");
    }
    
    uint8 *bits = b->data;
    uint32 byte = bit / 8;
    uint32 offset = bit % 8;
    
    if (byte >= 1024) {
        panic("bitmap_free: bit out of range");
    }
    
    // 清除位
    bits[byte] &= ~(1 << offset);
    buf_write(b);  // 标记为dirty
    buf_release(b);
}

// 数据块分配
uint32 balloc(void) {
    uint32 bitmap_blockno = bmap_block();
    uint32 bit = bitmap_alloc(bitmap_blockno);
    
    if (bit == 0 && (bitmap_blockno == bmap_block())) {
        // 检查是否真的分配成功（bit=0可能是第一个块，也可能是失败）
        // 简化处理：如果bit=0，检查是否已分配
        struct buf *b = buf_read(bitmap_blockno);
        if (b && (b->data[0] & 1) == 0) {
            // 第一个位确实是空闲的，已经分配了
            b->data[0] |= 1;
            buf_write(b);
            buf_release(b);
            return sb.datastart + bit;
        }
        if (b) buf_release(b);
    }
    
    if (bit >= sb.nblocks) {
        panic("balloc: bit out of range");
    }
    
    return sb.datastart + bit;
}

// 数据块释放
void bfree(uint32 blockno) {
    if (blockno < sb.datastart || blockno >= sb.datastart + sb.nblocks) {
        panic("bfree: blockno out of range");
    }
    
    uint32 bit = blockno - sb.datastart;
    uint32 bitmap_blockno = bmap_block();
    bitmap_free(bitmap_blockno, bit);
}

// inode分配
uint32 ialloc(void) {
    uint32 bitmap_blockno = imap_block();
    uint32 bit = bitmap_alloc(bitmap_blockno);
    
    if (bit == 0 && (bitmap_blockno == imap_block())) {
        // 检查是否真的分配成功
        struct buf *b = buf_read(bitmap_blockno);
        if (b && (b->data[0] & 1) == 0) {
            // 第一个位确实是空闲的，已经分配了
            b->data[0] |= 1;
            buf_write(b);
            buf_release(b);
            return bit + 1;  // inode编号从1开始
        }
        if (b) buf_release(b);
    }
    
    if (bit >= sb.ninodes) {
        panic("ialloc: bit out of range");
    }
    
    return bit + 1;  // inode编号从1开始（0表示无效）
}

// inode释放
void ifree(uint32 inum) {
    if (inum < 1 || inum > sb.ninodes) {
        panic("ifree: inum out of range");
    }
    
    uint32 bit = inum - 1;  // inode编号从1开始，bit从0开始
    uint32 bitmap_blockno = imap_block();
    bitmap_free(bitmap_blockno, bit);
}

