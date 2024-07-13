#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/ptrace.h>
#include <linux/kprobes.h>
#include <linux/reboot.h>

/*
** module macros
*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("infosec-sjtu");
MODULE_DESCRIPTION("hook sys_call_table");

#define MAX_LENGTH 256

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

demo_sys_call_ptr_t *get_syscall_table(void);

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

void netlink_release(void);
void netlink_init(void);

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
unsigned int level;
pte_t *pte;

asmlinkage long hooked_sys_open(struct pt_regs *regs)
{
    char pathname[MAX_LENGTH];
    int flags;
    int nbytes;
    int ret;

    memset(pathname, 0, MAX_LENGTH);
    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->di), MAX_LENGTH); // regs->di:pathname
    flags = regs->si; // regs->si:flags, O_RDONLY、O_WRONLY、O_RDWR

    ret = orig_open(regs);

    AuditOpen(pathname, flags, ret);

    return ret;
}

asmlinkage long hooked_sys_openat(struct pt_regs *regs)
{
    int dfd;
    char pathname[MAX_LENGTH];
    int flags;
    int nbytes;
    int ret;

    memset(pathname, 0, MAX_LENGTH);
    dfd = regs->di;

    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->si), MAX_LENGTH); // regs->si:pathname
    flags = regs->dx; // regs->dx:flags, O_RDONLY、O_WRONLY、O_RDWR

    ret = orig_openat(regs);

    AuditOpenat(dfd, pathname, flags, ret);

    return ret;
}

asmlinkage long hooked_sys_unlink(struct pt_regs *regs)
{
    char pathname[MAX_LENGTH];
    int nbytes;
    int ret;

    memset(pathname, 0, MAX_LENGTH);
    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->di), MAX_LENGTH); // regs->di:pathname

    ret = orig_unlink(regs);

    AuditUnlink(pathname, ret);

    return ret;
}

asmlinkage long hooked_sys_unlinkat(struct pt_regs *regs)
{
    int dfd;
    char pathname[MAX_LENGTH];
    int nbytes;
    int ret;

    memset(pathname, 0, MAX_LENGTH);
    dfd = regs->di;

    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->si), MAX_LENGTH); // regs->si:pathname

    ret = orig_unlinkat(regs);

    AuditUnlinkat(dfd, pathname, ret);

    return ret;
}

asmlinkage long hooked_sys_execve(struct pt_regs *regs)
{
    char pathname[MAX_LENGTH];
    int nbytes;
    int ret;

    memset(pathname, 0, MAX_LENGTH);
    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->di), MAX_LENGTH); // regs->di:pathname

    ret = orig_execve(regs);

    AuditExec(pathname, ret);

    return ret;
}

asmlinkage long hooked_sys_execveat(struct pt_regs *regs)
{
    int dfd;
    char pathname[MAX_LENGTH];
    int flags;
    int nbytes;
    int ret;

    memset(pathname, 0, MAX_LENGTH);
    dfd = regs->di;
    nbytes = strncpy_from_user(pathname, (const char __user *)(regs->si), MAX_LENGTH); // regs->si:pathname
    flags = regs->dx; // regs->dx:flags

    ret = orig_execveat(regs);

    AuditExecat(dfd, pathname, flags, ret);

    return ret;
}


asmlinkage long hooked_sys_reboot(struct pt_regs *regs)
{
    char audit_msg[MAX_LENGTH];
    int ret;
    int cmd = (unsigned int)regs->dx;

    memset(audit_msg, 0, MAX_LENGTH);

    if (cmd & LINUX_REBOOT_CMD_POWER_OFF) {
        // Shutdown operation
        snprintf(audit_msg, MAX_LENGTH, "Shutdown called with cmd: %u", cmd);
        ret = 0;  // Assuming success for shutdown
        AuditShutdown(audit_msg, ret);
    } else {
        // Reboot operation
        snprintf(audit_msg, MAX_LENGTH, "Reboot called with cmd: %u", cmd);
        ret = orig_reboot(regs);
        AuditReboot(audit_msg, ret);
    }

    return ret;
}


// asmlinkage long hooked_sys_insmod(struct pt_regs *regs)
// {
    
// }

// asmlinkage long hooked_sys_insmodat(struct pt_regs *regs)
// {

// }


// asmlinkage long hooked_sys_rmmod(struct pt_regs *regs)
// {

// }

// asmlinkage long hooked_sys_rmmodat(struct pt_regs *regs)
// {

// }


// asmlinkage long hooked_sys_deviceadd(struct pt_regs *regs)
// {

// }

// asmlinkage long hooked_sys_deviceaddat(struct pt_regs *regs)
// {

// }


// asmlinkage long hooked_sys_deviceremove(struct pt_regs *regs)
// {

// }

// asmlinkage long hooked_sys_deviceremoveat(struct pt_regs *regs)
// {

// }


asmlinkage long hooked_sys_mount(struct pt_regs *regs)
{
    char source[MAX_LENGTH];
    char target[MAX_LENGTH];
    char filesystemtype[MAX_LENGTH];
    unsigned long mountflags;
    const void *data;
    int ret;
    int nbytes;

    // 初始化缓冲区
    memset(source, 0, MAX_LENGTH);
    memset(target, 0, MAX_LENGTH);
    memset(filesystemtype, 0, MAX_LENGTH);

    // 从用户空间获取参数
    nbytes = strncpy_from_user(source, (const char __user *)regs->di, MAX_LENGTH); // source
    nbytes = strncpy_from_user(target, (const char __user *)regs->si, MAX_LENGTH); // target
    nbytes = strncpy_from_user(filesystemtype, (const char __user *)regs->dx, MAX_LENGTH); // filesystemtype
    mountflags = regs->r10; // mountflags
    data = (const void *)regs->r8; // data

    // 调用原始的 mount 系统调用
    ret = orig_mount(regs);

    // 调用审计函数
    AuditMount(source, target, filesystemtype, mountflags, data, ret);

    return ret;
}

asmlinkage long hooked_sys_umount2(struct pt_regs *regs)
{
    char target[MAX_LENGTH];
    int flags;
    int ret;
    int nbytes;

    memset(target, 0, MAX_LENGTH);
    nbytes = strncpy_from_user(target, (const char __user *)regs->di, MAX_LENGTH);
    flags = regs->si; // flags

    ret = orig_umount2(regs);

    AuditUnmount2(target, flags, ret);

    return ret;
}



// asmlinkage long hooked_sys_http(struct pt_regs *regs)
// {

// }

// asmlinkage long hooked_sys_httpat(struct pt_regs *regs)
// {

// }


// asmlinkage long hooked_sys_ftp(struct pt_regs *regs)
// {

// }

// asmlinkage long hooked_sys_ftpat(struct pt_regs *regs)
// {

// }


// asmlinkage long hooked_sys_ssh(struct pt_regs *regs)
// {

// }

// asmlinkage long hooked_sys_sshat(struct pt_regs *regs)
// {

// }


// asmlinkage long hooked_sys_database(struct pt_regs *regs)
// {

// }

// asmlinkage long hooked_sys_databaseat(struct pt_regs *regs)
// {

// }




static int __init audit_init(void)
{
    sys_call_table = get_syscall_table();

    orig_open = (orig_open_t)sys_call_table[__NR_open];
    orig_openat = (orig_openat_t)sys_call_table[__NR_openat];
    orig_unlink = (orig_unlink_t)sys_call_table[__NR_unlink];
    orig_unlinkat = (orig_unlinkat_t)sys_call_table[__NR_unlinkat];
    orig_execve = (orig_execve_t)sys_call_table[__NR_execve];
    orig_execveat = (orig_execveat_t)sys_call_table[__NR_execveat];
    orig_reboot = (orig_reboot_t)sys_call_table[__NR_reboot];
    orig_mount = (orig_mount_t)sys_call_table[__NR_mount];
    orig_umount2 = (orig_umount2_t)sys_call_table[__NR_umount2];


    pte = lookup_address((unsigned long)sys_call_table, &level);
    set_pte_atomic(pte, pte_mkwrite(*pte));
    printk("Info: Disable write-protection of page with sys_call_table\n");

    sys_call_table[__NR_open] = (demo_sys_call_ptr_t)hooked_sys_open;
    sys_call_table[__NR_openat] = (demo_sys_call_ptr_t)hooked_sys_openat;
    sys_call_table[__NR_unlink] = (demo_sys_call_ptr_t)hooked_sys_unlink;
    sys_call_table[__NR_unlinkat] = (demo_sys_call_ptr_t)hooked_sys_unlinkat;
    sys_call_table[__NR_execve] = (demo_sys_call_ptr_t)hooked_sys_execve;
    sys_call_table[__NR_execveat] = (demo_sys_call_ptr_t)hooked_sys_execveat;
    sys_call_table[__NR_reboot] = (demo_sys_call_ptr_t)hooked_sys_reboot;
    sys_call_table[__NR_mount] = (demo_sys_call_ptr_t)hooked_sys_mount;
    sys_call_table[__NR_umount2] = (demo_sys_call_ptr_t)hooked_sys_umount2;

    set_pte_atomic(pte, pte_clear_flags(*pte, _PAGE_RW));
    printk("Info: sys_call_table hooked!\n");

    netlink_init();
    return 0;
}

static void __exit audit_exit(void)
{
    pte = lookup_address((unsigned long)sys_call_table, &level);
    set_pte_atomic(pte, pte_mkwrite(*pte));

    sys_call_table[__NR_open] = (demo_sys_call_ptr_t)orig_open;
    sys_call_table[__NR_openat] = (demo_sys_call_ptr_t)orig_openat;
    sys_call_table[__NR_unlink] = (demo_sys_call_ptr_t)orig_unlink;
    sys_call_table[__NR_unlinkat] = (demo_sys_call_ptr_t)orig_unlinkat;
    sys_call_table[__NR_execve] = (demo_sys_call_ptr_t)orig_execve;
    sys_call_table[__NR_execveat] = (demo_sys_call_ptr_t)orig_execveat;
    sys_call_table[__NR_reboot] = (demo_sys_call_ptr_t)orig_reboot;
    sys_call_table[__NR_mount] = (demo_sys_call_ptr_t)orig_mount;
    sys_call_table[__NR_umount2] = (demo_sys_call_ptr_t)orig_umount2;

    set_pte_atomic(pte, pte_clear_flags(*pte, _PAGE_RW));

    netlink_release();
}

module_init(audit_init);
module_exit(audit_exit);
