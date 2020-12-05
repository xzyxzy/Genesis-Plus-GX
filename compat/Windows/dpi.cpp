#include <windows.h>

int main (int argc, char *argv[]);

// Fuck windows and its janky ass hidpi shit
// No, I am not including a manifest file just to stop you from fucking with scaling, microsoft
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nShowCmd)
{
    SetProcessDPIAware();
    return main(__argc, __argv);
}