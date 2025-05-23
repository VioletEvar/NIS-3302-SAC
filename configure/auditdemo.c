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

#define TM_FMT "%Y-%m-%d %H:%M:%S" // 定义时间格式

#define NETLINK_TEST 29 // 定义Netlink协议号
#define MAX_PAYLOAD 1024  // 定义Netlink消息的最大有效载荷大小
#define AUDITPATH "/home/wbw/Desktop/test" // 定义审计路径
int sock_fd; // Netlink socket文件描述符
struct msghdr msg; // 消息头结构体
struct nlmsghdr *nlh = NULL; // Netlink消息头指针
struct sockaddr_nl src_addr, dest_addr; // 源地址和目标地址
struct iovec iov; // I/O向量

FILE *logfile; // 日志文件指针

// 检查文件路径是否以指定前缀开头
int starts_with(const char *file_path, const char *prefix) {
    size_t prefix_len = strlen(prefix);
    return strncmp(file_path, prefix, prefix_len) == 0;
}

// 记录日志
void Log(char *commandname, int uid, int pid, char *file_path, int flags, int ret) {
    char logtime[64];
    char username[32];
    struct passwd *pwinfo;
    char operationresult[10];
    char operationtype[16];

    // 如果文件路径不以AUDITPATH开头，则不记录
    if (starts_with(file_path, AUDITPATH) == 0) {
        return;
    }

    // 设置操作结果
    if (ret >= 0) strcpy(operationresult, "success");
    else strcpy(operationresult, "failed");

    // 根据命令名设置操作类型
    if (strcmp(commandname, "rm") == 0) {
        strcpy(operationtype, "Delete");
    } else if (strcmp(commandname, "insmod") == 0) {
        strcpy(operationtype, "ModuleInstall");
    } else if (strcmp(commandname, "rmmod") == 0) {
        strcpy(operationtype, "ModuleRemove");
    } else if (strcmp(commandname, "reboot") == 0) {
        strcpy(operationtype, "Reboot");
    } else if (strcmp(commandname, "shutdown") == 0) {
        strcpy(operationtype, "Shutdown");
    } else if (strcmp(commandname, "mount") == 0) {
        strcpy(operationtype, "Mount");
    } else if (strcmp(commandname, "umount") == 0) {
        strcpy(operationtype, "Umount");
    } else if (strcmp(commandname, "socket") == 0) {
        strcpy(operationtype, "Socket");
    } else if (strcmp(commandname, "connect") == 0) {
        strcpy(operationtype, "Connect");
    } else if (strcmp(commandname, "accept") == 0) {
        strcpy(operationtype, "Accept");
    } else if (strcmp(commandname, "sendto") == 0) {
        strcpy(operationtype, "Sendto");
    } else if (strcmp(commandname, "recvfrom") == 0) {
        strcpy(operationtype, "Recvfrom");
    } else if (strcmp(commandname, "close") == 0) {
        strcpy(operationtype, "Close");
    } else if (flags == 524288) {
        strcpy(operationtype, "Execute");
    } else {
        if (flags & O_RDONLY) strcpy(operationtype, "Read");
        else if (flags & O_WRONLY) strcpy(operationtype, "Write");
        else if (flags & O_RDWR) strcpy(operationtype, "Read/Write");
        else strcpy(operationtype, "Other");
    }

    // 获取当前时间
    time_t t = time(0);
    if (logfile == NULL) return;
    pwinfo = getpwuid(uid); // 获取用户信息
    strcpy(username, pwinfo->pw_name);

    strftime(logtime, sizeof(logtime), TM_FMT, localtime(&t));
    // 记录日志到文件
    fprintf(logfile, "%s(%d) %s(%d) %s \"%s\" %s %s\n", username, uid, commandname, pid, logtime, file_path, operationtype, operationresult);
    fflush(logfile);
}

// 发送进程ID给Netlink
void sendpid(unsigned int pid) {
    // 初始化消息
    memset(&msg, 0, sizeof(msg));
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = pid;  // 自己的PID
    src_addr.nl_groups = 0;  // 不属于组播组
    bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;   // 发送给内核
    dest_addr.nl_groups = 0; // 单播

    // 填充Netlink消息头
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = pid;  // 自己的PID
    nlh->nlmsg_flags = 0;
    // 填充Netlink消息的有效载荷
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    printf(" Sending message pid. ...\n");
    sendmsg(sock_fd, &msg, 0);
}

// 处理进程被杀死的情况
void killdeal_func() {
    printf("The process is killed! \n");
    close(sock_fd); // 关闭socket
    if (logfile != NULL)
        fclose(logfile); // 关闭日志文件
    if (nlh != NULL)
        free(nlh); // 释放Netlink消息头
    exit(0); // 退出进程
}

int main(int argc, char *argv[]) {
    char buff[110];
    char logpath[32];
    if (argc == 1) strcpy(logpath, "./log"); // 默认日志路径
    else if (argc == 2) strncpy(logpath, argv[1], 32); // 使用命令行参数指定日志路径
    else {
        printf("commandline parameters error! please check and try it! \n");
        exit(1); // 命令行参数错误，退出
    }

    signal(SIGTERM, killdeal_func); // 捕捉SIGTERM信号
    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_TEST); // 创建Netlink socket
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD)); // 分配Netlink消息头
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD)); // 清空消息头

    sendpid(getpid()); // 发送进程ID给Netlink

    // 在程序开始时打开日志文件，避免该操作引起死锁
    logfile = fopen(logpath, "w+");
    if (logfile == NULL) {
        printf("Warning: can not create log file\n");
        exit(1); // 无法创建日志文件，退出
    }
    // 循环接收消息
    while(1) {
        unsigned int uid, pid, flags, ret;
        char * file_path;
        char * commandname;

        recvmsg(sock_fd, &msg, 0); // 从内核接收消息

        file_path = (char *)(4 + 16/4 + (int *)NLMSG_DATA(nlh));
        if (starts_with(file_path, AUDITPATH) == 0) {
            continue; // 如果文件路径不以AUDITPATH开头，则跳过
        }

        uid = *( (unsigned int *)NLMSG_DATA(nlh) );
        printf("uid: %d \n", uid);
        pid = *( 1 + (int *)NLMSG_DATA(nlh) );
        printf("pid: %d \n", pid);
        flags = *( 2 + (int *)NLMSG_DATA(nlh) );
        printf("flags: %d \n", flags);
        ret = *( 3 + (int *)NLMSG_DATA(nlh) );
        printf("ret: %d \n", ret);
        commandname = (char *)( 4 + (int *)NLMSG_DATA(nlh));
        printf("commandname: %s \n", commandname);
        file_path = (char *)( 4 + 16/4 + (int *)NLMSG_DATA(nlh));
        printf("file_path: %s \n", file_path);
        Log(commandname, uid, pid, file_path, flags, ret); // 记录日志
    }
    close(sock_fd); // 关闭socket
    free(nlh); // 释放Netlink消息头
    fclose(logfile); // 关闭日志文件
    return 0;
