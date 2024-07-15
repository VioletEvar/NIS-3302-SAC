import subprocess
import os

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

def main():
    print("Starting compilation process...")
    
    # 编译 configure 目录中的文件
    compile_configure()
    
    # 编译 kernel_module 目录中的文件
    compile_kernel_module()
    
    print("All modules compiled successfully.")

if __name__ == "__main__":
    main()
