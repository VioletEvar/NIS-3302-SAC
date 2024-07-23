#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <linux/module.h>       // 内核模块的基本定义和操作
#include <linux/kernel.h>       // 内核信息输出
#include <linux/init.h>         // 模块初始化和清理宏
#include <linux/syscalls.h>     // 系统调用表和相关定义
#include <linux/file.h>         // 文件相关操作
#include <linux/fs.h>           // 文件系统操作
#include <linux/string.h>       // 字符串操作函数
#include <linux/mm.h>           // 内存管理
#include <linux/sched.h>        // 进程调度
#include <linux/unistd.h>       // 系统调用号定义
#include <asm/pgtable.h>        // 页表操作
#include <asm/uaccess.h>        // 用户空间和内核空间数据访问
#include <asm/ptrace.h>         // 进程追踪
#include <linux/kprobes.h>      // 内核探针
#include <linux/reboot.h>       // 系统重启相关定义

/*
** module macros
*/
// 模块许可证
MODULE_LICENSE("GPL"); // 许可证声明，表明模块是GPL兼容的
// 模块作者
MODULE_AUTHOR("infosec-sjtu"); // 作者信息
// 模块描述
MODULE_DESCRIPTION("hook sys_call_table"); // 模块功能描述

#define MAX_LENGTH 256 // 定义最大长度为256，用于缓冲区

// 定义各种系统调用函数指针类型
typedef void (* demo_sys_call_ptr_t)(void);
typedef asmlinkage long (*orig_open_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_openat_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_unlink_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_unlinkat_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_execve_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_execveat_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_reboot_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_mount_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_umount2_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_insmod_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_rmmod_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_socket_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_connect_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_accept_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_sendto_t)(struct pt_regs *regs);
typedef asmlinkage long (*orig_recvfrom_t)(struct pt_regs *regs);
// typedef asmlinkage long (*orig_close_t)(struct pt_regs *regs); // 可选系统调用

// 声明系统调用表获取函数
demo_sys_call_ptr_t *get_syscall_table(void);

// 声明审计函数
int AuditOpen(const char *pathname, int flags, int ret);
int AuditOpenat(int dfd, const char *pathname, int flags, int ret);
int AuditUnlink(const char *pathname, int ret);
int AuditUnlinkat(int dfd, const char *pathname, int ret);
int AuditExec(const char *pathname, int ret);
int AuditExecat(int dfd, const char *pathname, int flags, int ret);
int AuditReboot(const char *message, int ret);
int AuditShutdown(const char *message, int ret);
int AuditMount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data, int ret);
int AuditUnmount2(const char *target, int flags, int ret);
int AuditInsmod(const char *pathname, int ret);
int AuditRmmod(const char *module_name, int ret);
int AuditSocket(int domain, int type, int protocol);
int AuditConnect(int sockfd, struct sockaddr *addr, int addrlen);
int AuditAccept(int sockfd, struct sockaddr *addr, int *addrlen);
int AuditSendto(int sockfd, void *buf, size_t len, int flags, struct sockaddr *dest_addr, int addrlen);
int AuditRecvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, int *addrlen);
// int AuditClose(int fd); // 可选审计函数

// 函数声明：netlink 初始化和释放
void netlink_release(void);
void netlink_init(void);

// 系统调用函数指针初始化
demo_sys_call_ptr_t *sys_call_table = NULL;
orig_open_t orig_open = NULL;
orig_openat_t orig_openat = NULL;
orig_unlink_t orig_unlink = NULL;
orig_unlinkat_t orig_unlinkat = NULL;
orig_execve_t orig_execve = NULL;
orig_execveat_t orig_execveat = NULL;
orig_reboot_t orig_reboot = NULL;
orig_mount_t orig_mount = NULL;
orig_umount2_t orig_umount2 = NULL;
orig_insmod_t orig_insmod = NULL;
orig_rmmod_t orig_rmmod = NULL;
orig_socket_t orig_socket = NULL;
orig_connect_t orig_connect = NULL;
orig_accept_t orig_accept = NULL;
orig_sendto_t orig_sendto = NULL;
orig_recvfrom_t orig_recvfrom = NULL;
// orig_close_t orig_close = NULL; // 可选

unsigned int level; // 页表级别
pte_t *pte; // 页表条目指针

/*
** Hooked system calls
*/

// 钩取的 sys_open 系统调用
asmlinkage long hooked_sys_open(struct pt_regs *regs)
{
    char pathname[MAX_LENGTH]; // 存储路径名
    int flags; // 存储文件打开标志
    int nbytes; // 存储从用户空间读取的字节数
    int ret; // 存储系统调用返回值

    memset(pathname, 0, MAX_LENGTH); // 初始化路径名缓冲区
    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->di), MAX_LENGTH); // 从用户空间复制路径名
    flags = regs->si; // 获取文件打开标志

    ret = orig_open(regs); // 调用原始 sys_open 系统调用

    AuditOpen(pathname, flags, ret); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_openat 系统调用
asmlinkage long hooked_sys_openat(struct pt_regs *regs)
{
    int dfd; // 描述符
    char pathname[MAX_LENGTH]; // 存储路径名
    int flags; // 存储文件打开标志
    int nbytes; // 存储从用户空间读取的字节数
    int ret; // 存储系统调用返回值

    memset(pathname, 0, MAX_LENGTH); // 初始化路径名缓冲区
    dfd = regs->di; // 获取描述符

    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->si), MAX_LENGTH); // 从用户空间复制路径名
    flags = regs->dx; // 获取文件打开标志

    ret = orig_openat(regs); // 调用原始 sys_openat 系统调用

    AuditOpenat(dfd, pathname, flags, ret); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_unlink 系统调用
asmlinkage long hooked_sys_unlink(struct pt_regs *regs)
{
    char pathname[MAX_LENGTH]; // 存储路径名
    int nbytes; // 存储从用户空间读取的字节数
    int ret; // 存储系统调用返回值

    memset(pathname, 0, MAX_LENGTH); // 初始化路径名缓冲区
    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->di), MAX_LENGTH); // 从用户空间复制路径名

    ret = orig_unlink(regs); // 调用原始 sys_unlink 系统调用

    AuditUnlink(pathname, ret); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_unlinkat 系统调用
asmlinkage long hooked_sys_unlinkat(struct pt_regs *regs)
{
    int dfd; // 描述符
    char pathname[MAX_LENGTH]; // 存储路径名
    int nbytes; // 存储从用户空间读取的字节数
    int ret; // 存储系统调用返回值

    memset(pathname, 0, MAX_LENGTH); // 初始化路径名缓冲区
    dfd = regs->di; // 获取描述符

    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->si), MAX_LENGTH); // 从用户空间复制路径名

    ret = orig_unlinkat(regs); // 调用原始 sys_unlinkat 系统调用

    AuditUnlinkat(dfd, pathname, ret); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_execve 系统调用
asmlinkage long hooked_sys_execve(struct pt_regs *regs)
{
    char pathname[MAX_LENGTH]; // 存储路径名
    int nbytes; // 存储从用户空间读取的字节数
    int ret; // 存储系统调用返回值

    memset(pathname, 0, MAX_LENGTH); // 初始化路径名缓冲区
    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->di), MAX_LENGTH); // 从用户空间复制路径名

    ret = orig_execve(regs); // 调用原始 sys_execve 系统调用

    AuditExec(pathname, ret); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_execveat 系统调用
asmlinkage long hooked_sys_execveat(struct pt_regs *regs)
{
    int dfd; // 描述符
    char pathname[MAX_LENGTH]; // 存储路径名
    int flags; // 存储文件打开标志
    int nbytes; // 存储从用户空间读取的字节数
    int ret; // 存储系统调用返回值

    memset(pathname, 0, MAX_LENGTH); // 初始化路径名缓冲区
    dfd = regs->di; // 获取描述符

    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->si), MAX_LENGTH); // 从用户空间复制路径名
    flags = regs->dx; // 获取文件打开标志

    ret = orig_execveat(regs); // 调用原始 sys_execveat 系统调用

    AuditExecat(dfd, pathname, flags, ret); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_reboot 系统调用
asmlinkage long hooked_sys_reboot(struct pt_regs *regs)
{
    char message[MAX_LENGTH]; // 存储重启消息
    int nbytes; // 存储从用户空间读取的字节数
    int ret; // 存储系统调用返回值

    memset(message, 0, MAX_LENGTH); // 初始化消息缓冲区
    nbytes = strncpy_from_user(message, (const char __user *)(regs->di), MAX_LENGTH); // 从用户空间复制消息

    ret = orig_reboot(regs); // 调用原始 sys_reboot 系统调用

    AuditReboot(message, ret); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_mount 系统调用
asmlinkage long hooked_sys_mount(struct pt_regs *regs)
{
    char source[MAX_LENGTH]; // 存储源
    char target[MAX_LENGTH]; // 存储目标
    char filesystemtype[MAX_LENGTH]; // 存储文件系统类型
    int nbytes; // 存储从用户空间读取的字节数
    unsigned long mountflags; // 存储挂载标志
    const void *data; // 存储数据
    int ret; // 存储系统调用返回值

    memset(source, 0, MAX_LENGTH); // 初始化源缓冲区
    memset(target, 0, MAX_LENGTH); // 初始化目标缓冲区
    memset(filesystemtype, 0, MAX_LENGTH); // 初始化文件系统类型缓冲区

    nbytes = strncpy_from_user(source, (const char __user *)(regs->di), MAX_LENGTH); // 从用户空间复制源
    nbytes = strncpy_from_user(target, (const char __user *)(regs->si), MAX_LENGTH); // 从用户空间复制目标
    nbytes = strncpy_from_user(filesystemtype, (const char __user *)(regs->dx), MAX_LENGTH); // 从用户空间复制文件系统类型
    mountflags = regs->r10; // 获取挂载标志
    data = (const void __user *)(regs->r8); // 获取数据指针

    ret = orig_mount(regs); // 调用原始 sys_mount 系统调用

    AuditMount(source, target, filesystemtype, mountflags, data, ret); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_umount2 系统调用
asmlinkage long hooked_sys_umount2(struct pt_regs *regs)
{
    char target[MAX_LENGTH]; // 存储目标
    int flags; // 存储标志
    int nbytes; // 存储从用户空间读取的字节数
    int ret; // 存储系统调用返回值

    memset(target, 0, MAX_LENGTH); // 初始化目标缓冲区
    nbytes = strncpy_from_user(target, (const char __user *)(regs->di), MAX_LENGTH); // 从用户空间复制目标
    flags = regs->si; // 获取标志

    ret = orig_umount2(regs); // 调用原始 sys_umount2 系统调用

    AuditUnmount2(target, flags, ret); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_insmod 系统调用
asmlinkage long hooked_sys_insmod(struct pt_regs *regs)
{
    char pathname[MAX_LENGTH]; // 存储路径名
    int nbytes; // 存储从用户空间读取的字节数
    int ret; // 存储系统调用返回值

    memset(pathname, 0, MAX_LENGTH); // 初始化路径名缓冲区
    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->di), MAX_LENGTH); // 从用户空间复制路径名

    ret = orig_insmod(regs); // 调用原始 sys_insmod 系统调用

    AuditInsmod(pathname, ret); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_rmmod 系统调用
asmlinkage long hooked_sys_rmmod(struct pt_regs *regs)
{
    char module_name[MAX_LENGTH]; // 存储模块名
    int nbytes; // 存储从用户空间读取的字节数
    int ret; // 存储系统调用返回值

    memset(module_name, 0, MAX_LENGTH); // 初始化模块名缓冲区
    nbytes = strncpy_from_user(module_name, (const char __user *)(regs->di), MAX_LENGTH); // 从用户空间复制模块名

    ret = orig_rmmod(regs); // 调用原始 sys_rmmod 系统调用

    AuditRmmod(module_name, ret); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_socket 系统调用
asmlinkage long hooked_sys_socket(struct pt_regs *regs)
{
    int domain; // 存储域
    int type; // 存储类型
    int protocol; // 存储协议
    int ret; // 存储系统调用返回值

    domain = regs->di; // 获取域
    type = regs->si; // 获取类型
    protocol = regs->dx; // 获取协议

    ret = orig_socket(regs); // 调用原始 sys_socket 系统调用

    AuditSocket(domain, type, protocol); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_connect 系统调用
asmlinkage long hooked_sys_connect(struct pt_regs *regs)
{
    int sockfd; // 套接字描述符
    struct sockaddr addr; // 地址结构
    int addrlen; // 地址长度
    int ret; // 存储系统调用返回值

    sockfd = regs->di; // 获取套接字描述符
    addrlen = regs->dx; // 获取地址长度

    if (copy_from_user(&addr, (void __user *)regs->si, addrlen)) { // 从用户空间复制地址
        return -EFAULT; // 复制失败，返回错误
    }

    ret = orig_connect(regs); // 调用原始 sys_connect 系统调用

    AuditConnect(sockfd, &addr, addrlen); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_accept 系统调用
asmlinkage long hooked_sys_accept(struct pt_regs *regs)
{
    int sockfd; // 套接字描述符
    struct sockaddr addr; // 地址结构
    int addrlen; // 地址长度
    int ret; // 存储系统调用返回值

    sockfd = regs->di; // 获取套接字描述符
    addrlen = regs->dx; // 获取地址长度

    if (copy_from_user(&addr, (void __user *)regs->si, addrlen)) { // 从用户空间复制地址
        return -EFAULT; // 复制失败，返回错误
    }

    ret = orig_accept(regs); // 调用原始 sys_accept 系统调用

    AuditAccept(sockfd, &addr, &addrlen); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_sendto 系统调用
asmlinkage long hooked_sys_sendto(struct pt_regs *regs)
{
    int sockfd; // 套接字描述符
    void *buf; // 缓冲区
    size_t len; // 长度
    int flags; // 标志
    struct sockaddr addr; // 地址结构
    int addrlen; // 地址长度
    int ret; // 存储系统调用返回值

    sockfd = regs->di; // 获取套接字描述符
    buf = (void *)regs->si; // 获取缓冲区
    len = regs->dx; // 获取长度
    flags = regs->r10; // 获取标志
    addr = *(struct sockaddr *)(regs->r8); // 获取地址结构
    addrlen = regs->r9; // 获取地址长度

    ret = orig_sendto(regs); // 调用原始 sys_sendto 系统调用

    AuditSendto(sockfd, buf, len, flags, &addr, addrlen); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 钩取的 sys_recvfrom 系统调用
asmlinkage long hooked_sys_recvfrom(struct pt_regs *regs)
{
    int sockfd; // 套接字描述符
    void *buf; // 缓冲区
    size_t len; // 长度
    int flags; // 标志
    struct sockaddr addr; // 地址结构
    int *addrlen; // 地址长度指针
    int ret; // 存储系统调用返回值

    sockfd = regs->di; // 获取套接字描述符
    buf = (void *)regs->si; // 获取缓冲区
    len = regs->dx; // 获取长度
    flags = regs->r10; // 获取标志
    addr = *(struct sockaddr *)(regs->r8); // 获取地址结构
    addrlen = (int *)regs->r9; // 获取地址长度指针

    ret = orig_recvfrom(regs); // 调用原始 sys_recvfrom 系统调用

    AuditRecvfrom(sockfd, buf, len, flags, &addr, addrlen); // 调用审计函数

    return ret; // 返回系统调用结果
}

// 模块初始化函数
static int __init audit_init(void)
{
    // 获取系统调用表的地址
    sys_call_table = get_syscall_table();

    // 保存原始系统调用的地址
    orig_open = (orig_open_t)sys_call_table[__NR_open];
    orig_openat = (orig_openat_t)sys_call_table[__NR_openat];
    orig_unlink = (orig_unlink_t)sys_call_table[__NR_unlink];
    orig_unlinkat = (orig_unlinkat_t)sys_call_table[__NR_unlinkat];
    orig_execve = (orig_execve_t)sys_call_table[__NR_execve];
    orig_execveat = (orig_execveat_t)sys_call_table[__NR_execveat];
    orig_reboot = (orig_reboot_t)sys_call_table[__NR_reboot];
    orig_mount = (orig_mount_t)sys_call_table[__NR_mount];
    orig_umount2 = (orig_umount2_t)sys_call_table[__NR_umount2];
    orig_insmod = (orig_insmod_t)sys_call_table[__NR_init_module];
    orig_rmmod = (orig_rmmod_t)sys_call_table[__NR_delete_module];

    // 添加网络相关的系统调用
    orig_socket = (orig_socket_t)sys_call_table[__NR_socket];
    orig_connect = (orig_connect_t)sys_call_table[__NR_connect];
    orig_accept = (orig_accept_t)sys_call_table[__NR_accept];
    orig_sendto = (orig_sendto_t)sys_call_table[__NR_sendto];
    orig_recvfrom = (orig_recvfrom_t)sys_call_table[__NR_recvfrom];

    // 获取系统调用表的页表项
    pte = lookup_address((unsigned long)sys_call_table, &level);

    // 使系统调用表的页表项可写
    set_pte_atomic(pte, pte_mkwrite(*pte));
    printk("Info: Disable write-protection of page with sys_call_table\n");

    // 替换系统调用表中的系统调用地址为钩取的实现
    sys_call_table[__NR_open] = (demo_sys_call_ptr_t)hooked_sys_open;
    sys_call_table[__NR_openat] = (demo_sys_call_ptr_t)hooked_sys_openat;
    sys_call_table[__NR_unlink] = (demo_sys_call_ptr_t)hooked_sys_unlink;
    sys_call_table[__NR_unlinkat] = (demo_sys_call_ptr_t)hooked_sys_unlinkat;
    sys_call_table[__NR_execve] = (demo_sys_call_ptr_t)hooked_sys_execve;
    sys_call_table[__NR_execveat] = (demo_sys_call_ptr_t)hooked_sys_execveat;
    sys_call_table[__NR_reboot] = (demo_sys_call_ptr_t)hooked_sys_reboot;
    sys_call_table[__NR_mount] = (demo_sys_call_ptr_t)hooked_sys_mount;
    sys_call_table[__NR_umount2] = (demo_sys_call_ptr_t)hooked_sys_umount2;
    sys_call_table[__NR_init_module] = (demo_sys_call_ptr_t)hooked_sys_insmod;
    sys_call_table[__NR_delete_module] = (demo_sys_call_ptr_t)hooked_sys_rmmod;

    // 钩取网络相关的系统调用
    sys_call_table[__NR_socket] = (demo_sys_call_ptr_t)hooked_sys_socket;
    sys_call_table[__NR_connect] = (demo_sys_call_ptr_t)hooked_sys_connect;
    sys_call_table[__NR_accept] = (demo_sys_call_ptr_t)hooked_sys_accept;
    sys_call_table[__NR_sendto] = (demo_sys_call_ptr_t)hooked_sys_sendto;
    sys_call_table[__NR_recvfrom] = (demo_sys_call_ptr_t)hooked_sys_recvfrom;

    // 恢复系统调用表的写保护
    set_pte_atomic(pte, pte_clear_flags(*pte, _PAGE_RW));
    printk("Info: sys_call_table hooked!\n");

    // 初始化 netlink 通信
    netlink_init();
    return 0; // 初始化成功返回0
}

// 模块卸载函数
static void __exit audit_exit(void)
{
    // 获取系统调用表的页表项
    pte = lookup_address((unsigned long)sys_call_table, &level);

    // 使系统调用表的页表项可写
    set_pte_atomic(pte, pte_mkwrite(*pte));

    // 恢复系统调用表中的原始系统调用地址
    sys_call_table[__NR_open] = (demo_sys_call_ptr_t)orig_open;
    sys_call_table[__NR_openat] = (demo_sys_call_ptr_t)orig_openat;
    sys_call_table[__NR_unlink] = (demo_sys_call_ptr_t)orig_unlink;
    sys_call_table[__NR_unlinkat] = (demo_sys_call_ptr_t)orig_unlinkat;
    sys_call_table[__NR_execve] = (demo_sys_call_ptr_t)orig_execve;
    sys_call_table[__NR_execveat] = (demo_sys_call_ptr_t)orig_execveat;
    sys_call_table[__NR_reboot] = (demo_sys_call_ptr_t)orig_reboot;
    sys_call_table[__NR_mount] = (demo_sys_call_ptr_t)orig_mount;
    sys_call_table[__NR_umount2] = (demo_sys_call_ptr_t)orig_umount2;
    sys_call_table[__NR_init_module] = (demo_sys_call_ptr_t)orig_insmod;
    sys_call_table[__NR_delete_module] = (demo_sys_call_ptr_t)orig_rmmod;

    // 恢复网络相关系统调用的原始地址
    sys_call_table[__NR_socket] = (demo_sys_call_ptr_t)orig_socket;
    sys_call_table[__NR_connect] = (demo_sys_call_ptr_t)orig_connect;
    sys_call_table[__NR_accept] = (demo_sys_call_ptr_t)orig_accept;
    sys_call_table[__NR_sendto] = (demo_sys_call_ptr_t)orig_sendto;
    sys_call_table[__NR_recvfrom] = (demo_sys_call_ptr_t)orig_recvfrom;

    // 恢复系统调用表的写保护
    set_pte_atomic(pte, pte_clear_flags(*pte, _PAGE_RW));

    // 释放 netlink 通信资源
    netlink_release();
}

// 定义模块的初始化和卸载函数
module_init(audit_init);
module_exit(audit_exit);
