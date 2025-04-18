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
std::mutex config_mutex;
std::unordered_map<std::string, bool> resource_flags;
std::atomic<bool> config_changed(false);
std::atomic<bool> stop_logging(false);

// readConfig function, used to read config file and load config into resource_flags
void readConfig(const std::string& config_file) {
    // store config in temp flag, transfer to resource_flag later
    std::unordered_map<std::string, bool> temp_flags;
    // open config file
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Error opening config file." << std::endl;
        return;
    }
    std::string line;
    // parse config using iss function
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string resource;
        bool flag;
        if (!(iss >> resource >> flag)) {
            std::cerr << "Error reading config line: " << line << std::endl;
            continue;
        }
        // store config in temp flags
        temp_flags[resource] = flag;
    }
    // lock resource_flags to update
    std::lock_guard<std::mutex> lock(config_mutex);
    resource_flags = std::move(temp_flags);
    // set config_changed flag
    config_changed = true;
}

// writeConfig function, used to update resource_flags
void writeConfig(const std::string& config_file) {
    // lock resource_flags to update
    std::lock_guard<std::mutex> lock(config_mutex);
    std::ofstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Error opening config file for writing." << std::endl;
        return;
    }
    // update config file
    for (const auto& [resource, flag] : resource_flags) {
        file << resource << " " << flag << std::endl;
    }
}

// Log function, used to generate log and write log into log.txt
void Log(const std::string& commandname, int uid, int pid, const std::string& file_path, int flags, int ret) {
    // check if the command should be logged
    bool should_log = false;
    {
        // lock resource_flags to check 
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

    if (ret >= 0) operationresult = "success";
    else operationresult = "failed";
    // switch operationtype according to commandname
    if (commandname == "rm") {
        operationtype = "Delete";
    }
    else if(commandname == "insmod"){
        operationtype = "ModuleInstall";
    } 
    else if(commandname == "rmmod"){
        operationtype = "ModuleRemove";
    }
    else if(commandname == "reboot"){
        operationtype = "Reboot";
    }
    else if(commandname == "shutdown"){
        operationtype = "Shutdown";
    }
    else if(commandname == "mount"){
        operationtype = "umount";
    }
    else if(commandname == "socket"){
        operationtype = "Socket";
    }
    else if(commandname == "connect"){
        operationtype = "Connect";
    }
    else if(commandname == "accept"){
        operationtype = "Accept";
    }
    else if(commandname == "sendto"){
        operationtype = "Sendto";
    }
    else if(commandname == "recvfrom"){
        operationtype = "Recvfrom";
    }
    else if(commandname == "close"){
        operationtype = "Close";
    }
    else if(flags == 524288){
        operationtype = "Execute";
    }
    else {
        if (flags & O_RDONLY) operationtype = "Read";
        else if (flags & O_WRONLY) operationtype = "Write";
        else if (flags & O_RDWR) operationtype = "Read/Write";
        else operationtype = "Other";
    }

    time_t t = time(0);
    if (!logfile.is_open()) return;
    pwinfo = getpwuid(uid);
    strcpy(username, pwinfo->pw_name);

    strftime(logtime, sizeof(logtime), TM_FMT, localtime(&t));
    // write log into logfile
    logfile << username << "(" << uid << ") " << commandname << "(" << pid << ") " << logtime 
            << " \"" << file_path << "\" " << operationtype << " " << operationresult << std::endl;
    logfile.flush();
}

// sendpid function from auditdemo
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

// killdeal function from auditdemo
void killdeal_func(int signum) {
    std::cout << "The process is killed!" << std::endl;
    close(sock_fd);
    if (logfile.is_open())
        logfile.close();
    if (nlh != nullptr)
        free(nlh);
    exit(0);
}

// logGeneration function, used to receive and analyse kernel message and decide if log should be generated
void logGeneration() {
    // set the socket to non-blocking mode
    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

    while (!stop_logging) {
        if (config_changed) {
            std::lock_guard<std::mutex> lock(config_mutex);
            config_changed = false;
            // Print "logging enabled for resource" message when config is updated
            for (const auto& [resource, flag] : resource_flags) {
                if (flag) {
                    std::cout << "Logging enabled for resource: " << resource << std::endl;
                }
            }
        }

        // read message from kernel
        unsigned int uid, pid, flags, ret;
        char *file_path;
        char *commandname;

        int ret_val = recvmsg(sock_fd, &msg, 0);
        if (ret_val < 0) {
            // if block, sleep for 0.1 second and continue, which prevents busy waiting
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));  // short sleep to avoid busy-waiting
                continue;
            } else {
                perror("recvmsg");
                break;
            }
        }

        // analyze log info
        uid = *(reinterpret_cast<unsigned int *>(NLMSG_DATA(nlh)));
        pid = *(1 + reinterpret_cast<int *>(NLMSG_DATA(nlh)));
        flags = *(2 + reinterpret_cast<int *>(NLMSG_DATA(nlh)));
        ret = *(3 + reinterpret_cast<int *>(NLMSG_DATA(nlh)));
        commandname = reinterpret_cast<char *>(4 + reinterpret_cast<int *>(NLMSG_DATA(nlh)));
        file_path = reinterpret_cast<char *>(4 + 16 / 4 + reinterpret_cast<int *>(NLMSG_DATA(nlh)));

        // check resource flags to decide if should log
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

        // print log info to terminal
        std::cout << "uid: " << uid << std::endl;
        std::cout << "pid: " << pid << std::endl;
        std::cout << "flags: " << flags << std::endl;
        std::cout << "ret: " << ret << std::endl;
        std::cout << "commandname: " << commandname << std::endl;
        std::cout << "file_path: " << file_path << std::endl;
        
        // generate log using Log function
        Log(commandname, uid, pid, file_path, flags, ret);
    }
}

// userInteraction function, used to interact with user and update config
void userInteraction(const std::string& config_file) {
    std::string command;
    while (true) {
        std::cout << "Enter 'update' to modify flags, 'exit' to quit: " << std::endl;
        std::cin >> command;
        if (command == "update") {
            std::string resource;
            bool flag;
            std::cout << "Enter resource name: ";
            std::cin >> resource;
            std::cout << "Enter flag (0 or 1): ";
            std::cin >> flag;
            {
                // lock resource_flags to write
                std::lock_guard<std::mutex> lock(config_mutex);
                resource_flags[resource] = flag;
                config_changed = true;
            }
        } else if (command == "exit") {
            break;
        }
    }
    stop_logging = true;
    // update config file
    writeConfig(config_file);
}

int main(int argc, char *argv[]) {
    std::string config_file = "config.txt";
    char logpath[32];
    // receive terminal command
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
    
    // open logfile
    logfile.open(logpath, std::ios::out | std::ios::app);
    if (!logfile.is_open()) {
        std::cerr << "Warning: cannot create log file" << std::endl;
        exit(1);
    }
    
    // read config file
    readConfig(config_file);
    
    // create log and user thread
    std::thread log_thread(logGeneration);
    std::thread user_thread(userInteraction, config_file);
    
    // wait for thread to terminate
    user_thread.join();
    log_thread.join();

    close(sock_fd);
    free(nlh);
    if (logfile.is_open())
        logfile.close();

    return 0;
}
