// ── FinanceDashboard launcher (Windows) ──────────────────────────────────────
//
// Small Win32 program, deliberately WITHOUT any Qt dependency and linked
// statically, so it can always start — even when the real app can't (a Qt DLL
// missing, Windows too old). It checks the requirements listed in
// app\requirements.txt (written by docker/scripts/deploy.py), shows a window
// listing every requirement as met / not met if something is wrong, and
// otherwise starts app\FinanceDashboard.exe, forwarding the command line.
// If the app then dies with a Windows loader error (DLL / entry point not
// found), the launcher reports that too, since the app itself never got to
// run any code. In-app checks (after Qt is up) live in src/startup/ — see
// docs/startup.md.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr const wchar_t* kTitle = L"Finance Dashboard";
// Qt 6.11 supports Windows 10 1809 (build 17763) or later —
// https://doc.qt.io/qt-6/supported-platforms.html. Overridable from
// requirements.txt ("min-windows-build").
constexpr unsigned long kDefaultMinWindowsBuild = 17763;

// NTSTATUS values a process exits with when the Windows loader can't start it.
constexpr DWORD kStatusDllNotFound        = 0xC0000135;
constexpr DWORD kStatusEntryPointNotFound = 0xC0000139;
constexpr DWORD kStatusInvalidImageFormat = 0xC000007B;
constexpr DWORD kStatusDllInitFailed      = 0xC0000142;

struct Requirements {
    unsigned long minWindowsBuild = kDefaultMinWindowsBuild;
    std::vector<std::wstring> files; // relative to the app directory
    bool manifestFound = false;
};

struct CheckLine {
    bool ok;
    std::wstring text;
};

std::wstring widen(const std::string& s)
{
    if (s.empty()) {
        return {};
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

bool fileExists(const std::wstring& path)
{
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring moduleDir()
{
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n < buf.size()) {
            buf.resize(n);
            break;
        }
        buf.resize(buf.size() * 2);
    }
    const auto slash = buf.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring(L".") : buf.substr(0, slash);
}

// RtlGetVersion reports the real version (GetVersionEx lies to un-manifested apps).
DWORD windowsBuild(DWORD& major, DWORD& minor)
{
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) — GetProcAddress needs a cast by design.
        auto fn = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
        if (fn != nullptr && fn(&info) == 0) {
            major = info.dwMajorVersion;
            minor = info.dwMinorVersion;
            return info.dwBuildNumber;
        }
    }
    major = 0;
    minor = 0;
    return 0;
}

// requirements.txt: one "key value" per line, '#' comments.
//   min-windows-build 17763
//   file Qt6Core.dll
Requirements readRequirements(const std::wstring& appDir)
{
    Requirements req;
    std::ifstream in(std::filesystem::path(appDir + L"\\requirements.txt"));
    if (!in) {
        return req;
    }
    req.manifestFound = true;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto space = line.find(' ');
        if (space == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, space);
        const std::string value = line.substr(space + 1);
        if (key == "min-windows-build") {
            req.minWindowsBuild = std::stoul(value);
        } else if (key == "file") {
            std::wstring path = widen(value);
            for (auto& c : path) {
                if (c == L'/') {
                    c = L'\\';
                }
            }
            req.files.push_back(path);
        }
    }
    return req;
}

std::vector<CheckLine> checkRequirements(const std::wstring& appDir, const std::wstring& appExe)
{
    std::vector<CheckLine> lines;
    const Requirements req = readRequirements(appDir);

    DWORD major = 0;
    DWORD minor = 0;
    const DWORD build = windowsBuild(major, minor);
    {
        std::wostringstream s;
        s << L"Windows version " << major << L'.' << minor << L" build " << build
          << L" (needs Windows 10 build " << req.minWindowsBuild << L" or later)";
        lines.push_back({build >= req.minWindowsBuild, s.str()});
    }

    lines.push_back({fileExists(appExe), L"Application: " + appExe});

    if (!req.manifestFound) {
        lines.push_back({true, L"Application files: no requirements.txt next to the app "
                               L"(local build) - not checked"});
    } else {
        std::vector<std::wstring> missing;
        for (const auto& f : req.files) {
            if (!fileExists(appDir + L"\\" + f)) {
                missing.push_back(f);
            }
        }
        std::wostringstream s;
        if (missing.empty()) {
            s << L"Application files: all " << req.files.size() << L" present";
        } else {
            s << L"Application files: " << missing.size() << L" of " << req.files.size() << L" missing:";
            const size_t shown = missing.size() < 15 ? missing.size() : 15;
            for (size_t i = 0; i < shown; ++i) {
                s << L"\n        " << missing[i];
            }
            if (missing.size() > shown) {
                s << L"\n        ... and " << (missing.size() - shown) << L" more";
            }
        }
        lines.push_back({missing.empty(), s.str()});
    }
    return lines;
}

bool allOk(const std::vector<CheckLine>& lines)
{
    for (const auto& l : lines) {
        if (!l.ok) {
            return false;
        }
    }
    return true;
}

void showReport(const std::wstring& headline, const std::vector<CheckLine>& lines, const std::wstring& footer)
{
    std::wostringstream s;
    s << headline << L"\n\nRequirements:\n";
    for (const auto& l : lines) {
        s << (l.ok ? L"  [ OK ]  " : L"  [FAIL]  ") << l.text << L'\n';
    }
    if (!footer.empty()) {
        s << L'\n' << footer;
    }
    MessageBoxW(nullptr, s.str().c_str(), kTitle, MB_OK | MB_ICONERROR);
}

// Everything after argv[0] in the raw command line, unchanged.
std::wstring forwardedArguments()
{
    const std::wstring cmd = GetCommandLineW();
    size_t i = 0;
    if (!cmd.empty() && cmd[0] == L'"') {
        i = cmd.find(L'"', 1);
        i = (i == std::wstring::npos) ? cmd.size() : i + 1;
    } else {
        while (i < cmd.size() && cmd[i] != L' ' && cmd[i] != L'\t') {
            ++i;
        }
    }
    return cmd.substr(i);
}

const wchar_t* loaderErrorName(DWORD code)
{
    switch (code) {
    case kStatusDllNotFound:        return L"a required DLL was not found (0xC0000135)";
    case kStatusEntryPointNotFound: return L"a DLL is the wrong version - entry point not found (0xC0000139)";
    case kStatusInvalidImageFormat: return L"a DLL is invalid or for the wrong architecture (0xC000007B)";
    case kStatusDllInitFailed:      return L"a DLL failed to initialize (0xC0000142)";
    default:                        return nullptr;
    }
}

} // namespace

int WINAPI WinMain(HINSTANCE /*instance*/, HINSTANCE /*prev*/, LPSTR /*cmdLine*/, int /*show*/)
{
    const std::wstring dir = moduleDir();
    // Packaged layout (dist/…): launcher next to an app\ folder. Local build:
    // launcher (FinanceDashboardLauncher.exe) next to FinanceDashboard.exe.
    std::wstring appDir = dir + L"\\app";
    if (!fileExists(appDir + L"\\FinanceDashboard.exe") && fileExists(dir + L"\\FinanceDashboard.exe")) {
        appDir = dir;
    }
    const std::wstring appExe = appDir + L"\\FinanceDashboard.exe";

    const auto lines = checkRequirements(appDir, appExe);
    if (!allOk(lines)) {
        showReport(L"Finance Dashboard can't start on this computer.", lines,
                   L"Rebuild/re-copy the whole folder (tools/docker-build), or see docs/startup.md.");
        return 1;
    }

    // Suppress Windows' own "DLL not found" box in the child: we report it
    // ourselves below, with the requirement list.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    std::wstring cmdLine = L"\"" + appExe + L"\"" + forwardedArguments();
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(appExe.c_str(), cmdLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi) == 0) {
        std::wostringstream s;
        s << L"Windows refused to start the application (error " << GetLastError() << L").";
        showReport(s.str(), lines, L"");
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (const wchar_t* why = loaderErrorName(exitCode)) {
        showReport(std::wstring(L"Finance Dashboard could not start: ") + why + L".",
                   checkRequirements(appDir, appExe),
                   L"If every requirement above is OK, the folder probably mixes files from different "
                   L"builds: delete it and rebuild/re-copy it (tools/docker-build).");
    }
    return static_cast<int>(exitCode);
}
