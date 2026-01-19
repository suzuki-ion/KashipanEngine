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

class GameTimer {
public:

    // デフォルトコンストラクタ
    GameTimer() = default;

    /// @brief コンストラクタ
    /// @param duration タイマーの継続時間
    /// @param loop ループするかどうか
    GameTimer(float duration, bool loop = false);

    /// @brief タイマーの更新
    void Update();

    /// @brief タイマーを開始
    /// @param duration タイマーの継続時間
    /// @param loop ループするかどうか
    void Start(float duration, bool loop = false);

    /// @brief タイマーを停止
    void Stop();

    /// @brief タイマーをリセット
    void Reset();

    /// @brief タイマーを一時停止
    void Pause();

    /// @brief タイマーを再開
    void Resume();

    /// @brief タイマーが動作中かどうか
    /// @return アクティブな場合true
    bool IsActive() const;

    /// @brief タイマーが完了したかどうか
    /// @return 完了した場合true
    bool IsFinished() const;

    /// @brief タイマーの進行状況を取得 0.0～1.0
    /// @return 進行状況 0.0 = 開始、1.0 = 完了
    float GetProgress() const;

    /// @brief タイマーの逆進行状況を取得 1.0～0.0 
    /// @return 逆進行状況 1.0=開始、0.0=完了
    float GetReverseProgress() const;

    /// @brief 残り時間を取得
    /// @return 残り時間
    float GetRemainingTime() const;

    /// @brief 経過時間を取得
    /// @return 経過時間
    float GetElapsedTime() const;

    /// @brief タイマーの継続時間を取得
    /// @return 継続時間
    float GetDuration() const;

    /// @brief 継続時間を変更
    /// @param duration 新しい継続時間
    void SetDuration(float duration);

    /// @brief ループ設定を変更
    /// @param loop ループするかどうか
    void SetLoop(bool loop);

private:
    float currentTime_ = 0.0f;
    float duration_ = 0.0f;
    bool isActive_ = false;
    bool loop_ = false;
    bool finished_ = false;
    bool loopedThisFrame_ = false;
    int totalFrames_ = 0;
    bool useFrameMode_ = false;
    float targetFPS_ = 60.0f;
};

} // namespace KashipanEngine