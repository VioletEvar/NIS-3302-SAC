#include <stdio.h>        // 标准输入输出库，提供基本的输入输出功能
#include <stdlib.h>       // 标准库，包含内存分配、程序退出等功能
#include <string.h>       // 字符串操作库，提供字符串处理函数
#include <unistd.h>       // Unix标准函数库，提供访问系统功能的函数
#include <fcntl.h>        // 文件控制选项，提供文件操作相关的定义
#include <time.h>         // 时间库，提供时间获取和处理功能
#include <pwd.h>          // 密码库，提供用户账户信息的获取功能
#include <libudev.h>      // udev库，提供设备管理功能
#include <sys/types.h>    // 系统数据类型定义，提供基本数据类型
#include <sys/select.h>   // select函数库，提供多路输入输出选择功能
#include <sys/socket.h>   // 套接字库，提供网络通信相关功能

// 定义日志文件的路径
#define LOG_FILE "/home/szy/Desktop/demo/log"

// 定义时间戳格式
#define TIMESTAMP_FMT "%Y-%m-%d %H:%M:%S"

// 获取当前时间的函数
char *get_current_time() {
    time_t now = time(NULL);             // 获取当前时间（自1970年1月1日以来的秒数）
    struct tm *tm_info;                 // 用于存储本地时间的结构体
    static char timestamp[20];          // 存储格式化时间的字符串
    tm_info = localtime(&now);          // 将时间转换为本地时间
    strftime(timestamp, sizeof(timestamp), TIMESTAMP_FMT, tm_info); // 格式化时间
    return timestamp;                   // 返回格式化的时间字符串
}

// 获取当前用户名的函数
char *get_username() {
    uid_t uid = geteuid();              // 获取有效用户ID
    struct passwd *pw = getpwuid(uid); // 根据用户ID获取用户信息
    if (pw) {                          // 如果成功获取到用户信息
        return pw->pw_name;            // 返回用户名
    } else {
        return "unknown";              // 如果获取失败，返回"unknown"
    }
}

// 记录事件到日志文件的函数
void log_event(const char *action, const char *device) {
    FILE *logfile = fopen(LOG_FILE, "a"); // 以追加模式打开日志文件
    if (logfile) {                       // 如果文件打开成功
        char *timestamp = get_current_time(); // 获取当前时间
        char *username = get_username();     // 获取当前用户名
        pid_t pid = getpid();                // 获取当前进程ID
        // 写入日志文件
        fprintf(logfile, "%s(%d) %s(%d) %s \"/dev/%s\" Device %s\n", 
                username, getuid(), action, pid, timestamp, device, action);
        fclose(logfile);                  // 关闭日志文件
    }
}

// 处理设备事件的函数
void device() {
    struct udev *udev;                  // udev对象，用于管理设备
    struct udev_monitor *mon;           // udev监视器，用于监视设备事件
    struct udev_device *dev;            // udev设备对象

    udev = udev_new();                  // 创建新的udev对象
    if (!udev) {                        // 如果创建失败
        fprintf(stderr, "Cannot create udev\n"); // 输出错误信息
        return;
    }

    // 创建udev监视器，并通过netlink连接
    mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "block", "disk"); // 设置过滤条件，监视块设备（磁盘）
    udev_monitor_enable_receiving(mon); // 启用接收事件

    int fd = udev_monitor_get_fd(mon);  // 获取udev监视器的文件描述符
    if (fd < 0) {                       // 如果获取失败
        fprintf(stderr, "Failed to get udev monitor file descriptor\n"); // 输出错误信息
        udev_unref(udev);               // 释放udev对象
        return;
    }

    while (1) {                         // 无限循环，持续监视设备事件
        fd_set fds;                     // 文件描述符集合
        FD_ZERO(&fds);                 // 清空文件描述符集合
        FD_SET(fd, &fds);              // 将udev监视器的文件描述符添加到集合中

        // 使用select函数等待事件发生
        int ret = select(fd + 1, &fds, NULL, NULL, NULL);
        if (ret < 0) {                  // 如果select函数出错
            perror("select");           // 输出错误信息
            break;                      // 退出循环
        }

        if (FD_ISSET(fd, &fds)) {       // 如果udev监视器的文件描述符有事件发生
            dev = udev_monitor_receive_device(mon); // 接收设备事件
            if (dev) {                  // 如果成功接收到设备事件
                const char *action = udev_device_get_action(dev); // 获取设备动作（add或remove）
                const char *devnode = udev_device_get_devnode(dev); // 获取设备节点
                if (action && devnode) { // 如果动作和设备节点都有效
                    const char *devname = strrchr(devnode, '/'); // 从设备节点中获取设备名称
                    if (devname) {
                        devname++; // 跳过'/'字符
                        // 根据设备动作记录日志
                        if (strcmp(action, "add") == 0) {
                            log_event("deviceadd", devname);
                        } else if (strcmp(action, "remove") == 0) {
                            log_event("deviceremove", devname);
                        }
                    }
                }
                udev_device_unref(dev); // 释放设备对象
            }
        }
    }

    udev_unref(udev); // 释放udev对象
}

// 主函数
int main() {
    device(); // 调用设备处理函数
    return 0; // 程序正常退出
}
