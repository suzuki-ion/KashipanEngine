#pragma once

namespace KashipanEngine {

class GameEngine;
/// @brief デルタタイムの更新（GameEngine専用）
void UpdateDeltaTime(Passkey<GameEngine>);

/// @brief 時間記録構造体
struct TimeRecord {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    long long second;
    long long millisecond;
};

/// @brief 現在の年を取得する
int GetNowTimeYear();
/// @brief 現在の月を取得する
int GetNowTimeMonth();
/// @brief 現在の日を取得する
int GetNowTimeDay();
/// @brief 現在の時を取得する
int GetNowTimeHour();
/// @brief 現在の分を取得する
int GetNowTimeMinute();
/// @brief 現在の秒を取得する
long long GetNowTimeSecond();
/// @brief 現在のミリ秒を取得する
long long GetNowTimeMillisecond();

/// @brief 現在の時間を取得する
TimeRecord GetNowTime();

/// @brief ゲームの実行開始からの経過年数を取得する
int GetGameRuntimeYear();
/// @brief ゲームの実行開始からの経過月数を取得する
int GetGameRuntimeMonth();
/// @brief ゲームの実行開始からの経過日数を取得する
int GetGameRuntimeDay();
/// @brief ゲームの実行開始からの経過時間（時）を取得する
int GetGameRuntimeHour();
/// @brief ゲームの実行開始からの経過時間（分）を取得する
int GetGameRuntimeMinute();
/// @brief ゲームの実行開始からの経過時間（秒）を取得する
long long GetGameRuntimeSecond();
/// @brief ゲームの実行開始からの経過時間（ミリ秒）を取得する
long long GetGameRuntimeMillisecond();

/// @brief ゲームの実行開始からの経過時間を取得する
TimeRecord GetGameRuntime();

/// @brief 時間計測の開始
/// @param label 計測ラベル
void StartTimeMeasurement(const std::string &label);
/// @brief 時間計測の終了
/// @param label 計測ラベル
/// @return 経過時間
TimeRecord EndTimeMeasurement(const std::string &label);

/// @brief 前フレームからの経過時間（秒）を取得する
float GetDeltaTime();

/// @brief 現在の時間を文字列で取得する
/// @param format フォーマット（Year: %Y, Month: %m, Day: %d, Hour: %H, Minute: %M, Second: %S）
/// @return 時間文字列
std::string GetNowTimeString(const std::string &format = "%Y-%m-%d %H:%M:%S");

} // namespace KashipanEngine