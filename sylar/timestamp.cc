#include <inttypes.h>
#include <sys/time.h>
#include "timestamp.h"

namespace sylar {

Timestamp::Timestamp() : m_microSecondSinceEpoch(0) {
}

Timestamp::Timestamp(int64_t microSecondsSinceEpochArg)
    : m_microSecondSinceEpoch(microSecondsSinceEpochArg) {
}

std::string Timestamp::toString() const {
    int64_t seconds = m_microSecondSinceEpoch / kMicroSecondsPerSecond;
    int64_t microseconds = m_microSecondSinceEpoch % kMicroSecondsPerSecond;
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
    return buf;
}

std::string Timestamp::toFormattedString(bool showMicrosencods) const {
    char buf[64] = {0};
    time_t seconds = static_cast<time_t>(m_microSecondSinceEpoch / kMicroSecondsPerSecond);
    struct tm tm_time;
    gmtime_r(&seconds, &tm_time);

    if (showMicrosencods) {
        int microseconds = static_cast<int>(m_microSecondSinceEpoch % kMicroSecondsPerSecond);
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
                tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
                microseconds);
    } else {
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
                tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    }

    return buf;
}

/// get time of now
Timestamp Timestamp::now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t seconds = tv.tv_sec;
    return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

}
