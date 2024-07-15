import subprocess
import os

def clean_kernel_module():
    print("Cleaning kernel module...")
    os.chdir("kernel_module")
    subprocess.run(["make", "clean"], check=True)
    print("Kernel module cleaned successfully.")
    os.chdir("..")

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

def unload_kernel_module():
    print("Unloading AuditModule.ko...")
    subprocess.run(["sudo", "rmmod", "AuditModule.ko"], check=True)
    print("AuditModule.ko unloaded successfully.")

def main():
    print("Starting uninstall process...")
    clean_kernel_module()
    remove_executables()
    unload_kernel_module()
    print("Uninstall process completed successfully.")

if __name__ == "__main__":
    main()
