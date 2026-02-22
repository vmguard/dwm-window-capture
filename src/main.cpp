#include "capture.h"
#include <iostream>
#include <ctime>

std::wstring GetTimestampPath() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    wchar_t buf[64];
    wcsftime(buf, 64, L"cap_%Y%m%d_%H%M%S.png", &tm);
    return buf;
}

int main() {
    try {
        ComInitializer com;
        D3DContext d3d;
        PngEncoder encoder;
        WindowCapturer capturer(d3d);

        const int HOTKEY_ID = 1;
        if (!RegisterHotKey(nullptr, HOTKEY_ID, MOD_ALT, VK_INSERT)) {
            std::cerr << "Hotkey registration failed: " << GetLastError() << std::endl;
            return 1;
        }

        std::cout << "Press Alt + insert to capture." << std::endl;

        MSG msg{};
        while (GetMessage(&msg, nullptr, 0, 0)) {
            if (msg.message == WM_HOTKEY && msg.wParam == HOTKEY_ID) {
                HWND target = GetForegroundWindow();
                if (!target) continue;

                try {
                    auto result = capturer.Capture(target);
                    auto path = GetTimestampPath();
                    encoder.Save(path, result);
                    std::wcout << L"[+] Saved to: " << path << std::endl;
                }
                catch (const std::exception& e) {
                    std::cerr << "Capture failed: " << e.what() << std::endl;
                }
            }
            DispatchMessage(&msg);
        }

        UnregisterHotKey(nullptr, HOTKEY_ID);
    }
    catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
