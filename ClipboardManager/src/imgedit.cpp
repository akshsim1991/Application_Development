// Picture editor: crop, draw, highlight, rotate, resize; then save as a new history item, copy, or save to a file.
#include "app.h"

namespace {

enum { ID_CROPMODE = 1, ID_DRAWMODE, ID_HILITE, ID_COLOUR, ID_APPLYCROP, ID_ROTATE, ID_RESIZE, ID_UNDO, ID_SAVEITEM, ID_COPY, ID_SAVEFILE, ID_CLOSE };

struct Editor {
    HWND h = nullptr;
    bool done = false;
    Gdiplus::Bitmap* bmp = nullptr;
    std::vector<Gdiplus::Bitmap*> undo;
    int mode = ID_DRAWMODE;
    int colorIdx = 0;
    bool dragging = false;
    Gdiplus::Point selA{0, 0}, selB{0, 0};
    bool haveSel = false;
    Gdiplus::Point last{0, 0};
    bool changed = false;
    HWND btn[16] = {};
    std::wstring srcPath;
    RECT canvas{};
    double scale = 1;
    int offX = 0, offY = 0;
};
Editor* E = nullptr;

int S(int v) { HDC dc = GetDC(nullptr); int d = GetDeviceCaps(dc, LOGPIXELSY); ReleaseDC(nullptr, dc); return MulDiv(v, d, 96); }

const COLORREF kColours[] = {RGB(230, 40, 40), RGB(255, 200, 0), RGB(30, 110, 230), RGB(20, 20, 20), RGB(30, 170, 80), RGB(255, 255, 255)};
const wchar_t* kColourNames[] = {L"Red", L"Yellow", L"Blue", L"Black", L"Green", L"White"};

void PushUndo() {
    if (!E->bmp) return;
    E->undo.push_back(E->bmp->Clone(0, 0, E->bmp->GetWidth(), E->bmp->GetHeight(), PixelFormat32bppARGB));
    if (E->undo.size() > 12) { delete E->undo.front(); E->undo.erase(E->undo.begin()); }
}

void ComputeCanvas() {
    RECT rc;
    GetClientRect(E->h, &rc);
    E->canvas = {S(10), S(56), rc.right - S(10), rc.bottom - S(10)};
    int cw = E->canvas.right - E->canvas.left, ch = E->canvas.bottom - E->canvas.top;
    int iw = (int)E->bmp->GetWidth(), ih = (int)E->bmp->GetHeight();
    E->scale = std::min(1.0, std::min((double)cw / iw, (double)ch / ih));
    int dw = (int)(iw * E->scale), dh = (int)(ih * E->scale);
    E->offX = E->canvas.left + (cw - dw) / 2;
    E->offY = E->canvas.top + (ch - dh) / 2;
}

Gdiplus::Point ToImage(int x, int y) {
    int ix = (int)((x - E->offX) / E->scale), iy = (int)((y - E->offY) / E->scale);
    ix = std::clamp(ix, 0, (int)E->bmp->GetWidth() - 1);
    iy = std::clamp(iy, 0, (int)E->bmp->GetHeight() - 1);
    return Gdiplus::Point(ix, iy);
}

void Swap(Gdiplus::Bitmap* nb) {
    delete E->bmp;
    E->bmp = nb;
    E->haveSel = false;
    E->changed = true;
    ComputeCanvas();
    InvalidateRect(E->h, nullptr, TRUE);
}

std::wstring TempPng() { return JoinPath(g_store.dir, L"edit-" + std::to_wstring(GetTickCount64()) + L".png.tmp"); }

bool SaveBmp(const std::wstring& path) {
    CLSID png = EncoderFor(L"image/png");
    return E->bmp->Save(path.c_str(), &png, nullptr) == Gdiplus::Ok;
}

void UpdateButtons() {
    InvalidateRect(E->btn[ID_CROPMODE], nullptr, TRUE);
    InvalidateRect(E->btn[ID_DRAWMODE], nullptr, TRUE);
    InvalidateRect(E->btn[ID_HILITE], nullptr, TRUE);
}

void DrawStroke(Gdiplus::Point a, Gdiplus::Point b) {
    Gdiplus::Graphics g(E->bmp);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    COLORREF c = kColours[E->colorIdx];
    bool hi = E->mode == ID_HILITE;
    float wpx = (float)std::max(3, (int)(std::max(E->bmp->GetWidth(), E->bmp->GetHeight()) / 250)) * (hi ? 5.0f : 1.0f);
    Gdiplus::Pen pen(Gdiplus::Color(hi ? 110 : 255, GetRValue(c), GetGValue(c), GetBValue(c)), wpx);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    if (a.Equals(b)) {
        Gdiplus::SolidBrush br(Gdiplus::Color(hi ? 110 : 255, GetRValue(c), GetGValue(c), GetBValue(c)));
        g.FillEllipse(&br, a.X - wpx / 2, a.Y - wpx / 2, wpx, wpx);
    } else g.DrawLine(&pen, a, b);
}

void ApplyCrop() {
    if (!E->haveSel) return;
    int x = std::min(E->selA.X, E->selB.X), y = std::min(E->selA.Y, E->selB.Y);
    int w = std::abs(E->selA.X - E->selB.X), h = std::abs(E->selA.Y - E->selB.Y);
    if (w < 4 || h < 4) return;
    PushUndo();
    Gdiplus::Bitmap* nb = E->bmp->Clone(x, y, w, h, PixelFormat32bppARGB);
    if (nb) Swap(nb);
}

void Resize() {
    HMENU m = CreatePopupMenu();
    const int pct[] = {75, 50, 25, 200};
    for (int i = 0; i < 4; i++) AppendMenuW(m, MF_STRING, 1 + i, (std::to_wstring(pct[i]) + L"%").c_str());
    RECT r;
    GetWindowRect(E->btn[ID_RESIZE], &r);
    int pick = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, E->h, nullptr);
    DestroyMenu(m);
    if (pick <= 0) return;
    int p = pct[pick - 1];
    int nw = std::max(1, (int)E->bmp->GetWidth() * p / 100), nh = std::max(1, (int)E->bmp->GetHeight() * p / 100);
    PushUndo();
    Gdiplus::Bitmap* nb = new Gdiplus::Bitmap(nw, nh, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(nb);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.DrawImage(E->bmp, Gdiplus::Rect(0, 0, nw, nh), 0, 0, (INT)E->bmp->GetWidth(), (INT)E->bmp->GetHeight(), Gdiplus::UnitPixel);
    }
    Swap(nb);
}

void SaveAsNewItem(bool copy) {
    std::wstring tmp = TempPng();
    if (!SaveBmp(tmp)) { MsgInfo(E->h, T(L"The file could not be saved."), MB_ICONWARNING); return; }
    std::string bytes;
    ReadFileBytes(tmp, bytes);
    uint64_t hash = core::Fnv64(bytes.data(), bytes.size());
    bool isNew = true;
    uint64_t id = g_store.AddPictureFile(tmp, (int)E->bmp->GetWidth(), (int)E->bmp->GetHeight(), hash, L"Feather Clipboard Manager", &isNew);
    if (!id) { DeleteFileW(tmp.c_str()); MsgInfo(E->h, T(L"The file could not be saved."), MB_ICONWARNING); return; }
    Item* it = g_store.Find(id);
    if (it) {
        if (copy) ClipPutBitmapFile(g_main, g_store.MainPath(*it));
        if (isNew && g_set.ocr) OcrStart(g_main, id, g_store.MainPath(*it));
    }
    E->changed = false;
    E->done = true;
}

void SaveFile() {
    wchar_t file[MAX_PATH] = L"picture.png";
    OPENFILENAMEW o = {sizeof o};
    o.hwndOwner = E->h;
    o.lpstrFile = file;
    o.nMaxFile = MAX_PATH;
    o.lpstrDefExt = L"png";
    std::wstring filter = std::wstring(T(L"PNG picture (*.png)")) + L'\0' + L"*.png" + L'\0' + L"JPEG picture (*.jpg)" + L'\0' + L"*.jpg" + L'\0' + L"BMP picture (*.bmp)" + L'\0' + L"*.bmp" + L'\0';
    o.lpstrFilter = filter.c_str();
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&o)) return;
    std::wstring ext = core::Lower(PathFindExtensionW(file));
    std::wstring tmp = TempPng();
    bool ok = SaveBmp(tmp);
    if (ok) {
        if (ext == L".jpg" || ext == L".jpeg") ok = PngToSizedCopy(tmp, file, L"image/jpeg", 100);
        else if (ext == L".bmp") ok = PngToSizedCopy(tmp, file, L"image/bmp", 100);
        else ok = CopyFileW(tmp.c_str(), file, FALSE) != 0;
    }
    DeleteFileW(tmp.c_str());
    if (!ok) MsgInfo(E->h, T(L"The file could not be saved."), MB_ICONWARNING);
}

LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: {
        if (!E || !E->bmp) break;
        ComputeCanvas();
        InvalidateRect(h, nullptr, TRUE);
        return 0;
    }
    case WM_GETMINMAXINFO: ((MINMAXINFO*)l)->ptMinTrackSize = {S(760), S(420)}; return 0;
    case WM_DRAWITEM: {
        auto* di = (DRAWITEMSTRUCT*)l;
        if (di->CtlID == ID_COLOUR) {
            HDC dc = di->hDC;
            RECT r = di->rcItem;
            DrawThemedButton(di, false);
            RECT sw = {r.left + S(8), r.top + S(7), r.left + S(8) + S(18), r.bottom - S(7)};
            HBRUSH b = CreateSolidBrush(kColours[E->colorIdx]);
            FillRect(dc, &sw, b);
            DeleteObject(b);
            FrameRect(dc, &sw, (HBRUSH)GetStockObject(GRAY_BRUSH));
            return TRUE;
        }
        // highlight the active tool
        if ((di->CtlID == ID_CROPMODE || di->CtlID == ID_DRAWMODE || di->CtlID == ID_HILITE) && (int)di->CtlID == E->mode) {
            DRAWITEMSTRUCT copy = *di;
            copy.itemState |= ODS_SELECTED;
            return DrawThemedButton(&copy, false);
        }
        return DrawThemedButton(di, false);
    }
    case WM_CTLCOLORSTATIC: SetBkColor((HDC)w, g_th.bg); SetTextColor((HDC)w, g_th.text); return (LRESULT)ThemeBrush(0);
    case WM_COMMAND: {
        int id = LOWORD(w);
        switch (id) {
        case ID_CROPMODE: case ID_DRAWMODE: case ID_HILITE: E->mode = id; UpdateButtons(); break;
        case ID_COLOUR: {
            HMENU pm = CreatePopupMenu();
            for (int i = 0; i < 6; i++) AppendMenuW(pm, MF_STRING | (i == E->colorIdx ? MF_CHECKED : 0), 1 + i, T(kColourNames[i]));
            RECT r;
            GetWindowRect(E->btn[ID_COLOUR], &r);
            int pick = TrackPopupMenu(pm, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, h, nullptr);
            DestroyMenu(pm);
            if (pick > 0) { E->colorIdx = pick - 1; InvalidateRect(E->btn[ID_COLOUR], nullptr, TRUE); }
            break;
        }
        case ID_APPLYCROP: ApplyCrop(); break;
        case ID_ROTATE: PushUndo(); E->bmp->RotateFlip(Gdiplus::Rotate90FlipNone); E->changed = true; E->haveSel = false; ComputeCanvas(); InvalidateRect(h, nullptr, TRUE); break;
        case ID_RESIZE: Resize(); break;
        case ID_UNDO:
            if (!E->undo.empty()) { Gdiplus::Bitmap* b = E->undo.back(); E->undo.pop_back(); delete E->bmp; E->bmp = b; E->haveSel = false; ComputeCanvas(); InvalidateRect(h, nullptr, TRUE); }
            break;
        case ID_SAVEITEM: SaveAsNewItem(false); break;
        case ID_COPY: SaveAsNewItem(true); break;
        case ID_SAVEFILE: SaveFile(); break;
        case ID_CLOSE: E->done = true; break;
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        if (x < E->offX || y < E->offY || x > E->offX + (int)(E->bmp->GetWidth() * E->scale) || y > E->offY + (int)(E->bmp->GetHeight() * E->scale)) return 0;
        SetCapture(h);
        E->dragging = true;
        Gdiplus::Point p = ToImage(x, y);
        if (E->mode == ID_CROPMODE) { E->selA = E->selB = p; E->haveSel = true; }
        else { PushUndo(); E->last = p; DrawStroke(p, p); E->changed = true; }
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (E->dragging) {
            Gdiplus::Point p = ToImage(GET_X_LPARAM(l), GET_Y_LPARAM(l));
            if (E->mode == ID_CROPMODE) E->selB = p;
            else { DrawStroke(E->last, p); E->last = p; }
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (E->dragging) {
            E->dragging = false;
            ReleaseCapture();
        }
        return 0;
    case WM_SETCURSOR: {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(h, &p);
        if (p.y > S(50)) { SetCursor(LoadCursor(nullptr, IDC_CROSS)); return TRUE; }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP mb = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HGDIOBJ old = SelectObject(mem, mb);
        FillRect(mem, &rc, ThemeBrush(0));
        {
            Gdiplus::Graphics g(mem);
            int dw = std::max(1, (int)(E->bmp->GetWidth() * E->scale)), dh = std::max(1, (int)(E->bmp->GetHeight() * E->scale));
            // light checkerboard under transparent pictures
            Gdiplus::SolidBrush a(Gdiplus::Color(255, 235, 235, 235)), b(Gdiplus::Color(255, 205, 205, 205));
            int cs = S(10);
            for (int yy = 0; yy < dh; yy += cs)
                for (int xx = 0; xx < dw; xx += cs)
                    g.FillRectangle(((xx / cs + yy / cs) & 1) ? &b : &a, E->offX + xx, E->offY + yy, std::min(cs, dw - xx), std::min(cs, dh - yy));
            g.SetInterpolationMode(E->scale < 1 ? Gdiplus::InterpolationModeHighQualityBicubic : Gdiplus::InterpolationModeNearestNeighbor);
            g.DrawImage(E->bmp, Gdiplus::Rect(E->offX, E->offY, dw, dh), 0, 0, (INT)E->bmp->GetWidth(), (INT)E->bmp->GetHeight(), Gdiplus::UnitPixel);
            if (E->haveSel) {
                int x1 = E->offX + (int)(std::min(E->selA.X, E->selB.X) * E->scale), y1 = E->offY + (int)(std::min(E->selA.Y, E->selB.Y) * E->scale);
                int x2 = E->offX + (int)(std::max(E->selA.X, E->selB.X) * E->scale), y2 = E->offY + (int)(std::max(E->selA.Y, E->selB.Y) * E->scale);
                Gdiplus::SolidBrush dim(Gdiplus::Color(110, 0, 0, 0));
                g.FillRectangle(&dim, E->offX, E->offY, dw, y1 - E->offY);
                g.FillRectangle(&dim, E->offX, y2, dw, E->offY + dh - y2);
                g.FillRectangle(&dim, E->offX, y1, x1 - E->offX, y2 - y1);
                g.FillRectangle(&dim, x2, y1, E->offX + dw - x2, y2 - y1);
                Gdiplus::Pen pen(Gdiplus::Color(255, 255, 255, 255), 1.0f);
                pen.SetDashStyle(Gdiplus::DashStyleDash);
                g.DrawRectangle(&pen, x1, y1, x2 - x1, y2 - y1);
            }
        }
        std::wstring info = std::to_wstring(E->bmp->GetWidth()) + L" × " + std::to_wstring(E->bmp->GetHeight()) + L" px";
        RECT ir = {rc.right - S(180), S(18), rc.right - S(12), S(40)};
        SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, g_th.sub);
        SelectObject(mem, g_fontSmall);
        DrawTextW(mem, info.c_str(), -1, &ir, DT_RIGHT | DT_SINGLELINE);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(mb);
        DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CLOSE:
        if (E->changed && !MsgAsk(h, T(L"Close without saving your changes?"))) return 0;
        E->done = true;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

}  // namespace

void ShowImageEditor(HWND parent, const Item& it) {
    GdiplusStart();
    std::string bytes;
    if (!ReadFileBytes(g_store.MainPath(it), bytes)) { MsgInfo(parent, T(L"The picture file could not be read."), MB_ICONWARNING); return; }
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!g) return;
    memcpy(GlobalLock(g), bytes.data(), bytes.size());
    GlobalUnlock(g);
    IStream* st = nullptr;
    if (FAILED(CreateStreamOnHGlobal(g, TRUE, &st))) { GlobalFree(g); return; }
    Gdiplus::Bitmap tmp(st);
    Gdiplus::Bitmap* work = tmp.GetLastStatus() == Gdiplus::Ok ? tmp.Clone(0, 0, tmp.GetWidth(), tmp.GetHeight(), PixelFormat32bppARGB) : nullptr;
    st->Release();
    if (!work) { MsgInfo(parent, T(L"The picture file could not be read."), MB_ICONWARNING); return; }

    static bool reg = false;
    if (!reg) {
        reg = true;
        WNDCLASSEXW wc = {sizeof wc};
        wc.lpfnWndProc = EditProc;
        wc.hInstance = g_inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CmImageEditor";
        wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
        RegisterClassExW(&wc);
    }
    Editor ed;
    E = &ed;
    ed.bmp = work;
    int W = S(980), H = S(700);
    RECT pr = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    ed.h = CreateWindowExW(0, L"CmImageEditor", T(L"Edit the picture"), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, (pr.right - W) / 2, (pr.bottom - H) / 2, W, H, parent, nullptr, g_inst, nullptr);
    BOOL dark = g_th.dark;
    DwmSetWindowAttribute(ed.h, 20, &dark, sizeof dark);
    struct B { int id; const wchar_t* text; int w; };
    B bs[] = {{ID_CROPMODE, T(L"Crop"), 64}, {ID_APPLYCROP, T(L"Apply crop"), 92}, {ID_DRAWMODE, T(L"Draw"), 64}, {ID_HILITE, T(L"Highlight"), 84}, {ID_COLOUR, L"", 46},
              {ID_ROTATE, T(L"Rotate"), 70}, {ID_RESIZE, T(L"Resize ▾"), 80}, {ID_UNDO, T(L"Undo"), 60},
              {ID_SAVEITEM, T(L"Save as new item"), 130}, {ID_COPY, T(L"Save and copy"), 118}, {ID_SAVEFILE, T(L"Save as file…"), 108}, {ID_CLOSE, T(L"Close"), 64}};
    int x = S(10);
    for (auto& b : bs) {
        ed.btn[b.id] = CreateWindowExW(0, L"BUTTON", b.text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, x, S(10), S(b.w), S(30), ed.h, (HMENU)(UINT_PTR)b.id, g_inst, nullptr);
        SendMessageW(ed.btn[b.id], WM_SETFONT, (WPARAM)g_font, TRUE);
        x += S(b.w) + S(6);
    }
    ComputeCanvas();
    EnableWindow(parent, FALSE);
    ShowWindow(ed.h, SW_SHOW);
    SetForegroundWindow(ed.h);
    MSG msg;
    while (!ed.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && GetAncestor(msg.hwnd, GA_ROOT) == ed.h) {
            if (msg.wParam == 'Z' && GetKeyState(VK_CONTROL) < 0) { SendMessageW(ed.h, WM_COMMAND, ID_UNDO, 0); continue; }
            if (msg.wParam == VK_ESCAPE) { SendMessageW(ed.h, WM_CLOSE, 0, 0); continue; }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(parent, TRUE);
    DestroyWindow(ed.h);
    SetForegroundWindow(parent);
    delete ed.bmp;
    for (auto* u : ed.undo) delete u;
    E = nullptr;
}
