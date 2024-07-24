import subprocess
import os

# 定义清理内核模块编译的函数
def clean_kernel_module():
    print("Cleaning kernel module...")
    os.chdir("kernel_module")
    subprocess.run(["make", "clean"], check=True)
    print("Kernel module cleaned successfully.")
    os.chdir("..")

# 定义清理configure文件夹里可执行文件的函数
def remove_executables():
    print("Removing executables in configure folder...")
    os.chdir("configure")
    try:
        os.remove("audit")
        print("Removed audit executable.")
    except FileNotFoundError:
        print("Audit executable not found.")
    try:
        os.remove("log_processor")
        print("Removed log_processor executable.")
    except FileNotFoundError:
        print("Log_processor executable not found.")
    os.chdir("..")

# 定义移除内核加载模组的函数
def unload_kernel_module():
    print("Unloading AuditModule.ko...")
    subprocess.run(["sudo", "rmmod", "AuditModule.ko"], check=True)
    print("AuditModule.ko unloaded successfully.")

def main():
    print("Starting uninstall process...")
    # 清理内核模块编译
    clean_kernel_module()
    # 移除可执行文件
    remove_executables()
    # 移除内核上加载的AuditModule.ko
    unload_kernel_module()
    print("Uninstall process completed successfully.")

if __name__ == "__main__":
    main()
