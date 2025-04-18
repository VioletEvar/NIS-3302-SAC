#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/utsname.h>
#include <asm/pgtable.h>
#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include <asm/uaccess.h>
#include <asm/ptrace.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <linux/ctype.h>



typedef void (* demo_sys_call_ptr_t)(void);

static struct kprobe kp = {
   .symbol_name = "sys_call_table"
};



demo_sys_call_ptr_t * get_syscall_table(void)
{
     demo_sys_call_ptr_t * sys_call_table=NULL;

     /*Use kprobe to get sys_call_table */

     register_kprobe(&kp);

     sys_call_table=(demo_sys_call_ptr_t *) kp.addr;

     unregister_kprobe(&kp);


     //Print sys_call_table address
     printk("Info: sys_call_table is at %lx\n", (long) sys_call_table);

     return sys_call_table;
}
