#define _CRT_SECURE_NO_WARNINGS
#include <iomanip>
#include <sstream>
#include <string>
#include <ctime>

#include "history_utils.h"
#include "log_types.h"

bool isoTimestampToLocalTm(const string &isoTimestamp, tm &outTm) {
    if (isoTimestamp.size() < 19)
        return false;

    tm timeStruct{};
    timeStruct.tm_year = stoi(isoTimestamp.substr(0, 4)) - 1900;
    timeStruct.tm_mon = stoi(isoTimestamp.substr(5, 2)) - 1;
    timeStruct.tm_mday = stoi(isoTimestamp.substr(8, 2));
    timeStruct.tm_hour = stoi(isoTimestamp.substr(11, 2));
    timeStruct.tm_min = stoi(isoTimestamp.substr(14, 2));
    timeStruct.tm_sec = stoi(isoTimestamp.substr(17, 2));

#ifdef _WIN32
    time_t timeValue = _mkgmtime(&timeStruct);
#else
    time_t timeValue = timegm(&timeStruct);
#endif

    if (timeValue == static_cast<time_t>(-1))
        return false;
#ifdef _WIN32
    return localtime_s(&outTm, &timeValue) == 0;
#else
    tm *localPtr = localtime(&timeValue);
    if (!localPtr)
        return false;
    outTm = *localPtr;
    return true;
#endif
}

using namespace std;

string ordSuffix(int day) {
    if (day % 10 == 1 && day % 100 != 11)
        return "st";
    if (day % 10 == 2 && day % 100 != 12)
        return "nd";
    if (day % 10 == 3 && day % 100 != 13)
        return "rd";
    return "th";
}

string friendlyTimestamp(const string &isoTimestamp) {
    tm localTime{};
    if (!isoTimestampToLocalTm(isoTimestamp, localTime))
        return isoTimestamp;

    static const char *WEEK[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *MONTH[] = {
        "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November",
        "December"
    };

    int hour12 = localTime.tm_hour % 12;
    if (hour12 == 0)
        hour12 = 12;
    bool pm = localTime.tm_hour >= 12;

    ostringstream outputStream;
    outputStream << WEEK[localTime.tm_wday] << ", "
            << MONTH[localTime.tm_mon] << " " << localTime.tm_mday << ordSuffix(localTime.tm_mday)
            << ", " << hour12 << ':' << setfill('0') << setw(2) << localTime.tm_min << (pm ? " PM" : " AM");
    return outputStream.str();
}

string niceLabel(const LogInfo &logInfo) {
    if (!logInfo.timestamp.empty()) {
        tm localTime{};
        if (isoTimestampToLocalTm(logInfo.timestamp, localTime)) {
            int hour12 = localTime.tm_hour % 12;
            if (hour12 == 0)
                hour12 = 12;
            string minute = (localTime.tm_min < 10 ? "0" : "") + to_string(localTime.tm_min);
            string ampm = localTime.tm_hour >= 12 ? "PM" : "AM";
            ostringstream ss;
            ss << hour12 << ':' << minute << ' ' << ampm;
            return ss.str();
        }
    }
    return logInfo.fileName;
}
