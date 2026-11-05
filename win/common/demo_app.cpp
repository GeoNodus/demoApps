#include "demo_app.h"

#include "demo_scenes.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

// Common Controls 6 — modern look of the buttons and the slider
#pragma comment(linker, \
    "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace demo {

namespace {

const wchar_t* MAIN_CLASS = L"GeoNodusDemoMain";
const wchar_t* VIEW_CLASS = L"GeoNodusDemoView";

enum ControlId { ID_OPEN = 101, ID_SCENE, ID_PLAY, ID_SLIDER, ID_STATUS };

const float PI = 3.14159265358979f;
const float FOVY = 45.0f * PI / 180.0f;
// Wheel: each notch (120 units) changes the distance to the target by ZOOM_PER_NOTCH, as in the Qt client
const float ZOOM_PER_NOTCH = 1.15f;

std::wstring toWide(const std::string& s)
{
    if (s.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    w.resize(static_cast<size_t>(n - 1));
    return w;
}

class DemoApp
{
public:
    DemoApp(IRenderer& renderer, const wchar_t* title)
        : m_renderer(renderer)
        , m_title(title)
    {}

    int run();

private:
    static LRESULT CALLBACK mainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK viewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT onMain(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT onView(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    bool createWindows();
    void createFont();
    void layout();
    int scale(int px) const { return MulDiv(px, m_dpi, 96); }

    void openDialog();
    void openFile(const std::wstring& path);
    // Builds demo scene `index` of demo::scenes::demoScenes (the scene combo box)
    void openScene(int index);
    // After a new graph: the timeline of the slider, the first frame, the camera is framed on the first geometry
    void sceneReplaced();
    void setPlaying(bool play);
    void restartPlayClock();
    void requestFrame(int frame);
    void tick();
    // Time until the next playback frame, ms
    DWORD playWaitMs() const;
    void updateTitle();
    void updateStatus();

    void frameAll();
    ViewParams viewParams() const;
    Vec3 eye() const;
    void onKey(WPARAM key);

    IRenderer& m_renderer;
    std::wstring m_title;
    std::wstring m_file;
    int m_scene_index = -1; // the demo scene shown, -1 — a graph file (m_file) or nothing
    GnScene m_scene;
    SceneGeometry m_geometry;

    HWND m_main = nullptr;
    HWND m_view = nullptr;
    HWND m_open_btn = nullptr;
    HWND m_scene_combo = nullptr;
    HWND m_play_btn = nullptr;
    HWND m_slider = nullptr;
    HWND m_status = nullptr;
    HFONT m_font = nullptr;
    int m_dpi = 96;
    bool m_renderer_ok = false;

    // Camera: an orbit around the target, angles in radians
    Vec3 m_target;
    float m_distance = 10.0f;
    float m_yaw = 0.785f;
    float m_pitch = 0.45f;
    bool m_framed = false;
    POINT m_drag_start{};
    int m_drag_button = 0; // 1 — rotate, 2 — pan, 3 — zoom

    // Real-time playback (as Realtime in the Qt client): the frame follows the elapsed time, frames whose cook takes
    // longer than 1/fps are skipped
    bool m_playing = false;
    int m_play_start_frame = 0;
    ULONGLONG m_play_start_ms = 0;
    int m_requested_frame = 0;
};

int DemoApp::run()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    if (!m_scene.isValid()) {
        MessageBoxW(nullptr, L"Cannot create a GeoNodus session.", m_title.c_str(), MB_ICONERROR);
        return 1;
    }
    if (!createWindows())
        return 1;

    std::string error;
    m_renderer_ok = m_renderer.init(m_view, error);
    if (!m_renderer_ok) {
        MessageBoxW(m_main, toWide(error).c_str(), m_title.c_str(), MB_ICONERROR);
        DestroyWindow(m_main);
        return 1;
    }
    RECT rc;
    GetClientRect(m_view, &rc);
    m_renderer.resize(rc.right, rc.bottom);
    updateTitle();
    updateStatus();

    // Command line: [graph.json] [--scene <node type>] [--play]. Without a graph — the first demo scene
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool play = false;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--play") {
            play = true;
        } else if (arg == L"--scene" && i + 1 < argc) {
            const std::wstring wide = argv[++i];
            std::string name; // node types are ASCII
            for (wchar_t c : wide)
                name += static_cast<char>(c);
            int count = 0;
            const scenes::DemoScene* list = scenes::demoScenes(&count);
            const scenes::DemoScene* found = scenes::findDemoScene(name.c_str());
            if (found)
                openScene(static_cast<int>(found - list));
            else
                MessageBoxW(m_main, (L"No demo scene for the node type " + wide).c_str(), m_title.c_str(),
                    MB_ICONWARNING);
        } else {
            openFile(arg);
        }
    }
    LocalFree(argv);
    if (!m_scene.isLoaded())
        openScene(0);
    if (play && m_scene.isLoaded())
        setPlaying(true);

    ShowWindow(m_main, SW_SHOW);
    SetFocus(m_view);

    MSG msg;
    for (;;) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return static_cast<int>(msg.wParam);
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        tick();
        // Wait for messages: while cooking — poll its state every millisecond, while playing — until the frame
        // changes, otherwise — without a limit
        const DWORD wait = m_scene.isCooking() ? 1 : (m_playing ? playWaitMs() : INFINITE);
        MsgWaitForMultipleObjectsEx(0, nullptr, wait, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
}

bool DemoApp::createWindows()
{
    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = mainProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = MAIN_CLASS;
    RegisterClassExW(&wc);

    // View window: its own DC (OpenGL needs CS_OWNDC), the background is not erased — the renderer draws everything
    WNDCLASSEXW vc{sizeof(vc)};
    vc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    vc.lpfnWndProc = viewProc;
    vc.hInstance = inst;
    vc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    vc.lpszClassName = VIEW_CLASS;
    RegisterClassExW(&vc);

    m_main = CreateWindowExW(0, MAIN_CLASS, m_title.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT,
        CW_USEDEFAULT, 1280, 800, nullptr, nullptr, inst, this);
    if (!m_main)
        return false;
    m_dpi = static_cast<int>(GetDpiForWindow(m_main));
    // Window size — in logical pixels
    SetWindowPos(m_main, nullptr, 0, 0, scale(1280), scale(800), SWP_NOMOVE | SWP_NOZORDER);
    DragAcceptFiles(m_main, TRUE);

    m_view = CreateWindowExW(0, VIEW_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, 1, 1,
        m_main, nullptr, inst, this);
    m_open_btn = CreateWindowExW(0, L"BUTTON", L"Open...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 1,
        1, m_main, reinterpret_cast<HMENU>(ID_OPEN), inst, nullptr);
    // Demo scenes built in code: one per supported node type (the height of a drop-down list is that of the list)
    m_scene_combo = CreateWindowExW(0, WC_COMBOBOXW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0, 0, 1, 1, m_main,
        reinterpret_cast<HMENU>(ID_SCENE), inst, nullptr);
    int sceneCount = 0;
    const scenes::DemoScene* sceneList = scenes::demoScenes(&sceneCount);
    for (int i = 0; i < sceneCount; ++i) {
        const std::string type = sceneList[i].nodeType;
        SendMessageW(m_scene_combo, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(std::wstring(type.begin(), type.end()).c_str()));
    }
    SendMessageW(m_scene_combo, CB_SETMINVISIBLE, 20, 0);
    m_play_btn = CreateWindowExW(0, L"BUTTON", L"Play", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 1, 1,
        m_main, reinterpret_cast<HMENU>(ID_PLAY), inst, nullptr);
    m_slider = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 0,
        0, 1, 1, m_main, reinterpret_cast<HMENU>(ID_SLIDER), inst, nullptr);
    m_status = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX, 0, 0,
        1, 1, m_main, reinterpret_cast<HMENU>(ID_STATUS), inst, nullptr);
    EnableWindow(m_play_btn, FALSE);
    EnableWindow(m_slider, FALSE);
    createFont();
    layout();
    return m_view != nullptr;
}

void DemoApp::createFont()
{
    if (m_font)
        DeleteObject(m_font);
    NONCLIENTMETRICSW ncm{sizeof(ncm)};
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, static_cast<UINT>(m_dpi));
    m_font = CreateFontIndirectW(&ncm.lfMessageFont);
    for (HWND h : {m_open_btn, m_scene_combo, m_play_btn, m_slider, m_status})
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
}

void DemoApp::layout()
{
    if (!m_view)
        return;
    RECT rc;
    GetClientRect(m_main, &rc);
    const int w = rc.right, h = rc.bottom;
    const int bar = scale(40), pad = scale(6), btnW = scale(84), comboW = scale(160), statusW = scale(420);
    const int viewH = (std::max) (0, h - bar);
    const int y = viewH + pad, ch = bar - 2 * pad;
    MoveWindow(m_view, 0, 0, w, viewH, TRUE);
    MoveWindow(m_open_btn, pad, y, btnW, ch, TRUE);
    // A combo box is as high as its font makes it: centered on the bar; the given height is that of the drop-down list
    RECT cr{};
    GetWindowRect(m_scene_combo, &cr);
    const int comboH = cr.bottom - cr.top;
    MoveWindow(m_scene_combo, 2 * pad + btnW, viewH + (bar - comboH) / 2, comboW, scale(400), TRUE);
    MoveWindow(m_play_btn, 3 * pad + btnW + comboW, y, btnW, ch, TRUE);
    const int sliderX = 4 * pad + 2 * btnW + comboW;
    const int sliderW = (std::max) (scale(60), w - sliderX - statusW - 2 * pad);
    MoveWindow(m_slider, sliderX, y, sliderW, ch, TRUE);
    MoveWindow(m_status, sliderX + sliderW + pad, y, statusW, ch, TRUE);
}

LRESULT CALLBACK DemoApp::mainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCCREATE)
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
    auto* app = reinterpret_cast<DemoApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return app ? app->onMain(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK DemoApp::viewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCCREATE)
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
    auto* app = reinterpret_cast<DemoApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return app ? app->onView(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT DemoApp::onMain(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZE:
        layout();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        mmi->ptMinTrackSize = {scale(640), scale(360)};
        return 0;
    }
    case WM_DPICHANGED: {
        m_dpi = HIWORD(wp);
        createFont();
        const RECT* r = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        layout();
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_SCENE) {
            // Only a chosen item: the combo box also reports focus and opening of its list
            if (HIWORD(wp) == CBN_SELCHANGE) {
                const int index = static_cast<int>(SendMessageW(m_scene_combo, CB_GETCURSEL, 0, 0));
                if (index >= 0 && index != m_scene_index)
                    openScene(index);
            } else if (HIWORD(wp) == CBN_CLOSEUP) {
                SetFocus(m_view);
            }
            return 0;
        }
        if (LOWORD(wp) == ID_OPEN)
            openDialog();
        else if (LOWORD(wp) == ID_PLAY)
            setPlaying(!m_playing);
        SetFocus(m_view);
        return 0;
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lp) == m_slider) {
            const int pos = static_cast<int>(SendMessageW(m_slider, TBM_GETPOS, 0, 0));
            requestFrame(pos);
            if (m_playing)
                restartPlayClock();
            if (LOWORD(wp) == TB_ENDTRACK)
                SetFocus(m_view);
        }
        return 0;
    case WM_DROPFILES: {
        HDROP drop = reinterpret_cast<HDROP>(wp);
        wchar_t path[MAX_PATH * 4];
        if (DragQueryFileW(drop, 0, path, static_cast<UINT>(std::size(path))))
            openFile(path);
        DragFinish(drop);
        return 0;
    }
    case WM_MOUSEWHEEL:
        // The wheel goes to the focused window — forward it to the view
        return SendMessageW(m_view, msg, wp, lp);
    case WM_KEYDOWN:
        onKey(wp);
        return 0;
    case WM_DESTROY:
        setPlaying(false);
        // Renderer resources (Vulkan surface, OpenGL DC) — while the view window is still alive
        if (m_renderer_ok)
            m_renderer.shutdown();
        m_renderer_ok = false;
        if (m_font)
            DeleteObject(m_font);
        m_font = nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT DemoApp::onView(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        if (m_renderer_ok)
            m_renderer.resize(LOWORD(lp), HIWORD(lp));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        if (m_renderer_ok)
            m_renderer.render(viewParams());
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        SetFocus(hwnd);
        SetCapture(hwnd);
        m_drag_button = msg == WM_LBUTTONDOWN ? 1 : (msg == WM_MBUTTONDOWN ? 2 : 3);
        // Shift + left — pan, Ctrl + left — zoom (for touchpads without a middle button)
        if (m_drag_button == 1 && (wp & MK_SHIFT))
            m_drag_button = 2;
        else if (m_drag_button == 1 && (wp & MK_CONTROL))
            m_drag_button = 3;
        m_drag_start = {static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        m_drag_button = 0;
        ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        m_drag_button = 0;
        return 0;
    case WM_MOUSEMOVE: {
        if (!m_drag_button)
            return 0;
        const POINT p{static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
        const float dx = float(p.x - m_drag_start.x), dy = float(p.y - m_drag_start.y);
        m_drag_start = p;
        const float k = 96.0f / float(m_dpi);
        if (m_drag_button == 1) {
            m_yaw -= dx * 0.008f * k;
            m_pitch = (std::clamp) (m_pitch + dy * 0.008f * k, -1.55f, 1.55f);
        } else if (m_drag_button == 2) {
            // Pan the target in the screen plane: a pixel is a fraction of the visible height at the target distance
            RECT rc;
            GetClientRect(hwnd, &rc);
            const float perPixel = 2.0f * m_distance * std::tan(FOVY * 0.5f) / float((std::max) (1L, rc.bottom));
            const Vec3 f = normalize(m_target - eye());
            const Vec3 right = normalize(cross(f, {0, 1, 0}));
            const Vec3 up = cross(right, f);
            m_target = m_target - right * (dx * perPixel) + up * (dy * perPixel);
        } else {
            m_distance *= std::pow(ZOOM_PER_NOTCH, dy * k / 40.0f);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEWHEEL:
        m_distance *= std::pow(ZOOM_PER_NOTCH, -float(GET_WHEEL_DELTA_WPARAM(wp)) / WHEEL_DELTA);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_KEYDOWN:
        onKey(wp);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void DemoApp::onKey(WPARAM key)
{
    if (key == 'F') {
        frameAll();
        InvalidateRect(m_view, nullptr, FALSE);
    } else if (key == VK_SPACE) {
        setPlaying(!m_playing);
    } else if (key == 'O' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        openDialog();
    } else if (key == VK_PRIOR || key == VK_NEXT) {
        // Page Up / Page Down — the previous / next demo scene
        int count = 0;
        scenes::demoScenes(&count);
        const int current = m_scene_index < 0 ? (key == VK_NEXT ? -1 : 0) : m_scene_index;
        openScene((current + (key == VK_NEXT ? 1 : count - 1)) % count);
    } else if ((key == VK_LEFT || key == VK_RIGHT) && m_scene.isLoaded()) {
        setPlaying(false);
        const int range = m_scene.lastFrame() - m_scene.firstFrame() + 1;
        const int f = m_requested_frame - m_scene.firstFrame() + (key == VK_RIGHT ? 1 : range - 1);
        requestFrame(m_scene.firstFrame() + f % range);
    }
}

void DemoApp::openDialog()
{
    wchar_t path[MAX_PATH * 4] = L"";
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = m_main;
    ofn.lpstrFilter = L"GeoNodus graph (*.json)\0*.json\0All files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = static_cast<DWORD>(std::size(path));
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn))
        openFile(path);
}

void DemoApp::openFile(const std::wstring& path)
{
    setPlaying(false);
    std::string error;
    if (!m_scene.load(path, error)) {
        MessageBoxW(m_main, (L"Cannot load " + path + L"\n\n" + toWide(error)).c_str(), m_title.c_str(), MB_ICONERROR);
        return;
    }
    m_file = path;
    m_scene_index = -1;
    SendMessageW(m_scene_combo, CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
    sceneReplaced();
}

void DemoApp::openScene(int index)
{
    int count = 0;
    const scenes::DemoScene* list = scenes::demoScenes(&count);
    if (index < 0 || index >= count)
        return;
    const bool wasPlaying = m_playing;
    setPlaying(false);
    std::string error;
    const bool ok = m_scene.build(list[index], error);
    m_file.clear();
    m_scene_index = index;
    SendMessageW(m_scene_combo, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
    sceneReplaced();
    if (!ok) {
        const std::string type = list[index].nodeType;
        MessageBoxW(m_main,
            (L"The scene \"" + std::wstring(type.begin(), type.end()) + L"\" was built with problems:\n\n"
                + toWide(error))
                .c_str(),
            m_title.c_str(), MB_ICONWARNING);
    }
    // Switching scenes while playing keeps playing
    if (wasPlaying)
        setPlaying(true);
}

void DemoApp::sceneReplaced()
{
    m_framed = false;
    m_geometry.clear();
    m_renderer.setGeometry(m_geometry);
    SendMessageW(m_slider, TBM_SETRANGEMIN, FALSE, m_scene.firstFrame());
    SendMessageW(m_slider, TBM_SETRANGEMAX, TRUE, m_scene.lastFrame());
    EnableWindow(m_play_btn, TRUE);
    EnableWindow(m_slider, TRUE);
    requestFrame(m_scene.startFrame());
    updateTitle();
    updateStatus();
}

void DemoApp::setPlaying(bool play)
{
    if (play && !m_scene.isLoaded())
        return;
    m_playing = play;
    if (play)
        restartPlayClock();
    SetWindowTextW(m_play_btn, play ? L"Pause" : L"Play");
}

void DemoApp::restartPlayClock()
{
    m_play_start_frame = m_requested_frame;
    m_play_start_ms = GetTickCount64();
}

void DemoApp::requestFrame(int frame)
{
    m_requested_frame = frame;
    if (static_cast<int>(SendMessageW(m_slider, TBM_GETPOS, 0, 0)) != frame)
        SendMessageW(m_slider, TBM_SETPOS, TRUE, frame);
}

DWORD DemoApp::playWaitMs() const
{
    const double frameMs = 1000.0 / m_scene.fps();
    const double elapsed = double(GetTickCount64() - m_play_start_ms);
    const double next = (std::floor(elapsed / frameMs) + 1.0) * frameMs;
    return static_cast<DWORD>((std::max) (1.0, next - elapsed));
}

void DemoApp::tick()
{
    if (!m_scene.isLoaded())
        return;
    if (m_playing) {
        const int first = m_scene.firstFrame();
        const int range = m_scene.lastFrame() - first + 1;
        const long long elapsed = static_cast<long long>(
            double(GetTickCount64() - m_play_start_ms) * m_scene.fps() / 1000.0);
        const int frame = first + static_cast<int>((m_play_start_frame - first + elapsed) % range);
        if (frame != m_requested_frame)
            requestFrame(frame);
    }

    if (m_scene.isCooking()) {
        if (m_scene.pollCook(m_geometry)) {
            m_renderer.setGeometry(m_geometry);
            if (!m_framed && m_geometry.hasBounds) {
                frameAll();
                m_framed = true;
            }
            updateStatus();
            InvalidateRect(m_view, nullptr, FALSE);
        }
    }
    // A new cook only after the previous one finished: with slow cooks the frames in between are skipped
    if (!m_scene.isCooking() && m_scene.cookedFrame() != m_requested_frame)
        m_scene.startCook(m_requested_frame);
}

void DemoApp::updateTitle()
{
    std::wstring t = m_title + (m_scene.isDemoEdition() ? L" (demo edition)" : L"") + L" — "
                     + m_renderer.description();
    if (!m_file.empty()) {
        const size_t slash = m_file.find_last_of(L"\\/");
        t += L" — " + (slash == std::wstring::npos ? m_file : m_file.substr(slash + 1));
    } else if (m_scene_index >= 0) {
        int count = 0;
        const scenes::DemoScene& s = scenes::demoScenes(&count)[m_scene_index];
        const std::string text = std::string(s.nodeType) + ": " + s.description;
        t += L" — " + toWide(text);
    }
    SetWindowTextW(m_main, t.c_str());
}

void DemoApp::updateStatus()
{
    wchar_t text[256];
    if (!m_scene.isLoaded()) {
        swprintf(text, std::size(text),
            L"Choose a node type or open / drop a graph .json.  LMB rotate, wheel / RMB zoom, MMB / Shift+LMB pan, F "
            L"frame");
    } else {
        swprintf(text, std::size(text), L"Frame %d   cook %.1f ms   read %.1f ms   %zu tris   %zu lines   %zu points",
            m_scene.cookedFrame(), m_scene.lastCookMs(), m_scene.lastFetchMs(), m_geometry.triangles.size() / 3,
            m_geometry.lines.size() / 2, m_geometry.points.size());
    }
    SetWindowTextW(m_status, text);
}

Vec3 DemoApp::eye() const
{
    const Vec3 dir{std::cos(m_pitch) * std::sin(m_yaw), std::sin(m_pitch), std::cos(m_pitch) * std::cos(m_yaw)};
    return m_target + dir * m_distance;
}

void DemoApp::frameAll()
{
    if (!m_geometry.hasBounds)
        return;
    const float* a = m_geometry.boundsMin;
    const float* b = m_geometry.boundsMax;
    m_target = {(a[0] + b[0]) * 0.5f, (a[1] + b[1]) * 0.5f, (a[2] + b[2]) * 0.5f};
    const Vec3 size{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const float radius = (std::max) (0.5f * std::sqrt(dot(size, size)), 1e-3f);
    m_distance = radius / std::sin(FOVY * 0.5f) * 1.05f;
}

ViewParams DemoApp::viewParams() const
{
    ViewParams vp;
    vp.view = lookAt(eye(), m_target, {0, 1, 0});
    vp.fovy = FOVY;
    // Near and far planes from the scene bounds, so that nothing is clipped at any distance
    float radius = m_distance;
    float centerDist = m_distance;
    if (m_geometry.hasBounds) {
        const float* a = m_geometry.boundsMin;
        const float* b = m_geometry.boundsMax;
        const Vec3 c{(a[0] + b[0]) * 0.5f, (a[1] + b[1]) * 0.5f, (a[2] + b[2]) * 0.5f};
        const Vec3 size{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        radius = (std::max) (0.5f * std::sqrt(dot(size, size)), 1e-3f);
        const Vec3 d = c - eye();
        centerDist = std::sqrt(dot(d, d));
    }
    vp.zFar = (std::max) (centerDist + radius * 1.1f, 1e-2f);
    vp.zNear = (std::max) (centerDist - radius * 1.1f, vp.zFar * 1e-4f);
    return vp;
}

} // namespace

int runDemo(IRenderer& renderer, const wchar_t* title)
{
    DemoApp app(renderer, title);
    return app.run();
}

} // namespace demo
