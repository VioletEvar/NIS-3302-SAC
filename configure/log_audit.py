import subprocess
import argparse

def run_audit(filename):
    # 替换为 audit 程序的可执行文件路径
    subprocess.run(["./audit", filename])

def run_log_processor(filepath):
    # 替换为 log_processor 程序的可执行文件路径
    subprocess.run(["./log_processor", filepath])

def main():
    parser = argparse.ArgumentParser(description="Trigger C++ programs from Python")
    parser.add_argument("--run", choices=['audit', 'log_processor'], help="Select the program to run")
    parser.add_argument("--file", type=str, help="Path to the log file for log_processor")
    args = parser.parse_args()

    if args.run == 'audit':
        if args.file:
            run_audit(args.file)
        else:
            run_audit()
    elif args.run == 'log_processor':
        if args.file:
            run_log_processor(args.file)
        else:
            print("Please provide a file path for the log processor.")
    else:
        print("Invalid command. Use --help for more information.")

if __name__ == "__main__":
    main()
