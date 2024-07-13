
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

//发送netlink消息message
int netlink_sendmsg(const void *buffer, unsigned int size)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int len = NLMSG_SPACE(MAX_MSGSIZE);
	if((!buffer) || (!nl_sk) || (pid == 0))
		return 1;
	skb = alloc_skb(len, GFP_ATOMIC); 	//分配一个新的sk_buffer
	if (!skb){
		printk(KERN_ERR "net_link: allocat_skb failed.\n");
		return 1;
	}
	nlh = nlmsg_put(skb,0,0,0,MAX_MSGSIZE,0);
	NETLINK_CB(skb).creds.pid = 0;      /* from kernel */


	memcpy(NLMSG_DATA(nlh), buffer, size);


	if( netlink_unicast(nl_sk, skb, pid, MSG_DONTWAIT) < 0){
		printk(KERN_ERR "net_link: can not unicast skb \n");
		return 1;
	}
	return 0;
}


int get_fullname(struct dentry *parent_dentry,const char *pathname,char *fullname)
{
	char buf[MAX_LENGTH];
    char *filestart=NULL;


	memset(buf,0,MAX_LENGTH);

	if(!strncmp(pathname,"/",1))	//pathname is a fullname
	{
        strcpy(fullname,pathname);

        //printk("Info: get_fullname, fullname is  %s \n",fullname);
    }
    else // pathname is not a fullname, in the path with the dentry "parent_dentry"
	{

		filestart=dentry_path_raw(parent_dentry,buf,MAX_LENGTH);

        if (IS_ERR(filestart)){
            printk("Info: get_fullname error!\n");
            return 1;
        }

        strcpy(fullname,filestart);
        strcat(fullname,"/");
        strcat(fullname,pathname);

        //printk("Info: get_fullname, fullname is  %s \n",fullname);

    }

	return 0;
}


int AuditOpen(char * pathname,int flags,int ret)
{
    char commandname[TASK_COMM_LEN];
    char fullname[MAX_LENGTH];
    unsigned int size;   // = strlen(fullname) + 16 + TASK_COMM_LEN +1;
    void * buffer; // = kmalloc(size, 0);
    char auditpath[MAX_LENGTH];
    const struct cred *cred;
    struct dentry *parent_dentry;

    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);

    parent_dentry = current->fs->pwd.dentry;

    get_fullname(parent_dentry,pathname,fullname);

    strcpy(auditpath,AUDITPATH);

    if (strncmp(fullname,auditpath,strlen(auditpath)) != 0)
    	return 1;

    printk("Info: in AuditOpen; fullname is  %s \t; Auditpath is  %s \n",fullname,AUDITPATH);

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    memset(buffer, 0, size);

    strncpy(commandname,current->comm,TASK_COMM_LEN);
    cred = current_cred();
    *((int*)buffer) = cred->uid.val; //uid
    *((int*)buffer + 1) = current->pid;
    *((int*)buffer + 2) = flags; // flags: O_RDONLY、O_WRONLY、O_RDWR.......
    *((int*)buffer + 3) = ret;
    strcpy( (char*)( 4 + (int*)buffer),commandname);
    strcpy( (char*)( 4 + TASK_COMM_LEN/4 +(int*)buffer),fullname);
    netlink_sendmsg(buffer, size);

    return 0;
}

int AuditOpenat(int dfd, char * pathname, int flags, int ret)
{
    char commandname[TASK_COMM_LEN];
    char fullname[MAX_LENGTH];
    char auditpath[MAX_LENGTH];
    char buf[MAX_LENGTH];
    struct file * filep=NULL;
    unsigned int size;   // = strlen(fullname) + 16 + TASK_COMM_LEN +1;
    void * buffer; // = kmalloc(size, 0);
    const struct cred *cred;
    struct dentry *parent_dentry;

    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);
    memset(buf, 0, MAX_LENGTH);


    if(!strncmp(pathname,"/",1))	//pathname is a fullname
    	strcpy(fullname,pathname);
    else
    {

    	if(dfd==AT_FDCWD)	//pathname is in current path
    	{
    		parent_dentry = current->fs->pwd.dentry;
    		get_fullname(parent_dentry,pathname,fullname);
    	}
    	else	//pathname is in the path with dfd
    	{

    	    //printk("Info: AuditOpenat, dfd: %d, pathname: %s\n",dfd,pathname);
    	    filep=fget_raw(dfd);

    	    parent_dentry=filep->f_path.dentry;

    	    get_fullname(parent_dentry,pathname,fullname);

    	}

      }


    strcpy(auditpath,AUDITPATH);


    if (strncmp(fullname,auditpath,strlen(auditpath)) != 0)
    	return 1;

    printk("Info: in AuditOpenat, fullname is  %s \t; Auditpath is  %s \n",fullname,AUDITPATH);

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    memset(buffer, 0, size);

    strncpy(commandname,current->comm,TASK_COMM_LEN);
    cred = current_cred();
    *((int*)buffer) = cred->uid.val; //uid
    *((int*)buffer + 1) = current->pid;
    *((int*)buffer + 2) = flags; // flags: O_RDONLY、O_WRONLY、O_RDWR.......
    *((int*)buffer + 3) = ret;
    strcpy( (char*)( 4 + (int*)buffer ), commandname);
    strcpy( (char*)( 4 + TASK_COMM_LEN/4 +(int*)buffer ), fullname);
    netlink_sendmsg(buffer, size);


    return 0;
}


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

    parent_dentry = current->fs->pwd.dentry;

    get_fullname(parent_dentry, pathname, fullname);

    strcpy(auditpath, AUDITPATH);

    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    memset(buffer, 0, size);

    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();
    *((int *)buffer) = cred->uid.val;
    *((int *)buffer + 1) = current->pid;
    *((int *)buffer + 2) = O_WRONLY; // 用 O_WRONLY 表示写操作
    *((int *)buffer + 3) = ret;
    strcpy((char *)(4 + (int *)buffer), commandname);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), fullname);
    netlink_sendmsg(buffer, size);

    return 0;
}

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

    if (!strncmp(pathname, "/", 1))
        strcpy(fullname, pathname);
    else
    {
        if (dfd == AT_FDCWD)
        {
            parent_dentry = current->fs->pwd.dentry;
            get_fullname(parent_dentry, pathname, fullname);
        }
        else
        {
            filep = fget_raw(dfd);
            parent_dentry = filep->f_path.dentry;
            get_fullname(parent_dentry, pathname, fullname);
        }
    }

    strcpy(auditpath, AUDITPATH);

    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    memset(buffer, 0, size);

    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();
    *((int *)buffer) = cred->uid.val;
    *((int *)buffer + 1) = current->pid;
    *((int *)buffer + 2) = O_WRONLY;
    *((int *)buffer + 3) = ret;
    strcpy((char *)(4 + (int *)buffer), commandname);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), fullname);
    netlink_sendmsg(buffer, size);

    return 0;
}

int AuditExec(const char *pathname, int ret) {
    char commandname[TASK_COMM_LEN];
    char fullname[MAX_LENGTH];
    unsigned int size;
    void *buffer;
    char auditpath[MAX_LENGTH];
    const struct cred *cred;
    struct dentry *parent_dentry;

    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);

    parent_dentry = current->fs->pwd.dentry;

    get_fullname(parent_dentry, pathname, fullname);

    strcpy(auditpath, AUDITPATH);

    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    printk("Info: in AuditExec, fullname is  %s \t; Auditpath is  %s \n", fullname, AUDITPATH);

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    memset(buffer, 0, size);

    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();
    *((int *)buffer) = cred->uid.val; // uid
    *((int *)buffer + 1) = current->pid;
    *((int *)buffer + 2) = 0; // 使用 O_RDONLY 表示执行操作
    *((int *)buffer + 3) = ret;
    strcpy((char *)(4 + (int *)buffer), commandname);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), fullname);
    netlink_sendmsg(buffer, size);

    return 0;
}


int AuditExecat(int dfd, char *pathname, int flags, int ret) {
    char commandname[TASK_COMM_LEN];
    char fullname[MAX_LENGTH];
    char auditpath[MAX_LENGTH];
    struct file *filep = NULL;
    unsigned int size;
    void *buffer;
    const struct cred *cred;
    struct dentry *parent_dentry;

    memset(fullname, 0, MAX_LENGTH);
    memset(auditpath, 0, MAX_LENGTH);

    if (!strncmp(pathname, "/", 1)) // 如果 pathname 是绝对路径
        strcpy(fullname, pathname);
    else {
        if (dfd == AT_FDCWD) {
            parent_dentry = current->fs->pwd.dentry;
            get_fullname(parent_dentry, pathname, fullname);
        } else {
            filep = fget_raw(dfd);
            parent_dentry = filep->f_path.dentry;
            get_fullname(parent_dentry, pathname, fullname);
        }
    }

    strcpy(auditpath, AUDITPATH);

    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    printk("Info: in AuditExecat, fullname is  %s \t; Auditpath is  %s \n", fullname, AUDITPATH);

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    memset(buffer, 0, size);

    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();
    *((int *)buffer) = cred->uid.val; // uid
    *((int *)buffer + 1) = current->pid;
    *((int *)buffer + 2) = 0; // 根据 flags 确定具体的执行权限
    *((int *)buffer + 3) = ret;
    strcpy((char *)(4 + (int *)buffer), commandname);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), fullname);
    netlink_sendmsg(buffer, size);

    return 0;
}


int AuditShutdown(const char *message, int ret)
{
    char commandname[TASK_COMM_LEN];
    unsigned int size;
    void *buffer;
    const struct cred *cred;

    size = strlen(message) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, GFP_KERNEL);
    if (!buffer) {
        printk(KERN_ERR "AuditShutdown: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size);

    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();

    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid;
    *((int *)buffer + 2) = 0; // Shutdown 没有 flags 参数，所以设置为 0
    *((int *)buffer + 3) = ret; // 返回值
    strcpy((char *)(4 + (int *)buffer), commandname);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), message);

    netlink_sendmsg(buffer, size);
    kfree(buffer);

    return 0;
}


int AuditReboot(const char *message, int ret)
{
    char commandname[TASK_COMM_LEN];
    unsigned int size;
    void *buffer;
    const struct cred *cred;

    size = strlen(message) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    if (!buffer) {
        printk(KERN_ERR "AuditReboot: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size);

    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();

    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid;
    *((int *)buffer + 2) = 0; // reboot 没有 flags 参数，所以设置为 0
    *((int *)buffer + 3) = ret; // 返回值
    strcpy((char *)(4 + (int *)buffer), commandname);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), message);

    netlink_sendmsg(buffer, size);
    kfree(buffer);

    return 0;
}


// 函数声明和必要的包含文件在之前已经给出，这里假设这些已经存在
#include <linux/module.h>
#include <linux/kernel.h>

// AuditInsmod 函数
int AuditInsmod(const char *pathname, int ret)
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

    parent_dentry = current->fs->pwd.dentry;

    get_fullname(parent_dentry, pathname, fullname);

    strcpy(auditpath, AUDITPATH);

    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    printk("Info: in AuditInsmod, fullname is %s \t; Auditpath is %s \n", fullname, AUDITPATH);

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, GFP_KERNEL);
    if (!buffer) {
        printk(KERN_ERR "AuditInsmod: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size);

    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid;
    *((int *)buffer + 2) = 0; // 使用 0 表示 insmod 操作
    *((int *)buffer + 3) = ret;
    strcpy((char *)(4 + (int *)buffer), commandname);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), fullname);
    netlink_sendmsg(buffer, size);
    kfree(buffer);

    return 0;
}

// AuditRmmod 函数
int AuditRmmod(const char *pathname, int ret)
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

    parent_dentry = current->fs->pwd.dentry;

    get_fullname(parent_dentry, pathname, fullname);

    strcpy(auditpath, AUDITPATH);

    if (strncmp(fullname, auditpath, strlen(auditpath)) != 0)
        return 1;

    printk("Info: in AuditRmmod, fullname is %s \t; Auditpath is %s \n", fullname, AUDITPATH);

    size = strlen(fullname) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, GFP_KERNEL);
    if (!buffer) {
        printk(KERN_ERR "AuditRmmod: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size);

    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid;
    *((int *)buffer + 2) = 0; // 使用 0 表示 rmmod 操作
    *((int *)buffer + 3) = ret;
    strcpy((char *)(4 + (int *)buffer), commandname);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), fullname);
    netlink_sendmsg(buffer, size);
    kfree(buffer);

    return 0;
}


// int AuditDeviceAdd(char *pathname, int ret)
// {
    
// }

// int AuditDeviceAddat(int dfd, char *pathname, int flags, int ret)
// {

// }


// int AuditDeviceRemove(char *pathname, int ret)
// {
    
// }

// int AuditDeviceRemoveat(int dfd, char *pathname, int flags, int ret)
// {

// }


int AuditMount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data, int ret)
{
    char commandname[TASK_COMM_LEN];
    unsigned int size;
    void *buffer;
    const struct cred *cred;

    // 计算缓冲区大小
    size = strlen(source) + strlen(target) + strlen(filesystemtype) + 32 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, GFP_KERNEL);
    if (!buffer) {
        printk(KERN_ERR "AuditMount: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size);

    // 获取当前进程的命令名和凭据
    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();

    // 将信息写入缓冲区
    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid;
    *((unsigned long *)buffer + 2) = mountflags;
    *((int *)buffer + 3) = ret; // 返回值
    strcpy((char *)(4 + (int *)buffer), commandname);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), source);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + strlen(source) + 1 + (int *)buffer), target);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + strlen(source) + strlen(target) + 2 + (int *)buffer), filesystemtype);

    // 发送 netlink 消息
    netlink_sendmsg(buffer, size);
    kfree(buffer);

    return 0;
}

int AuditUnmount2(const char *target, int flags, int ret)
{
    char commandname[TASK_COMM_LEN];
    unsigned int size;
    void *buffer;
    const struct cred *cred;

    size = strlen(target) + 16 + TASK_COMM_LEN + 1;
    buffer = kmalloc(size, 0);
    if (!buffer) {
        printk(KERN_ERR "AuditUnmount2: kmalloc failed\n");
        return -ENOMEM;
    }
    memset(buffer, 0, size);

    strncpy(commandname, current->comm, TASK_COMM_LEN);
    cred = current_cred();

    *((int *)buffer) = cred->uid.val; // UID
    *((int *)buffer + 1) = current->pid;
    *((int *)buffer + 2) = flags;
    *((int *)buffer + 3) = ret;
    strcpy((char *)(4 + (int *)buffer), commandname);
    strcpy((char *)(4 + TASK_COMM_LEN / 4 + (int *)buffer), target);

    netlink_sendmsg(buffer, size);
    kfree(buffer);

    return 0;
}



// int AuditHttp(char *pathname, int ret)
// {
    
// }

// int AuditHttpat(int dfd, char *pathname, int flags, int ret)
// {

// }


// int AuditFtp(char *pathname, int ret)
// {
    
// }

// int AuditFtpat(int dfd, char *pathname, int flags, int ret)
// {

// }


// int AuditSsh(char *pathname, int ret)
// {
    
// }

// int AuditSshat(int dfd, char *pathname, int flags, int ret)
// {

// }


// int AuditDatabase(char *pathname, int ret)
// {
    
// }

// int AuditDatabaseat(int dfd, char *pathname, int flags, int ret)
// {

// }


void nl_data_ready(struct sk_buff *__skb)
 {
    struct sk_buff *skb;
    struct nlmsghdr *nlh;

    skb = skb_get (__skb);

    if (skb->len >= NLMSG_SPACE(0)) {
	nlh = nlmsg_hdr(skb);

	pid = nlh->nlmsg_pid; /*pid of sending process */
	printk("net_link: pid is %d, data %s:\n", pid, (char *)NLMSG_DATA(nlh));

	kfree_skb(skb);
    }
    return;
}



void netlink_init(void) {
    struct netlink_kernel_cfg cfg = {
        .input = nl_data_ready,
    };

    nl_sk=netlink_kernel_create(&init_net,NETLINK_TEST, &cfg);

    if (!nl_sk)
    {
	printk(KERN_ERR "net_link: Cannot create netlink socket.\n");
	if (nl_sk != NULL)
    		sock_release(nl_sk->sk_socket);
    }
    else
    	printk("net_link: create socket ok.\n");
}


void netlink_release(void) {
    if (nl_sk != NULL)
 	sock_release(nl_sk->sk_socket);
}
