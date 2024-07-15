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
        std::string timePart;
        std::getline(iss, timePart, ' ');
        entry.time += " " + timePart;

        std::getline(iss, entry.file_path, '"');
        std::getline(iss, entry.file_path, '"');

        int size = entry.file_path.size()-1;
        if(entry.file_path[size] == '.') entry.file_path.pop_back();

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

        if (i == 0 || entry.username != lastEntry.username || entry.uid != lastEntry.uid || entry.operation != lastEntry.operation || entry.file_path != lastEntry.file_path || entry.details!= lastEntry.details || entry.status != lastEntry.status) {
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
    const int colWidth = 20;
    std::cout << std::left << std::setw(colWidth) << "| username"
              << std::left << std::setw(colWidth) << "| uid"
              << std::left << std::setw(colWidth) << "| operation"
              << std::left << std::setw(colWidth) << "| operation_id"
              << std::left << std::setw(colWidth) << "| time"
              << std::left << std::setw(colWidth) << "| file_path"
              << std::left << std::setw(colWidth) << "| details"
              << std::left << std::setw(colWidth) << "| status"
              << std::endl;
    for (const auto& entry : logEntries) {
        std::cout << std::left << std::setw(colWidth) << "| " << entry.username
                  << std::left << std::setw(colWidth) << "| " << entry.uid
                  << std::left << std::setw(colWidth) << "| " << entry.operation
                  << std::left << std::setw(colWidth) << "| " << entry.operation_id
                  << std::left << std::setw(colWidth) << "| " << entry.time
                  << std::left << std::setw(colWidth) << "| " << entry.file_path
                  << std::left << std::setw(colWidth) << "| " << entry.details
                  << std::left << std::setw(colWidth) << "| " << entry.status
                  << std::endl;
    }
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
    std::cout << "Queried Entries by " << queryCondition << std::endl;
    printLogEntries(queriedEntries);
}

void sortOption(mysqlpp::Connection& conn) {
    std::string sortCondition;
    sortCondition.clear();
    std::cout << "Please enter the sorting conditions and specify ascending (ASC) or descending (DESC) order. For compound sorting, separate conditions with a comma and a space." << std::endl << " For example:username ASC, time DESC " << std::endl;
    getline(std::cin,sortCondition);
    std::vector<LogEntry> sortedEntries = sortLogEntries(conn, sortCondition);
    std::cout << "Sorted Entries by " << sortCondition << std::endl;
    printLogEntries(sortedEntries);
}

void mergeMode(mysqlpp::Connection& conn) {
    std::vector<LogEntry> mergedEntries = mergeLogEntries(conn);
    std::cout << "Merged Entries:" << std::endl;
    printLogEntries(mergedEntries);
}



void userHelp(){
    std::cout << "Welcome to the Log Management System. This system supports the following operations on log entries:" << std::endl ;
    std::cout << "query: Retrieve log entries based on specific conditions." << std::endl;
    std::cout << "sort: Sort and output log entries based on specific conditions." << std::endl;
    std::cout << "merge: Merge and output log entries with duplicate operations." << std::endl;
    std::cout << "quit: Exit the system." << std::endl;
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




int main(int argc, char* argv[]) {

    if(argc < 2) {
        std::cout << "The file path was not detected."  << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    // 读取日志文件
    std::vector<LogEntry> logEntries = readLogFile(filename);//命令行输入

    std::ifstream file(filename);
    if(!file.is_open()){
        std:: cerr << "Error opening file." << std:: endl;
        return 1;
    }

    std::string line;
    size_t i = 0;
    while(std::getline(file, line)) {
        logEntries[i].status = extractLastItem(line);
        std::cout << "Last item :" << logEntries[i].status << " " << logEntries[i].status.size() << std::endl; //调试信息
        i++;
    }

    // 连接到数据库
    mysqlpp::Connection conn = connectDatabase();

    
    // 帮助界面
    userHelp();

    while(1){
        std::string operate;
        //清空数据库表
        clearLogEntries(conn);
        //将日志条目插入数据库
        insertLogEntries(conn, logEntries);

        std::cout << "Enter the operation (query/sort/merge/quit):"  ;
        std::getline(std::cin, operate);
        switch (operate[operate.size()-3])
        {
            case 'e':
            queryOption(conn);
            break;
            case 'o':
            sortOption(conn);
            break;
            case 'r':
            mergeMode(conn);
            break;
            case 'u':
            return 0;
            default: std::cout << "Wrong input, please retry." << std::endl; break;
        }
        //调试信息
        std::cout << "Opration finished." << std::endl;
    }
    return 0;
}


