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
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <sstream>
#include <atomic>

#define TM_FMT "%Y-%m-%d %H:%M:%S"
#define NETLINK_TEST 29
#define MAX_PAYLOAD 1024  /* maximum payload size*/

int sock_fd;
struct msghdr msg;
struct nlmsghdr *nlh = nullptr;
struct sockaddr_nl src_addr, dest_addr;
struct iovec iov;

std::ofstream logfile;
std::mutex log_mutex;
std::mutex config_mutex;
std::unordered_map<std::string, bool> resource_flags;
std::atomic<bool> config_changed(false);
std::atomic<bool> config_updated(false);

void readConfig(const std::string& config_file) {
    std::unordered_map<std::string, bool> temp_flags;
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Error opening config file." << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string resource;
        bool flag;
        if (!(iss >> resource >> flag)) {
            std::cerr << "Error reading config line: " << line << std::endl;
            continue;
        }
        temp_flags[resource] = flag;
    }
    std::lock_guard<std::mutex> lock(config_mutex);
    resource_flags = std::move(temp_flags);
    config_updated = true;
}

void writeConfig(const std::string& config_file) {
    std::lock_guard<std::mutex> lock(config_mutex);
    std::ofstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Error opening config file for writing." << std::endl;
        return;
    }
    for (const auto& [resource, flag] : resource_flags) {
        file << resource << " " << flag << std::endl;
    }
}

void Log(const std::string& commandname, int uid, int pid, const std::string& file_path, int flags, int ret) {
    bool should_log = false;
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        if (resource_flags.find(commandname) != resource_flags.end() && resource_flags[commandname]) {
            should_log = true;
        }
    }

    if (!should_log) {
        return;
    }

    char logtime[64];
    char username[32];
    struct passwd *pwinfo;
    std::string operationresult;
    std::string operationtype;
    //char* operationresult;
    //char* operationtype;
    if (ret >= 0) strcpy(operationresult,"success");
	else strcpy(operationresult,"failed");

	if (strcmp(commandname, "rm") == 0) {
        strcpy(operationtype, "Delete");
    } else {
        if (flags & O_RDONLY) strcpy(operationtype, "Read");
        else if (flags & O_WRONLY) strcpy(operationtype, "Write");
        else if (flags & O_RDWR) strcpy(operationtype, "Read/Write");
        else strcpy(operationtype, "Other");
    }

    time_t t = time(0);
    if (!logfile.is_open()) return;
    pwinfo = getpwuid(uid);
    strcpy(username, pwinfo->pw_name);

    strftime(logtime, sizeof(logtime), TM_FMT, localtime(&t));
    
    std::lock_guard<std::mutex> lock_log(log_mutex);
    logfile << username << "(" << uid << ") " << commandname << "(" << pid << ") " << logtime 
            << " \"" << file_path << "\" " << operationtype << " " << operationresult << std::endl;
    logfile.flush();
}

void sendpid(unsigned int pid) {
    // Send message to initialize
    memset(&msg, 0, sizeof(msg));
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = pid;  // self pid
    src_addr.nl_groups = 0;  // not in mcast groups
    bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;   // For Linux Kernel
    dest_addr.nl_groups = 0; // unicast

    // Fill the netlink message header
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = pid;  // self pid
    nlh->nlmsg_flags = 0;

    strcpy((char *)NLMSG_DATA(nlh), "Hello");
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    std::cout << "Sending message pid. ..." << std::endl;
    sendmsg(sock_fd, &msg, 0);
}

void killdeal_func(int signum) {
    std::cout << "The process is killed!" << std::endl;
    close(sock_fd);
    if (logfile.is_open())
        logfile.close();
    if (nlh != nullptr)
        free(nlh);
    exit(0);
}

void message_loop() {
    while (true) {  // Read message from kernel
        unsigned int uid, pid, flags, ret;
        char *file_path;
        char *commandname;
        
        recvmsg(sock_fd, &msg, 0);
        // Analyze log info
        uid = *(reinterpret_cast<unsigned int *>(NLMSG_DATA(nlh)));
        pid = *(1 + reinterpret_cast<int *>(NLMSG_DATA(nlh)));
        flags = *(2 + reinterpret_cast<int *>(NLMSG_DATA(nlh)));
        ret = *(3 + reinterpret_cast<int *>(NLMSG_DATA(nlh)));
        commandname = reinterpret_cast<char *>(4 + reinterpret_cast<int *>(NLMSG_DATA(nlh)));
        file_path = reinterpret_cast<char *>(4 + 16 / 4 + reinterpret_cast<int *>(NLMSG_DATA(nlh)));

        // Check resource flags
        bool should_log = false;
        {
            std::lock_guard<std::mutex> lock(config_mutex);
            auto it = resource_flags.find(commandname);
            if (it != resource_flags.end() && it->second) {
                should_log = true;
            }
        }

        if (!should_log) {
            continue;
        }

        // Print log info
        std::cout << "uid: " << uid << std::endl;
        std::cout << "pid: " << pid << std::endl;
        std::cout << "flags: " << flags << std::endl;
        std::cout << "ret: " << ret << std::endl;
        std::cout << "commandname: " << commandname << std::endl;
        std::cout << "file_path: " << file_path << std::endl;
        
        // Generate log
        Log(commandname, uid, pid, file_path, flags, ret);
    }
}

void monitorConfig(const std::string& config_file) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        readConfig(config_file);
        config_changed = true;
    }
}

void logGeneration() {
    while (true) {
        if (config_changed || config_updated) {
            {
                std::lock_guard<std::mutex> lock(config_mutex);
                config_changed = false;
                config_updated = false;
            }
            // Only print "logging enabled for resource" message when config is updated
            for (const auto& [resource, flag] : resource_flags) {
                if (flag) {
                    std::cout << "Logging enabled for resource: " << resource << std::endl;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void userInteraction(const std::string& config_file) {
    std::string command;
    while (true) {
        std::cout << "Enter 'update' to modify flags, 'exit' to quit: ";
        std::cin >> command;
        if (command == "update") {
            std::string resource;
            bool flag;
            std::cout << "Enter resource name: ";
            std::cin >> resource;
            std::cout << "Enter flag (0 or 1): ";
            std::cin >> flag;
            {
                std::lock_guard<std::mutex> lock(config_mutex);
                resource_flags[resource] = flag;
                writeConfig(config_file);
                config_changed = true;
            }
        } else if (command == "exit") {
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    std::string config_file = "config.txt";
    char logpath[32];
    if (argc == 1) strcpy(logpath, "./log");
    else if (argc == 2) strncpy(logpath, argv[1], 32);
    else if (argc == 3) {
        strncpy(logpath, argv[1], 32);
        config_file = argv[2];
    } else {
        std::cerr << "commandline parameters error! please check and try it!" << std::endl;
        exit(1);
    }

    signal(SIGTERM, killdeal_func);
    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_TEST);
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));

    sendpid(getpid());

    logfile.open(logpath, std::ios::out | std::ios::app);
    if (!logfile.is_open()) {
        std::cerr << "Warning: cannot create log file" << std::endl;
        exit(1);
    }

    readConfig(config_file);

    std::thread config_thread(monitorConfig, config_file);
    std::thread log_thread(logGeneration);
    std::thread msg_thread(message_loop);
    std::thread user_thread(userInteraction, config_file);

    config_thread.join();
    log_thread.join();
    msg_thread.join();
    user_thread.join();

    close(sock_fd);
    free(nlh);
    if (logfile.is_open())
        logfile.close();

    return 0;
}
