#!/usr/bin/perl -w

# 生成用户态系统调用桩代码

sub entry {
    my $name = shift;
    print ".global $name\n";
    print "${name}:\n";
    print " li a7, SYS_${name}\n";
    print " ecall\n";
    print " ret\n";
}

print "#include \"syscall.h\"\n\n";

entry("fork");
entry("exit");
entry("wait");
entry("kill");
entry("getpid");
entry("open");
entry("close");
entry("read");
entry("write");
entry("sbrk");

