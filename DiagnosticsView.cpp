#include "DiagnosticsView.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace {

    namespace {

        /**
         * Colors and labels used by the diagnostics view.
         *
         * These constants live in an anonymous namespace to keep them translation-unit local.
         */

         /** Background color for the diagnostics display (RGB). */
        const COLORREF Background = RGB(13, 20, 29);

        /** Default text color used when drawing labels and values. */
        const COLORREF TextColor = RGB(234, 241, 247);

        /**
         * Palette of colors used to draw individual traces/lines in the diagnostics graph.
         * The index into this array corresponds to the same index in `Names`.
         */
        const COLORREF Colors[] = {
            RGB(52,211,202), RGB(52,211,202), RGB(251,191,36),
            RGB(73,150,255), RGB(73,150,255), RGB(248,113,113), RGB(195,132,252), RGB(195,132,252),
            RGB(251,191,36), RGB(251,191,36), RGB(251,191,36)
        };

        /**
         * Human-readable labels for each telemetry trace shown in the diagnostics view.
         * These must remain in sync with the entries/indices expected elsewhere in the view.
         */
        const char* Names[] = {
            "PC sent position", "PC sent speed (magnitude)", "CMMT target",
            "CMMT actual position", "CMMT actual velocity", "Following error", "PLC received position", "PLC received speed",
            "PLC CSP command position", "PLC CSP velocity", "PLC CSP acceleration"
        };

        /**
         * Convert a floating-point value to a string using fixed notation with three
         * decimal places (e.g., 12.345).
         *
         * @param value The double value to format.
         * @return A std::string containing the formatted number.
         */
        std::string Number(double value) {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3) << value;
            return stream.str();
        }

        /**
         * Draw an ASCII text string onto the provided device context at the given
         * coordinates using the supplied color (defaults to TextColor).
         *
         * @param dc    Device context handle to draw into.
         * @param x     X coordinate in device units.
         * @param y     Y coordinate in device units.
         * @param text  The text to draw.
         * @param color Optional COLORREF to use for the text; defaults to TextColor.
         */
        void Text(HDC dc, int x, int y, const std::string& text, COLORREF color = TextColor) {
            SetTextColor(dc, color);
            TextOutA(dc, x, y, text.c_str(), static_cast<int>(text.size()));
        }
    }
}

// Register the window class and create the diagnostics child window.
bool DiagnosticsView::Create(HWND parent, HINSTANCE instance) {
    WNDCLASSW type{};
    type.lpfnWndProc = Procedure;
    type.hInstance = instance;
    type.hCursor = LoadCursor(nullptr, IDC_CROSS);
    type.lpszClassName = L"FlightDiagnostics";
    RegisterClassW(&type);
    // Pass 'this' so Procedure can bind the window to this instance.
    window_ = CreateWindowExW(WS_EX_CONTROLPARENT, type.lpszClassName, L"Diagnostics",
        WS_CHILD | WS_CLIPCHILDREN, 0, 82, 1000, 700, parent, nullptr, instance, this);
    return window_ != nullptr;
}

// Show/hide the view; start a 200 ms refresh timer while visible.
void DiagnosticsView::Show(bool visible, int width, int height) {
    Resize(width, height);
    ShowWindow(window_, visible ? SW_SHOW : SW_HIDE);
    if (visible) {
        RefreshFiles();
        SetTimer(window_, 1, 200, nullptr);
        Update();
    }
    else KillTimer(window_, 1);
}

// Fit the view below the 82 px header and place the details box at the bottom.
void DiagnosticsView::Resize(int width, int height) {
    if (window_) SetWindowPos(window_, HWND_TOP, 0, 82, width, (std::max)(1, height - 82), 0);
    if (details_) SetWindowPos(details_, nullptr, 24, (std::max)(190, height - 82 - 150), width - 48, 138, SWP_NOZORDER);
}

// Fill the session combo box with telemetry .jsonl files from the logs folder.
void DiagnosticsView::RefreshFiles() {
    SendMessage(files_, CB_RESETCONTENT, 0, 0);
    WIN32_FIND_DATAA entry{};
    const HANDLE search = FindFirstFileA("logs/telemetry/*.jsonl", &entry);
    if (search == INVALID_HANDLE_VALUE) return;
    do {
        // Skip directories, add files only.
        if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            SendMessageA(files_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(entry.cFileName));
    } while (FindNextFileA(search, &entry));
    FindClose(search);
    SendMessage(files_, CB_SETCURSEL, 0, 0);
}

// Static window procedure: maps the HWND to its DiagnosticsView and forwards messages.
LRESULT CALLBACK DiagnosticsView::Procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<DiagnosticsView*>(GetWindowLongPtr(window, GWLP_USERDATA));
    // On creation, store the instance pointer in the window's user data.
    if (message == WM_NCCREATE) {
        self = static_cast<DiagnosticsView*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->Handle(message, wParam, lParam) : DefWindowProc(window, message, wParam, lParam);
}

// Per-instance message handler.
LRESULT DiagnosticsView::Handle(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        // Create font and all child controls.
    case WM_CREATE: {
        font_ = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        // Helper: create a child control with the shared font (combo boxes get a taller drop-down).
        const auto control = [&](const wchar_t* type, const wchar_t* text, DWORD style, int x, int y, int width, int id) {
            HWND child = CreateWindowW(type, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                x, y, width, type[0] == L'C' ? 240 : 30, window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                GetModuleHandle(nullptr), nullptr);
            SendMessage(child, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
            return child;
            };
        // Actuator selector (1-6).
        axis_ = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST, 24, 8, 110, 1);
        for (int i = 1; i <= 6; ++i) {
            const auto name = L"Actuator " + std::to_wstring(i);
            SendMessageW(axis_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
        }
        SendMessage(axis_, CB_SETCURSEL, 0, 0);
        // View navigation buttons (IDs 2-7).
        control(L"BUTTON", L"Live", 0, 148, 8, 70, 2);
        control(L"BUTTON", L"Pause view", 0, 228, 8, 100, 3);
        control(L"BUTTON", L"< Earlier", 0, 338, 8, 90, 4);
        control(L"BUTTON", L"Later >", 0, 438, 8, 90, 5);
        control(L"BUTTON", L"Zoom +", 0, 538, 8, 80, 6);
        control(L"BUTTON", L"Zoom -", 0, 628, 8, 80, 7);
        // Saved session controls (IDs 8-11).
        files_ = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_SORT, 24, 48, 450, 8);
        control(L"BUTTON", L"Load session", 0, 488, 48, 120, 9);
        control(L"BUTTON", L"Refresh files", 0, 620, 48, 110, 10);
        control(L"BUTTON", L"Packet details", 0, 742, 48, 120, 11);
        // Read-only multiline box for raw packet details (hidden by default).
        details_ = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            24, 540, 1000, 138, window_, nullptr, GetModuleHandle(nullptr), nullptr);
        SendMessage(details_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        SendMessage(details_, EM_SETLIMITTEXT, 65536, 0);
        return 0;
    }
                  // Handle control input.
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        // Actuator selection changed.
        if (id == 1 && HIWORD(wParam) == CBN_SELCHANGE) actuator_ = static_cast<int>(SendMessage(axis_, CB_GETCURSEL, 0, 0));
        if (HIWORD(wParam) == BN_CLICKED) {
            // Live: follow latest data and clear cursor.
            if (id == 2) { live_ = true; message_.clear(); cursor_ = -1; }
            // Pause: freeze the view only.
            if (id == 3) { live_ = false; message_ = "View paused; recording and control continue"; }
            // Earlier/Later: pan by half the visible span.
            if (id == 4 || id == 5) { live_ = false; end_ += id == 4 ? -span_ / 2 : span_ / 2; }
            // Zoom in/out: halve/double span, clamped to 1-3600 s.
            if (id == 6) span_ = (std::max)(1.0, span_ / 2);
            if (id == 7) span_ = (std::min)(3600.0, span_ * 2);
            if (id == 10) RefreshFiles();
            // Toggle packet details box and re-layout.
            if (id == 11) {
                showDetails_ = !showDetails_;
                ShowWindow(details_, showDetails_ ? SW_SHOW : SW_HIDE);
                RECT client{}; GetClientRect(window_, &client);
                Resize(client.right, client.bottom + 82);
            }
            // Load selected session asynchronously (ignored if a load is already running).
            if (id == 9 && !loading_.valid()) {
                const auto selected = SendMessage(files_, CB_GETCURSEL, 0, 0);
                if (selected != CB_ERR) {
                    char filename[MAX_PATH]{};
                    SendMessageA(files_, CB_GETLBTEXT, selected, reinterpret_cast<LPARAM>(filename));
                    const std::string path = std::string("logs/telemetry/") + filename;
                    message_ = "Loading session...";
                    loading_ = std::async(std::launch::async, [path] {
                        Replay replay;
                        TelemetryStore::Load(path, replay.frames, replay.error);
                        return replay;
                        });
                }
            }
        }
        Update();
        return 0;
    }
                   // Periodic refresh.
    case WM_TIMER: Update(); return 0;
        // Click inside the plot area: place the inspection cursor at that time.
    case WM_LBUTTONDOWN: {
        RECT rect{};
        GetClientRect(window_, &rect);
        const int x = static_cast<short>(LOWORD(lParam));
        if (x >= 100 && x <= rect.right - 30)
            cursor_ = end_ - span_ + span_ * (x - 100) / (rect.right - 130.0);
        Update();
        return 0;
    }
    case WM_PAINT: Paint(); return 0;
        // Skip background erase; Paint double-buffers to avoid flicker.
    case WM_ERASEBKGND: return 1;
        // Stop timer and free the font.
    case WM_DESTROY: KillTimer(window_, 1); DeleteObject(font_); return 0;
    }
    return DefWindowProc(window_, message, wParam, lParam);
}

// Refresh data (finished load or live store), update details text, and request a repaint.
void DiagnosticsView::Update() {
    // Collect a finished async session load without blocking.
    if (loading_.valid() && loading_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto replay = loading_.get();
        if (replay.error.empty()) {
            frames_ = std::move(replay.frames);
            live_ = false;
            end_ = frames_.empty() ? span_ : frames_.back().seconds;
            cursor_ = -1;
            message_ = "Saved session; click a graph to inspect values";
        }
        else message_ = replay.error;
    }
    // In live mode, pull recent frames and track the current time.
    if (live_) {
        frames_ = store_.Recent();
        end_ = store_.Status().elapsedSeconds;
    }
    // Ensure the window never starts before time 0.
    end_ = (std::max)(span_, end_);
    if (showDetails_) {
        // Inspect at the cursor if visible, otherwise at the right edge.
        const double inspect = cursor_ >= end_ - span_ && cursor_ <= end_ ? cursor_ : end_;
        // Find the last feedback-type packet at or before the inspect time.
        const TelemetryFrame* packet = nullptr;
        for (const auto& frame : frames_) {
            if (frame.seconds > inspect) break;
            if (frame.kind == "feedback" || frame.kind == "invalid-diagnostics" || frame.kind == "rejected-feedback") packet = &frame;
        }
        const std::string detail = packet ? "PC receipt " + Number(packet->seconds) + " s | connection "
            + std::to_string(packet->connection) + " | " + packet->kind + "\r\n" + packet->payload : "No feedback packet at this time";
        SetWindowTextA(details_, detail.c_str());
    }
    InvalidateRect(window_, nullptr, FALSE);
}

// Draw one graph panel: title, legend values, grid, signal traces, and cursor line.
void DiagnosticsView::Graph(HDC dc, RECT rect, const char* title, const std::vector<std::size_t>& signals) {
    Text(dc, 24, rect.top, title);
    int legend = 280;
    const double inspect = cursor_ >= end_ - span_ && cursor_ <= end_ ? cursor_ : end_;
    // Legend: show each signal's latest value at the inspect time.
    for (auto signal : signals) {
        const TelemetryFrame* latest = nullptr;
        for (const auto& frame : frames_) {
            if (frame.seconds > inspect) break;
            // A (re)connect invalidates older values.
            if (frame.kind == "disconnect" || frame.kind == "connect") latest = nullptr;
            // Signals 0-1 come from sent frames, 2+ from feedback frames.
            if ((frame.kind == "send" && signal < 2) || (frame.kind == "feedback" && signal >= 2)) {
                // For target (2) and following error (5), keep the last finite value.
                if ((signal != 2 && signal != 5) || std::isfinite(frame.values[signal][actuator_])) latest = &frame;
            }
        }
        const double value = latest ? latest->values[signal][actuator_] : NAN;
        // Signals 2 and 5 also show how old the value is.
        const auto label = std::string(Names[signal]) + ": " + (std::isfinite(value) ? Number(value) : "N/A")
            + (latest && (signal == 2 || signal == 5) ? " (" + Number(inspect - latest->seconds) + "s old)" : "");
        Text(dc, legend, rect.top, label, Colors[signal]);
        legend += 255;
        // Wrap legend to a new line when out of space.
        if (legend + 240 > rect.right) { legend = 280; rect.top += 20; }
    }
    // Shrink rect to the plot area.
    rect.top += 30;
    rect.left = 100;
    rect.right -= 30;
    rect.bottom -= 24;
    // Find the min/max of visible samples for Y scaling.
    double low = 0, high = 0;
    bool any = false;
    for (const auto& frame : frames_) {
        if (frame.seconds < end_ - span_ || frame.seconds > end_) continue;
        for (auto signal : signals) {
            const double value = frame.values[signal][actuator_];
            if (std::isfinite(value)) {
                low = any ? (std::min)(low, value) : value;
                high = any ? (std::max)(high, value) : value;
                any = true;
            }
        }
    }
    // Add 8% padding (at least 0.1) above and below.
    const double margin = (std::max)(0.1, (high - low) * 0.08);
    low -= margin; high += margin;
    // Grid: 3 horizontal lines with Y labels, 5 vertical lines with time labels.
    HPEN grid = CreatePen(PS_SOLID, 1, RGB(49, 65, 82));
    HGDIOBJ old = SelectObject(dc, grid);
    for (int row = 0; row <= 2; ++row) {
        const int y = rect.top + (rect.bottom - rect.top) * row / 2;
        MoveToEx(dc, rect.left, y, nullptr); LineTo(dc, rect.right, y);
        Text(dc, 24, y - 8, any ? Number(high - (high - low) * row / 2) : "--");
    }
    for (int column = 0; column <= 4; ++column) {
        const int x = rect.left + (rect.right - rect.left) * column / 4;
        MoveToEx(dc, x, rect.top, nullptr); LineTo(dc, x, rect.bottom);
        Text(dc, x - 18, rect.bottom + 5, Number(end_ - span_ + span_ * column / 4) + " s");
    }
    SelectObject(dc, old); DeleteObject(grid);
    if (!any) Text(dc, rect.left + 20, rect.top + 15, "No samples available in this interval");
    // Keep every sample, including spikes. Break lines at missing data, reconnects,
    // or gaps over 250 ms rather than visually inventing continuous feedback.
    for (auto signal : signals) {
        HPEN pen = CreatePen(PS_SOLID, 1, Colors[signal]);
        old = SelectObject(dc, pen);
        bool connected = false;
        double previous = -1;
        std::uint64_t connection = 0;
        for (const auto& frame : frames_) {
            if (frame.kind == "connect" || frame.kind == "disconnect") connected = false;
            if (frame.seconds < end_ - span_ || frame.seconds > end_) continue;
            if ((signal < 2 && frame.kind != "send") || (signal >= 2 && frame.kind != "feedback")) continue;
            const double value = frame.values[signal][actuator_];
            if (!std::isfinite(value)) { connected = false; continue; }
            // Map time/value to pixel coordinates.
            const int x = rect.left + static_cast<int>((frame.seconds - end_ + span_) / span_ * (rect.right - rect.left));
            const int y = rect.bottom - static_cast<int>((value - low) / (high - low) * (rect.bottom - rect.top));
            if (connected && frame.connection == connection && frame.seconds - previous <= 0.25) LineTo(dc, x, y);
            else MoveToEx(dc, x, y, nullptr);
            // Mark each sample; signals 2 and 5 get a larger dot.
            SetPixelV(dc, x, y, Colors[signal]);
            if (signal == 2 || signal == 5) {
                SetPixelV(dc, x + 1, y, Colors[signal]);
                SetPixelV(dc, x, y + 1, Colors[signal]);
            }
            connected = true; previous = frame.seconds; connection = frame.connection;
        }
        SelectObject(dc, old); DeleteObject(pen);
    }
    // Draw dotted cursor line if it's in view.
    if (cursor_ >= end_ - span_ && cursor_ <= end_) {
        HPEN pen = CreatePen(PS_DOT, 1, TextColor);
        old = SelectObject(dc, pen);
        const int x = rect.left + static_cast<int>((cursor_ - end_ + span_) / span_ * (rect.right - rect.left));
        MoveToEx(dc, x, rect.top, nullptr); LineTo(dc, x, rect.bottom);
        SelectObject(dc, old); DeleteObject(pen);
    }
}

// Paint the whole view into an off-screen bitmap, then copy it to the window.
void DiagnosticsView::Paint() {
    PAINTSTRUCT paint{};
    HDC target = BeginPaint(window_, &paint);
    RECT rect{}; GetClientRect(window_, &rect);
    // Set up memory DC for double buffering.
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, rect.right, rect.bottom);
    auto oldBitmap = SelectObject(dc, bitmap);
    HBRUSH background = CreateSolidBrush(Background);
    FillRect(dc, &rect, background); DeleteObject(background);
    auto oldFont = SelectObject(dc, font_); SetBkMode(dc, TRANSPARENT);
    // Status header: recording state, dropped events, mode, and help text.
    const auto status = store_.Status();
    Text(dc, 24, 92, std::string(status.recording ? "Recording: " : "NOT RECORDING: ") + status.file);
    Text(dc, 24, 114, "Lost recording events: " + std::to_string(status.dropped) + "  " + status.error, RGB(251, 191, 36));
    Text(dc, 24, 136, (live_ ? "LIVE  |  " : "REVIEW  |  ") + message_);
    Text(dc, 24, 158, "PC time on X axis. Native controller units; confirm scaling. N/A = unavailable. Click to inspect.");
    // Split remaining height into 4 graph rows (minus details box if shown).
    const int top = 190;
    const int height = (rect.bottom - top - 12 - (showDetails_ ? 150 : 0)) / 4;
    // Use CSP signals if any frame has a CSP command position for this actuator.
    bool csp = false;
    for (const auto& frame : frames_) {
        if (std::isfinite(frame.values[static_cast<std::size_t>(TelemetrySignal::CommandPosition)][actuator_])) {
            csp = true; break;
        }
    }
    Graph(dc, RECT{ 24, top, rect.right, top + height }, "POSITION (user units)", csp ? std::vector<std::size_t>{0, 3, 8} : std::vector<std::size_t>{ 0,3,6 });
    Graph(dc, RECT{ 24, top + height, rect.right, top + 2 * height }, "VELOCITY (user units/s)", csp ? std::vector<std::size_t>{4, 9} : std::vector<std::size_t>{ 1,4,7 });
    Graph(dc, RECT{ 24, top + 2 * height, rect.right, top + 3 * height }, csp ? "CSP ACCELERATION (mm/s^2)" : "DRIVE TARGET (raw 0x607A)", csp ? std::vector<std::size_t>{10} : std::vector<std::size_t>{ 2 });
    Graph(dc, RECT{ 24, top + 3 * height, rect.right, top + 4 * height }, "FOLLOWING ERROR (raw)", { 5 });
    // Copy buffer to screen and release GDI objects.
    BitBlt(target, 0, 0, rect.right, rect.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldFont); SelectObject(dc, oldBitmap);
    DeleteObject(bitmap); DeleteDC(dc); EndPaint(window_, &paint);
}