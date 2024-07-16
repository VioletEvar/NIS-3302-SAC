#include <iostream>
#include <cstdio>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>

#define logpath "./bootlog"

using namespace std;

string formatTime(int year, const string& month, int day, const string& time) {
    struct tm tm = {};
    tm.tm_year = year - 1900; // Year since 1900
    if (month == "Jan") tm.tm_mon = 0;
    else if (month == "Feb") tm.tm_mon = 1;
    else if (month == "Mar") tm.tm_mon = 2;
    else if (month == "Apr") tm.tm_mon = 3;
    else if (month == "May") tm.tm_mon = 4;
    else if (month == "Jun") tm.tm_mon = 5;
    else if (month == "Jul") tm.tm_mon = 6;
    else if (month == "Aug") tm.tm_mon = 7;
    else if (month == "Sep") tm.tm_mon = 8;
    else if (month == "Oct") tm.tm_mon = 9;
    else if (month == "Nov") tm.tm_mon = 10;
    else if (month == "Dec") tm.tm_mon = 11;
    tm.tm_mday = day;
    sscanf(time.c_str(), "%2d:%2d", &tm.tm_hour, &tm.tm_min);

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return string(buffer);
}

int main() {
    FILE* fp1, * fp2;
    char buffer1[1024], buffer2[1024];
    string placeholder, week, month, boot_time, close_time, span;
    int day;

    // Example user information, replace with actual data if available
    string username = "user";
    int uid = 1000; // Example UID
    string commandname = "command";
    int pid = 0000; // Example PID
    string file_path = "/path/to/file"; // Example file path
    string operationtype = "type"; // Example operation type
    string operationresult = "result"; // Example operation result

    fp1 = popen("last -x reboot", "r");
    fp2 = popen("last -x shutdown", "r");
    if (fp1 == NULL || fp2 == NULL) {
        cerr << "Error executing command" << endl;
        return 1;
    }

    FILE* logfile;
    logfile = fopen(logpath, "w+");
    if (logfile == NULL) {
        cerr << "Warning: cannot create log file" << endl;
        exit(1);
    }
    fprintf(logfile, "Username(UID) Commandname(PID) LogTime FilePath OperationType OperationResult\n\n");

    while (fgets(buffer1, sizeof(buffer1), fp1) != NULL && fgets(buffer2, sizeof(buffer2), fp2) != NULL) {
        istringstream iss1(buffer1), iss2(buffer2);
        if (iss1 >> placeholder >> placeholder >> placeholder >> placeholder >> week >> month >> day >> boot_time >> placeholder >> close_time) {
            string logtime = formatTime(2024, month, day, boot_time); // Assuming current year is 2024
            if (close_time != "running") {
                iss1 >> span;
                operationtype = "Boot";
                commandname = "boot";
                operationresult = "Completed";
                fprintf(logfile, "%s(%d) %s(%d) %s \"%s\" %s %s\n", username.c_str(), uid, commandname.c_str(), pid, logtime.c_str(), file_path.c_str(), operationtype.c_str(), operationresult.c_str());
            } else {
                operationtype = "Boot";
                commandname = "boot";
                operationresult = "Still_running";
                fprintf(logfile, "%s(%d) %s(%d) %s \"%s\" %s %s\n", username.c_str(), uid, commandname.c_str(), pid, logtime.c_str(), file_path.c_str(), operationtype.c_str(), operationresult.c_str());
            }
        }
        if (iss2 >> placeholder >> placeholder >> placeholder >> placeholder >> week >> month >> day >> close_time >> placeholder >> boot_time >> span) {
            string logtime = formatTime(2024, month, day, close_time); // Assuming current year is 2024
            operationtype = "Shutdown";
            commandname = "shutdown";
            operationresult = "Completed";
            fprintf(logfile, "%s(%d) %s(%d) %s \"%s\" %s %s\n", username.c_str(), uid, commandname.c_str(), pid, logtime.c_str(), file_path.c_str(), operationtype.c_str(), operationresult.c_str());
        }
    }

    pclose(fp1);
    pclose(fp2);
    fclose(logfile);
    return 0;
}
