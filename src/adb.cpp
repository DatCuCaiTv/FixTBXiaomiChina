// adb.cpp - thuc thi adb (chuoi lenh duoc ma hoa bang XSTR)
#include "adb.h"
#include "obf.h"
#include "res.h"
#include <shlobj.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace adb {

static std::string g_path;
static std::string g_serial;

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\r' || s[b-1] == '\n')) --b;
    return s.substr(a, b - a);
}

static bool fileExists(const std::string& p) {
    DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::string exeDir() {
    char buf[MAX_PATH * 2] = {0};
    GetModuleFileNameA(NULL, buf, (DWORD)sizeof(buf) - 1);
    std::string s(buf);
    size_t k = s.find_last_of("\\/");
    return (k == std::string::npos) ? std::string(".") : s.substr(0, k);
}

std::string path() { return g_path; }
void setPath(const std::string& p) { g_path = p; }
void setSerial(const std::string& s) { g_serial = s; }
std::string serial() { return g_serial; }

static bool extractEmbedded(std::string& out) {
    char appdata[MAX_PATH] = {0};
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata))) return false;
    std::string dir = std::string(appdata) + XSTR("\\XiaomiNotifTool");
    CreateDirectoryA(dir.c_str(), NULL);

    struct Item { int id; const char* name; };
    Item items[3];
    items[0].id = IDR_ADB;          items[0].name = XSTR("adb.exe");
    items[1].id = IDR_ADBWINAPI;    items[1].name = XSTR("AdbWinApi.dll");
    items[2].id = IDR_ADBWINUSBAPI; items[2].name = XSTR("AdbWinUsbApi.dll");

    bool gotAdb = false;
    for (int i = 0; i < 3; ++i) {
        std::string target = dir + XSTR("\\") + items[i].name;
        HRSRC hres = FindResourceA(NULL, MAKEINTRESOURCEA(items[i].id), RT_RCDATA);
        if (!hres) continue;
        DWORD size = SizeofResource(NULL, hres);
        HGLOBAL hg = LoadResource(NULL, hres);
        void* data = hg ? LockResource(hg) : NULL;
        if (!data || size == 0) continue;

        bool need = true;
        if (fileExists(target)) {
            HANDLE hf = CreateFileA(target.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hf != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER sz; sz.QuadPart = 0;
                GetFileSizeEx(hf, &sz);
                CloseHandle(hf);
                if ((DWORD)sz.QuadPart == size) need = false;
            }
        }
        if (need) {
            HANDLE hf = CreateFileA(target.c_str(), GENERIC_WRITE, 0, NULL,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hf == INVALID_HANDLE_VALUE) continue;
            DWORD wrote = 0;
            WriteFile(hf, data, size, &wrote, NULL);
            CloseHandle(hf);
        }
        if (i == 0) { out = target; gotAdb = true; }
    }
    return gotAdb;
}

bool locate(std::string& outPath, std::string& outSrc) {
    if (!g_path.empty() && fileExists(g_path)) { outPath = g_path; outSrc = XSTR("adb dang dung"); return true; }

    std::string dir = exeDir();
    const char* subs[2];
    subs[0] = XSTR("\\platform-tools\\adb.exe");
    subs[1] = XSTR("\\adb.exe");
    for (int i = 0; i < 2; ++i) {
        std::string p = dir + subs[i];
        if (fileExists(p)) { g_path = p; outPath = p; outSrc = XSTR("adb trong thu muc tool"); return true; }
    }

    std::string ex;
    if (extractEmbedded(ex)) { g_path = ex; outPath = ex; outSrc = XSTR("adb di kem trong tool"); return true; }

    char up[MAX_PATH] = {0}, lad[MAX_PATH] = {0};
    SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, up);
    SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, lad);
    std::string cands[7];
    cands[0] = std::string(up) + XSTR("\\Downloads\\platform-tools\\adb.exe");
    cands[1] = std::string(up) + XSTR("\\platform-tools\\adb.exe");
    cands[2] = std::string(up) + XSTR("\\Desktop\\platform-tools\\adb.exe");
    cands[3] = std::string(lad) + XSTR("\\Android\\Sdk\\platform-tools\\adb.exe");
    cands[4] = XSTR("C:\\platform-tools\\adb.exe");
    cands[5] = XSTR("C:\\Program Files (x86)\\Android\\android-sdk\\platform-tools\\adb.exe");
    cands[6] = XSTR("C:\\Program Files\\Android\\android-sdk\\platform-tools\\adb.exe");
    for (int i = 0; i < 7; ++i) {
        if (fileExists(cands[i])) { g_path = cands[i]; outPath = cands[i]; outSrc = cands[i]; return true; }
    }
    return false;
}

std::string run(const std::string& args, DWORD timeoutMs, int* exitCode) {
    if (g_path.empty()) { std::string p, s; if (!locate(p, s)) return ""; }
    if (exitCode) *exitCode = -1;

    std::string cmd = std::string(XSTR("\"")) + g_path + XSTR("\"");
    if (!g_serial.empty()) cmd += std::string(XSTR(" -s ")) + g_serial;
    if (!args.empty()) cmd += std::string(XSTR(" ")) + args;

    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 1 << 20)) return "";
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = NULL;

    PROCESS_INFORMATION pi = {0};
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    BOOL ok = CreateProcessA(NULL, buf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); return ""; }

    std::string out;
    DWORD deadline = GetTickCount() + (timeoutMs ? timeoutMs : 60000);
    char tmp[8192];
    for (;;) {
        DWORD avail = 0;
        if (PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            DWORD got = 0;
            DWORD want = avail > sizeof(tmp) ? (DWORD)sizeof(tmp) : avail;
            if (ReadFile(rd, tmp, want, &got, NULL) && got > 0) { out.append(tmp, got); continue; }
        }
        DWORD w = WaitForSingleObject(pi.hProcess, 25);
        if (w == WAIT_OBJECT_0) {
            for (;;) {
                DWORD got = 0;
                if (!ReadFile(rd, tmp, sizeof(tmp), &got, NULL) || got == 0) break;
                out.append(tmp, got);
            }
            break;
        }
        if (GetTickCount() > deadline) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 2000);
            break;
        }
    }
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    if (exitCode) *exitCode = (int)code;
    CloseHandle(rd);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return out;
}

std::vector<Device> devices() {
    std::vector<Device> list;
    std::string saved = g_serial;
    g_serial.clear();
    run(XSTR("start-server"), 20000);
    std::string d = run(XSTR("devices"), 20000);
    g_serial = saved;

    size_t pos = 0;
    while (pos < d.size()) {
        size_t e = d.find('\n', pos);
        std::string line = d.substr(pos, (e == std::string::npos ? d.size() : e) - pos);
        pos = (e == std::string::npos) ? d.size() : e + 1;
        while (!line.empty() && (line[line.size()-1] == '\r' || line[line.size()-1] == ' ')) line.erase(line.size()-1);
        if (line.empty() || line[0] == '*') continue;
        if (line.compare(0, 14, XSTR("List of device")) == 0) continue;
        size_t t = line.find('\t');
        if (t == std::string::npos) t = line.find(' ');
        if (t == std::string::npos) continue;
        Device dev;
        dev.serial = line.substr(0, t);
        dev.state = line.substr(t + 1);
        while (!dev.state.empty() && (dev.state[0] == ' ' || dev.state[0] == '\t')) dev.state.erase(0, 1);
        list.push_back(dev);
    }
    return list;
}

Device describe(const std::string& s) {
    Device d;
    d.serial = s;
    d.state = XSTR("device");
    std::string saved = g_serial;
    g_serial = s;
    std::string v = trim(run(XSTR("shell getprop ro.product.marketname")));
    if (v.empty()) v = trim(run(XSTR("shell getprop ro.product.model")));
    d.model = v;
    d.miui     = trim(run(XSTR("shell getprop ro.mi.os.version.incremental")));
    d.android  = trim(run(XSTR("shell getprop ro.build.version.release")));
    d.codename = trim(run(XSTR("shell getprop ro.product.name")));
    d.region   = trim(run(XSTR("shell getprop ro.build.version.incremental")));
    g_serial = saved;
    return d;
}

} // namespace adb
