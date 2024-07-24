import subprocess
import os

# 定义下载mysql所需库的函数
def install_dependencies():
    print("Installing dependencies...")
    subprocess.run([
        "sudo", "apt", "install", "g++", "libmysql++-dev", "mysql-server"
    ], check=True)
    print("Dependencies installed successfully.")

# 定义初始化数据库的函数
def configure_database():
    print("Configuring MySQL database...")
    commands = [
        "CREATE DATABASE IF NOT EXISTS log_db;",
        "USE log_db;",
        "CREATE TABLE IF NOT EXISTS log_entries ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "username VARCHAR(255), "
        "uid INT, "
        "operation VARCHAR(255), "
        "operation_id INT, "
        "time VARCHAR(255), "
        "file_path VARCHAR(255), "
        "details VARCHAR(255), "
        "status VARCHAR(255)"
        ");",
        "DESCRIBE log_entries;"
    ]
    # 将命令连接成单个字符串，每个命令后跟一个分号
    command_string = ' '.join(commands)
    subprocess.run([
        "sudo", "mysql", "-u", "root", "-p"
    ], input=command_string.encode(), check=True)
    print("Database configured successfully.")
    
def compile_configure():
    # 切换到 configure 目录
    os.chdir("configure")
    
    # 编译 audit.cpp
    subprocess.run(["g++", "-o", "audit", "audit.cpp"], check=True)
    print("Compiled audit successfully.")
    
    # 编译 log_processor.cpp
    subprocess.run(["g++", "-o", "log_processor", "log_processor.cpp",
                    "-I/usr/include/mysql", "-I/usr/include/mysql++",
                    "-lmysqlpp", "-lmysqlclient"], check=True)
    print("Compiled log_processor successfully.")
    
    # 返回到上一级目录
    os.chdir("..")

def compile_kernel_module():
    # 切换到 kernel_module 目录
    os.chdir("kernel_module")
    
    # 使用 Makefile 编译
    subprocess.run(["make"], check=True)
    print("Compiled kernel module successfully.")
    
    # 返回到上一级目录
    os.chdir("..")

def load_kernel_module():
    os.chdir("kernel_module")
    # 使用 sudo 来加载内核模块
    subprocess.run(["sudo", "insmod", "AuditModule.ko"], check=True)
    print("Loaded AuditModule.ko successfully.")
    os.chdir("..")

def main():
    print("Starting compilation process...")
    install_dependencies()
    configure_database()
    # 编译 configure 目录中的文件
    compile_configure()
    
    # 编译 kernel_module 目录中的文件
    compile_kernel_module()
    # 加载内核模块
    load_kernel_module()
    print("All modules compiled successfully.")

if __name__ == "__main__":
    main()
