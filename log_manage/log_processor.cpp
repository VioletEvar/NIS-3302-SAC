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
iss.ignore(2, ')');
std::getline(iss, entry.operation, '(');
iss >> entry.operation_id;
iss.ignore(2, ')');
iss >> entry.time;
std::getline(iss, entry.file_path, '"');
std::getline(iss, entry.file_path, '"');
std::getline(iss, entry.details, ' ');
std::getline(iss, entry.status);
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

// 将日志条目插入数据库
void insertLogEntries(mysqlpp::Connection& conn, const std::vector<LogEntry>& logEntries) {
mysqlpp::Query query = conn.query();
query << "INSERT INTO log_entries (username, uid, operation, operation_id, time, file_path, details, status) VALUES ";
for (size_t i = 0; i < logEntries.size(); ++i) {
const LogEntry& entry = logEntries[i];
query << "('" << entry.username << "', " << entry.uid << ", '" << entry.operation << "', " << entry.operation_id << ", '"
<< entry.time << "', '" << entry.file_path << "', '" << entry.details << "', '" << entry.status << "')";
if (i != logEntries.size() - 1) {
query << ", ";
}
}
query.execute();
std::cout << "Inserted " << logEntries.size() << " log entries into database." << std::endl; // 调试输出
}

// 根据条件查询日志条目
std::vector<LogEntry> queryLogEntries(mysqlpp::Connection& conn, const std::string& condition) {
mysqlpp::Query query = conn.query("SELECT username, uid, operation, operation_id, time, file_path, details, status FROM log_entries WHERE " + condition);
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
mysqlpp::Query query = conn.query("SELECT username, uid, operation, operation_id, time, file_path, details, status FROM log_entries ORDER BY username, uid, operation, time");
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

if (i == 0 || entry.username != lastEntry.username || entry.uid != lastEntry.uid || entry.operation != lastEntry.operation) {
logEntries.push_back(entry);
} else {
logEntries.back() = entry; // 更新为最新的操作
}
lastEntry = entry;
}
std::cout << "Merged log entries, resulting in " << logEntries.size() << " unique entries." << std::endl; // 调试输出
return logEntries;
}

// 输出日志条目
void printLogEntries(const std::vector<LogEntry>& logEntries) {
for (const auto& entry : logEntries) {
std::cout << entry.username << "(" << entry.uid << ") " << entry.operation << "(" << entry.operation_id << ") "
<< entry.time << " \"" << entry.file_path << "\" " << entry.details << " " << entry.status << std::endl;
}
}

int main() {
try {
// 读取日志文件
std::vector<LogEntry> logEntries = readLogFile("/home/zach/Documents/log.txt");
printLogEntries(logEntries);
std::cout << std::endl << std::endl;
// 连接到数据库
mysqlpp::Connection conn = connectDatabase();

// 将日志条目插入数据库
insertLogEntries(conn, logEntries);

// 示例：查询特定用户名的日志条目
std::vector<LogEntry> queriedEntries = queryLogEntries(conn, "username = 'violet'");
std::cout << "Queried Entries:" << std::endl;
printLogEntries(queriedEntries);

// 示例：按用户名排序
std::vector<LogEntry> sortedEntries = sortLogEntries(conn, "username");
std::cout << "Sorted Entries by username:" << std::endl;
printLogEntries(sortedEntries);

// 示例：合并日志条目
std::vector<LogEntry> mergedEntries = mergeLogEntries(conn);
std::cout << "Merged Entries:" << std::endl;
printLogEntries(mergedEntries);

std::cout << "Operations completed successfully." << std::endl;
} catch (const std::exception& ex) {
std::cerr << "Error: " << ex.what() << std::endl;
}

return 0;
}
    
   


