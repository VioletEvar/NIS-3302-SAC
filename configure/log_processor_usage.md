# log_Processor.cpp使用方法

### 配置环境：

1.  进行g++,mysql++，以及mysql库的下载；

```c++
sudo apt install g++ libmysql++-dev mysql-server
```

2.启动终端，配置mysql数据库表：

```c++
sudo mysql -u root -p;  //进入mysql;
create database log_db; //创建数据库；
use log_db; //进入该数据库；
CREATE TABLE log_entries (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(255),
    uid INT,
    operation VARCHAR(255),
    operation_id INT,
    time VARCHAR(255),
    file_path VARCHAR(255),
    details VARCHAR(255),
    status VARCHAR(255)
);       //创建数据库表；

DESCRIBE log_entries; //查看数据库表结构，验证是否创建成功；

quit //可退出,也可以不退出新开终端,方便在程序运行途中随时查看数据库表内的内容变化；
```

### 编译指令

```c++
g++ -o log_processor log_processor.cpp -I/usr/include/mysql -I/usr/include/mysql++ -lmysqlpp -lmysqlclient    //根据具体的.cpp文件名修改即可
```

### 运行指令

```c++
sudo ./log_processor (自己本地log文件的绝对路径)//需要sudo提权进行数据库操作
```

### 备注：一些简单的数据库指令

**在程序运行进入数据库后，可以使用一些指令来查询数据库表内的内容：**

```c++
SELECT * FROM log_entries;  //查询数据库表内所有数据
SELECT username，uid FROM log_entries WHERE username = 'zach';
//类似的查询格式可以为：SELECT 查询内容，查询内容，查询内容（想查询的内容之间用逗号+空格分隔） FROM log_entries WHERE 查询条件 ;（内容项目 = '某指定内容' , 多个查询条件之间依旧用逗号+空格分隔）
```

