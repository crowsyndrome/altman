#pragma once

#include <string>
#include <ctime>
#include "log_types.h"

std::string ordSuffix(int day);

std::string friendlyTimestamp(const std::string& isoTimestamp);

std::string niceLabel(const LogInfo& logInfo);

// Convert an ISO8601 UTC timestamp to local time. Returns true on success.
bool isoTimestampToLocalTm(const std::string& isoTimestamp, std::tm& outTm);
