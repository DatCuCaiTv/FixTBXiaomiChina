// adb.h - tim adb, chay lenh adb, liet ke thiet bi
#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace adb {

struct Device {
    std::string serial;
    std::string state;      // device | unauthorized | offline
    std::string model;
    std::string miui;       // ro.mi.os.version.incremental
    std::string android;    // ro.build.version.release
    std::string codename;   // ro.product.name (vd: xuanyuan)
    std::string region;     // ro.build.version.incremental
    bool ready() const { return state == "device"; }
    std::string label() const {
        return (model.empty() ? std::string("Thiet bi Android") : model) + "   (" + serial + ")";
    }
};

// Thu muc chua file exe
std::string exeDir();

// Tim adb: 1) canh exe  2) adb nhung san -> giai nen ra %LOCALAPPDATA%\XiaomiNotifTool  3) noi quen thuoc
bool locate(std::string& outPath, std::string& outSrc);

// Chay adb voi tham so, tra ve stdout+stderr (da gop). timeoutMs = 0 -> mac dinh 60s
std::string run(const std::string& args, DWORD timeoutMs = 60000, int* exitCode = nullptr);

// Chay adb gan -s <serial>
void setSerial(const std::string& serial);
std::string serial();

// Danh sach thiet bi
std::vector<Device> devices();

// Doc thuoc tinh cua 1 may
Device describe(const std::string& serial);

// Duong dan adb dang dung
std::string path();

// Dat duong dan adb thu cong (nguoi dung chon)
void setPath(const std::string& p);

} // namespace adb
