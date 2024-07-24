
# NIS-3302-SAC
## 分工
* 安全日志范围扩展：邵振宇、王博文
* 日志处理扩展：柯斌
* 日志系统灵活性扩展：刘伊天
* GUI：彭娜

## 总体设计
* 安全日志范围扩展模块：根据demo改编，日志输出与demo结构保持一致
* 日志处理扩展：读入日志并结构化存储到数据库中，进而使用mysql对其进行排序
* 日志系统灵活性扩展：在应用层通过配置文件config.txt以消息通信形式向audit发送资源管理信息，在应用层生成log前进行资源合法性检验
* GUI改编：实现图形化界面，为用户提供更为便捷的可视化操作。通过QT设计并实现一个图形用户界面，方便管理员进行配置和查看日志，界面功能包括配置指定操作、浏览和查询日志、导出日志等。

## 程序使用说明
* 内核模块说明请见kernel_module文件夹中的说明文档：kernel_module_manual
* audit和log_processor的详细使用说明请见configure文件夹中的对应markdown文档：audit_usage.md和log_processor_usage.md
* GUI部分详见GUI.zip

## 程序编译
* 在SAC-project文件夹下运行
```bash
sudo python3 setup.py
```

## 命令行界面程序使用
* audit.cpp：编译完成后，在configure文件夹中运行
```bash
python3 log_audit.py --run audit
```
* log_processor.cpp：编译完成后，在configure文件夹中运行
```bash
sudo python3 log_audit.py --run log_processor --file /path/to/log.txt
```

## 程序卸载
* 在SAC-project文件夹下运行
```bash
sudo python3 uninstall.py
```
