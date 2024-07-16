#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <libudev.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#define LOG_FILE "/home/szy/Desktop/demo/log"
#define TIMESTAMP_FMT "%Y-%m-%d %H:%M:%S"

char *get_current_time() {
    time_t now = time(NULL);
    struct tm *tm_info;
    static char timestamp[20];
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), TIMESTAMP_FMT, tm_info);
    return timestamp;
}

char *get_username() {
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        return pw->pw_name;
    } else {
        return "unknown";
    }
}

void log_event(const char *action, const char *device) {
    FILE *logfile = fopen(LOG_FILE, "a");
    if (logfile) {
        char *timestamp = get_current_time();
        char *username = get_username();
        pid_t pid = getpid();
        fprintf(logfile, "%s(%d) %s(%d) %s \"/dev/%s\" Device %s\n", 
                username, getuid(), action, pid, timestamp, device, action);
        fclose(logfile);
    }
}

void device() {
    struct udev *udev;
    struct udev_monitor *mon;
    struct udev_device *dev;

    udev = udev_new();
    if (!udev) {
        fprintf(stderr, "Cannot create udev\n");
        return;
    }

    mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "block", "disk");
    udev_monitor_enable_receiving(mon);

    int fd = udev_monitor_get_fd(mon);
    if (fd < 0) {
        fprintf(stderr, "Failed to get udev monitor file descriptor\n");
        udev_unref(udev);
        return;
    }

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        int ret = select(fd + 1, &fds, NULL, NULL, NULL);
        if (ret < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(fd, &fds)) {
            dev = udev_monitor_receive_device(mon);
            if (dev) {
                const char *action = udev_device_get_action(dev);
                const char *devnode = udev_device_get_devnode(dev);
                if (action && devnode) {
                    const char *devname = strrchr(devnode, '/');
                    if (devname) {
                        devname++; // move past '/'
                        if (strcmp(action, "add") == 0) {
                            log_event("deviceadd", devname);
                        } else if (strcmp(action, "remove") == 0) {
                            log_event("deviceremove", devname);
                        }
                    }
                }
                udev_device_unref(dev);
            }
        }
    }

    udev_unref(udev);
}

int main() {
    device();
    return 0;
}
