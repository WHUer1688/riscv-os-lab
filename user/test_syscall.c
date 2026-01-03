#include "kernel/types.h"
#include "user/user.h"

int main(void)
{
    printf("pid=%d\n", getpid());
    
    // 测试 getpid
    int pid = getpid();
    if(pid >= 0) {
        printf("getpid test passed: pid=%d\n", pid);
    } else {
        printf("getpid test failed\n");
    }
    
    exit(0);
}

