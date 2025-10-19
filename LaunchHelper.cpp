#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>

#pragma comment(linker,\
    "\"/manifestdependency:type='win32' "\
    "name='Microsoft.Windows.Common-Controls' "\
    "version='6.0.0.0' "\
    "processorArchitecture='*' "\
    "publicKeyToken='6595b64144ccf1df' "\
    "language='*'\"")

#ifdef _WIN64
#ifdef _M_ARM64
#define CURRENT_ARCH L"a64"
#else
#define CURRENT_ARCH L"x64"
#endif
#else
#define CURRENT_ARCH L"x86"
#endif

std::wstring Trim(const std::wstring& s) {
    auto b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return L"";
    auto e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<std::pair<std::wstring, std::wstring>> LoadTasks(const std::wstring& ini) {
    std::vector<std::pair<std::wstring, std::wstring>> v;
    std::wifstream f(ini);
    std::wstring line;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == L'#') continue;
        auto pos = line.find(L'=');
        if (pos == std::wstring::npos) continue;
        std::wstring k = Trim(line.substr(0, pos));
        std::wstring val = Trim(line.substr(pos + 1));
        v.emplace_back(k, val);
    }
    return v;
}
// 等窗口出现，返回顶层窗口句柄
static HWND WaitForTopWindow(DWORD pid, DWORD timeoutMs = 3000) {
    DWORD tick = GetTickCount();
    do {
        HWND h = nullptr;
        while ((h = FindWindowExW(nullptr, h, nullptr, nullptr))) {
            DWORD wpid = 0;
            GetWindowThreadProcessId(h, &wpid);
            if (wpid == pid && IsWindowVisible(h)) return h;
        }
        Sleep(100);
    } while (GetTickCount() - tick < timeoutMs);
    return nullptr;
}

void RunProcess(const std::wstring& exe, bool wait) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    std::wstring cmd = L"\"" + exe + L"\"";

    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        std::wstring err = L"无法启动: " + exe;
        MessageBoxW(nullptr, err.c_str(), L"Host Launcher", MB_ICONERROR);
        return;
    }

    if (wait) {
        // 1. 等顶层窗口出现
        HWND hwnd = WaitForTopWindow(pi.dwProcessId);
        // 2. 等窗口关闭
        if (hwnd) {
            while (IsWindow(hwnd)) Sleep(200);
        }
        // 3. 保险等进程退出
        WaitForSingleObject(pi.hProcess, INFINITE);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    std::wstring ini = []() {
        wchar_t mod[MAX_PATH];
        GetModuleFileNameW(nullptr, mod, MAX_PATH);
        std::wstring s = mod;
        auto pos = s.find_last_of(L"\\/");
        return s.substr(0, pos + 1) + L"launch.ini";
        }();

    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wstring msg =
            L"找不到 launch.ini 文件！\n\n\n"
            L"==========launch.ini用法示例==========\n"
            L"setup_all=notepad.exe\n"
            L"setup_x64=calc.exe\n"
            L"start_all=cmd\n"
            L"================================\n"
            L"# start_???  不等exe进程退出\n"
            L"# start_???  会等exe进程退出\n"
            L"# 适用架构:  all/x86/x64/a64\n"
            L"================================\n"
            ;
        MessageBoxW(nullptr, msg.c_str(), L"Host Launcher - 缺少配置", MB_OK | MB_ICONWARNING);
        return 1;
    }

    auto tasks = LoadTasks(ini);

    for (auto& [key, val] : tasks) {
        bool wait = false;
        std::wstring arch;
        // start 系列
        if (key == L"start_all")       arch = L"all";
        else if (key == L"start_x86")  arch = L"x86";
        else if (key == L"start_x64")  arch = L"x64";
        else if (key == L"start_a64")  arch = L"a64";
        else if (key == L"open_all")  arch = L"all";
        else if (key == L"open_x86")  arch = L"x86";
        else if (key == L"open_x64")  arch = L"x64";
        else if (key == L"open_a64")  arch = L"a64";
        // setup 系列
        else if (key == L"setup_all") { arch = L"all"; wait = true; }
        else if (key == L"setup_x86") { arch = L"x86"; wait = true; }
        else if (key == L"setup_x64") { arch = L"x64"; wait = true; }
        else if (key == L"setup_a64") { arch = L"a64"; wait = true; }
        else if (key == L"wait_all") { arch = L"all"; wait = true; }
        else if (key == L"wait_x86") { arch = L"x86"; wait = true; }
        else if (key == L"wait_x64") { arch = L"x64"; wait = true; }
        else if (key == L"wait_a64") { arch = L"a64"; wait = true; }
        else continue;

        if (arch == L"all" || arch == CURRENT_ARCH)
            RunProcess(val, wait);   // start 不等待，setup 等待进程树退出
    }
    return 0;
}