// core.h - loi nghiep vu: fix, doc trang thai, sao luu, hoan tac, chan doan
#pragma once
#include <string>
#include <vector>

namespace core {

struct AppState {
    std::string pkg;
    std::string uid;
    std::string m53;      // appops 10053 (tu khoi dong - HyperOS 3)
    std::string m08;      // appops 10008 (tu khoi dong - MIUI cu)
    std::string post;     // POST_NOTIFICATION
    std::string ops;      // "RUN_IN_BACKGROUND=allow;WAKE_LOCK=default;..."
    std::string bucket;
    bool inDoze;          // co trong danh sach mien tru pin
    bool inNetpol;        // co trong whitelist du lieu nen
    bool isFixed() const {
        return m53 == "allow" || m08 == "allow" || inDoze;
    }
};

struct SnapEntry {
    std::string pkg, m53, m08, post, ops, bucket, wl;
};

// -------- quet --------
struct ScanResult {
    bool ok;
    std::string how;                    // cach liet ke da dung
    std::string log;
    std::vector<std::pair<std::string, std::string> > apps;   // pkg, uid
};

ScanResult scanPackages(bool includeSystem);

// doc trang thai 1 app (1 lenh appops get + 1 bucket)
AppState readState(const std::string& pkg, const std::string& uid,
                   const std::string& dozeDump, const std::string& netpolDump);

// doc nhanh cac bang dung chung
std::string dozeDump();
std::string netpolDump();
bool uidOf(const std::string& pkg, std::string& uid);

// -------- fix --------
std::string fixApp(const std::string& pkg, const std::string& uid, bool doNetpol);
std::string applyGlobals(bool standbyOff, bool freezerOff, bool mobileData);
std::string refreshSettings();

// -------- snapshot --------
std::string snapshotPath();
struct Snapshot {
    std::string info;
    std::string netpol;                 // danh sach uid luc chup
    std::vector<SnapEntry> items;
    bool loaded;
};
bool saveSnapshot(const std::vector<AppState>& apps, const std::string& model,
                  const std::string& serial, std::string& err);
Snapshot loadSnapshot();

// -------- hoan tac --------
std::string restoreApp(const AppState& now, const Snapshot& snap);

// -------- doze whitelist (mien tru pin) --------
std::string addDoze(const std::vector<std::pair<std::string, std::string> >& apps,
                    void (*progress)(int, int) = NULL);
std::string removeDoze(const std::vector<std::pair<std::string, std::string> >& apps,
                       void (*progress)(int, int) = NULL);
bool inDozeList(const std::string& pkg);

// -------- nhan trang thai ngan gon --------
std::string stateTag(const AppState& s);

// tien ich
std::string lower(const std::string& s);
bool contains(const std::string& hay, const std::string& needle);
std::string trim(const std::string& s);
std::string pickOp(const std::string& dump, const std::string& opName);
std::string pickMiui(const std::string& dump, const std::string& code);

} // namespace core
