#ifndef logger_h
#define logger_h
#include<iostream>
#include<fstream>
#include <ctime>
#include<string.h>

// using namespace std;

enum LogPriority {
        TraceP, DebugP, InfoP, WarnP, ErrorP, FatalP
};

class BasicLogger {
    private:
        static LogPriority verbosity;
        static const char* filepath;
        

    public:

        static std::string GetDateTime() {
            std::string cur_date, cur_time, datetime;
            time_t *rawtime = new time_t; // we'll get the time here by time() function.
            struct tm * timeinfo; /* we'll get the time info. (sec, min, hour, etc...) here from rawtime by the localtime() function */
            time(rawtime); // Get time into rawtime
            timeinfo = localtime(rawtime); // Get time info into timeinfo
            cur_date = std::to_string(timeinfo->tm_mday) + "/" + std::to_string(timeinfo->tm_mon + 1) + "/" + std::to_string(timeinfo->tm_year + 1900);
            cur_time = std::to_string(timeinfo->tm_hour) + ":" + std::to_string(timeinfo->tm_min) + ":" + std::to_string(timeinfo->tm_sec);
            datetime = cur_date + " " + cur_time;
            return datetime;
        }

        static void SetVerbosity(LogPriority new_priority) {
            verbosity = new_priority;
            // Open file and clear if exists
            std::ofstream FILE(filepath, std::ios::out | std::ios::trunc);
            FILE<<"Client Log set with verbosity "<< new_priority<<'\n';
            FILE.close(); //Using microsoft incremental linker version 14
    }
        static void Log(LogPriority priority, std::string message) {
                if (priority >= verbosity) {
                        std::ofstream FILE(filepath, std::ios_base::app);
                        FILE<<GetDateTime()<<" ";
                        switch (priority) {
                                case TraceP: FILE << "Trace:\t"; break;
                                case DebugP: FILE << "Debug:\t"; break;
                                case InfoP: FILE << "Info:\t"; break;
                                case WarnP: FILE << "Warn:\t"; break;
                                case ErrorP: FILE << "Error:\t"; break;
                                case FatalP: FILE << "Fatal:\t"; break;
                                }
                        FILE << message << "\n";
                        FILE.close();
                        }
                }
};

LogPriority BasicLogger::verbosity = TraceP;
// const char* BasicLogger::filepath = "tracker_log.txt";

#endif //logger_h