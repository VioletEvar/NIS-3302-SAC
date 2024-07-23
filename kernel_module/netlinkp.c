#include <linux/string.h>
#include <linux/mm.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/sched.h>
#include <linux/fs_struct.h>
#include <linux/limits.h>

#define TASK_COMM_LEN 16
#define NETLINK_TEST 29
#define AUDITPATH "/home/szy/Desktop/test"
#define MAX_LENGTH 256
#define MAX_MSGSIZE 1200
#define MAX_PAYLOAD 1024  // 根据需要设置合适的大小

static u32 pid=0;
static struct sock *nl_sk = NULL;

// 发送 netlink 消息
int netlink_sendmsg(const void *buffer, unsigned int size)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    int len = NLMSG_SPACE(MAX_MSGSIZE);
    
    // 检查参数有效性
    if ((!buffer) || (!nl_sk) || (pid == 0))
        return 1;
    
    // 分配一个新的 sk_buff
    skb = alloc_skb(len, GFP_ATOMIC); 	
    if (!skb){
        printk(KERN_ERR "net_link: allocat_skb failed.\n");
        return 1;
    }
    
    // 填充 netlink 消息头
    nlh = nlmsg_put(skb, 0, 0, 0, MAX_MSGSIZE, 0);
    NETLINK_CB(skb).creds.pid = 0;  // 消息来自内核

    // 复制数据到 netlink 消息体
    memcpy(NLMSG_DATA(nlh), buffer, size);

    // 发送消息
    if (netlink_unicast(nl_sk, skb, pid, MSG_DONTWAIT) < 0){
        printk(KERN_ERR "net_link: can not unicast skb \n");
        return 1;
    }
    
    return 0;
}

// 获取完整的文件路径
int get_fullname(struct dentry *parent_dentry, const char *pathname, char *fullname)
{
    char buf[MAX_LENGTH];
    char *filestart = NULL;

    memset(buf, 0, MAX_LENGTH);

    // 如果 pathname 是绝对路径
    if (!strncmp(pathname, "/", 1)) {
        strcpy(fullname, pathname);
    }
    else {
        // 获取相对路径的完整路径
        filestart = dentry_path_raw(parent_dentry, buf, MAX_LENGTH);
        if (IS_ERR(filestart)){
            printk("Info: get_fullname error!\n");
            return 1;
        }
        strcpy(fullname, filestart);
        strcat(fullname, "/");
        strcat(fullname, pathname);
    }

    return 0;
}

// 审计文件打开操作
int AuditOpen(char *pathname, int flags, int ret)
{
    char commandname[TASK_COMM_LEN];
    char fullname[MAX_LENGTH];
    unsigned int size;
    void *buffer;
    char auditpath[MAX_LENGTH];
    const struct cred *cred;
    struct dentry *parent_dentry;

    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);

    // 获取当前目录的 dentry
    parent_dentry = current->fs->pwd.dentry;

    // 获取完整的文件路径
    get_fullname(parent_dentry, pathname, fullname);

    // 设置审计路径
    strcpy(auditpath, AUDITPATH);

    // 如果路径不匹配审计路径，则返回
    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    // 打印调试信息
    printk("Info: in AuditOpen; fullname is %s \t; Auditpath is %s \n", fullname, AUDITPATH);

    // 分配缓冲区并初始化
    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    memset(buffer, 0, size);

    // 获取命令名和当前进程的凭据
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();

    // 将审计信息写入缓冲区
    *((int*)buffer) = cred->uid.val; // UID
    *((int*)buffer + 1) = current->pid;
    *((int*)buffer + 2) = flags; // 打开标志
    *((int*)buffer + 3) = ret;
    strcpy((char*)(4 + (int*)buffer), commandname);
    strcpy((char*)(4 + TASK_COMM_LEN/4 + (int*)buffer), fullname);

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);

    return 0;
}

// 审计文件打开操作（带目录文件描述符）
int AuditOpenat(int dfd, char *pathname, int flags, int ret)
{
    char commandname[TASK_COMM_LEN];
    char fullname[MAX_LENGTH];
    char auditpath[MAX_LENGTH];
    char buf[MAX_LENGTH];
    struct file *filep = NULL;
    unsigned int size;
    void *buffer;
    const struct cred *cred;
    struct dentry *parent_dentry;

    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);
    memset(buf, 0, MAX_LENGTH);

    // 如果 pathname 是绝对路径
    if (!strncmp(pathname, "/", 1))
        strcpy(fullname, pathname);
    else {
        // 如果 dfd 是当前工作目录
        if (dfd == AT_FDCWD) {
            parent_dentry = current->fs->pwd.dentry;
            get_fullname(parent_dentry, pathname, fullname);
        }
        else {
            // 获取 dfd 对应的文件路径
            filep = fget_raw(dfd);
            parent_dentry = filep->f_path.dentry;
            get_fullname(parent_dentry, pathname, fullname);
        }
    }

    strcpy(auditpath, AUDITPATH);

    // 如果路径不匹配审计路径，则返回
    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    // 打印调试信息
    printk("Info: in AuditOpenat, fullname is %s \t; Auditpath is %s \n", fullname, AUDITPATH);

    // 分配缓冲区并初始化
    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    memset(buffer, 0, size);

    // 获取命令名和当前进程的凭据
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();

    // 将审计信息写入缓冲区
    *((int*)buffer) = cred->uid.val; // UID
    *((int*)buffer + 1) = current->pid;
    *((int*)buffer + 2) = flags; // 打开标志
    *((int*)buffer + 3) = ret;
    strcpy((char*)(4 + (int*)buffer), commandname);
    strcpy((char*)(4 + TASK_COMM_LEN/4 + (int*)buffer), fullname);

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);

    return 0;
}

// 审计文件删除操作
int AuditUnlink(char *pathname, int ret)
{
    char commandname[TASK_COMM_LEN];
    char fullname[MAX_LENGTH];
    unsigned int size;
    void *buffer;
    char auditpath[MAX_LENGTH];
    const struct cred *cred;
    struct dentry *parent_dentry;

    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);

    // 获取当前目录的 dentry
    parent_dentry = current->fs->pwd.dentry;

    // 获取完整的文件路径
    get_fullname(parent_dentry, pathname, fullname);

    // 设置审计路径
    strcpy(auditpath, AUDITPATH);

    // 如果路径不匹配审计路径，则返回
    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    // 分配缓冲区并初始化
    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    memset(buffer, 0, size);

    // 获取命令名和当前进程的凭据
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();

    // 将审计信息写入缓冲区
    *((int*)buffer) = cred->uid.val; // UID
    *((int*)buffer + 1) = current->pid;
    *((int*)buffer + 2) = O_WRONLY; // 用 O_WRONLY 表示写操作
    *((int*)buffer + 3) = ret;
    strcpy((char*)(4 + (int*)buffer), commandname);
    strcpy((char*)(4 + TASK_COMM_LEN/4 + (int*)buffer), fullname);

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);

    return 0;
}

// 审计文件删除操作（带目录文件描述符）
int AuditUnlinkat(int dfd, char *pathname, int ret)
{
    char commandname[TASK_COMM_LEN];
    char fullname[MAX_LENGTH];
    char auditpath[MAX_LENGTH];
    char buf[MAX_LENGTH];
    struct file *filep = NULL;
    unsigned int size;
    void *buffer;
    const struct cred *cred;
    struct dentry *parent_dentry;

    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);
    memset(buf, 0, MAX_LENGTH);

    // 如果 pathname 是绝对路径
    if (!strncmp(pathname, "/", 1))
        strcpy(fullname, pathname);
    else {
        // 如果 dfd 是当前工作目录
        if (dfd == AT_FDCWD) {
            parent_dentry = current->fs->pwd.dentry;
            get_fullname(parent_dentry, pathname, fullname);
        }
        else {
            // 获取 dfd 对应的文件路径
            filep = fget_raw(dfd);
            parent_dentry = filep->f_path.dentry;
            get_fullname(parent_dentry, pathname, fullname);
        }
    }

    strcpy(auditpath, AUDITPATH);

    // 如果路径不匹配审计路径，则返回
    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    // 分配缓冲区并初始化
    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    memset(buffer, 0, size);

    // 获取命令名和当前进程的凭据
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();

    // 将审计信息写入缓冲区
    *((int*)buffer) = cred->uid.val; // UID
    *((int*)buffer + 1) = current->pid;
    *((int*)buffer + 2) = O_WRONLY; // 用 O_WRONLY 表示写操作
    *((int*)buffer + 3) = ret;
    strcpy((char*)(4 + (int*)buffer), commandname);
    strcpy((char*)(4 + TASK_COMM_LEN/4 + (int*)buffer), fullname);

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);

    return 0;
}


// AuditExec 函数
// 该函数用于审计执行文件操作。
int AuditExec(const char *pathname, int ret) {
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    char fullname[MAX_LENGTH]; // 用于存储完整的文件路径
    unsigned int size; // 用于存储缓冲区的大小
    void *buffer; // 指向分配的缓冲区
    char auditpath[MAX_LENGTH]; // 用于存储审计路径
    const struct cred *cred; // 指向当前进程的凭据
    struct dentry *parent_dentry; // 指向当前目录的dentry

    // 初始化fullname和auditpath数组
    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);

    // 获取当前目录的dentry
    parent_dentry = current->fs->pwd.dentry;

    // 获取完整的文件路径
    get_fullname(parent_dentry, pathname, fullname);

    // 将审计路径复制到auditpath中
    strcpy(auditpath, AUDITPATH);

    // 如果完整路径不匹配审计路径，则返回
    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    // 打印调试信息
    printk("Info: in AuditExec, fullname is  %s \t; Auditpath is  %s \n", fullname, AUDITPATH);

    // 计算缓冲区大小
    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0); // 分配缓冲区
    memset(buffer, 0, size); // 初始化缓冲区

    // 获取当前进程的命令名
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid; // PID
    *((int *)buffer + 2) = 0; // 使用 0 表示执行操作
    *((int *)buffer + 3) = ret; // 返回值
    strcpy((char *)(4 + (int *)buffer), commandname); // 命令名
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), fullname); // 完整路径

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);

    return 0;
}

// AuditExecat 函数
// 该函数用于审计带目录文件描述符的执行文件操作。
int AuditExecat(int dfd, char *pathname, int flags, int ret) {
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    char fullname[MAX_LENGTH]; // 用于存储完整的文件路径
    char auditpath[MAX_LENGTH]; // 用于存储审计路径
    struct file *filep = NULL; // 指向文件的指针
    unsigned int size; // 用于存储缓冲区的大小
    void *buffer; // 指向分配的缓冲区
    const struct cred *cred; // 指向当前进程的凭据
    struct dentry *parent_dentry; // 指向父目录的dentry

    // 初始化fullname和auditpath数组
    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);

    // 判断 pathname 是否为绝对路径
    if (!strncmp(pathname, "/", 1)) // 如果 pathname 是绝对路径
        strcpy(fullname, pathname);
    else {
        if (dfd == AT_FDCWD) { // 如果 dfd 是当前工作目录
            parent_dentry = current->fs->pwd.dentry;
            get_fullname(parent_dentry, pathname, fullname); // 获取完整路径
        } else {
            filep = fget_raw(dfd); // 获取 dfd 对应的文件
            parent_dentry = filep->f_path.dentry;
            get_fullname(parent_dentry, pathname, fullname); // 获取完整路径
        }
    }

    // 将审计路径复制到auditpath中
    strcpy(auditpath, AUDITPATH);

    // 如果完整路径不匹配审计路径，则返回
    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    // 打印调试信息
    printk("Info: in AuditExecat, fullname is  %s \t; Auditpath is  %s \n", fullname, AUDITPATH);

    // 计算缓冲区大小
    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0); // 分配缓冲区
    memset(buffer, 0, size); // 初始化缓冲区

    // 获取当前进程的命令名
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid; // PID
    *((int *)buffer + 2) = 0; // 根据 flags 确定具体的执行权限
    *((int *)buffer + 3) = ret; // 返回值
    strcpy((char *)(4 + (int *)buffer), commandname); // 命令名
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), fullname); // 完整路径

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);

    return 0;
}

// AuditShutdown 函数
// 该函数用于审计系统关机操作。
int AuditShutdown(const char *message, int ret) {
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    unsigned int size; // 用于存储缓冲区的大小
    void *buffer; // 指向分配的缓冲区
    const struct cred *cred; // 指向当前进程的凭据

    // 计算缓冲区大小
    size = strlen(message) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, GFP_KERNEL); // 分配缓冲区
    if (!buffer) {
        printk(KERN_ERR "AuditShutdown: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size); // 初始化缓冲区

    // 获取当前进程的命令名
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid; // PID
    *((int *)buffer + 2) = 0; // Shutdown 没有 flags 参数，所以设置为 0
    *((int *)buffer + 3) = ret; // 返回值
    strcpy((char *)(4 + (int *)buffer), commandname); // 命令名
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), message); // 关机消息

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);
    kfree(buffer); // 释放缓冲区

    return 0;
}

// AuditReboot 函数
// 该函数用于审计系统重启操作。
int AuditReboot(const char *message, int ret) {
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    unsigned int size; // 用于存储缓冲区的大小
    void *buffer; // 指向分配的缓冲区
    const struct cred *cred; // 指向当前进程的凭据

    // 计算缓冲区大小
    size = strlen(message) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0); // 分配缓冲区
    if (!buffer) {
        printk(KERN_ERR "AuditReboot: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size); // 初始化缓冲区

    // 获取当前进程的命令名
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid; // PID
    *((int *)buffer + 2) = 0; // reboot 没有 flags 参数，所以设置为 0
    *((int *)buffer + 3) = ret; // 返回值
    strcpy((char *)(4 + (int *)buffer), commandname); // 命令名
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), message); // 重启消息

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);
    kfree(buffer); // 释放缓冲区

    return 0;
}

// AuditInsmod 函数
// 该函数用于审计内核模块加载操作。
int AuditInsmod(const char *pathname, int ret) {
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    char fullname[MAX_LENGTH]; // 用于存储完整的文件路径
    unsigned int size; // 用于存储缓冲区的大小
    void *buffer; // 指向分配的缓冲区
    char auditpath[MAX_LENGTH]; // 用于存储审计路径
    const struct cred *cred; // 指向当前进程的凭据
    struct dentry *parent_dentry; // 指向当前目录的dentry

    // 初始化fullname和auditpath数组
    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);

    // 获取当前目录的dentry
    parent_dentry = current->fs->pwd.dentry;

    // 获取完整的文件路径
    get_fullname(parent_dentry, pathname, fullname);

    // 将审计路径复制到auditpath中
    strcpy(auditpath, AUDITPATH);

    // 如果完整路径不匹配审计路径，则返回
    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    // 打印调试信息
    printk("Info: in AuditInsmod, fullname is %s \t; Auditpath is %s \n", fullname, AUDITPATH);

    // 计算缓冲区大小
    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, GFP_KERNEL); // 分配缓冲区
    if (!buffer) {
        printk(KERN_ERR "AuditInsmod: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size); // 初始化缓冲区

    // 获取当前进程的命令名
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid; // PID
    *((int *)buffer + 2) = 0; // 使用 0 表示 insmod 操作
    *((int *)buffer + 3) = ret; // 返回值
    strcpy((char *)(4 + (int *)buffer), commandname); // 命令名
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), fullname); // 完整路径

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);
    kfree(buffer); // 释放缓冲区

    return 0;
}

// AuditRmmod 函数
// 该函数用于审计内核模块卸载操作。
int AuditRmmod(const char *pathname, int ret) {
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    char fullname[MAX_LENGTH]; // 用于存储完整的文件路径
    unsigned int size; // 用于存储缓冲区的大小
    void *buffer; // 指向分配的缓冲区
    char auditpath[MAX_LENGTH]; // 用于存储审计路径
    const struct cred *cred; // 指向当前进程的凭据
    struct dentry *parent_dentry; // 指向当前目录的dentry

    // 初始化fullname和auditpath数组
    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);

    // 获取当前目录的dentry
    parent_dentry = current->fs->pwd.dentry;

    // 获取完整的文件路径
    get_fullname(parent_dentry, pathname, fullname);

    // 将审计路径复制到auditpath中
    strcpy(auditpath, AUDITPATH);

    // 如果完整路径不匹配审计路径，则返回
    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    // 打印调试信息
    printk("Info: in AuditRmmod, fullname is %s \t; Auditpath is %s \n", fullname, AUDITPATH);

    // 计算缓冲区大小
    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, GFP_KERNEL); // 分配缓冲区
    if (!buffer) {
        printk(KERN_ERR "AuditRmmod: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size); // 初始化缓冲区

    // 获取当前进程的命令名
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid; // PID
    *((int *)buffer + 2) = 0; // 使用 0 表示 rmmod 操作
    *((int *)buffer + 3) = ret; // 返回值
    strcpy((char *)(4 + (int *)buffer), commandname); // 命令名
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), fullname); // 完整路径

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);
    kfree(buffer); // 释放缓冲区

    return 0;
}

// AuditMount 函数
// 该函数用于审计挂载操作。
int AuditMount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data, int ret) {
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    unsigned int size; // 用于存储缓冲区的大小
    void *buffer; // 指向分配的缓冲区
    const struct cred *cred; // 指向当前进程的凭据

    // 计算缓冲区大小
    size = strlen(source) + strlen(target) + strlen(filesystemtype) + 32 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, GFP_KERNEL); // 分配缓冲区
    if (!buffer) {
        printk(KERN_ERR "AuditMount: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size); // 初始化缓冲区

    // 获取当前进程的命令名
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid; // PID
    *((unsigned long *)buffer + 2) = mountflags; // 挂载标志
    *((int *)buffer + 3) = ret; // 返回值
    strcpy((char *)(4 + (int *)buffer), commandname); // 命令名
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), source); // 源路径
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + strlen(source) + 1 + (int *)buffer), target); // 目标路径
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + strlen(source) + strlen(target) + 2 + (int *)buffer), filesystemtype); // 文件系统类型

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);
    kfree(buffer); // 释放缓冲区

    return 0;
}

// AuditUnmount2 函数
// 该函数用于审计卸载操作。
int AuditUnmount2(const char *target, int flags, int ret) {
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    unsigned int size; // 用于存储缓冲区的大小
    void *buffer; // 指向分配的缓冲区
    const struct cred *cred; // 指向当前进程的凭据

    // 计算缓冲区大小
    size = strlen(target) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0); // 分配缓冲区
    if (!buffer) {
        printk(KERN_ERR "AuditUnmount2: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size); // 初始化缓冲区

    // 获取当前进程的命令名
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid; // PID
    *((int *)buffer + 2) = flags; // 卸载标志
    *((int *)buffer + 3) = ret; // 返回值
    strcpy((char *)(4 + (int *)buffer), commandname); // 命令名
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), target); // 目标路径

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);
    kfree(buffer); // 释放缓冲区

    return 0;
}

// AuditSocket 函数
// 该函数用于审计socket创建操作。
int AuditSocket(int domain, int type, int protocol) {
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    char fullname[MAX_LENGTH]; // 用于存储完整的路径（尽管socket操作没有文件路径，这里保留该字段）
    unsigned int size; // 用于存储缓冲区的大小
    void *buffer; // 指向分配的缓冲区
    const struct cred *cred; // 指向当前进程的凭据
    struct dentry *parent_dentry; // 指向当前目录的dentry

    // 初始化fullname数组
    memset(fullname, 0, MAX_LENGTH);

    // 获取当前目录的dentry
    parent_dentry = current->fs->pwd.dentry;

    // 获取完整的路径
    get_fullname(parent_dentry, "", fullname);

    // 打印调试信息
    printk("Info: in AuditSocket; fullname is %s\n", fullname);

    // 计算缓冲区大小
    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0); // 分配缓冲区
    if (!buffer) {
        printk(KERN_ERR "AuditSocket: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size); // 初始化缓冲区

    // 获取当前进程的命令名
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid; // PID
    *((int *)buffer + 2) = type; // socket 类型
    *((int *)buffer + 3) = 0; // socket 创建操作没有返回值
    strcpy(commandname, "socket"); // 命令名
    strcpy((char *)(4 + (int *)buffer), commandname); // 命令名
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), fullname); // 完整路径

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);
    kfree(buffer); // 释放缓冲区

    return 0;
}
// AuditConnect 函数
// 该函数用于审计 socket 连接操作。
int AuditConnect(int sockfd, struct sockaddr *addr, int addrlen)
{
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    char fullname[MAX_LENGTH]; // 用于存储完整路径（尽管 connect 操作没有文件路径，这里保留该字段）
    unsigned int size; // 用于存储缓冲区的大小
    void * buffer; // 指向分配的缓冲区
    const struct cred *cred; // 指向当前进程的凭据
    struct dentry *parent_dentry; // 指向当前目录的 dentry

    memset(fullname, 0, MAX_LENGTH); // 初始化 fullname 数组

    parent_dentry = current->fs->pwd.dentry; // 获取当前目录的 dentry

    // 获取完整的路径
    get_fullname(parent_dentry, "", fullname); // 假设 connect 操作没有文件路径

    printk("Info: in AuditConnect; fullname is %s\n", fullname); // 打印调试信息

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1; // 计算缓冲区大小
    buffer = kmalloc(size, 0); // 分配缓冲区
    memset(buffer, 0, size); // 初始化缓冲区

    strncpy(commandname, current->comm, TASK_COMM_LEN); // 获取当前进程的命令名
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int*)buffer) = cred->uid.val; // UID
    *((int*)buffer + 1) = current->pid; // PID
    *((int*)buffer + 2) = sockfd; // socket 描述符
    *((int*)buffer + 3) = 0; // connect 操作没有返回值
    strcpy(commandname, "connect"); // 设置命令名
    strcpy((char*)(4 + (int*)buffer), commandname); // 将命令名写入缓冲区
    strcpy((char*)(4 + TASK_COMM_LEN / 4 + (int*)buffer), fullname); // 将完整路径写入缓冲区
    netlink_sendmsg(buffer, size); // 发送 netlink 消息

    return 0; // 返回
}

// AuditAccept 函数
// 该函数用于审计 socket 接受连接操作。
int AuditAccept(int sockfd, struct sockaddr *addr, int *addrlen)
{
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    char fullname[MAX_LENGTH]; // 用于存储完整路径（尽管 accept 操作没有文件路径，这里保留该字段）
    unsigned int size; // 用于存储缓冲区的大小
    void * buffer; // 指向分配的缓冲区
    const struct cred *cred; // 指向当前进程的凭据
    struct dentry *parent_dentry; // 指向当前目录的 dentry

    memset(fullname, 0, MAX_LENGTH); // 初始化 fullname 数组

    parent_dentry = current->fs->pwd.dentry; // 获取当前目录的 dentry

    // 获取完整的路径
    get_fullname(parent_dentry, "", fullname); // 假设 accept 操作没有文件路径

    printk("Info: in AuditAccept; fullname is %s\n", fullname); // 打印调试信息

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1; // 计算缓冲区大小
    buffer = kmalloc(size, 0); // 分配缓冲区
    memset(buffer, 0, size); // 初始化缓冲区

    strncpy(commandname, current->comm, TASK_COMM_LEN); // 获取当前进程的命令名
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int*)buffer) = cred->uid.val; // UID
    *((int*)buffer + 1) = current->pid; // PID
    *((int*)buffer + 2) = sockfd; // socket 描述符
    *((int*)buffer + 3) = 0; // accept 操作没有返回值
    strcpy(commandname, "accept"); // 设置命令名
    strcpy((char*)(4 + (int*)buffer), commandname); // 将命令名写入缓冲区
    strcpy((char*)(4 + TASK_COMM_LEN / 4 + (int*)buffer), fullname); // 将完整路径写入缓冲区
    netlink_sendmsg(buffer, size); // 发送 netlink 消息

    return 0; // 返回
}

// AuditSendto 函数
// 该函数用于审计 socket 发送数据操作。
int AuditSendto(int sockfd, void *buf, size_t len, int flags, struct sockaddr *dest_addr, int addrlen)
{
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    char fullname[MAX_LENGTH]; // 用于存储完整路径（尽管 sendto 操作没有文件路径，这里保留该字段）
    unsigned int size; // 用于存储缓冲区的大小
    void * buffer; // 指向分配的缓冲区
    const struct cred *cred; // 指向当前进程的凭据
    struct dentry *parent_dentry; // 指向当前目录的 dentry

    memset(fullname, 0, MAX_LENGTH); // 初始化 fullname 数组

    parent_dentry = current->fs->pwd.dentry; // 获取当前目录的 dentry

    // 获取完整的路径
    get_fullname(parent_dentry, "", fullname); // 假设 sendto 操作没有文件路径

    printk("Info: in AuditSendto; fullname is %s\n", fullname); // 打印调试信息

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1; // 计算缓冲区大小
    buffer = kmalloc(size, 0); // 分配缓冲区
    memset(buffer, 0, size); // 初始化缓冲区

    strncpy(commandname, current->comm, TASK_COMM_LEN); // 获取当前进程的命令名
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int*)buffer) = cred->uid.val; // UID
    *((int*)buffer + 1) = current->pid; // PID
    *((int*)buffer + 2) = sockfd; // socket 描述符
    *((int*)buffer + 3) = len; // 发送的数据长度
    strcpy(commandname, "sendto"); // 设置命令名
    strcpy((char*)(4 + (int*)buffer), commandname); // 将命令名写入缓冲区
    strcpy((char*)(4 + TASK_COMM_LEN / 4 + (int*)buffer), fullname); // 将完整路径写入缓冲区
    netlink_sendmsg(buffer, size); // 发送 netlink 消息

    return 0; // 返回
}

// AuditRecvfrom 函数
// 该函数用于审计 socket 接收数据操作。
int AuditRecvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, int *addrlen)
{
    char commandname[TASK_COMM_LEN]; // 用于存储当前进程的命令名
    char fullname[MAX_LENGTH]; // 用于存储完整路径（尽管 recvfrom 操作没有文件路径，这里保留该字段）
    unsigned int size; // 用于存储缓冲区的大小
    void * buffer; // 指向分配的缓冲区
    const struct cred *cred; // 指向当前进程的凭据
    struct dentry *parent_dentry; // 指向当前目录的 dentry

    memset(fullname, 0, MAX_LENGTH); // 初始化 fullname 数组

    parent_dentry = current->fs->pwd.dentry; // 获取当前目录的 dentry

    // 获取完整的路径
    get_fullname(parent_dentry, "", fullname); // 假设 recvfrom 操作没有文件路径

    printk("Info: in AuditRecvfrom; fullname is %s\n", fullname); // 打印调试信息

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1; // 计算缓冲区大小
    buffer = kmalloc(size, 0); // 分配缓冲区
    memset(buffer, 0, size); // 初始化缓冲区

    strncpy(commandname, current->comm, TASK_COMM_LEN); // 获取当前进程的命令名
    cred = current_cred(); // 获取当前进程的凭据

    // 将审计信息写入缓冲区
    *((int*)buffer) = cred->uid.val; // UID
    *((int*)buffer + 1) = current->pid; // PID
    *((int*)buffer + 2) = sockfd; // socket 描述符
    *((int*)buffer + 3) = len; // 接收的数据长度
    strcpy(commandname, "recvfrom"); // 设置命令名
    strcpy((char*)(4 + (int*)buffer), commandname); // 将命令名写入缓冲区
    strcpy((char*)(4 + TASK_COMM_LEN / 4 + (int*)buffer), fullname); // 将完整路径写入缓冲区
    netlink_sendmsg(buffer, size); // 发送 netlink 消息

    return 0; // 返回
}

// nl_data_ready 函数
// 该函数是 netlink 套接字的回调函数，用于处理收到的 netlink 消息。
void nl_data_ready(struct sk_buff *__skb)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;

    skb = skb_get (__skb); // 获取 sk_buff

    if (skb->len >= NLMSG_SPACE(0)) { // 检查消息长度
        nlh = nlmsg_hdr(skb); // 获取 netlink 消息头

        pid = nlh->nlmsg_pid; // 获取发送进程的 PID
        printk("net_link: pid is %d, data %s:\n", pid, (char *)NLMSG_DATA(nlh)); // 打印调试信息

        kfree_skb(skb); // 释放 sk_buff
    }
    return;
}

// netlink_init 函数
// 该函数用于初始化 netlink 套接字。
void netlink_init(void)
{
    struct netlink_kernel_cfg cfg = {
        .input = nl_data_ready, // 设置回调函数
    };

    nl_sk = netlink_kernel_create(&init_net, NETLINK_TEST, &cfg); // 创建 netlink 套接字

    if (!nl_sk) {
        printk(KERN_ERR "net_link: Cannot create netlink socket.\n"); // 创建失败时打印错误信息
        if (nl_sk != NULL)
            sock_release(nl_sk->sk_socket); // 释放套接字
    }
    else
        printk("net_link: create socket ok.\n"); // 创建成功时打印信息
}

// netlink_release 函数
// 该函数用于释放 netlink 套接字。
void netlink_release(void)
{
    if (nl_sk != NULL)
        sock_release(nl_sk->sk_socket); // 释放套接字
}
