#include <Windows.h>

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 現時点ではまだ使わないため、未使用警告を抑制
    static_cast<void>(hInstance);
    static_cast<void>(hPrevInstance);
    static_cast<void>(lpCmdLine);
    static_cast<void>(nCmdShow);
    MessageBox(NULL, L"Hello, World!", L"My First Windows App", MB_OK);
    OutputDebugStringA("Debug: Hello from OutputDebugStringA!\n");
    return 0;
}