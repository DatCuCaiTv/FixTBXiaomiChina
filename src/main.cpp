// main.cpp - giao dien Win32: FIX + HOAN TAC, cua so co gian duoc, doc trang thai tung app
#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include "res.h"
#include "obf.h"
#include "adb.h"
#include "core.h"

using namespace Gdiplus;

static HWND g_lblWarn = NULL;
static HWND g_hwnd = NULL, g_map = NULL, g_list = NULL, g_status = NULL, g_device = NULL,
            g_progress = NULL, g_search = NULL, g_btnAction = NULL, g_chkSystem = NULL,
            g_chkStandby = NULL, g_chkFreezer = NULL, g_chkMobile = NULL, g_radioSel = NULL,
            g_radioAll = NULL, g_btnSnap = NULL, g_btnConnect = NULL, g_btnScan = NULL,
            g_btnAdb = NULL, g_btnAll = NULL, g_btnNone = NULL, g_btnChat = NULL, g_btnWallet = NULL,
            g_btnDozeAdd = NULL, g_btnDozeDel = NULL, g_tabFix = NULL, g_tabRevert = NULL,
            g_lblSearch = NULL;

static HFONT g_font = NULL, g_fontB = NULL, g_fontT = NULL, g_fontS = NULL, g_fontD = NULL;
static HBRUSH g_brWhite = NULL, g_brDark = NULL;
static HBITMAP g_bg = NULL;
static int g_dpi = 96;
static int g_mode = 0;                 // 0 = FIX, 1 = HOAN TAC
static bool g_busy = false;

static core::ScanResult g_scan;
static std::vector<std::string> g_shown;      // pkg dang hien
static std::vector<char> g_checked;           // tick theo g_shown
static std::vector<core::AppState> g_states;  // trang thai theo g_shown
static adb::Device g_dev;
static std::vector<std::pair<std::string, std::string> > g_tags;   // pkg -> trang thai

#define S(x) MulDiv((x), g_dpi, 96)

static void setStatus(const char* fmt, ...) {
    char b[1024];
    va_list ap; va_start(ap, fmt);
    wvsprintfA(b, fmt, ap);
    va_end(ap);
    SetWindowTextA(g_status, b);
}

static void listSetSub(int idx, const char* txt) {
    LVITEMA it = {0};
    it.mask = LVIF_TEXT;
    it.iItem = idx;
    it.iSubItem = 1;
    it.pszText = (LPSTR)txt;
    ListView_SetItem(g_list, &it);
}

static void listAdd(int idx, const char* a, const char* b, bool checked) {
    LVITEMA it = {0};
    it.mask = LVIF_TEXT;
    it.iItem = idx;
    it.pszText = (LPSTR)a;
    ListView_InsertItem(g_list, &it);
    listSetSub(idx, b);
    ListView_SetCheckState(g_list, idx, checked ? TRUE : FALSE);
}

static void listClear() {
    ListView_DeleteAllItems(g_list);
    g_shown.clear();
    g_checked.clear();
    g_states.clear();
}

static std::string pkgAt(int idx) {
    if (idx < 0 || idx >= (int)g_shown.size()) return "";
    return g_shown[idx];
}

static bool matchAny(const std::string& p, const char* const* k, int n) {
    std::string s = core::lower(p);
    for (int i = 0; i < n; ++i) if (s.find(k[i]) != std::string::npos) return true;
    return false;
}

static void setBusy(bool b, const char* msg) {
    g_busy = b;
    HWND all[12] = { g_btnConnect, g_btnScan, g_btnAdb, g_btnSnap, g_btnAction,
                     g_btnDozeAdd, g_btnDozeDel, g_btnAll, g_btnNone, g_btnChat, g_btnWallet, g_tabRevert };
    for (int i = 0; i < 12; ++i) if (all[i]) EnableWindow(all[i], !b);
    if (msg) setStatus(msg);
    if (!b) SendMessage(g_progress, PBM_SETPOS, 0, 0);
}

typedef void (*TaskFn)(void*);
struct Task { TaskFn fn; void* arg; };
static DWORD WINAPI taskThread(LPVOID p) {
    Task* t = (Task*)p;
    t->fn(t->arg);
    delete t;
    return 0;
}
static void runTask(TaskFn fn, void* arg) {
    Task* t = new Task(); t->fn = fn; t->arg = arg;
    HANDLE h = CreateThread(NULL, 0, taskThread, t, 0, NULL);
    if (h) CloseHandle(h); else delete t;
}

// ---------------- bo cuc (goi moi khi doi kich thuoc) ----------------
static void layout() {
    if (!g_hwnd) return;
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    int pad = S(12);
    int leftW = W * 56 / 100;
    if (leftW < S(560)) leftW = S(560);
    if (leftW > W - S(260)) leftW = W - S(260);
    int cw = leftW - pad * 2;

    int mapX = leftW;
    SetWindowPos(g_map, NULL, mapX, 0, W - mapX, H, SWP_NOZORDER);

    int y = S(96);
    int hBtn = S(28), hChk = S(20), hEdit = S(22), hRow = S(26);
    int bw = (cw - S(8) * 3) / 4;

    SetWindowPos(g_tabFix,    NULL, pad, y, S(90),  hRow, SWP_NOZORDER);
    SetWindowPos(g_tabRevert, NULL, pad + S(96), y, S(110), hRow, SWP_NOZORDER);
    y += hRow + S(8);

    SetWindowPos(g_device, NULL, pad, y, cw, S(20), SWP_NOZORDER);
    y += S(24);

    SetWindowPos(g_btnConnect, NULL, pad, y, bw, hBtn, SWP_NOZORDER);
    SetWindowPos(g_btnScan,    NULL, pad + (bw + S(8)), y, bw, hBtn, SWP_NOZORDER);
    SetWindowPos(g_btnAdb,     NULL, pad + (bw + S(8)) * 2, y, bw, hBtn, SWP_NOZORDER);
    SetWindowPos(g_btnSnap,    NULL, pad + (bw + S(8)) * 3, y, bw, hBtn, SWP_NOZORDER);
    y += hBtn + S(6);

    int cwid = cw / 4;
    SetWindowPos(g_chkSystem,  NULL, pad, y, cwid, hChk, SWP_NOZORDER);
    SetWindowPos(g_chkStandby, NULL, pad + cwid, y, cwid, hChk, SWP_NOZORDER);
    SetWindowPos(g_chkFreezer, NULL, pad + cwid * 2, y, cwid, hChk, SWP_NOZORDER);
    SetWindowPos(g_chkMobile,  NULL, pad + cwid * 3, y, cwid, hChk, SWP_NOZORDER);
    y += hChk + S(2);
    SetWindowPos(g_lblWarn, NULL, pad, y, cw, S(18), SWP_NOZORDER);
    y += S(18);
    y += S(4);

    SetWindowPos(g_lblSearch, NULL, pad, y + S(3), S(42), hChk, SWP_NOZORDER);
    SetWindowPos(g_search, NULL, pad + S(46), y, cw - S(46), hEdit, SWP_NOZORDER);
    y += hEdit + S(6);

    // duoi: 5 hang co dinh
    int bottomH = hRow * 2 + hChk + S(32) + S(14) + S(20) + S(30);
    int listH = H - y - bottomH - pad;
    if (listH < S(90)) listH = S(90);
    SetWindowPos(g_list, NULL, pad, y, cw, listH, SWP_NOZORDER);
    y += listH + S(6);

    int bw4 = (cw - S(6) * 3) / 4;
    SetWindowPos(g_btnAll,    NULL, pad, y, bw4, hRow, SWP_NOZORDER);
    SetWindowPos(g_btnNone,   NULL, pad + (bw4 + S(6)), y, bw4, hRow, SWP_NOZORDER);
    SetWindowPos(g_btnChat,   NULL, pad + (bw4 + S(6)) * 2, y, bw4, hRow, SWP_NOZORDER);
    SetWindowPos(g_btnWallet, NULL, pad + (bw4 + S(6)) * 3, y, bw4, hRow, SWP_NOZORDER);
    y += hRow + S(6);

    int bwd = (cw - S(6)) / 2;
    SetWindowPos(g_btnDozeAdd, NULL, pad, y, bwd, hRow, SWP_NOZORDER);
    SetWindowPos(g_btnDozeDel, NULL, pad + bwd + S(6), y, bwd, hRow, SWP_NOZORDER);
    y += hRow + S(6);

    SetWindowPos(g_radioSel, NULL, pad, y, cw / 2, hChk, SWP_NOZORDER);
    SetWindowPos(g_radioAll, NULL, pad + cw / 2, y, cw / 2, hChk, SWP_NOZORDER);
    y += hChk + S(6);

    SetWindowPos(g_btnAction, NULL, pad, y, cw, S(32), SWP_NOZORDER);
    y += S(32) + S(4);
    SetWindowPos(g_progress, NULL, pad, y, cw, S(14), SWP_NOZORDER);
    y += S(14) + S(2);
    SetWindowPos(g_status, NULL, pad, y, cw, S(20), SWP_NOZORDER);

    // cot danh sach gian theo be rong
    LVCOLUMNA c = {0};
    c.mask = LVCF_WIDTH;
    c.cx = cw * 45 / 100; ListView_SetColumn(g_list, 0, &c);
    c.cx = cw * 53 / 100; ListView_SetColumn(g_list, 1, &c);
    InvalidateRect(g_hwnd, NULL, TRUE);
}

// ---------------- ve ban do ----------------
static LRESULT CALLBACK MapProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HBITMAP old = (HBITMAP)SelectObject(mem, bm);
        HBRUSH bw = CreateSolidBrush(RGB(255,255,255));
        FillRect(mem, &rc, bw);
        DeleteObject(bw);
        if (g_bg) {
            int sw = 772, sh = 941;
            double sx = (double)rc.right / sw, sy = (double)rc.bottom / sh;
            double s = sx < sy ? sx : sy;
            int dw = (int)(sw * s), dh = (int)(sh * s);
            int dx = (rc.right - dw) / 2, dy = (rc.bottom - dh) / 2;
            HDC src = CreateCompatibleDC(dc);
            HBITMAP ob = (HBITMAP)SelectObject(src, g_bg);
            SetStretchBltMode(mem, HALFTONE);
            SetBrushOrgEx(mem, 0, 0, NULL);
            StretchBlt(mem, dx, dy, dw, dh, src, 900, 0, sw, sh, SRCCOPY);
            SelectObject(src, ob);
            DeleteDC(src);
        }
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bm);
        DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcA(h, m, w, l);
}

// ---------------- luong nen ----------------
static void taskConnect(void*) {
    std::string p, src;
    if (!adb::locate(p, src)) {
        char* m = new char[512];
        lstrcpyA(m, XSTR("Khong tim thay adb.exe. Bam nut ADB... de chon adb.exe tren may."));
        PostMessageA(g_hwnd, WM_APP_DEVICES, 0, (LPARAM)m);
        return;
    }
    std::vector<adb::Device> ds = adb::devices();
    std::vector<adb::Device> ready;
    for (size_t i = 0; i < ds.size(); ++i) if (ds[i].ready()) ready.push_back(ds[i]);

    char* m = new char[1024];
    if (ready.empty()) {
        if (!ds.empty() && ds[0].state == XSTR("unauthorized"))
            lstrcpyA(m, XSTR("May da cam cap nhung CHUA cho phep go loi USB. Nhin vao man hinh dien thoai va bam Cho phep."));
        else
            lstrcpyA(m, XSTR("Khong thay dien thoai nao. Cam cap DU LIEU va bat Go loi USB trong Tuy chon nha phat trien."));
        PostMessageA(g_hwnd, WM_APP_DEVICES, 0, (LPARAM)m);
        return;
    }
    g_dev = adb::describe(ready[0].serial);
    adb::setSerial(g_dev.serial);
    std::string info = g_dev.model + std::string(XSTR("   -   Android ")) + g_dev.android;
    if (!g_dev.miui.empty())     info += std::string(XSTR("   -   ROM ")) + g_dev.miui;
    info += std::string(XSTR("   -   ")) + g_dev.serial;
    strncpy(m, info.c_str(), 1023); m[1023] = 0;
    PostMessageA(g_hwnd, WM_APP_DEVICES, 1, (LPARAM)m);
}

// quet app + doc trang thai tung app
struct StateMsg { std::string pkg; std::string tag; };

static void taskScan(void* arg) {
    core::ScanResult r = core::scanPackages(arg != NULL);
    // GUI danh sach TRUOC, roi moi doc trang thai tung app (neu nguoc se bi ghi de)
    core::ScanResult* out = new core::ScanResult(r);
    PostMessageA(g_hwnd, WM_APP_SCAN, 0, (LPARAM)out);
    if (!r.ok) return;
    std::string dz = core::dozeDump();
    std::string np = core::netpolDump();
    for (size_t i = 0; i < r.apps.size(); ++i) {
        PostMessageA(g_hwnd, WM_APP_PROGRESS, i + 1, (LPARAM)r.apps.size());
        core::AppState st = core::readState(r.apps[i].first, r.apps[i].second, dz, np);
        StateMsg* sm = new StateMsg();
        sm->pkg = r.apps[i].first;
        sm->tag = core::stateTag(st);
        PostMessageA(g_hwnd, WM_APP_STATE, 0, (LPARAM)sm);
    }
    PostMessageA(g_hwnd, WM_APP_PROGRESS, 0, 0);
}

struct WorkArg {
    std::vector<std::pair<std::string, std::string> > apps;
    bool a, b, c;
    int kind;         // 0 fix, 1 revert, 2 doze add, 3 doze del
};

static void taskWork(void* p) {
    WorkArg* a = (WorkArg*)p;
    if (a->kind == 2 || a->kind == 3) {
        std::string msg = (a->kind == 2) ? core::addDoze(a->apps, NULL) : core::removeDoze(a->apps, NULL);
        delete a;
        std::string* rep = new std::string(msg);
        PostMessageA(g_hwnd, WM_APP_DONE, 5, (LPARAM)rep);
        return;
    }
    if (a->kind == 1) {
        core::Snapshot snap = core::loadSnapshot();
        std::string* rep = new std::string();
        char h[220];
        wsprintfA(h, XSTR("Hoan tac %d app  (%s)\n\n"), (int)a->apps.size(),
                  snap.loaded ? XSTR("theo ban sao luu") : XSTR("ve mac dinh"));
        *rep += h;
        std::string dz = core::dozeDump(), np = core::netpolDump();
        for (size_t i = 0; i < a->apps.size(); ++i) {
            PostMessageA(g_hwnd, WM_APP_PROGRESS, i + 1, (LPARAM)a->apps.size());
            core::AppState now = core::readState(a->apps[i].first, a->apps[i].second, dz, np);
            *rep += a->apps[i].first + std::string(XSTR("\n      ")) + core::restoreApp(now, snap) + XSTR("\n");
        }
        core::refreshSettings();
        delete a;
        PostMessageA(g_hwnd, WM_APP_DONE, 3, (LPARAM)rep);
        return;
    }
    // kind 0 = FIX
    std::string* rep = new std::string();
    // chi in phan tuy chon toan cuc khi THUC SU co bat (tranh hieu nham la da doi may)
    std::string gl = core::applyGlobals(a->a, a->b, a->c);
    if (!gl.empty()) {
        *rep += XSTR("TUY CHON TOAN CUC (da bat, ap dung cho MOI app):\n");
        *rep += gl;
        *rep += XSTR("\n");
    } else {
        *rep += XSTR("(Khong bat tuy chon toan cuc nao - chi sua rieng tung app duoc chon)\n\n");
    }
    for (size_t i = 0; i < a->apps.size(); ++i) {
        PostMessageA(g_hwnd, WM_APP_PROGRESS, i + 1, (LPARAM)a->apps.size());
        *rep += a->apps[i].first + std::string(XSTR("\n      "))
              + core::fixApp(a->apps[i].first, a->apps[i].second, true) + XSTR("\n");
    }
    core::refreshSettings();
    delete a;
    PostMessageA(g_hwnd, WM_APP_DONE, 1, (LPARAM)rep);
}

static void taskSnap(void*) {
    core::ScanResult r = core::scanPackages(false);
    std::vector<core::AppState> st;
    std::string dz = core::dozeDump(), np = core::netpolDump();
    for (size_t i = 0; i < r.apps.size(); ++i) {
        PostMessageA(g_hwnd, WM_APP_PROGRESS, i + 1, (LPARAM)r.apps.size());
        st.push_back(core::readState(r.apps[i].first, r.apps[i].second, dz, np));
    }
    std::string err;
    bool ok = core::saveSnapshot(st, g_dev.model, g_dev.serial, err);
    std::string* m = new std::string();
    char b[256];
    if (ok) { wsprintfA(b, XSTR("Da sao luu trang thai goc cua %d app."), (int)st.size()); *m = b; }
    else { *m = std::string(XSTR("Sao luu loi: ")) + err; }
    PostMessageA(g_hwnd, WM_APP_DONE, 2, (LPARAM)m);
}

static void showReport(const char* title, const std::string& txt) {
    char tmp[MAX_PATH] = {0};
    GetTempPathA(MAX_PATH, tmp);
    std::string f = std::string(tmp) + XSTR("xnf_ket-qua.txt");
    FILE* fp = fopen(f.c_str(), XSTR("wb"));
    if (fp) { fwrite(txt.data(), 1, txt.size(), fp); fclose(fp); }
    std::string msg = txt;
    if (msg.size() > 5000) msg = msg.substr(0, 5000) + std::string(XSTR("\n...\n(day du trong file: ")) + f + XSTR(")");
    MessageBoxA(g_hwnd, msg.c_str(), title, MB_OK | MB_ICONINFORMATION);
}

static void gather(bool all, std::vector<std::pair<std::string, std::string> >& out) {
    out.clear();
    if (all) { out = g_scan.apps; return; }
    for (size_t i = 0; i < g_shown.size(); ++i) {
        if (i < g_checked.size() && g_checked[i]) {
            for (size_t j = 0; j < g_scan.apps.size(); ++j)
                if (g_scan.apps[j].first == g_shown[i]) { out.push_back(g_scan.apps[j]); break; }
        }
    }
}

static void startWork(int kind) {
    if (g_busy) return;
    if (kind <= 1 && !g_scan.ok) {
        MessageBoxA(g_hwnd, XSTR("Chua quet ung dung. Bam QUET & DOC TRANG THAI truoc."), XSTR("Thong bao"), MB_OK);
        return;
    }
    bool all = (SendMessage(g_radioAll, BM_GETCHECK, 0, 0) == BST_CHECKED);
    WorkArg* a = new WorkArg();
    a->kind = kind;
    gather(all, a->apps);
    if (a->apps.empty()) {
        delete a;
        MessageBoxA(g_hwnd, XSTR("Chua chon app nao (tick vao danh sach hoac chon Toan bo)."), XSTR("Thong bao"), MB_OK);
        return;
    }
    if (kind == 0) {
        a->a = SendMessage(g_chkStandby, BM_GETCHECK, 0, 0) == BST_CHECKED;
        a->b = SendMessage(g_chkFreezer, BM_GETCHECK, 0, 0) == BST_CHECKED;
        a->c = SendMessage(g_chkMobile, BM_GETCHECK, 0, 0) == BST_CHECKED;
        int nGlob = (a->a ? 1 : 0) + (a->b ? 1 : 0) + (a->c ? 1 : 0);
        if (nGlob > 0) {
            char q[900];
            wsprintfA(q, XSTR("Ban dang bat %d tuy chon TOAN CUC (ap dung cho MOI app tren may):\n\n- Tat App Standby\n- Tat dong bang app nen\n- Giu 4G luon bat\n\nCac tuy chon nay giup thong bao on dinh hon nhung SE TON PIN HON.\nKhuyen nghi chi bat khi that su can. Tiep tuc?"), nGlob);
            if (MessageBoxA(g_hwnd, q, XSTR("Canh bao ton pin"), MB_YESNO | MB_ICONWARNING) != IDYES) {
                delete a;
                setStatus(XSTR("Da huy - chua thay doi gi tren may."));
                return;
            }
        }
        setBusy(true, XSTR("Dang fix..."));
    } else if (kind == 1) {
        setBusy(true, XSTR("Dang hoan tac..."));
    } else if (kind == 2) {
        setBusy(true, XSTR("Dang them vao danh sach mien tru pin..."));
    } else {
        setBusy(true, XSTR("Dang bo khoi danh sach mien tru pin..."));
    }
    runTask(taskWork, a);
}

static void setTab(int m) {
    g_mode = m;
    SetWindowTextA(g_btnAction, m == 0 ? XSTR("BAT DAU FIX THONG BAO") : XSTR("HOAN TAC VE TRANG THAI GOC"));
    ShowWindow(g_btnSnap, (m == 1) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_chkStandby, (m == 0) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_chkFreezer, (m == 0) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_chkMobile,  (m == 0) ? SW_SHOW : SW_HIDE);
    layout();
    if (m == 0) setStatus(XSTR("Che do FIX: tick app can fix (hoac Toan bo) roi bam nut do."));
    else setStatus(XSTR("Che do HOAN TAC: bam SAO LUU GOC truoc khi fix de sau nay tra ve dung nhu cu."));
}

// ---------------- WndProc ----------------
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_hwnd = h;
        HDC sdc = GetDC(NULL);
        g_dpi = GetDeviceCaps(sdc, LOGPIXELSX);
        ReleaseDC(NULL, sdc);
        g_brWhite = CreateSolidBrush(RGB(255,255,255));
        g_brDark  = CreateSolidBrush(RGB(20,28,42));

        g_map = CreateWindowExA(0, XSTR("XnfMap"), "", WS_CHILD | WS_VISIBLE, 0,0,10,10, h, NULL, NULL, NULL);

        g_tabFix    = CreateWindowExA(0, XSTR("BUTTON"), XSTR("FIX THONG BAO"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_TAB_FIX, NULL, NULL);
        g_tabRevert = CreateWindowExA(0, XSTR("BUTTON"), XSTR("HOAN TAC"),      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_TAB_REVERT, NULL, NULL);

        g_device     = CreateWindowExA(0, XSTR("STATIC"), XSTR("Dang kiem tra ket noi..."), WS_CHILD | WS_VISIBLE, 0,0,10,10, h, (HMENU)IDC_DEVICE, NULL, NULL);
        g_btnConnect = CreateWindowExA(0, XSTR("BUTTON"), XSTR("KIEM TRA KET NOI"),   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_BTN_CONNECT, NULL, NULL);
        g_btnScan    = CreateWindowExA(0, XSTR("BUTTON"), XSTR("QUET & DOC TRANG THAI"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_BTN_SCAN, NULL, NULL);
        g_btnAdb     = CreateWindowExA(0, XSTR("BUTTON"), XSTR("ADB..."),             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_BTN_ADB, NULL, NULL);
        g_btnSnap    = CreateWindowExA(0, XSTR("BUTTON"), XSTR("SAO LUU GOC"),        WS_CHILD | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_BTN_SNAP, NULL, NULL);

        g_chkSystem  = CreateWindowExA(0, XSTR("BUTTON"), XSTR("Gom app he thong"),  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0,0,10,10, h, (HMENU)IDC_CHK_SYSTEM, NULL, NULL);
        g_chkStandby = CreateWindowExA(0, XSTR("BUTTON"), XSTR("Tat App Standby (toan may)"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0,0,10,10, h, (HMENU)IDC_CHK_STANDBY, NULL, NULL);
        g_chkFreezer = CreateWindowExA(0, XSTR("BUTTON"), XSTR("Tat dong bang nen (toan may)"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0,0,10,10, h, (HMENU)IDC_CHK_FREEZER, NULL, NULL);
        g_chkMobile  = CreateWindowExA(0, XSTR("BUTTON"), XSTR("Giu 4G luon bat (toan may)"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0,0,10,10, h, (HMENU)IDC_CHK_MOBILE, NULL, NULL);
        // KHONG tick san: tuy chon toan cuc ton pin, chi ap dung khi nguoi dung TU tick
        SendMessage(g_chkFreezer, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_chkMobile, BM_SETCHECK, BST_UNCHECKED, 0);
        g_lblWarn = CreateWindowExA(0, XSTR("STATIC"),
            XSTR("(!) 3 o tick toan cuc ben tren ap dung cho MOI app - BAT SE TON PIN HON, chi bat khi that can"),
            WS_CHILD | WS_VISIBLE, 0,0,10,10, h, (HMENU)IDC_WARN, NULL, NULL);
        g_lblSearch = CreateWindowExA(0, XSTR("STATIC"), XSTR("Tim:"), WS_CHILD | WS_VISIBLE, 0,0,10,10, h, NULL, NULL, NULL);
        g_search    = CreateWindowExA(WS_EX_CLIENTEDGE, XSTR("EDIT"), "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0,0,10,10, h, (HMENU)IDC_EDIT_SEARCH, NULL, NULL);

        g_list = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                                 WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
                                 0,0,10,10, h, (HMENU)IDC_LIST, NULL, NULL);
        ListView_SetExtendedListViewStyle(g_list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LVCOLUMNA c = {0};
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.pszText = (LPSTR)XSTR("Ung dung");  c.cx = 200; ListView_InsertColumn(g_list, 0, &c);
        c.pszText = (LPSTR)XSTR("Trang thai"); c.cx = 240; ListView_InsertColumn(g_list, 1, &c);

        g_btnAll    = CreateWindowExA(0, XSTR("BUTTON"), XSTR("Chon het"),           WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_BTN_ALL, NULL, NULL);
        g_btnNone   = CreateWindowExA(0, XSTR("BUTTON"), XSTR("Bo chon"),            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_BTN_NONE, NULL, NULL);
        g_btnChat   = CreateWindowExA(0, XSTR("BUTTON"), XSTR("Chon app chat"),      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_BTN_CHAT, NULL, NULL);
        g_btnWallet = CreateWindowExA(0, XSTR("BUTTON"), XSTR("Chon vi/ngan hang"),  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_BTN_WALLET, NULL, NULL);

        g_btnDozeAdd = CreateWindowExA(0, XSTR("BUTTON"), XSTR("+ THEM VAO MIEN TRU PIN (DOZE WHITELIST)"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_BTN_DOZE_ADD, NULL, NULL);
        g_btnDozeDel = CreateWindowExA(0, XSTR("BUTTON"), XSTR("- BO KHOI MIEN TRU PIN"),                   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_BTN_DOZE_DEL, NULL, NULL);

        g_radioSel = CreateWindowExA(0, XSTR("BUTTON"), XSTR("Tu chon ung dung"),    WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 0,0,10,10, h, (HMENU)IDC_RADIO_SEL, NULL, NULL);
        g_radioAll = CreateWindowExA(0, XSTR("BUTTON"), XSTR("Toan bo app da quet"), WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,           0,0,10,10, h, (HMENU)IDC_RADIO_ALL, NULL, NULL);
        SendMessage(g_radioSel, BM_SETCHECK, BST_CHECKED, 0);

        g_btnAction = CreateWindowExA(0, XSTR("BUTTON"), XSTR("BAT DAU FIX THONG BAO"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,10,10, h, (HMENU)IDC_BTN_ACTION, NULL, NULL);
        g_progress  = CreateWindowExA(0, PROGRESS_CLASS, "", WS_CHILD | WS_VISIBLE, 0,0,10,10, h, (HMENU)IDC_PROGRESS, NULL, NULL);
        g_status    = CreateWindowExA(0, XSTR("STATIC"), XSTR("San sang."), WS_CHILD | WS_VISIBLE, 0,0,10,10, h, (HMENU)IDC_STATUS, NULL, NULL);

        g_font  = CreateFontA(-S(13), 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, XSTR("Segoe UI"));
        g_fontB = CreateFontA(-S(13), 0,0,0, FW_BOLD,   0,0,0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, XSTR("Segoe UI"));
        g_fontT = CreateFontA(-S(21), 0,0,0, FW_BOLD,   0,0,0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, XSTR("Segoe UI"));
        g_fontS = CreateFontA(-S(12), 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, XSTR("Segoe UI"));
        g_fontD = CreateFontA(-S(16), 0,0,0, FW_BOLD,   0,0,0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, XSTR("Segoe UI"));
        EnumChildWindows(h, [](HWND c, LPARAM) -> BOOL {
            SendMessage(c, WM_SETFONT, (WPARAM)g_font, TRUE);
            return TRUE;
        }, 0);
        SendMessage(g_device, WM_SETFONT, (WPARAM)g_fontB, TRUE);
        SendMessage(g_btnAction, WM_SETFONT, (WPARAM)g_fontB, TRUE);
        SendMessage(g_btnScan, WM_SETFONT, (WPARAM)g_fontB, TRUE);
        SendMessage(g_btnDozeAdd, WM_SETFONT, (WPARAM)g_fontB, TRUE);
        SendMessage(g_lblWarn, WM_SETFONT, (WPARAM)g_fontS, TRUE);

        setTab(0);
        runTask(taskConnect, NULL);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)l;
        mm->ptMinTrackSize.x = S(900);
        mm->ptMinTrackSize.y = S(600);
        return 0;
    }
    case WM_SIZE:
        layout();
        InvalidateRect(g_map, NULL, TRUE);
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(20,28,42));
        return (LRESULT)g_brWhite;
    }
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(h, &rc);
        FillRect((HDC)w, &rc, g_brWhite);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        int leftW = rc.right * 56 / 100;
        RECT hdr = {0, 0, leftW, S(92)};
        FillRect(dc, &hdr, g_brDark);
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_fontT);
        SetTextColor(dc, RGB(255, 77, 94));
        TextOutA(dc, S(14), S(6), XSTR("TOOL FIX THONG BAO XIAOMI"), 25);
        SelectObject(dc, g_fontS);
        SetTextColor(dc, RGB(170, 188, 205));
        TextOutA(dc, S(14), S(36), XSTR("ROM noi dia Trung  -  tu khoi dong + pin + thong bao + chay nen"), 63);
        SelectObject(dc, g_fontD);
        SetTextColor(dc, RGB(255, 212, 0));
        TextOutA(dc, S(14), S(60), XSTR("Phan Tan Dat   -   MB Bank 0982414903"), 35);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_NOTIFY: {
        LPNMHDR nh = (LPNMHDR)l;
        if (nh && nh->idFrom == IDC_LIST && nh->code == LVN_ITEMCHANGED) {
            LPNMLISTVIEW p = (LPNMLISTVIEW)l;
            if (p->uChanged & LVIF_STATE) {
                bool checked = (((p->uNewState & LVIS_STATEIMAGEMASK) >> 12) == 2);
                int i = p->iItem;
                if (i >= 0 && i < (int)g_checked.size()) g_checked[i] = checked ? 1 : 0;
            }
        }
        break;      // -> DefWindowProc
    }
    case WM_COMMAND: {
        int id = LOWORD(w);
        switch (id) {
        case IDC_TAB_FIX: setTab(0); break;
        case IDC_TAB_REVERT: setTab(1); break;
        case IDC_BTN_CONNECT: setBusy(true, XSTR("Dang kiem tra ket noi...")); runTask(taskConnect, NULL); break;
        case IDC_BTN_SCAN: {
            setBusy(true, XSTR("Dang quet va doc trang thai..."));
            bool sys = SendMessage(g_chkSystem, BM_GETCHECK, 0, 0) == BST_CHECKED;
            runTask(taskScan, sys ? (void*)1 : NULL);
            break;
        }
        case IDC_BTN_ADB: {
            OPENFILENAMEA of = {0};
            char f[MAX_PATH] = {0};
            of.lStructSize = sizeof(of);
            of.hwndOwner = h;
            of.lpstrFilter = XSTR("adb.exe\0adb.exe\0Tat ca (*.exe)\0*.exe\0");
            of.lpstrFile = f;
            of.nMaxFile = MAX_PATH;
            of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameA(&of)) {
                adb::setPath(f);
                adb::setSerial("");
                setBusy(true, XSTR("Da chon adb, dang kiem tra..."));
                runTask(taskConnect, NULL);
            }
            break;
        }
        case IDC_BTN_SNAP:
            setBusy(true, XSTR("Dang doc trang thai de sao luu..."));
            runTask(taskSnap, NULL);
            break;
        case IDC_BTN_ACTION: startWork(g_mode == 0 ? 0 : 1); break;
        case IDC_BTN_DOZE_ADD: startWork(2); break;
        case IDC_BTN_DOZE_DEL: startWork(3); break;
        case IDC_BTN_ALL: case IDC_BTN_NONE: case IDC_BTN_CHAT: case IDC_BTN_WALLET: {
            static const char* chatK[] = { "zalo","facebook","messenger","telegram","whatsapp","viber","instagram",
                "trill","douyin","discord","locket","line","kakao","wechat","twitter","barcelona","reddit",
                "snapchat","badoo","tiktok","skype","signal","chat" };
            static const char* bankK[] = { "momo","zalopay","vnpay","viettelpay","shopeepay","mbmobile","vietinbank",
                "techcombank","viettin","agribank","bidv","tpbank","acb","sacombank","vnid","viettel","vinaphone",
                "mobifone","bank","pay","cake","timo","tnex","vib","vpbank","hdbank","ocb","seabank","msb","lpbank","eximbank" };
            for (int i = 0; i < (int)g_shown.size(); ++i) {
                bool v = g_checked[i] != 0;
                if (id == IDC_BTN_ALL) v = true;
                else if (id == IDC_BTN_NONE) v = false;
                else if (id == IDC_BTN_CHAT) v = v || matchAny(g_shown[i], chatK, 23);
                else if (id == IDC_BTN_WALLET) v = v || matchAny(g_shown[i], bankK, 31);
                g_checked[i] = v ? 1 : 0;
                ListView_SetCheckState(g_list, i, v);
            }
            break;
        }
        case IDC_EDIT_SEARCH:
            if (HIWORD(w) == EN_CHANGE) {
                char q[256] = {0};
                GetWindowTextA(g_search, q, 255);
                std::string s = core::lower(q);
                // giu lai trang thai da doc
                std::vector<std::string> oldPkgs = g_shown;
                std::vector<core::AppState> oldSt = g_states;
                std::vector<char> oldCk = g_checked;
                listClear();
                for (size_t i = 0; i < g_scan.apps.size(); ++i) {
                    std::string pkg = g_scan.apps[i].first;
                    if (!s.empty() && core::lower(pkg).find(s) == std::string::npos) continue;
                    g_shown.push_back(pkg);
                    { bool ck = false; for (size_t j = 0; j < oldPkgs.size(); ++j) if (oldPkgs[j] == pkg) { ck = oldCk[j] != 0; break; } g_checked.push_back(ck ? 1 : 0); }
                    std::string tag = XSTR("chua doc");
                    for (size_t k2 = 0; k2 < g_tags.size(); ++k2)
                        if (g_tags[k2].first == pkg) { tag = g_tags[k2].second; break; }
                    listAdd((int)g_shown.size() - 1, pkg.c_str(), tag.c_str(), g_checked[g_shown.size()-1] != 0);
                }
            }
            break;
        }
        return 0;
    }
    case WM_APP_STATE: {
        StateMsg* sm = (StateMsg*)l;
        g_tags.push_back(std::make_pair(sm->pkg, sm->tag));
        int n = ListView_GetItemCount(g_list);
        for (int i = 0; i < n; ++i) {
            if (i < (int)g_shown.size() && g_shown[i] == sm->pkg) { listSetSub(i, sm->tag.c_str()); break; }
        }
        delete sm;
        return 0;
    }
    case WM_APP_DEVICES: {
        char* msg = (char*)l;
        if (w == 1) {
            SetWindowTextA(g_device, (std::string(XSTR("Da ket noi: ")) + msg).c_str());
            setBusy(false, NULL);
            setStatus(XSTR("Da ket noi. Dang quet va doc trang thai..."));
            bool sys = SendMessage(g_chkSystem, BM_GETCHECK, 0, 0) == BST_CHECKED;
            runTask(taskScan, sys ? (void*)1 : NULL);
        } else {
            SetWindowTextA(g_device, XSTR("Chua ket noi duoc dien thoai"));
            setBusy(false, NULL);
            MessageBoxA(h, msg, XSTR("Khong ket noi duoc"), MB_OK | MB_ICONWARNING);
        }
        delete[] msg;
        return 0;
    }
    case WM_APP_SCAN: {
        core::ScanResult* r = (core::ScanResult*)l;
        g_scan = *r; delete r;
        listClear();
        g_tags.clear();
        for (size_t i = 0; i < g_scan.apps.size(); ++i) {
            g_shown.push_back(g_scan.apps[i].first);
            g_checked.push_back(0);
            listAdd((int)i, g_scan.apps[i].first.c_str(), XSTR("chua doc"), false);
        }
        setBusy(false, NULL);
        if (!g_scan.ok) setStatus(XSTR("KHONG quet duoc app nao. %s"), g_scan.log.c_str());
        else {
            char b[300];
            wsprintfA(b, XSTR("Xong: %d app. Cot Trang thai ghi ro app nao da bat het."), (int)g_scan.apps.size());
            setStatus("%s", b);
        }
        return 0;
    }
    case WM_APP_PROGRESS: {
        SendMessage(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, (int)l));
        SendMessage(g_progress, PBM_SETPOS, (int)w, 0);
        char b[128];
        wsprintfA(b, XSTR("Dang xu ly... %d/%d"), (int)w, (int)l);
        setStatus("%s", b);
        return 0;
    }
    case WM_APP_DONE: {
        std::string* rep = (std::string*)l;
        setBusy(false, NULL);
        if (w == 5) { setStatus("%s", rep->c_str()); MessageBoxA(h, rep->c_str(), XSTR("Doze whitelist"), MB_OK); }
        else if (w == 2) { setStatus("%s", rep->c_str()); MessageBoxA(h, rep->c_str(), XSTR("Sao luu trang thai goc"), MB_OK); }
        else {
            showReport(w == 1 ? XSTR("Ket qua fix") : XSTR("Ket qua hoan tac"), *rep);
            setStatus(w == 1 ? XSTR("Da fix xong. Nen khoi dong lai dien thoai.")
                             : XSTR("Da hoan tac xong."));
        }
        delete rep;
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR cmd, int) {
    // ---- che do dong lenh: --check (kiem tra nhanh, khong can giao dien) ----
    if (cmd && strstr(cmd, "--check")) {
        std::string outp;
        std::string ap, asrc;
        if (!adb::locate(ap, asrc)) outp += "KHONG TIM THAY ADB\n";
        else {
            outp += "adb: " + ap + "\n";
            std::vector<adb::Device> ds = adb::devices();
            outp += "so thiet bi: " + std::to_string((long long)ds.size()) + "\n";
            if (!ds.empty()) {
                adb::Device d = adb::describe(ds[0].serial);
                adb::setSerial(d.serial);
                outp += "may: " + d.model + " | ROM " + d.miui + " | android " + d.android + " | " + d.codename + " | build " + d.region + "\n";
                core::ScanResult r = core::scanPackages(false);
                outp += "quet duoc: " + std::to_string((long long)r.apps.size()) + " app (" + r.how + ")\n";
                if (r.ok) {
                    std::string dz = core::dozeDump();
                    std::string np = core::netpolDump();
                    int full = 0, shown = 0;
                    for (size_t i = 0; i < r.apps.size(); ++i) {
                        core::AppState st = core::readState(r.apps[i].first, r.apps[i].second, dz, np);
                        std::string tag = core::stateTag(st);
                        if (tag.find(XSTR("DA BAT HET")) != std::string::npos) ++full;
                        if (shown < 15) { outp += "  " + r.apps[i].first + "  =>  " + tag + "\n"; ++shown; }
                    }
                    char b[128];
                    wsprintfA(b, "TONG: %d/%d app DA BAT HET\n", full, (int)r.apps.size());
                    outp += b;
                }
            }
        }
        char tmp[MAX_PATH] = {0};
        GetTempPathA(MAX_PATH, tmp);
        std::string path = std::string(tmp) + XSTR("xnf_cpp.txt");
        FILE* fp = fopen(path.c_str(), XSTR("wb"));
        if (fp) { fwrite(outp.data(), 1, outp.size(), fp); fclose(fp); }
        return 0;
    }

    SetProcessDPIAware();
    INITCOMMONCONTROLSEX ic = {0};
    ic.dwSize = sizeof(ic);
    ic.dwICC = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&ic);

    ULONG_PTR gpToken = 0;
    GdiplusStartupInput gpsi;
    GdiplusStartup(&gpToken, &gpsi, NULL);

    HRSRC hr = FindResourceA(NULL, MAKEINTRESOURCEA(IDR_BG), RT_RCDATA);
    if (hr) {
        HGLOBAL hg = LoadResource(NULL, hr);
        void* data = hg ? LockResource(hg) : NULL;
        DWORD sz = SizeofResource(NULL, hr);
        if (data && sz) {
            HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, sz);
            if (copy) {
                memcpy(GlobalLock(copy), data, sz);
                GlobalUnlock(copy);
                IStream* is = NULL;
                if (CreateStreamOnHGlobal(copy, TRUE, &is) == S_OK) {
                    Bitmap* bmp = Bitmap::FromStream(is);
                    if (bmp && bmp->GetLastStatus() == Ok) bmp->GetHBITMAP(Color(255,255,255), &g_bg);
                    if (bmp) delete bmp;
                    is->Release();
                }
            }
        }
    }

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = XSTR("XnfMain");
    wc.hIcon = LoadIconA(hi, MAKEINTRESOURCEA(IDI_APP));
    RegisterClassA(&wc);

    WNDCLASSA mc = {0};
    mc.lpfnWndProc = MapProc;
    mc.hInstance = hi;
    mc.hCursor = LoadCursor(NULL, IDC_ARROW);
    mc.lpszClassName = XSTR("XnfMap");
    RegisterClassA(&mc);

    RECT r = {0, 0, 1180, 660};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, XSTR("XnfMain"), XSTR("Tool fix thong bao Xiaomi (ban C++)"),
                               WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                               NULL, NULL, hi, NULL);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
