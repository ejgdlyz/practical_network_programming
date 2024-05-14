#pragma once

#include <iostream>
#include <stdint.h>

namespace sylar {

class Timestamp {
public:
    Timestamp();

    explicit Timestamp(int64_t microSecondsSinceEpochArg);

    void swap(Timestamp& oth) { std::swap(m_microSecondSinceEpoch, oth.m_microSecondSinceEpoch); }

    std::string toString() const;
    std::string toFormattedString(bool showMicrosencods = true) const;

    bool valid() const { return m_microSecondSinceEpoch > 0;}

    /// internal usage
    int64_t  microSecondSinceEpoch() const { return m_microSecondSinceEpoch;}
    time_t secondSinceEpoch() const { return static_cast<time_t>(m_microSecondSinceEpoch / kMicroSecondsPerSecond);}

    /// get time of now
    static Timestamp now();
    static Timestamp invalid() { return Timestamp();}

    static Timestamp fromUnixTime(time_t t) { return fromUnixTime(t, 0);}
    static Timestamp fromUnixTime(time_t t, int microseconds) { 
        return Timestamp(static_cast<int64_t>(t) * kMicroSecondsPerSecond + microseconds);
    }

    static const int kMicroSecondsPerSecond = 1000 * 1000;
private:
    int64_t m_microSecondSinceEpoch;
};

inline bool operator<(const Timestamp& lhs, const Timestamp& rhs) {
    return lhs.microSecondSinceEpoch() < rhs.microSecondSinceEpoch();
}

inline bool operator==(const Timestamp& lhs, const Timestamp& rhs) {
    return lhs.microSecondSinceEpoch() == rhs.microSecondSinceEpoch();
}

inline double timeDifference(const Timestamp& high, const Timestamp& low) {
    int64_t diff = high.microSecondSinceEpoch() - low.microSecondSinceEpoch();
    return static_cast<double>(diff * 1.0 / Timestamp::kMicroSecondsPerSecond);
}

inline std::string timeDifferenceFormat(const Timestamp& high, const Timestamp& low) {
    int64_t diff = high.microSecondSinceEpoch() - low.microSecondSinceEpoch();
    int64_t total_seconds = diff / Timestamp::kMicroSecondsPerSecond;
    int days = total_seconds / (24 * 3600);
    int hours = total_seconds / (3600);
    total_seconds %= 3600;
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;
    
    char buf[24] = {0};
    snprintf(buf, sizeof(buf), "%ddays %02d:%02d:%02d", days, hours, minutes, seconds);
    return buf;
}

inline Timestamp addTime(Timestamp& timestamp, double seconds) {
    int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondSinceEpoch() + delta);
}

}
