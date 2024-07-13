#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <fcntl.h>
#include <asm/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <pwd.h>

#define TM_FMT "%Y-%m-%d %H:%M:%S"

#define NETLINK_TEST 29
#define MAX_PAYLOAD 1024  /* maximum payload size*/
int sock_fd;
struct msghdr msg;
struct nlmsghdr *nlh = NULL;
struct sockaddr_nl src_addr, dest_addr;
struct iovec iov;

FILE *logfile;

void Log(char *commandname,int uid, int pid, char *file_path, int flags,int ret)
{
    char logtime[64];
    char username[32];
    struct passwd *pwinfo;
    char operationresult[10];
    char operationtype[16];

    if (ret >= 0) strcpy(operationresult,"success");
    else strcpy(operationresult,"failed");

    if (strcmp(commandname, "rm") == 0) {
        strcpy(operationtype, "Delete");
    } 
    else if (strcmp(commandname, "insmod") == 0 || strcmp(commandname, "rmmod") == 0)
    {
        strcpy(operationtype, "Module");
    }
    else if (strcmp(commandname, "reboot") == 0) {
        strcpy(operationtype, "Reboot");
    }
    else if (strcmp(commandname, "shutdown") == 0) {
        strcpy(operationtype, "Shutdown");
    }
    else if (strcmp(commandname, "mount") == 0) {
        strcpy(operationtype, "Mount");
    }
    else if (strcmp(commandname, "umount") == 0) {
        strcpy(operationtype, "Umount");
    }
    else if (flags == 524288) {
        strcpy(operationtype, "Execute");
    } 
    else {
        if (flags & O_RDONLY) strcpy(operationtype, "Read");
        else if (flags & O_WRONLY) strcpy(operationtype, "Write");
        else if (flags & O_RDWR) strcpy(operationtype, "Read/Write");
        else strcpy(operationtype, "Other");
    }

    time_t t=time(0);
    if (logfile == NULL)    return;
    pwinfo = getpwuid(uid);
    strcpy(username,pwinfo->pw_name);

    strftime(logtime, sizeof(logtime), TM_FMT, localtime(&t) );
    fprintf(logfile,"%s(%d) %s(%d) %s \"%s\" %s %s\n",username,uid,commandname,pid,logtime,file_path,operationtype, operationresult);
    fflush(logfile);
}


void sendpid(unsigned int pid)
{
    //Send message to initialize
    memset(&msg, 0, sizeof(msg));
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = pid;  //self pid
    src_addr.nl_groups = 0;  //not in mcast groups
    bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;   //For Linux Kernel
    dest_addr.nl_groups = 0; //unicast

    /* Fill the netlink message header */
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = pid;  /* self pid */
    nlh->nlmsg_flags = 0;
    /* Fill in the netlink message payload */
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    printf(" Sending message pid. ...\n");
    sendmsg(sock_fd, &msg, 0);
}

void killdeal_func()
{
    printf("The process is killed! \n");
    close(sock_fd);
    if (logfile != NULL)
        fclose(logfile);
    if (nlh != NULL)
        free(nlh);
    exit(0);
}

int main(int argc, char *argv[]){
    char buff[110];
    //void killdeal_func();
    char logpath[32];
    if (argc == 1) strcpy(logpath,"./log");
    else if (argc == 2) strncpy(logpath, argv[1],32);
    else {
        printf("commandline parameters error! please check and try it! \n");
        exit(1);
    }


    signal(SIGTERM,killdeal_func);
    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_TEST);
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));

    sendpid(getpid());

    /* open the log file at the begining of demo, in case of this operation causes deadlock */
    logfile=fopen(logpath, "w+");
    if (logfile == NULL) {
        printf("Warning: can not create log file\n");
        exit(1);
    }
    //Loop to get message
    while(1){    //Read message from kernel
        unsigned int uid, pid,flags,ret;
        char * file_path;
        char * commandname;

        recvmsg(sock_fd, &msg, 0);

        uid = *( (unsigned int *)NLMSG_DATA(nlh) );
        printf("uid: %d \n",uid);
        pid = *( 1 + (int *)NLMSG_DATA(nlh)  );
        printf("pid: %d \n",pid);
        flags = *( 2 + (int *)NLMSG_DATA(nlh)  );
        printf("flags: %d \n",flags);
        ret = *( 3 + (int *)NLMSG_DATA(nlh)  );
        printf("ret: %d \n",ret);
        commandname = (char *)( 4 + (int *)NLMSG_DATA(nlh));
        printf("commandname: %s \n",commandname);
        file_path = (char *)( 4 + 16/4 + (int *)NLMSG_DATA(nlh));
        printf("file_path: %s \n",file_path);
        Log(commandname, uid, pid, file_path, flags, ret);

    }
    close(sock_fd);
    free(nlh);
    fclose(logfile);
    return 0;
}
