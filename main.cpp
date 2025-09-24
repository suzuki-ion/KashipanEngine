#include <Windows.h>

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    MessageBox(NULL, L"Hello, World!", L"My First Windows App", MB_OK);
    OutputDebugStringA("Debug: Hello from OutputDebugStringA!\n");
    return 0;
}