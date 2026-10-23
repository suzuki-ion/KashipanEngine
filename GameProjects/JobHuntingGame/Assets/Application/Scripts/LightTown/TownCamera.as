#include "TownState.as"
// 右ドラッグ：自転、中ドラッグ：パン、WASD：移動、ホイール：視線方向へ前後移動。
class TownCameraRig {
    Object@ camera;
    WindowObject@ window;
    Vector3 position;
    // distanceは初期配置用。焦点位置はCityControllerが選択セルを基準に制御する。
    float yaw = -0.6435f, pitch = 0.72f, distance = 48;
    Vector2 previousMouse;
    bool rotating = false, panning = false;
    bool manipulating = false;
    void Initialize(Scene@ scene) {
        @camera = scene.GetObject("TownCamera");
        // 初期の見え方を維持し、その後は位置と向きを独立して扱う。
        position = Vector3(0, 0, 0) - Orientation().RotateVector(Vector3(0, 0, 1)) * distance;
        NormalWindowObject@ normal; scene.GetObject("TownWindow").GetComponent(@normal); @window = normal;
        previousMouse = window.GetMousePosition();
        Apply();
    }
    void Cancel() {
        rotating = false; panning = false; manipulating = false;
        if (window !is null) previousMouse = window.GetMousePosition();
    }
    Quaternion Orientation() {
        return Math::MakeRotateAxisAngle(Vector3(0, 1, 0), yaw) * Math::MakeRotateAxisAngle(Vector3(1, 0, 0), TownClamp(pitch, 0.3f, 1.3f));
    }
    void Rotate(const Vector2 &in delta) { yaw += delta.x * 0.005f; pitch += delta.y * 0.005f; }
    void Pan(const Vector2 &in delta) {
        Quaternion orientation = Orientation();
        position += orientation.RotateVector(Vector3(1, 0, 0)) * (-delta.x * 0.055f)
               + orientation.RotateVector(Vector3(0, 1, 0)) * (delta.y * 0.055f);
    }
    void Move(float horizontal, float vertical, float dt) {
        Quaternion orientation = Orientation();
        float scale = horizontal != 0 && vertical != 0 ? 0.7071068f : 1.0f;
        position += (orientation.RotateVector(Vector3(1, 0, 0)) * horizontal
                + orientation.RotateVector(Vector3(0, 0, 1)) * vertical) * (dt * 12 * scale);
    }
    void Scroll(float delta) {
        // GetCommandValueは入力を±1へ制限済み。生のホイール単位(120)で再除算しない。
        position += Orientation().RotateVector(Vector3(0, 0, 1)) * (TownClamp(delta, -1, 1) * 2.5f);
    }
    void Apply() {
        pitch = TownClamp(pitch, 0.3f, 1.3f);
        position.x = TownClamp(position.x, -90, 90); position.z = TownClamp(position.z, -90, 90);
        Quaternion orientation = Orientation();
        Transform@ transform = camera.GetTransform();
        transform.SetRotateQuaternion(orientation);
        transform.SetTranslate(position);
    }
    void Update(float dt, bool overUI) {
        Vector2 mouse = window.GetMousePosition();
        Vector2 delta = mouse - previousMouse; previousMouse = mouse;
        manipulating = false;
        if (!window.IsWindowFocused() || !window.IsMouseInside()) { Cancel(); return; }
        if (IsCommandTriggered("TownRotatePress")) rotating = !overUI;
        if (IsCommandTriggered("TownPanPress")) panning = !overUI;
        if (!IsCommandTriggered("TownRotateHold")) rotating = false;
        if (!IsCommandTriggered("TownPanHold")) panning = false;
        delta.x = TownClamp(delta.x, -120, 120); delta.y = TownClamp(delta.y, -120, 120);
        if (rotating) { Rotate(delta); manipulating = true; }
        if (panning) { Pan(delta); manipulating = true; }
        float horizontal = (IsCommandTriggered("TownCameraRight") ? 1.0f : 0.0f) - (IsCommandTriggered("TownCameraLeft") ? 1.0f : 0.0f);
        float vertical = (IsCommandTriggered("TownCameraForward") ? 1.0f : 0.0f) - (IsCommandTriggered("TownCameraBack") ? 1.0f : 0.0f);
        if (horizontal != 0 || vertical != 0) {
            Move(horizontal, vertical, dt); manipulating = true;
        }
        float wheel = GetCommandValue("TownCameraScroll");
        if (!overUI && wheel != 0) { Scroll(wheel); manipulating = true; }
        if (manipulating) Apply();
    }
}
