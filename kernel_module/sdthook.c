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

demo_sys_call_ptr_t *get_syscall_table(void);

int AuditOpen(const char *pathname, int flags, int ret);
int AuditOpenat(int dfd, const char *pathname, int flags, int ret);
int AuditUnlink(const char *pathname, int ret);
int AuditUnlinkat(int dfd, const char *pathname, int ret);
void netlink_release(void);
void netlink_init(void);

demo_sys_call_ptr_t *sys_call_table = NULL;
orig_open_t orig_open = NULL;
orig_openat_t orig_openat = NULL;
orig_unlink_t orig_unlink = NULL;
orig_unlinkat_t orig_unlinkat = NULL;
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

static int __init audit_init(void)
{
    sys_call_table = get_syscall_table();

    orig_open = (orig_open_t)sys_call_table[__NR_open];
    orig_openat = (orig_openat_t)sys_call_table[__NR_openat];
    orig_unlink = (orig_unlink_t)sys_call_table[__NR_unlink];
    orig_unlinkat = (orig_unlinkat_t)sys_call_table[__NR_unlinkat];

    pte = lookup_address((unsigned long)sys_call_table, &level);
    set_pte_atomic(pte, pte_mkwrite(*pte));
    printk("Info: Disable write-protection of page with sys_call_table\n");

    sys_call_table[__NR_open] = (demo_sys_call_ptr_t)hooked_sys_open;
    sys_call_table[__NR_openat] = (demo_sys_call_ptr_t)hooked_sys_openat;
    sys_call_table[__NR_unlink] = (demo_sys_call_ptr_t)hooked_sys_unlink;
    sys_call_table[__NR_unlinkat] = (demo_sys_call_ptr_t)hooked_sys_unlinkat;

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

    set_pte_atomic(pte, pte_clear_flags(*pte, _PAGE_RW));

    netlink_release();
}

module_init(audit_init);
module_exit(audit_exit);