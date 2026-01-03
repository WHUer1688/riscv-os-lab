#include "dir.h"
#include "inode.h"
#include "lib/print.h"
#include "mem/str.h"

// 在目录中添加条目
int dir_add_entry(struct inode* dp, uint16 inum, const char* name) {
    if (dp->dinode.type != T_DIR) {
        panic("dir_add_entry: not a directory");
    }
    
    // 查找空闲槽位
    uint32 off = 0;
    struct dirent de;
    while (off < dp->dinode.size) {
        if (inode_read_data(dp, off, &de, sizeof(de)) != sizeof(de)) {
            break;
        }
        if (de.inum == 0) {
            // 找到空闲槽位
            de.inum = inum;
            strncpy(de.name, name, DIRSIZ);
            de.name[DIRSIZ - 1] = '\0';
            if (inode_write_data(dp, off, &de, sizeof(de)) != sizeof(de)) {
                return -1;
            }
            return 0;
        }
        off += sizeof(de);
    }
    
    // 没有空闲槽位，在末尾添加
    de.inum = inum;
    strncpy(de.name, name, DIRSIZ);
    de.name[DIRSIZ - 1] = '\0';
    if (inode_write_data(dp, off, &de, sizeof(de)) != sizeof(de)) {
        return -1;
    }
    
    return 0;
}

// 在目录中查找条目
struct inode* dir_lookup(struct inode* dp, const char* name, uint16* poff) {
    if (dp->dinode.type != T_DIR) {
        panic("dir_lookup: not a directory");
    }
    
    struct dirent de;
    uint32 off = 0;
    
    while (off < dp->dinode.size) {
        if (inode_read_data(dp, off, &de, sizeof(de)) != sizeof(de)) {
            break;
        }
        if (de.inum != 0 && strncmp(de.name, name, DIRSIZ) == 0) {
            // 找到匹配的条目
            if (poff) {
                *poff = off;
            }
            return inode_get(dp->dev, de.inum);
        }
        off += sizeof(de);
    }
    
    return NULL;
}

// 解析路径，返回父目录inode和最后一级名字
struct inode* path_to_pinode(const char* path, char* name) {
    struct inode* ip;
    
    // 从根目录开始
    if (*path == '/') {
        ip = inode_get(1, 1);  // 假设根目录inode为1
        path++;
    } else {
        // 相对路径（简化实现，从当前目录开始）
        ip = inode_get(1, 1);
    }
    
    if (ip == NULL) {
        return NULL;
    }
    
    // 跳过开头的'/'
    while (*path == '/') {
        path++;
    }
    
    // 如果路径为空或只有'/'，返回根目录
    if (*path == '\0') {
        if (name) {
            name[0] = '\0';
        }
        return ip;
    }
    
    // 解析路径的每一级
    char* p = (char*)path;
    while (*p != '\0') {
        // 查找下一个'/'
        char* next = p;
        while (*next != '/' && *next != '\0') {
            next++;
        }
        
        // 如果是最后一级
        if (*next == '\0') {
            // 提取名字
            int len = next - p;
            if (len >= DIRSIZ) {
                len = DIRSIZ - 1;
            }
            if (name) {
                strncpy(name, p, len);
                name[len] = '\0';
            }
            return ip;
        }
        
        // 中间路径，继续查找
        int len = next - p;
        if (len >= DIRSIZ) {
            len = DIRSIZ - 1;
        }
        char component[DIRSIZ];
        strncpy(component, p, len);
        component[len] = '\0';
        
        struct inode* next_ip = dir_lookup(ip, component, NULL);
        if (next_ip == NULL) {
            inode_put(ip);
            return NULL;
        }
        
        inode_put(ip);
        ip = next_ip;
        
        // 跳过'/'
        p = next + 1;
        while (*p == '/') {
            p++;
        }
    }
    
    if (name) {
        name[0] = '\0';
    }
    return ip;
}

// 解析路径，返回最终inode
struct inode* path_to_inode(const char* path) {
    struct inode* ip;
    
    // 从根目录开始
    if (*path == '/') {
        ip = inode_get(1, 1);  // 假设根目录inode为1
        path++;
    } else {
        // 相对路径（简化实现，从当前目录开始）
        ip = inode_get(1, 1);
    }
    
    if (ip == NULL) {
        return NULL;
    }
    
    // 跳过开头的'/'
    while (*path == '/') {
        path++;
    }
    
    // 如果路径为空或只有'/'，返回根目录
    if (*path == '\0') {
        return ip;
    }
    
    // 解析路径的每一级
    char* p = (char*)path;
    while (*p != '\0') {
        // 查找下一个'/'
        char* next = p;
        while (*next != '/' && *next != '\0') {
            next++;
        }
        
        // 提取组件名
        int len = next - p;
        if (len >= DIRSIZ) {
            len = DIRSIZ - 1;
        }
        char component[DIRSIZ];
        strncpy(component, p, len);
        component[len] = '\0';
        
        struct inode* next_ip = dir_lookup(ip, component, NULL);
        if (next_ip == NULL) {
            inode_put(ip);
            return NULL;
        }
        
        inode_put(ip);
        ip = next_ip;
        
        // 跳过'/'
        if (*next == '\0') {
            break;
        }
        p = next + 1;
        while (*p == '/') {
            p++;
        }
    }
    
    return ip;
}

