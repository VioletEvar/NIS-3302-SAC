#include <linux/string.h>
#include <linux/mm.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/sched.h>
#include <linux/fs_struct.h>
#include <linux/limits.h>

#define TASK_COMM_LEN 16
#define NETLINK_TEST 29
#define AUDITPATH "/home/TestAudit"
#define MAX_LENGTH 256
#define MAX_MSGSIZE 1200

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


int get_fullname(struct dentry *parent_dentry,char *pathname,char *fullname)
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