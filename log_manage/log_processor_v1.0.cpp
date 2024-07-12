#include <mysql++/mysql++.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <ctime>

    // 日志条目结构体
struct LogEntry {
    std::string username;
    int uid;
    std::string operation;
    int operation_id;
    std::string time;
    std::string file_path;
    std::string details;
    std::string status;

};

// 从文件读取日志条目
std::vector<LogEntry> readLogFile(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<LogEntry> logEntries;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        LogEntry entry;

        std::getline(iss, entry.username, '(');
        iss >> entry.uid;
        iss.ignore(2, ' ');
        std::getline(iss, entry.operation, '(');
        iss >> entry.operation_id;
        iss.ignore(2, ' ');

        // 读取完整的时间部分，包括日期和时间
        std::getline(iss, entry.time, ' ');
        std::cout << entry.time << std::endl;
        std::string timePart;
        std::getline(iss, timePart, ' ');
        std::cout << timePart << std::endl;
        entry.time += " " + timePart;
        std::cout << entry.time << std::endl;

        std::getline(iss, entry.file_path, '"');
        std::getline(iss, entry.file_path, '"');
        iss.ignore(1);
        std::getline(iss, entry.details, ' ');
        logEntries.push_back(entry);
    }
    std::cout << "Read " << logEntries.size() << " log entries from file." << std::endl; // 调试输出
    return logEntries;
    }

    // 连接数据库
mysqlpp::Connection connectDatabase() {
    mysqlpp::Connection conn(false);
    if (!conn.connect("log_db", "localhost", "root", "03130039")) {
        throw std::runtime_error("DB connection failed: " + std::string(conn.error()));
    }
    std::cout << "Connected to database successfully." << std::endl; // 调试输出
    return conn;
}


// 清空日志条目表
void clearLogEntries(mysqlpp::Connection& conn) {
    mysqlpp::Query query = conn.query("TRUNCATE TABLE log_entries");
    query.execute();
    std::cout << "Cleared log_entries table." << std::endl; // 调试输出
}

// 将日志条目逐个插入数据库
void insertLogEntries(mysqlpp::Connection& conn, const std::vector<LogEntry>& logEntries) {
    for (const auto& entry : logEntries) {
    // 使用参数化查询以防止SQL注入
        mysqlpp::Query query = conn.query();
        query << "INSERT INTO log_entries (username, uid, operation, operation_id, time, file_path, details, status) " << "VALUES (%0q, %1, %2q, %3, %4q, %5q, %6q, %7q)";
        query.parse();
        try {query.execute(entry.username, entry.uid, entry.operation, entry.operation_id, entry.time, 	entry.file_path, entry.details, entry.status);}
        catch (const mysqlpp::BadQuery& e) {std::cerr << "Query error: " << e.what() << std::endl;} 
        catch (const mysqlpp::Exception& e) {std::cerr << "Error: " << e.what() << std::endl;}
    }
    std::cout << "Inserted " << logEntries.size() << " log entries into database." << std::endl; // 调试输出
}

// 根据条件查询日志条目
std::vector<LogEntry> queryLogEntries(mysqlpp::Connection& conn, const std::string& condition) {
    mysqlpp::Query query = conn.query("SELECT username, uid, operation, operation_id, time, file_path, details, status FROM log_entries WHERE "+ condition);
    mysqlpp::StoreQueryResult res = query.store();
    std::vector<LogEntry> logEntries;
    for (size_t i = 0; i < res.num_rows(); ++i) {
        LogEntry entry;
        entry.username = res[i]["username"].c_str();
        entry.uid = res[i]["uid"];
        entry.operation = res[i]["operation"].c_str();
        entry.operation_id = res[i]["operation_id"];
        entry.time = res[i]["time"].c_str();
        entry.file_path = res[i]["file_path"].c_str();
        entry.details = res[i]["details"].c_str();
        entry.status = res[i]["status"].c_str();
        logEntries.push_back(entry);
    }
    std::cout << "Queried " << logEntries.size() << " log entries with condition: " << condition << std::endl; // 调试输出
    return logEntries;
}

// 根据字段排序日志条目
std::vector<LogEntry> sortLogEntries(mysqlpp::Connection& conn, const std::string& field) {
    mysqlpp::Query query = conn.query("SELECT username, uid, operation, operation_id, time, file_path, details, status FROM log_entries ORDER BY " + field);
    mysqlpp::StoreQueryResult res = query.store();
    std::vector<LogEntry> logEntries;
    for (size_t i = 0; i < res.num_rows(); ++i) {
        LogEntry entry;
        entry.username = res[i]["username"].c_str();
        entry.uid = res[i]["uid"];
        entry.operation = res[i]["operation"].c_str();
        entry.operation_id = res[i]["operation_id"];
        entry.time = res[i]["time"].c_str();
        entry.file_path = res[i]["file_path"].c_str();
        entry.details = res[i]["details"].c_str();
        entry.status = res[i]["status"].c_str();
        logEntries.push_back(entry);
    }
    std::cout << "Sorted " << logEntries.size() << " log entries by " << field << std::endl; // 调试输出
    return logEntries;
}

// 合并日志条目
std::vector<LogEntry> mergeLogEntries(mysqlpp::Connection& conn) {
    mysqlpp::Query query = conn.query("SELECT username, uid, operation, operation_id, time, file_path, details, status FROM log_entries ORDER BY time");
    mysqlpp::StoreQueryResult res = query.store();
    std::vector<LogEntry> logEntries;
    LogEntry lastEntry;
    for (size_t i = 0; i < res.num_rows(); ++i) {
        LogEntry entry;
        entry.username = res[i]["username"].c_str();
        entry.uid = res[i]["uid"];
        entry.operation = res[i]["operation"].c_str();
        entry.operation_id = res[i]["operation_id"];
        entry.time = res[i]["time"].c_str();
        entry.file_path = res[i]["file_path"].c_str();
        entry.details = res[i]["details"].c_str();
        entry.status = res[i]["status"].c_str();

        if (i == 0 || entry.username != lastEntry.username || entry.uid != lastEntry.uid || entry.operation != lastEntry.operation || entry.operation_id != lastEntry.operation_id || entry.file_path != lastEntry.file_path || entry.details!= lastEntry.details || entry.status != lastEntry.status) {
            logEntries.push_back(entry);
        }
        else {
            logEntries.back() = entry; // 更新为最新的操作
        }
        lastEntry = entry;
    }
    std::cout << "Merged log entries, resulting in " << logEntries.size() << " unique entries." << std::endl; // 调试输出
    return logEntries;
}

// 输出日志条目
void printLogEntries(const std::vector<LogEntry>& logEntries) {
    for (const auto& entry : logEntries) 
        std::cout << entry.username << "(" << entry.uid << ") " << entry.operation << "(" << entry.operation_id << ") "<< entry.time << " \"" << entry.file_path << "\" " << entry.details << " " << entry.status << std::endl ;
}

void queryOption(mysqlpp::Connection& conn) {

    std::cout << "Please enter the every items you want to search, if not use enter" << std::endl;
    std::string queryCondition;
    queryCondition.clear();
    bool queryCondition_flag = false;
    std::cout << "Username:" ;
    std::getline(std::cin,queryCondition);
    if(!queryCondition.empty()) {
        queryCondition_flag = true;
        queryCondition = "username = \'" + queryCondition + "\'" ;
    }

    std::vector<std::string> strArray = {
        "uid",
        "operation",
        "operation_id",
        "time",
        "file_path",
        "details",
        "status",
    };

    for(int i = 0; i < strArray.size(); i++){
        std::cout << std::endl << strArray[i] << ":" ;
        std::string otherCondition;
        otherCondition.clear();
        std::getline(std::cin,otherCondition);
        if(queryCondition_flag && !otherCondition.empty()) queryCondition = queryCondition + " AND " + strArray[i] + " = \'" + otherCondition + "\'";
        else if(!queryCondition_flag && !otherCondition.empty()) queryCondition = strArray[i] + " = \'" + otherCondition + "\'";
        if(!queryCondition.empty()) queryCondition_flag = true;
    }
    std::vector<LogEntry> queriedEntries = queryLogEntries(conn, queryCondition);
    std::cout << "Condition is : " << queryCondition << std::endl; //调试模块
    std::cout << "Queried Entries:" << std::endl;
    printLogEntries(queriedEntries);
}

void sortOption(mysqlpp::Connection& conn) {
    std::string sortCondition;
    sortCondition.clear();
    std::cout << "输入想排序的条件以及升序(ASC)或降序(DESC)，复合排序以逗号+空格分隔,例如:username ASC, time DESC" << std::endl;
    getline(std::cin,sortCondition);
    std::vector<LogEntry> sortedEntries = sortLogEntries(conn, sortCondition);
    std::cout << "Sorted Entries by username:" << std::endl;
    printLogEntries(sortedEntries);
}

void mergeMode(mysqlpp::Connection& conn) {
    std::vector<LogEntry> mergedEntries = mergeLogEntries(conn);
    std::cout << "Merged Entries:" << std::endl;
    printLogEntries(mergedEntries);
}



void userHelp(){
    std::cout << "欢迎使用日志资源管理模块，输入字符来使用相应功能：q--查询，s--排序，m--重复操作合并，a--退出。" << std::endl ;
    return ;
}


// 提取最后一项内容的函数
std::string extractLastItem(const std::string& line) {
    size_t lastCommaPos = line.rfind(' ');
    if (lastCommaPos != std::string::npos) {
        std::string lastItem = line.substr(lastCommaPos + 1);
        // 去除前后的空格
        lastItem.erase(0, lastItem.find_first_not_of(' '));
        lastItem.erase(lastItem.find_last_not_of(' ') + 1);
        return lastItem;
    }
    return "";
}




int main() {

    // 读取日志文件
    std::vector<LogEntry> logEntries = readLogFile("/home/zach/Documents/log.txt");//命令行输入

    std::ifstream file("/home/zach/Documents/log.txt");
    if(!file.is_open()){
        std:: cerr << "Error opening file." << std:: endl;
        return 1;
    }

    std::string line;
    size_t i = 0;
    while(std::getline(file, line)) {
        
        logEntries[i].status = extractLastItem(line);
        logEntries[i].status.pop_back();
        std::cout << "Last item :" << logEntries[i].status <<" " << logEntries[i].status.size() << std::endl;
        i++;
    }

    // 连接到数据库
    mysqlpp::Connection conn = connectDatabase();
    LogEntry user1 = {"user1", 1001, "ls", 1234, "2024-07-09 12:34:56", "/home/user1", "details", "success"};//测试组
    logEntries.push_back(user1);

    printLogEntries(logEntries);
    // 帮助界面（非完整）
    userHelp();

    while(1){
        char operate;
        //清空数据库表
        clearLogEntries(conn);
        // 将日志条目插入数据库
        insertLogEntries(conn, logEntries);

        std::cout << "Enter the operate :" ;
        std::cin >> operate ;
        std::cin.get();
        switch (operate)
        {
            case 'q':
            queryOption(conn);
            break;
            case 's':
            sortOption(conn);
            break;
            case 'm':
            mergeMode(conn);
            break;
            case 'a':
            return 0;
        }
        //调试信息
        std::cout << "单次操作完成" << std::endl;
    }
    return 0;
}


