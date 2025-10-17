#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <Xinput.h>

namespace KashipanEngine{

// 前方宣言
class WinApp;
class ScreenBuffer;

// 入力デバイスの種類
enum class InputDeviceType {
    None,           // 入力なし
    Keyboard,       // キーボード
    Mouse,          // マウス
    XBoxController, // XBoxコントローラー
};

// XBoxコントローラーのボタンコード
enum class XBoxButtonCode {
    UP              = XINPUT_GAMEPAD_DPAD_UP,
    DOWN            = XINPUT_GAMEPAD_DPAD_DOWN,
    LEFT            = XINPUT_GAMEPAD_DPAD_LEFT,
    RIGHT           = XINPUT_GAMEPAD_DPAD_RIGHT,
    START           = XINPUT_GAMEPAD_START,
    BACK            = XINPUT_GAMEPAD_BACK,
    LEFT_THUMB      = XINPUT_GAMEPAD_LEFT_THUMB,
    RIGHT_THUMB     = XINPUT_GAMEPAD_RIGHT_THUMB,
    LEFT_SHOULDER   = XINPUT_GAMEPAD_LEFT_SHOULDER,
    RIGHT_SHOULDER  = XINPUT_GAMEPAD_RIGHT_SHOULDER,
    A               = XINPUT_GAMEPAD_A,
    B               = XINPUT_GAMEPAD_B,
    X               = XINPUT_GAMEPAD_X,
    Y               = XINPUT_GAMEPAD_Y,
    Count           = 14, // ボタンの数
};

// 入力はどこでも使えるようにしたいのでstaticにする

/// @brief 入力管理クラス
class Input {
public:
    static void SetMainScreen(ScreenBuffer *screen);

    Input() = delete;
    Input(const Input &) = delete;
    Input(Input &&) = delete;
    Input &operator=(const Input &) = delete;

    //==================================================
    // 入力状態のオプション
    //==================================================

    // 入力の種類のオプション
    enum class InputTypeOption {
        Digital,    // デジタル入力 (ボタンなど)
        Analog,     // アナログ入力 (スティックやトリガーなど)
    };

    // 押下状態のオプション
    enum class DownStateOption {
        Down,       // 押下中
        Trigger,    // トリガー状態
        Release,    // リリース状態
    };

    // 現在か前回かのオプション
    enum class CurrentOption {
        Current,    // 現在の入力状態を取得
        Previous,   // 前回の入力状態を取得
    };

    // 実際の値か差分値かのオプション
    enum class ValueOption {
        Actual,     // 実際の値を取得
        Delta,      // 差分値を取得
    };

    // 左右のオプション
    enum class LeftRightOption {
        Left,       // 左側の入力を取得
        Right,      // 右側の入力を取得
    };

    // 軸方向のオプション
    enum class AxisOption {
        X,          // X軸方向の入力を取得
        Y,          // Y軸方向の入力を取得
        Z,          // Z軸方向の入力を取得(マウスホイールなど)
    };

    //==================================================
    // 初期化・更新・終了処理
    //==================================================

    /// @brief 初期化処理
    /// @param winApp WinAppインスタンス
    static void Initialize(WinApp *winApp);

    /// @brief 終了処理
    static void Finalize();

    /// @brief 入力状態更新
    static void Update();

    //==================================================
    // 入力状態の取得
    //==================================================

    /// @brief 現在入力中のデバイスの種類を取得
    /// @return 入力デバイスの種類 (キーボード、マウス、XBoxコントローラー)
    static InputDeviceType GetCurrentInputDeviceType();

    /// @brief 入力状態の取得
    /// @tparam T 取得する値の型 (bool, int, float)
    /// @param inputDeviceType 入力デバイスの種類
    /// @param downStateOption 押下状態のオプション
    /// @param keyCode キーコード (XBoxコントローラーの場合はボタンコード)
    /// @param inputTypeOption 入力の種類のオプション (デジタル入力かアナログ入力か)
    /// @param AxisOption 軸方向のオプション (マウスカーソルやXBoxコントローラーのスティックの場合)
    /// @param leftRightOption 左右のオプション (マウスカーソルやXBoxコントローラーのスティックの場合)
    /// @param threshold 押下とみなす閾値 (0～) (マウスカーソルやXBoxコントローラーのスティックの場合
    /// @param controllerIndex コントローラーのインデックス (0～3) (XBoxコントローラーの場合)
    /// @return 取得した値
    template <typename T>
    static T Get(
        InputDeviceType inputDeviceType,
        DownStateOption downStateOption,
        int keyCode,
        InputTypeOption inputTypeOption = InputTypeOption::Digital,
        AxisOption axisOption = AxisOption::X,
        LeftRightOption leftRightOption = LeftRightOption::Left,
        int threshold = 0,
        int controllerIndex = 0
    );

    //==================================================
    // キーボード入力関連
    //==================================================

    /// @brief キーの押下状態を取得
    /// @param key キーコード
    /// @param currentOption 現在か前回かのオプション
    /// @param downStateOption 押下状態のオプション
    /// @return 押下状態
    static bool IsKey(CurrentOption currentOption, DownStateOption downStateOption, int key);

    /// @brief キーの押下状態を取得
    /// @param key キーコード
    /// @return 押下状態
    static bool IsKeyDown(int key);

    /// @brief 前回のキーの押下状態を取得
    /// @param key キーコード
    /// @return 前回の押下状態
    static bool IsPreKeyDown(int key);

    /// @brief キーのトリガー状態を取得
    /// @param key キーコード
    /// @return トリガー状態
    static bool IsKeyTrigger(int key);

    /// @brief キーのリリース状態を取得
    /// @param key キーコード
    /// @return リリース状態
    static bool IsKeyRelease(int key);

    //==================================================
    // マウス入力関連
    //==================================================

    /// @brief マウスの入力状態を取得
    /// @tparam T 取得する値の型 (bool, int, float)
    /// @param inputTypeOption 入力の種類のオプション (デジタル入力かアナログ入力か)
    /// @param downStateOption 押下状態のオプション
    /// @param keyCode マウスボタンコード
    /// @param axisOption X軸かY軸かZ軸(マウスホイール)のオプション
    /// @param threshold 押下とみなす閾値 (0～)
    /// @return 取得した値
    template <typename T>
    static T GetMouse(
        InputTypeOption inputTypeOption,
        DownStateOption downStateOption,
        int keyCode,
        AxisOption axisOption = AxisOption::X,
        int threshold = 0
    );

    //--------- ボタン ---------//

    /// @brief マウスのボタンの押下状態を取得
    /// @param currentOption 現在か前回かのオプション
    /// @param downStateOption 押下状態のオプション
    /// @param button マウスボタンコード
    /// @return 押下状態
    static bool IsMouseButton(CurrentOption currentOption, DownStateOption downStateOption, int button);

    /// @brief マウスのボタンの押下状態を取得
    /// @param button マウスボタンコード
    /// @return 押下状態
    static bool IsMouseButtonDown(int button);

    /// @brief 前回のマウスのボタンの押下状態を取得
    /// @param button マウスボタンコード
    /// @return 前回の押下状態
    static bool IsPreMouseButtonDown(int button);

    /// @brief マウスのボタンのトリガー状態を取得
    /// @param button マウスボタンコード
    /// @return トリガー状態
    static bool IsMouseButtonTrigger(int button);

    /// @brief マウスのボタンのリリース状態を取得
    /// @param button マウスボタンコード
    /// @return リリース状態
    static bool IsMouseButtonRelease(int button);

    //--------- カーソル ---------//

    /// @brief マウスカーソルの位置をboolで取得
    /// @param downStateOption 押下状態のオプション
    /// @param axisOption X軸かY軸かZ軸(マウスホイール)のオプション
    /// @param threshold 押下とみなす閾値 (0～) (マウスホイールの場合は無視)
    /// @return マウスカーソルの位置が閾値を超えているかどうか (true: 超えている, false: 超えていない)
    static bool IsMousePos(DownStateOption downStateOption, AxisOption axisOption, int threshold = 16);

    /// @brief マウスカーソルの位置を取得
    /// @param currentOption 現在か前回かのオプション
    /// @param axisOption X軸かY軸かのオプション
    /// @param valueOption 実際の値か差分値かのオプション
    /// @return マウスカーソルの位置
    static int GetMousePos(CurrentOption currentOption, AxisOption axisOption, ValueOption valueOption);

    /// @brief マウスカーソルのX座標を取得
    /// @return マウスカーソルのX座標
    static int GetMouseX();

    /// @brief マウスカーソルのY座標を取得
    /// @return マウスカーソルのY座標
    static int GetMouseY();

    /// @brief マウスカーソルのX座標の移動量を取得
    /// @return マウスカーソルのX座標の移動量
    static int GetMouseDeltaX();

    /// @brief マウスカーソルのY座標の移動量を取得
    /// @return マウスカーソルのY座標の移動量
    static int GetMouseDeltaY();

    /// @brief 前回のマウスカーソルのX座標を取得
    /// @return 前回のマウスカーソルのX座標
    static int GetPreMouseX();

    /// @brief 前回のマウスカーソルのY座標を取得
    /// @return 前回のマウスカーソルのY座標
    static int GetPreMouseY();

    /// @brief 前回のマウスカーソルのX座標の移動量を取得
    /// @return 前回のマウスカーソルのX座標の移動量
    static int GetPreMouseDeltaX();

    /// @brief 前回のマウスカーソルのY座標の移動量を取得
    /// @return 前回のマウスカーソルのY座標の移動量
    static int GetPreMouseDeltaY();

    //--------- ホイール ---------//

    /// @brief マウスホイールの回転量を取得
    /// @return マウスホイールの回転量
    static int GetMouseWheel();

    /// @brief 前回のマウスホイールの回転量を取得
    /// @return 前回のマウスホイールの回転量
    static int GetPreMouseWheel();

    //==================================================
    // XBoxコントローラー入力関連
    //==================================================

    /// @brief XBoxコントローラーの入力状態を取得
    /// @tparam T 取得する値の型 (bool, int, float)
    /// @param inputTypeOption 入力の種類のオプション (デジタル入力かアナログ入力か)
    /// @param downStateOption 押下状態のオプション
    /// @param buttonCode XBoxコントローラーのボタンコード
    /// @param axisOption X軸かY軸かのオプション
    /// @param leftRightOption 左側か右側かのオプション
    /// @param threshold 押下とみなす閾値 (0～255) (スティックやトリガーの場合)
    /// @param index コントローラーのインデックス (0～3)
    /// @return 取得した値
    template <typename T>
    static T GetXBoxController(
        InputTypeOption inputTypeOption,
        DownStateOption downStateOption,
        int buttonCode,
        AxisOption axisOption = AxisOption::X,
        LeftRightOption leftRightOption = LeftRightOption::Left,
        int threshold = 128,
        int index = 0
    );

    //--------- トリガーボタン ---------//

    /// @brief XBoxコントローラーのトリガーの押下状態をboolで取得
    /// @param downStateOption 押下状態のオプション
    /// @param leftRightOption 左側か右側かのオプション
    /// @param threshold 押下とみなす閾値 (0～255)
    /// @param index コントローラーのインデックス (0～3)
    /// @return トリガーの押下状態 (true: 押下中, false: 押下していない)
    static bool IsXBoxTrigger(DownStateOption downStateOption, LeftRightOption leftRightOption, int threshold = 128, int index = 0);

    /// @brief XBoxコントローラーのトリガーの押下状態を取得
    /// @param currentOption 現在か前回かのオプション
    /// @param leftRightOption 左側か右側かのオプション
    /// @param valueOption 実際の値か差分値かのオプション
    /// @param index コントローラーのインデックス (0～3)
    /// @return トリガーの押下状態 (0～255)
    static int GetXBoxTrigger(CurrentOption currentOption, LeftRightOption leftRightOption, ValueOption valueOption, int index = 0);

    /// @brief XBoxコントローラーのトリガーの押下状態を割合で取得
    /// @param currentOption 現在か前回かのオプション
    /// @param leftRightOption 左側か右側かのオプション
    /// @param valueOption 実際の値か差分値かのオプション
    /// @param index コントローラーのインデックス (0～3)
    /// @return トリガーの押下状態 (0.0f～1.0f)
    static float GetXBoxTriggerRatio(CurrentOption currentOption, LeftRightOption leftRightOption, ValueOption valueOption, int index = 0);

    /// @brief XBoxコントローラーの左トリガーの押下状態を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左トリガーの押下状態 (0～255)
    static int GetXBoxLeftTrigger(int index = 0);

    /// @brief XBoxコントローラーの右トリガーの押下状態を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右トリガーの押下状態 (0～255)
    static int GetXBoxRightTrigger(int index = 0);

    /// @brief XBoxコントローラーの左トリガーの押下状態を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左トリガーの押下状態の差分 (0～255)
    static int GetXBoxLeftTriggerDelta(int index = 0);

    /// @brief XBoxコントローラーの右トリガーの押下状態を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右トリガーの押下状態の差分 (0～255)
    static int GetXBoxRightTriggerDelta(int index = 0);

    /// @brief XBoxコントローラーの左トリガーの押下状態を割合で取得 (0.0f～1.0f)
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左トリガーの押下状態 (0.0f～1.0f)
    static float GetXBoxLeftTriggerRatio(int index = 0);

    /// @brief XBoxコントローラーの右トリガーの押下状態を割合で取得 (0.0f～1.0f)
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右トリガーの押下状態 (0.0f～1.0f)
    static float GetXBoxRightTriggerRatio(int index = 0);

    /// @brief XBoxコントローラーの左トリガーの押下状態を差分で割合で取得 (0.0f～1.0f)
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左トリガーの押下状態の差分割合 (0.0f～1.0f)
    static float GetXBoxLeftTriggerDeltaRatio(int index = 0);

    /// @brief XBoxコントローラーの右トリガーの押下状態を差分で割合で取得 (0.0f～1.0f)
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右トリガーの押下状態の差分割合 (0.0f～1.0f)
    static float GetXBoxRightTriggerDeltaRatio(int index = 0);

    /// @brief 前回のXBoxコントローラーの左トリガーの押下状態を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左トリガーの押下状態 (0～255)
    static int GetPreXBoxLeftTrigger(int index = 0);

    /// @brief 前回のXBoxコントローラーの右トリガーの押下状態を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右トリガーの押下状態 (0～255)
    static int GetPreXBoxRightTrigger(int index = 0);

    /// @brief 前回のXBoxコントローラーの左トリガーの押下状態を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左トリガーの押下状態の差分 (0～255)
    static int GetPreXBoxLeftTriggerDelta(int index = 0);

    /// @brief 前回のXBoxコントローラーの右トリガーの押下状態を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右トリガーの押下状態の差分 (0～255)
    static int GetPreXBoxRightTriggerDelta(int index = 0);

    /// @brief 前回のXBoxコントローラーの左トリガーの押下状態を割合で取得 (0.0f～1.0f)
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左トリガーの押下状態 (0.0f～1.0f)
    static float GetPreXBoxLeftTriggerRatio(int index = 0);

    /// @brief 前回のXBoxコントローラーの右トリガーの押下状態を割合で取得 (0.0f～1.0f)
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右トリガーの押下状態 (0.0f～1.0f)
    static float GetPreXBoxRightTriggerRatio(int index = 0);

    /// @brief 前回のXBoxコントローラーの左トリガーの押下状態を差分で割合で取得 (0.0f～1.0f)
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左トリガーの押下状態の差分割合 (0.0f～1.0f)
    static float GetPreXBoxLeftTriggerDeltaRatio(int index = 0);

    /// @brief 前回のXBoxコントローラーの右トリガーの押下状態を差分で割合で取得 (0.0f～1.0f)
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右トリガーの押下状態の差分割合 (0.0f～1.0f)
    static float GetPreXBoxRightTriggerDeltaRatio(int index = 0);

    //--------- スティック ---------//

    /// @brief XBoxコントローラーのスティックの座標をboolで取得
    /// @param downStateOption 押下状態のオプション
    /// @param leftRightOption 左側か右側かのオプション
    /// @param axisOption X軸かY軸かのオプション
    /// @param threshold 押下とみなす閾値 (-32768～32767)
    /// @param index コントローラーのインデックス (0～3)
    /// @return スティックの座標が閾値を超えているかどうか (true: 超えている, false: 超えていない)
    static bool IsXBoxStick(DownStateOption downStateOption, LeftRightOption leftRightOption, AxisOption axisOption, int threshold = 32768, int index = 0);

    /// @brief XBoxコントローラーのスティックの座標を取得
    /// @param currentOption 現在か前回かのオプション
    /// @param leftRightOption 左側か右側かのオプション
    /// @param axisOption X軸かY軸かのオプション
    /// @param valueOption 実際の値か差分値かのオプション
    /// @param index コントローラーのインデックス (0～3)
    /// @return スティックの座標 (-32768～32767)
    static int GetXBoxStick(CurrentOption currentOption, LeftRightOption leftRightOption, AxisOption axisOption, ValueOption valueOption, int index = 0);

    /// @brief XBoxコントローラーのスティックの座標を割合で取得
    /// @param currentOption 現在か前回かのオプション
    /// @param leftRightOption 左側か右側かのオプション
    /// @param axisOption X軸かY軸かのオプション
    /// @param valueOption 実際の値か差分値かのオプション
    /// @param index コントローラーのインデックス (0～3)
    /// @return スティックの座標の割合 (-1.0～1.0)
    static float GetXBoxStickRatio(CurrentOption currentOption, LeftRightOption leftRightOption, AxisOption axisOption, ValueOption valueOption, int index = 0);

    /// @brief XBoxコントローラーの左スティックのX座標を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左スティックのX座標 (-32768～32767)
    static int GetXBoxLeftStickX(int index = 0);

    /// @brief XBoxコントローラーの左スティックのY座標を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左スティックのY座標 (-32768～32767)
    static int GetXBoxLeftStickY(int index = 0);

    /// @brief XBoxコントローラーの左スティックのX座標を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左スティックのX座標の差分 (-32768～32767)
    static int GetXBoxLeftStickDeltaX(int index = 0);

    /// @brief XBoxコントローラーの左スティックのY座標を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左スティックのY座標の差分 (-32768～32767)
    static int GetXBoxLeftStickDeltaY(int index = 0);

    /// @brief XBoxコントローラーの左スティックのX座標を割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左スティックのX座標の割合 (-1.0～1.0)
    static float GetXBoxLeftStickRatioX(int index = 0);

    /// @brief XBoxコントローラーの左スティックのY座標を割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左スティックのY座標の割合 (-1.0～1.0)
    static float GetXBoxLeftStickRatioY(int index = 0);

    /// @brief XBoxコントローラーの左スティックのX座標を差分で割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左スティックのX座標の差分割合 (-1.0～1.0)
    static float GetXBoxLeftStickDeltaRatioX(int index = 0);

    /// @brief XBoxコントローラーの左スティックのY座標を差分で割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 左スティックのY座標の差分割合 (-1.0～1.0)
    static float GetXBoxLeftStickDeltaRatioY(int index = 0);

    /// @brief XBoxコントローラーの右スティックのX座標を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右スティックのX座標 (-32768～32767)
    static int GetXBoxRightStickX(int index = 0);

    /// @brief XBoxコントローラーの右スティックのY座標を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右スティックのY座標 (-32768～32767)
    static int GetXBoxRightStickY(int index = 0);

    /// @brief XBoxコントローラーの右スティックのX座標を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右スティックのX座標の差分 (-32768～32767)
    static int GetXBoxRightStickDeltaX(int index = 0);

    /// @brief XBoxコントローラーの右スティックのY座標を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右スティックのY座標の差分 (-32768～32767)
    static int GetXBoxRightStickDeltaY(int index = 0);

    /// @brief XBoxコントローラーの右スティックのX座標を割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右スティックのX座標の割合 (-1.0～1.0)
    static float GetXBoxRightStickRatioX(int index = 0);

    /// @brief XBoxコントローラーの右スティックのY座標を割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右スティックのY座標の割合 (-1.0～1.0)
    static float GetXBoxRightStickRatioY(int index = 0);

    /// @brief XBoxコントローラーの右スティックのX座標を差分で割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右スティックのX座標の差分割合 (-1.0～1.0)
    static float GetXBoxRightStickDeltaRatioX(int index = 0);

    /// @brief XBoxコントローラーの右スティックのY座標を差分で割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 右スティックのY座標の差分割合 (-1.0～1.0)
    static float GetXBoxRightStickDeltaRatioY(int index = 0);

    /// @brief 前回のXBoxコントローラーの左スティックのX座標を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左スティックのX座標 (-32768～32767)
    static int GetPreXBoxLeftStickX(int index = 0);

    /// @brief 前回のXBoxコントローラーの左スティックのY座標を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左スティックのY座標 (-32768～32767)
    static int GetPreXBoxLeftStickY(int index = 0);

    /// @brief 前回のXBoxコントローラーの左スティックのX座標を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左スティックのX座標の差分 (-32768～32767)
    static int GetPreXBoxLeftStickDeltaX(int index = 0);

    /// @brief 前回のXBoxコントローラーの左スティックのY座標を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左スティックのY座標の差分 (-32768～32767)
    static int GetPreXBoxLeftStickDeltaY(int index = 0);

    /// @brief 前回のXBoxコントローラーの左スティックのX座標を割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左スティックのX座標の割合 (-1.0～1.0)
    static float GetPreXBoxLeftStickRatioX(int index = 0);

    /// @brief 前回のXBoxコントローラーの左スティックのY座標を割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左スティックのY座標の割合 (-1.0～1.0)
    static float GetPreXBoxLeftStickRatioY(int index = 0);

    /// @brief 前回のXBoxコントローラーの左スティックのX座標を差分で割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左スティックのX座標の差分割合 (-1.0～1.0)
    static float GetPreXBoxLeftStickDeltaRatioX(int index = 0);

    /// @brief 前回のXBoxコントローラーの左スティックのY座標を差分で割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の左スティックのY座標の差分割合 (-1.0～1.0)
    static float GetPreXBoxLeftStickDeltaRatioY(int index = 0);

    /// @brief 前回のXBoxコントローラーの右スティックのX座標を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右スティックのX座標 (-32768～32767)
    static int GetPreXBoxRightStickX(int index = 0);

    /// @brief 前回のXBoxコントローラーの右スティックのY座標を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右スティックのY座標 (-32768～32767)
    static int GetPreXBoxRightStickY(int index = 0);

    /// @brief 前回のXBoxコントローラーの右スティックのX座標を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右スティックのX座標の差分 (-32768～32767)
    static int GetPreXBoxRightStickDeltaX(int index = 0);

    /// @brief 前回のXBoxコントローラーの右スティックのY座標を差分で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右スティックのY座標の差分 (-32768～32767)
    static int GetPreXBoxRightStickDeltaY(int index = 0);

    /// @brief 前回のXBoxコントローラーの右スティックのX座標を割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右スティックのX座標の割合 (-1.0～1.0)
    static float GetPreXBoxRightStickRatioX(int index = 0);

    /// @brief 前回のXBoxコントローラーの右スティックのY座標を割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右スティックのY座標の割合 (-1.0～1.0)
    static float GetPreXBoxRightStickRatioY(int index = 0);

    /// @brief 前回のXBoxコントローラーの右スティックのX座標を差分で割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右スティックのX座標の差分割合 (-1.0～1.0)
    static float GetPreXBoxRightStickDeltaRatioX(int index = 0);

    /// @brief 前回のXBoxコントローラーの右スティックのY座標を差分で割合で取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の右スティックのY座標の差分割合 (-1.0～1.0)
    static float GetPreXBoxRightStickDeltaRatioY(int index = 0);

    //--------- ボタン ---------//

    /// @brief XBoxコントローラーのボタンの押下状態を取得
    /// @param currentOption 現在か前回かのオプション
    /// @param downStateOption 押下状態かトリガー状態かのオプション
    /// @param button ボタンコード
    /// @param index コントローラーのインデックス (0～3)
    /// @return 押下状態
    static bool IsXBoxButton(CurrentOption currentOption, DownStateOption downStateOption, int button, int index = 0);

    /// @brief XBoxコントローラーのボタンの押下状態を取得
    /// @param button ボタンコード
    /// @param index コントローラーのインデックス (0～3)
    /// @return 押下状態
    static bool IsXBoxButtonDown(int button, int index = 0);

    /// @brief 前回のXBoxコントローラーのボタンの押下状態を取得
    /// @param button ボタンコード
    /// @param index コントローラーのインデックス (0～3)
    /// @return 前回の押下状態
    static bool IsPreXBoxButtonDown(int button, int index = 0);

    /// @brief XBoxコントローラーのボタンのトリガー状態を取得
    /// @param button ボタンコード
    /// @param index コントローラーのインデックス (0～3)
    /// @return トリガー状態
    static bool IsXBoxButtonTrigger(int button, int index = 0);

    /// @brief XBoxコントローラーのボタンのリリース状態を取得
    /// @param button ボタンコード
    /// @param index コントローラーのインデックス (0～3)
    /// @return リリース状態
    static bool IsXBoxButtonRelease(int button, int index = 0);

    //--------- 接続状態 ---------//

    /// @brief 接続されているXBoxコントローラーの数を取得
    /// @return 接続されているXBoxコントローラーの数 (0～4)
    static int GetXBoxConnectedCount();

    /// @brief XBoxコントローラーの接続状態を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 接続状態 (true: 接続されている, false: 接続されていない)
    static bool IsXBoxConnected(int index = 0);

    /// @brief 前回のXBoxコントローラーの接続状態を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @return 接続状態 (true: 接続されている, false: 接続されていない)
    static bool IsPreXBoxConnected(int index = 0);

    //--------- バイブレーション ---------//

    /// @brief XBoxコントローラーのバイブレーションを設定
    /// @param index コントローラーのインデックス (0～3)
    /// @param leftMotor 左モーターの強さ (0～65535) (-1で現在の値を維持)
    /// @param rightMotor 右モーターの強さ (0～65535) (-1で現在の値を維持)
    static void SetXBoxVibration(int index, int leftMotor, int rightMotor);

    /// @brief XBoxコントローラーのバイブレーションを停止
    /// @param index コントローラーのインデックス (0～3)
    static void StopXBoxVibration(int index);

    /// @brief XBoxコントローラーのバイブレーション値を取得
    /// @param index コントローラーのインデックス (0～3)
    /// @param leftRightOption 左側か右側かのオプション
    /// @return XBoxコントローラーのバイブレーション値
    static int GetXBoxVibration(int index, LeftRightOption leftRightOption);

private:
};

template<typename T>
inline T Input::Get(InputDeviceType inputDeviceType, DownStateOption downStateOption, int keyCode,
    InputTypeOption inputTypeOption, AxisOption axisOption, LeftRightOption leftRightOption,
    int threshold, int controllerIndex) {
    bool isKey = false;
    switch (inputDeviceType) {
        case InputDeviceType::Keyboard:
            isKey = IsKey(CurrentOption::Current, downStateOption, keyCode);
            if (inputTypeOption == InputTypeOption::Digital) {
                return isKey;
            } else if (inputTypeOption == InputTypeOption::Analog) {
                return isKey ? static_cast<T>(1) : static_cast<T>(0);
            }
        case InputDeviceType::Mouse:
            return GetMouse<T>(inputTypeOption, downStateOption, keyCode, axisOption, threshold);
        case InputDeviceType::XBoxController:
            return GetXBoxController<T>(inputTypeOption, downStateOption, keyCode, axisOption, leftRightOption, threshold, controllerIndex);
        default:
            return T();
    }
}

template<typename T>
inline T Input::GetMouse(InputTypeOption inputTypeOption, DownStateOption downStateOption,
    int keyCode, AxisOption axisOption, int threshold) {
    switch (inputTypeOption) {
        case InputTypeOption::Digital:
            if (keyCode < 0) {
                return IsMousePos(downStateOption, axisOption, threshold);
            } else {
                return IsMouseButton(CurrentOption::Current, downStateOption, keyCode);
            }
        case InputTypeOption::Analog:
            return static_cast<T>(GetMousePos(CurrentOption::Current, axisOption, ValueOption::Actual));
        default:
            return T();
    }
}

template<typename T>
inline T Input::GetXBoxController(InputTypeOption inputTypeOption, DownStateOption downStateOption, int buttonCode, AxisOption axisOption, LeftRightOption leftRightOption, int threshold, int index) {
    switch (inputTypeOption) {
        case InputTypeOption::Digital:
            if (buttonCode < 0) {
                return IsXBoxStick(downStateOption, leftRightOption, axisOption, threshold, index);
            } else {
                return IsXBoxButton(CurrentOption::Current, downStateOption, buttonCode, index);
            }
        case InputTypeOption::Analog:
            return GetXBoxStickRatio(CurrentOption::Current, leftRightOption, axisOption, ValueOption::Actual, index);
        default:
            return T();
    }
}

} // namespace KashipanEngine