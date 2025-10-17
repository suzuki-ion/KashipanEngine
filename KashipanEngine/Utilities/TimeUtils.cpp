#include "TimeUtils.h"
#include <chrono>
#include <format>
#include <ctime>
#include <unordered_map>

namespace KashipanEngine {

namespace {
/// @brief プログラム実行開始時間
const auto kProgramStartTime = std::chrono::system_clock::now();
/// @brief タイマーラベルの開始時間マップ
std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> sTimerStartTimes;

auto GetZonedTime() {
    auto now = std::chrono::system_clock::now();
    auto zone = std::chrono::current_zone();
    return std::chrono::zoned_time(zone, now);
}

} // namespace

int GetNowTimeYear() {
    auto zonedTime = GetZonedTime();
    auto localTime = zonedTime.get_local_time();
    auto ymd = std::chrono::year_month_day{ std::chrono::floor<std::chrono::days>(localTime) };
    return static_cast<int>(ymd.year());
}

int GetNowTimeMonth() {
    auto zonedTime = GetZonedTime();
    auto localTime = zonedTime.get_local_time();
    auto ymd = std::chrono::year_month_day{ std::chrono::floor<std::chrono::days>(localTime) };
    return static_cast<unsigned>(ymd.month());
}

int GetNowTimeDay() {
    auto zonedTime = GetZonedTime();
    auto localTime = zonedTime.get_local_time();
    auto ymd = std::chrono::year_month_day{ std::chrono::floor<std::chrono::days>(localTime) };
    return static_cast<unsigned>(ymd.day());
}

int GetNowTimeHour() {
    auto zonedTime = GetZonedTime();
    auto localTime = zonedTime.get_local_time();
    auto timeOfDay = std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::seconds>(localTime - std::chrono::floor<std::chrono::days>(localTime)) };
    return timeOfDay.hours().count();
}

int GetNowTimeMinute() {
    auto zonedTime = GetZonedTime();
    auto localTime = zonedTime.get_local_time();
    auto timeOfDay = std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::seconds>(localTime - std::chrono::floor<std::chrono::days>(localTime)) };
    return timeOfDay.minutes().count();
}

int GetNowTimeSecond() {
    auto zonedTime = GetZonedTime();
    auto localTime = zonedTime.get_local_time();
    auto timeOfDay = std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::seconds>(localTime - std::chrono::floor<std::chrono::days>(localTime)) };
    return timeOfDay.seconds().count();
}

int GetNowTimeMillisecond() {
    auto zonedTime = GetZonedTime();
    auto localTime = zonedTime.get_local_time();
    auto timeOfDay = std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::milliseconds>(localTime - std::chrono::floor<std::chrono::days>(localTime)) };
    return static_cast<int>(timeOfDay.subseconds().count());
}

TimeRecord GetNowTime() {
    TimeRecord record = {};
    auto zonedTime = GetZonedTime();
    auto localTime = zonedTime.get_local_time();
    auto ymd = std::chrono::year_month_day{ std::chrono::floor<std::chrono::days>(localTime) };
    record.year = static_cast<int>(ymd.year());
    record.month = static_cast<unsigned>(ymd.month());
    record.day = static_cast<unsigned>(ymd.day());
    auto timeOfDay = std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::milliseconds>(localTime - std::chrono::floor<std::chrono::days>(localTime)) };
    record.hour = timeOfDay.hours().count();
    record.minute = timeOfDay.minutes().count();
    record.second = timeOfDay.seconds().count();
    record.millisecond = static_cast<int>(timeOfDay.subseconds().count());
    return record;
}

int GetGameRuntimeYear() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - kProgramStartTime;
    auto years = std::chrono::duration_cast<std::chrono::years>(elapsed);
    return static_cast<int>(years.count());
}

int GetGameRuntimeMonth() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - kProgramStartTime;
    auto months = std::chrono::duration_cast<std::chrono::months>(elapsed);
    return static_cast<int>(months.count());
}

int GetGameRuntimeDay() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - kProgramStartTime;
    auto days = std::chrono::duration_cast<std::chrono::days>(elapsed);
    return static_cast<int>(days.count());
}

int GetGameRuntimeHour() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - kProgramStartTime;
    auto hours = std::chrono::duration_cast<std::chrono::hours>(elapsed);
    return static_cast<int>(hours.count());
}

int GetGameRuntimeMinute() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - kProgramStartTime;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(elapsed);
    return static_cast<int>(minutes.count());
}

int GetGameRuntimeSecond() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - kProgramStartTime;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed);
    return static_cast<int>(seconds.count());
}

int GetGameRuntimeMillisecond() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - kProgramStartTime;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
    return static_cast<int>(milliseconds.count());
}

TimeRecord GetGameRuntime() {
    TimeRecord record = {};
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - kProgramStartTime;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    record.year = static_cast<int>(ms / (static_cast<long long>(1000 * 60 * 60 * 24) * 365));
    ms %= (static_cast<long long>(1000 * 60 * 60 * 24) * 365);
    record.month = static_cast<int>(ms / (static_cast<long long>(1000 * 60 * 60 * 24) * 30));
    ms %= (static_cast<long long>(1000 * 60 * 60 * 24) * 30);
    record.day = static_cast<int>(ms / (1000 * 60 * 60 * 24));
    ms %= (1000 * 60 * 60 * 24);
    record.hour = static_cast<int>(ms / (1000 * 60 * 60));
    ms %= (1000 * 60 * 60);
    record.minute = static_cast<int>(ms / (1000 * 60));
    ms %= (1000 * 60);
    record.second = static_cast<int>(ms / 1000);
    record.millisecond = static_cast<int>(ms % 1000);
    return record;
}

void StartTimeMeasurement(const std::string &label) {
    sTimerStartTimes[label] = std::chrono::high_resolution_clock::now();
}

TimeRecord EndTimeMeasurement(const std::string &label) {
    TimeRecord record = {};
    auto endTime = std::chrono::high_resolution_clock::now();
    if (sTimerStartTimes.find(label) != sTimerStartTimes.end()) {
        auto startTime = sTimerStartTimes[label];
        auto duration = endTime - startTime;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        record.year = static_cast<int>(ms / (static_cast<long long>(1000 * 60 * 60 * 24) * 365));
        ms %= (static_cast<long long>(1000 * 60 * 60 * 24) * 365);
        record.month = static_cast<int>(ms / (static_cast<long long>(1000 * 60 * 60 * 24) * 30));
        ms %= (static_cast<long long>(1000 * 60 * 60 * 24) * 30);
        record.day = static_cast<int>(ms / (1000 * 60 * 60 * 24));
        ms %= (1000 * 60 * 60 * 24);
        record.hour = static_cast<int>(ms / (1000 * 60 * 60));
        ms %= (1000 * 60 * 60);
        record.minute = static_cast<int>(ms / (1000 * 60));
        ms %= (1000 * 60);
        record.second = static_cast<int>(ms / 1000);
        record.millisecond = static_cast<int>(ms % 1000);
        sTimerStartTimes.erase(label); // 計測終了後に削除
    }
    return record;
}

float GetDeltaTime() {
    return 0.0f;
}

std::string GetNowTimeString(const std::string &format) {
    auto now = std::chrono::system_clock::now();
    auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    auto zonedTime = std::chrono::zoned_time{ std::chrono::current_zone(), nowSeconds };
    auto timeString = std::vformat("{:" + format + "}", std::make_format_args(zonedTime));
    return timeString;
}

} // namespace KashipanEngine