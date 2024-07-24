# audit程序使用说明
audit程序有两种使用方式：直接使用与间接使用
## 从命令行直接使用
* 在configure文件夹下执行下列指令：
```bash
./audit log.txt
```
其中log.txt可替换为其他日志文件

## 从命令行间接使用
* 在项目总文件夹下执行下列指令：
```bash
python3 log_audit.py --run audit --file log.txt
```
其中log.txt可替换为其他日志文件

