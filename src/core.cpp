// core.cpp - nghiep vu fix / kiem tra / hoan tac / chan doan (chuoi lenh ma hoa XSTR)
#include "core.h"
#include "adb.h"
#include "obf.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace core {

static const char* OPS[5] = { NULL, NULL, NULL, NULL, NULL };
static bool g_init = false;

static void initStrings() {
    if (g_init) return;
    OPS[0] = XSTR("RUN_IN_BACKGROUND");
    OPS[1] = XSTR("RUN_ANY_IN_BACKGROUND");
    OPS[2] = XSTR("WAKE_LOCK");
    OPS[3] = XSTR("START_FOREGROUND");
    OPS[4] = XSTR("SCHEDULE_EXACT_ALARM");
    g_init = true;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\r' || s[b-1] == '\n')) --b;
    return s.substr(a, b - a);
}
std::string lower(const std::string& s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); ++i) r[i] = (char)tolower((unsigned char)r[i]);
    return r;
}
bool contains(const std::string& hay, const std::string& needle) {
    return lower(hay).find(lower(needle)) != std::string::npos;
}

static std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> v;
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t e = s.find('\n', pos);
        std::string l = (e == std::string::npos) ? s.substr(pos) : s.substr(pos, e - pos);
        v.push_back(trim(l));
        if (e == std::string::npos) break;
        pos = e + 1;
    }
    return v;
}

std::string pickOp(const std::string& dump, const std::string& opName) {
    std::vector<std::string> ls = splitLines(dump);
    std::string found;
    for (size_t i = 0; i < ls.size(); ++i) {
        const std::string& l = ls[i];
        if (l.size() > opName.size() + 1 && l.compare(0, opName.size(), opName) == 0 && l[opName.size()] == ':') {
            std::string v = trim(l.substr(opName.size() + 1));
            size_t sc = v.find(';');
            if (sc != std::string::npos) v = trim(v.substr(0, sc));
            if (!v.empty()) found = v;
        }
    }
    return found.empty() ? std::string(XSTR("default")) : found;
}

std::string pickMiui(const std::string& dump, const std::string& code) {
    std::string tag = std::string(XSTR("MIUIOP(")) + code + XSTR("):");
    std::vector<std::string> ls = splitLines(dump);
    std::string found;
    for (size_t i = 0; i < ls.size(); ++i) {
        const std::string& l = ls[i];
        if (l.size() >= tag.size() && l.compare(0, tag.size(), tag) == 0) {
            std::string v = trim(l.substr(tag.size()));
            size_t sc = v.find(';');
            if (sc != std::string::npos) v = trim(v.substr(0, sc));
            if (!v.empty()) found = v;
        }
    }
    return found.empty() ? std::string(XSTR("default")) : found;
}

std::string dozeDump() {
    return adb::run(std::string(XSTR("shell dumpsys deviceidle whitelist")));
}
std::string netpolDump() {
    return adb::run(std::string(XSTR("shell cmd netpolicy list restrict-background-whitelist")));
}

bool uidOf(const std::string& pkg, std::string& uid) {
    std::string outp = adb::run(std::string(XSTR("shell cmd package list packages -U ")) + pkg);
    size_t k = outp.find(XSTR("uid:"));
    if (k == std::string::npos) return false;
    uid = trim(outp.substr(k + 4));
    return !uid.empty();
}

// ---------------- quet app ----------------
ScanResult scanPackages(bool includeSystem) {
    initStrings();
    ScanResult r;
    r.ok = false;

    const char* tries[6];
    tries[0] = XSTR("shell pm list packages -3");
    tries[1] = XSTR("shell pm list packages -3 --user 0");
    tries[2] = XSTR("shell cmd package list packages -3");
    tries[3] = XSTR("shell pm list packages -3 --user 10");
    tries[4] = XSTR("shell pm list packages -3 --user 999");
    tries[5] = XSTR("shell pm list packages");
    const char* names[6];
    names[0] = "app ben thu 3";
    names[1] = "app ben thu 3 (user 0)";
    names[2] = "app ben thu 3 (cmd package)";
    names[3] = "app ben thu 3 (user 10)";
    names[4] = "app ben thu 3 (user 999)";
    names[5] = "TAT CA goi";

    int first = includeSystem ? 5 : 0;
    int count = includeSystem ? 1 : 6;

    for (int i = first; i < first + count && i < 6; ++i) {
        std::string outp = adb::run(tries[i], 90000);
        std::vector<std::string> ls = splitLines(outp);
        std::vector<std::string> pkgs;
        for (size_t k = 0; k < ls.size(); ++k) {
            if (ls[k].compare(0, 8, XSTR("package:")) != 0) continue;
            std::string p = trim(ls[k].substr(8));
            size_t sp = p.find(' ');
            if (sp != std::string::npos) p = p.substr(0, sp);
            if (!p.empty()) pkgs.push_back(p);
        }
        if (!pkgs.empty()) {
            std::sort(pkgs.begin(), pkgs.end());
            r.how = names[i];

            std::string um = adb::run(XSTR("shell cmd package list packages -U"), 90000);
            std::vector<std::string> uls = splitLines(um);
            std::vector<std::pair<std::string, std::string> > map;
            for (size_t k = 0; k < uls.size(); ++k) {
                const std::string& l = uls[k];
                if (l.compare(0, 8, XSTR("package:")) != 0) continue;
                size_t u = l.find(XSTR(" uid:"));
                if (u == std::string::npos) continue;
                map.push_back(std::make_pair(trim(l.substr(8, u - 8)), trim(l.substr(u + 5))));
            }
            for (size_t k = 0; k < pkgs.size(); ++k) {
                std::string uid;
                for (size_t j = 0; j < map.size(); ++j)
                    if (map[j].first == pkgs[k]) { uid = map[j].second; break; }
                r.apps.push_back(std::make_pair(pkgs[k], uid));
            }
            r.ok = true;
            return r;
        }
        r.log += std::string("[.] Thu cach liet ke \"") + names[i] + "\" khong co ket qua\n";
    }
    return r;
}

// ---------------- doc trang thai ----------------
AppState readState(const std::string& pkg, const std::string& uid,
                   const std::string& doze, const std::string& netpol) {
    initStrings();
    AppState s;
    s.pkg = pkg;
    s.uid = uid;
    std::string dump = adb::run(std::string(XSTR("shell cmd appops get ")) + pkg);
    s.m53  = pickMiui(dump, XSTR("10053"));
    s.m08  = pickMiui(dump, XSTR("10008"));
    s.post = pickOp(dump, XSTR("POST_NOTIFICATION"));
    std::string o;
    for (int i = 0; i < 5; ++i) {
        if (!o.empty()) o += ";";
        o += std::string(OPS[i]) + "=" + pickOp(dump, OPS[i]);
    }
    s.ops = o;
    s.bucket = trim(adb::run(std::string(XSTR("shell am get-standby-bucket ")) + pkg));
    s.inDoze = doze.find(std::string(",") + pkg + ",") != std::string::npos;
    s.inNetpol = (!uid.empty() && netpol.find(std::string(" ") + uid + " ") != std::string::npos);
    return s;
}

// ---------------- fix ----------------
std::string fixApp(const std::string& pkg, const std::string& uid, bool doNetpol) {
    initStrings();
    std::string out;
    adb::run(std::string(XSTR("shell cmd appops set ")) + pkg + XSTR(" 10053 allow"));
    adb::run(std::string(XSTR("shell cmd appops set ")) + pkg + XSTR(" 10008 allow"));
    adb::run(std::string(XSTR("shell dumpsys deviceidle whitelist +")) + pkg);
    adb::run(std::string(XSTR("shell am set-standby-bucket ")) + pkg + XSTR(" active"));
    adb::run(std::string(XSTR("shell cmd appops set ")) + pkg + XSTR(" POST_NOTIFICATION allow"));
    adb::run(std::string(XSTR("shell cmd appops set --uid ")) + pkg + XSTR(" POST_NOTIFICATION allow"));

    int ok = 0;
    for (int i = 0; i < 5; ++i) {
        std::string rr = adb::run(std::string(XSTR("shell cmd appops set ")) + pkg + " " + OPS[i] + XSTR(" allow"));
        if (!contains(rr, XSTR("Error")) && !contains(rr, XSTR("Unknown"))) ++ok;
    }

    bool np = false;
    if (doNetpol && !uid.empty()) {
        adb::run(std::string(XSTR("shell cmd netpolicy add restrict-background-whitelist ")) + uid);
        np = true;
    }

    std::string dump = adb::run(std::string(XSTR("shell cmd appops get ")) + pkg);
    bool autoOn = (pickMiui(dump, XSTR("10053")) == XSTR("allow")) || (pickMiui(dump, XSTR("10008")) == XSTR("allow"));
    bool notif  = (pickOp(dump, XSTR("POST_NOTIFICATION")) == XSTR("allow"));
    std::string dz = dozeDump();
    bool pin = dz.find(std::string(",") + pkg + ",") != std::string::npos;

    out += autoOn ? XSTR("[x] tu khoi dong  ") : XSTR("[ ] tu khoi dong  ");
    out += pin ? XSTR("[x] pin khong han che  ") : XSTR("[ ] pin khong han che  ");
    out += notif ? XSTR("[x] thong bao  ") : XSTR("[ ] thong bao  ");
    char b[64];
    wsprintfA(b, "[x] %d/5 quyen chay nen", ok);
    out += b;
    if (np) out += XSTR("  [x] khong chan du lieu nen (theo app nay)");
    return out;
}

std::string applyGlobals(bool standbyOff, bool freezerOff, bool mobileData) {
    std::string out;
    if (standbyOff) {
        adb::run(std::string(XSTR("shell settings put global ")) + XSTR("app_standby_enabled") + XSTR(" 0"));
        out += XSTR("  - App Standby: TAT\n");
    }
    if (freezerOff) {
        adb::run(std::string(XSTR("shell settings put global ")) + XSTR("cached_apps_freezer") + XSTR(" disabled"));
        out += XSTR("  - Dong bang app nen: TAT\n");
    }
    if (mobileData) {
        adb::run(std::string(XSTR("shell settings put global ")) + XSTR("mobile_data_always_on") + XSTR(" 1"));
        adb::run(std::string(XSTR("shell settings put global ")) + XSTR("wifi_sleep_policy") + XSTR(" 2"));
        out += XSTR("  - Giu 4G luon hoat dong + Wi-Fi khong ngu\n");
    }
    return out;
}

std::string refreshSettings() {
    return adb::run(XSTR("shell am force-stop com.miui.securitycenter"));
}

// ---------------- snapshot ----------------
std::string snapshotPath() {
    char appdata[MAX_PATH] = {0};
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) return "trang-thai-goc.txt";
    std::string dir = std::string(appdata) + XSTR("\\XiaomiNotifRevert");   // dung CHUNG voi tool C#
    CreateDirectoryA(dir.c_str(), NULL);
    return dir + XSTR("\\trang-thai-goc.txt");
}

static std::string getGlobal(const char* key) {
    return trim(adb::run(std::string(XSTR("shell settings get global ")) + key));
}

bool saveSnapshot(const std::vector<AppState>& apps, const std::string& model,
                  const std::string& serial, std::string& err) {
    initStrings();
    std::ofstream f(snapshotPath().c_str(), std::ios::binary);
    if (!f) { err = XSTR("khong mo duoc file"); return false; }
    SYSTEMTIME st; GetLocalTime(&st);
    char t[64];
    wsprintfA(t, "%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    f << XSTR("# XNF-SNAPSHOT 2\n");
    f << XSTR("# date=") << t << XSTR("\n");
    f << XSTR("# model=") << model << XSTR("\n");
    f << XSTR("# serial=") << serial << XSTR("\n");
    f << XSTR("# so_app=") << apps.size() << XSTR("\n");
    f << XSTR("# g_app_standby_enabled=") << getGlobal(XSTR("app_standby_enabled")) << XSTR("\n");
    f << XSTR("# g_cached_apps_freezer=") << getGlobal(XSTR("cached_apps_freezer")) << XSTR("\n");
    f << XSTR("# g_mobile_data_always_on=") << getGlobal(XSTR("mobile_data_always_on")) << XSTR("\n");
    f << XSTR("# netpol=") << trim(netpolDump()) << XSTR("\n");
    for (size_t i = 0; i < apps.size(); ++i) {
        const AppState& a = apps[i];
        f << a.pkg << "|" << a.m53 << "|" << a.m08 << "|" << a.post << "|"
          << (a.inDoze ? "1" : "0") << "|" << a.bucket << "|" << a.ops << "\n";
    }
    f.close();
    return true;
}

Snapshot loadSnapshot() {
    Snapshot s;
    s.loaded = false;
    std::ifstream f(snapshotPath().c_str(), std::ios::binary);
    if (!f) { s.info = "chua co ban sao luu"; return s; }
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (line[0] == '#') {
            if (line.compare(0, 7, "# date=") == 0) s.info = "sao luu luc " + line.substr(7);
            else if (line.compare(0, 8, "# model=") == 0) s.info += "  -  " + line.substr(8);
            else if (line.compare(0, 9, "# netpol=") == 0) s.netpol = line.substr(9);
            continue;
        }
        std::vector<std::string> p;
        size_t pos = 0;
        while (pos <= line.size()) {
            size_t e = line.find('|', pos);
            p.push_back((e == std::string::npos) ? line.substr(pos) : line.substr(pos, e - pos));
            if (e == std::string::npos) break;
            pos = e + 1;
        }
        if (p.size() < 6) continue;
        SnapEntry e2;
        e2.pkg = p[0]; e2.m53 = p[1]; e2.m08 = p[2]; e2.post = p[3];
        e2.wl = p[4]; e2.bucket = p[5];
        e2.ops = (p.size() >= 7) ? p[6] : "";
        s.items.push_back(e2);
    }
    s.loaded = !s.items.empty();
    if (s.info.empty()) s.info = "ban sao luu";
    return s;
}

static std::string opWant(const SnapEntry& e, const std::string& op) {
    if (e.ops.empty()) return XSTR("default");
    std::string key = op + "=";
    size_t pos = 0;
    while (pos < e.ops.size()) {
        size_t e2 = e.ops.find(';', pos);
        std::string kv = (e2 == std::string::npos) ? e.ops.substr(pos) : e.ops.substr(pos, e2 - pos);
        if (kv.compare(0, key.size(), key) == 0) return kv.substr(key.size());
        if (e2 == std::string::npos) break;
        pos = e2 + 1;
    }
    return XSTR("default");
}

std::string restoreApp(const AppState& now, const Snapshot& snap) {
    initStrings();
    const SnapEntry* e = NULL;
    for (size_t i = 0; i < snap.items.size(); ++i)
        if (snap.items[i].pkg == now.pkg) { e = &snap.items[i]; break; }

    std::string m53  = e ? e->m53  : XSTR("default");
    std::string m08  = e ? e->m08  : XSTR("default");
    std::string post = e ? e->post : XSTR("default");
    bool wl = e && e->wl == "1";

    adb::run(std::string(XSTR("shell cmd appops set ")) + now.pkg + XSTR(" 10053 ") + m53);
    adb::run(std::string(XSTR("shell cmd appops set ")) + now.pkg + XSTR(" 10008 ") + m08);
    adb::run(std::string(XSTR("shell cmd appops set ")) + now.pkg + XSTR(" POST_NOTIFICATION ") + post);
    if (wl) adb::run(std::string(XSTR("shell dumpsys deviceidle whitelist +")) + now.pkg);
    else    adb::run(std::string(XSTR("shell dumpsys deviceidle whitelist -")) + now.pkg);

    if (e && !e->bucket.empty() && e->bucket != "-1" && e->bucket != "0")
        adb::run(std::string(XSTR("shell am set-standby-bucket ")) + now.pkg + " " + e->bucket);
    else {
        std::string rr = adb::run(std::string(XSTR("shell am set-standby-bucket ")) + now.pkg + XSTR(" default"));
        if (contains(rr, XSTR("Error")) || contains(rr, XSTR("Unknown")))
            adb::run(std::string(XSTR("shell am set-standby-bucket ")) + now.pkg + XSTR(" rare"));
    }

    for (int i = 0; i < 5; ++i) {
        std::string want = e ? opWant(*e, OPS[i]) : std::string(XSTR("default"));
        adb::run(std::string(XSTR("shell cmd appops set ")) + now.pkg + " " + OPS[i] + " " + want);
    }

    if (!now.uid.empty()) {
        bool wasIn = !snap.netpol.empty() &&
                     snap.netpol.find(std::string(" ") + now.uid + " ") != std::string::npos;
        if (wasIn) adb::run(std::string(XSTR("shell cmd netpolicy add restrict-background-whitelist ")) + now.uid);
        else       adb::run(std::string(XSTR("shell cmd netpolicy remove restrict-background-whitelist ")) + now.uid);
    }

    std::string dump = adb::run(std::string(XSTR("shell cmd appops get ")) + now.pkg);
    std::string c53 = pickMiui(dump, XSTR("10053"));
    std::string cpost = pickOp(dump, XSTR("POST_NOTIFICATION"));
    std::string dz = dozeDump();
    bool pin = dz.find(std::string(",") + now.pkg + ",") != std::string::npos;

    std::string out = e ? XSTR("[theo ban sao luu] ") : XSTR("[ve mac dinh] ");
    out += std::string(XSTR("tu khoi dong=")) + c53;
    out += std::string(XSTR("   pin mien tru=")) + (pin ? "co" : "khong");
    out += std::string(XSTR("   thong bao=")) + cpost;
    return out;
}


// ---------------- doze whitelist (mien tru pin) ----------------
bool inDozeList(const std::string& pkg) {
    std::string dz = dozeDump();
    return dz.find(std::string(",") + pkg + ",") != std::string::npos;
}

std::string addDoze(const std::vector<std::pair<std::string, std::string> >& apps,
                    void (*progress)(int, int)) {
    initStrings();
    int ok = 0;
    for (size_t i = 0; i < apps.size(); ++i) {
        adb::run(std::string(XSTR("shell dumpsys deviceidle whitelist +")) + apps[i].first);
        if (progress) progress((int)i + 1, (int)apps.size());
        ++ok;
    }
    adb::run(XSTR("shell am force-stop com.miui.securitycenter"));
    char b[160];
    wsprintfA(b, XSTR("Da them %d app vao danh sach mien tru pin (Doze whitelist)."), ok);
    return b;
}

std::string removeDoze(const std::vector<std::pair<std::string, std::string> >& apps,
                       void (*progress)(int, int)) {
    initStrings();
    int ok = 0;
    for (size_t i = 0; i < apps.size(); ++i) {
        adb::run(std::string(XSTR("shell dumpsys deviceidle whitelist -")) + apps[i].first);
        if (progress) progress((int)i + 1, (int)apps.size());
        ++ok;
    }
    adb::run(XSTR("shell am force-stop com.miui.securitycenter"));
    char b[160];
    wsprintfA(b, XSTR("Da bo %d app khoi danh sach mien tru pin."), ok);
    return b;
}

// ---------------- nhan trang thai ngan gon (cho cot Trang thai) ----------------
std::string stateTag(const AppState& s) {
    initStrings();
    bool autoOn = (s.m53 == XSTR("allow")) || (s.m08 == XSTR("allow"));
    bool post   = (s.post == XSTR("allow"));
    bool buck   = (s.bucket == "5" || s.bucket == "10");
    int opOk = 0;
    for (int i = 0; i < 5; ++i) {
        std::string key = std::string(OPS[i]) + "=";
        size_t k = s.ops.find(key);
        bool on = false;
        if (k != std::string::npos) {
            std::string v = s.ops.substr(k + key.size());
            size_t sc = v.find(';');
            if (sc != std::string::npos) v = v.substr(0, sc);
            on = (v == XSTR("allow"));
        }
        if (on) ++opOk;
    }
    if (autoOn && post && buck && s.inDoze && opOk == 5) return XSTR("[DA BAT HET]");

    std::string miss;
    if (!autoOn) miss += XSTR("tu khoi dong, ");
    if (!s.inDoze) miss += XSTR("mien tru pin, ");
    if (!buck) miss += XSTR("nhom pin, ");
    if (opOk < 5) { char b[32]; wsprintfA(b, "chay nen %d/5, ", opOk); miss += b; }
    if (!post) miss += XSTR("thong bao, ");
    if (miss.empty()) return XSTR("[da bat het]");
    miss = miss.substr(0, miss.size() - 2);
    if (!autoOn && !s.inDoze && opOk == 0) return std::string(XSTR("[nguyen goc] ")) + miss;
    return std::string(XSTR("[thieu: ")) + miss + XSTR("]");
}

} // namespace core
