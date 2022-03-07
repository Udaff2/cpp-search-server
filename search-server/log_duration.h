#pragma once

#include <chrono>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x, cout) LogDuration UNIQUE_VAR_NAME_PROFILE(x, cout)

class LogDuration {
public:
    using Clock = std::chrono::steady_clock;

    LogDuration(const std::string& id, std::ostream& out = std::cerr);
    ~LogDuration();

private:
    const std::string id_;
    std::ostream& out_;
    const Clock::time_point start_time_ = Clock::now();
};