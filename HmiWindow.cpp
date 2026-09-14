#include "HmiWindow.h"

#include "DashboardModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#ifdef _MSC_VER
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#endif

namespace {
constexpr UINT_PTR RefreshTimerId = 1;
constexpr UINT RefreshIntervalMilliseconds = 50;
constexpr double Pi = 3.14159265358979323846;

const COLORREF Background = RGB(13, 20, 29);
const COLORREF Panel = RGB(21, 31, 43);
const COLORREF PanelRaised = RGB(27, 39, 53);
const COLORREF Border = RGB(49, 65, 82);
const COLORREF PrimaryText = RGB(234, 241, 247);
const COLORREF SecondaryText = RGB(145, 163, 180);
const COLORREF Cyan = RGB(52, 211, 202);
const COLORREF Green = RGB(74, 222, 128);
const COLORREF Amber = RGB(251, 191, 36);
const COLORREF Red = RGB(248, 113, 113);
const COLORREF Blue = RGB(73, 150, 255);

RECT MakeRect(int left, int top, int right, int bottom) {
    RECT result{ left, top, right, bottom };
    return result;
}

void FillRectangle(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void FillRoundedRectangle(HDC dc, const RECT& rect, COLORREF fill, COLORREF outline) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, outline);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, 14, 14);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawTextValue(
    HDC dc,
    HFONT font,
    const std::string& text,
    RECT rect,
    COLORREF color,
    UINT format = DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS) {
    HGDIOBJ previousFont = SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextA(dc, text.c_str(), static_cast<int>(text.size()), &rect, format);
    SelectObject(dc, previousFont);
}

std::string FormatNumber(double value, int precision = 1) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string FormatCount(std::uint64_t value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

std::string FormatElapsed(long long milliseconds) {
    const long long totalSeconds = std::max(0LL, milliseconds / 1000);
    const long long minutes = totalSeconds / 60;
    const long long seconds = totalSeconds % 60;
    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(2) << minutes
           << ':' << std::setw(2) << seconds;
    return stream.str();
}

std::string DataAge(long long now, long long sampleTime) {
    if (sampleTime < 0) {
        return "No data yet";
    }
    const long long age = std::max(0LL, now - sampleTime);
    if (age < 1000) {
        return FormatCount(static_cast<std::uint64_t>(age)) + " ms ago";
    }
    return FormatNumber(age / 1000.0, 1) + " s ago";
}

void DrawStatusCard(
    HDC dc,
    const RECT& rect,
    HFONT headingFont,
    HFONT bodyFont,
    const std::string& label,
    const std::string& status,
    const std::string& detail,
    COLORREF stateColor) {
    FillRoundedRectangle(dc, rect, Panel, Border);

    HBRUSH dotBrush = CreateSolidBrush(stateColor);
    HGDIOBJ previousBrush = SelectObject(dc, dotBrush);
    HGDIOBJ previousPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, rect.left + 18, rect.top + 19, rect.left + 30, rect.top + 31);
    SelectObject(dc, previousPen);
    SelectObject(dc, previousBrush);
    DeleteObject(dotBrush);

    DrawTextValue(dc, bodyFont, label, MakeRect(rect.left + 40, rect.top + 10, rect.right - 12, rect.top + 38), SecondaryText);
    DrawTextValue(dc, headingFont, status, MakeRect(rect.left + 18, rect.top + 36, rect.right - 12, rect.top + 62), PrimaryText);
    DrawTextValue(dc, bodyFont, detail, MakeRect(rect.left + 18, rect.top + 60, rect.right - 12, rect.bottom - 5), SecondaryText);
}

void DrawActuatorCard(
    HDC dc,
    const RECT& rect,
    HFONT headingFont,
    HFONT bodyFont,
    HFONT smallFont,
    int actuator,
    float current,
    float target,
    float speed,
    bool hasFeedback) {
    FillRoundedRectangle(dc, rect, Panel, Border);

    const COLORREF actuatorColor = hasFeedback ? Cyan : SecondaryText;
    DrawTextValue(dc, headingFont, "A" + std::to_string(actuator),
        MakeRect(rect.left + 14, rect.top + 8, rect.left + 56, rect.top + 36), actuatorColor);
    DrawTextValue(dc, headingFont, hasFeedback ? FormatNumber(current, 1) + " mm" : "--.- mm",
        MakeRect(rect.left + 58, rect.top + 8, rect.right - 12, rect.top + 36), PrimaryText,
        DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

    const int barLeft = rect.left + 14;
    const int barRight = rect.right - 14;
    const int barTop = rect.top + 45;
    const RECT track = MakeRect(barLeft, barTop, barRight, barTop + 8);
    FillRoundedRectangle(dc, track, RGB(38, 52, 67), RGB(38, 52, 67));

    if (hasFeedback) {
        const double currentRatio = std::max(0.0, std::min(1.0, current / 400.0));
        RECT fill = track;
        fill.right = fill.left + static_cast<int>((barRight - barLeft) * currentRatio);
        if (fill.right > fill.left) {
            FillRoundedRectangle(dc, fill, Cyan, Cyan);
        }

        const double targetRatio = std::max(0.0, std::min(1.0, target / 400.0));
        const int markerX = barLeft + static_cast<int>((barRight - barLeft) * targetRatio);
        HPEN markerPen = CreatePen(PS_SOLID, 2, Amber);
        HGDIOBJ oldPen = SelectObject(dc, markerPen);
        MoveToEx(dc, markerX, barTop - 3, nullptr);
        LineTo(dc, markerX, barTop + 11);
        SelectObject(dc, oldPen);
        DeleteObject(markerPen);
    }

    DrawTextValue(dc, smallFont, "TARGET  " + (hasFeedback ? FormatNumber(target, 1) : "--.-") + " mm",
        MakeRect(rect.left + 14, rect.top + 59, rect.right - 12, rect.top + 80), SecondaryText);
    DrawTextValue(dc, bodyFont, "SPEED  " + (hasFeedback ? FormatNumber(speed, 0) : "---") + " mm/s",
        MakeRect(rect.left + 14, rect.top + 80, rect.right - 12, rect.bottom - 5), PrimaryText);
}

COLORREF EventColor(DashboardEventLevel level) {
    switch (level) {
    case DashboardEventLevel::Warning:
        return Amber;
    case DashboardEventLevel::Error:
        return Red;
    default:
        return SecondaryText;
    }
}
}

HmiWindow::HmiWindow(
    DashboardModel& model,
    const std::string& targetName,
    const std::string& bindIp,
    int port)
    : model_(model),
      targetName_(targetName),
      endpoint_(bindIp + ':' + std::to_string(port)) {
}

HmiWindow::~HmiWindow() {
    if (window_ != nullptr) {
        DestroyWindow(window_);
    }
    DestroyFonts();
}

bool HmiWindow::Create(HINSTANCE instance, int showCommand) {
    const char* className = "FlightMotionHmiWindow";
    WNDCLASSEXA windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = className;

    if (!RegisterClassExA(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    CreateFonts();
    window_ = CreateWindowExA(
        0,
        className,
        "Flight Simulator Motion HMI",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        820,
        nullptr,
        nullptr,
        instance,
        this);
    if (window_ == nullptr) {
        DestroyFonts();
        return false;
    }

    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
    SetTimer(window_, RefreshTimerId, RefreshIntervalMilliseconds, nullptr);
    return true;
}

bool HmiWindow::IsOpen() const noexcept {
    return window_ != nullptr;
}

LRESULT CALLBACK HmiWindow::WindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    HmiWindow* instance = reinterpret_cast<HmiWindow*>(GetWindowLongPtr(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCT*>(lParam);
        instance = static_cast<HmiWindow*>(create->lpCreateParams);
        instance->window_ = window;
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(instance));
    }
    if (instance != nullptr) {
        return instance->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProc(window, message, wParam, lParam);
}

LRESULT HmiWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TIMER:
        if (wParam == RefreshTimerId) {
            InvalidateRect(window_, nullptr, FALSE);
        }
        return 0;
    case WM_PAINT:
        Paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_GETMINMAXINFO: {
        auto* information = reinterpret_cast<MINMAXINFO*>(lParam);
        information->ptMinTrackSize.x = 1120;
        information->ptMinTrackSize.y = 720;
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(window_);
        return 0;
    case WM_DESTROY:
        KillTimer(window_, RefreshTimerId);
        window_ = nullptr;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(window_, message, wParam, lParam);
    }
}

void HmiWindow::Paint() {
    PAINTSTRUCT paint{};
    HDC windowDc = BeginPaint(window_, &paint);
    RECT client{};
    GetClientRect(window_, &client);

    HDC memoryDc = CreateCompatibleDC(windowDc);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, client.right, client.bottom);
    HGDIOBJ previousBitmap = SelectObject(memoryDc, bitmap);
    Render(memoryDc, client, model_.GetSnapshot());
    BitBlt(windowDc, 0, 0, client.right, client.bottom, memoryDc, 0, 0, SRCCOPY);
    SelectObject(memoryDc, previousBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    EndPaint(window_, &paint);
}

void HmiWindow::Render(HDC dc, const RECT& client, const DashboardSnapshot& snapshot) {
    FillRectangle(dc, client, Background);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int padding = 24;

    DrawTextValue(dc, titleFont_, "FLIGHT MOTION CONTROL",
        MakeRect(padding, 16, width - padding, 50), PrimaryText);
    DrawTextValue(dc, bodyFont_, "6DOF STEWART PLATFORM  /  " + targetName_ + "  /  " + endpoint_,
        MakeRect(padding, 49, width - padding, 72), SecondaryText);
    DrawTextValue(dc, smallFont_, "LIVE TELEMETRY",
        MakeRect(width - 190, 22, width - padding, 46), Cyan,
        DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

    const int statusTop = 82;
    const int statusHeight = 82;
    const int statusGap = 14;
    const int statusWidth = (width - (2 * padding) - (2 * statusGap)) / 3;
    const RECT simulatorCard = MakeRect(padding, statusTop, padding + statusWidth, statusTop + statusHeight);
    const RECT controllerCard = MakeRect(simulatorCard.right + statusGap, statusTop,
        simulatorCard.right + statusGap + statusWidth, statusTop + statusHeight);
    const RECT dataCard = MakeRect(controllerCard.right + statusGap, statusTop,
        width - padding, statusTop + statusHeight);

    DrawStatusCard(dc, simulatorCard, headingFont_, smallFont_, "MICROSOFT FLIGHT SIMULATOR 2020",
        snapshot.simulatorConnected ? "Connected" : "Disconnected",
        snapshot.simulatorConnected ? "SimConnect is streaming aircraft data" : "Waiting for SimConnect",
        snapshot.simulatorConnected ? Green : Red);

    std::string controllerStatus = "Waiting";
    std::string controllerDetail = "Opening TCP listener";
    COLORREF controllerColor = Amber;
    if (snapshot.clientConnected && snapshot.positionFeedback) {
        controllerStatus = "Online";
        controllerDetail = "Position feedback is valid";
        controllerColor = Green;
    }
    else if (snapshot.clientConnected) {
        controllerStatus = "Connected";
        controllerDetail = "Awaiting six actuator positions";
        controllerColor = Amber;
    }
    else if (snapshot.tcpListening) {
        controllerDetail = "Listening at " + endpoint_;
    }
    DrawStatusCard(dc, controllerCard, headingFont_, smallFont_, targetName_ + " CONTROLLER",
        controllerStatus, controllerDetail, controllerColor);

    const bool dataLive = snapshot.positionFeedback
        && snapshot.lastFeedbackMilliseconds >= 0
        && snapshot.elapsedMilliseconds - snapshot.lastFeedbackMilliseconds < 1000;
    DrawStatusCard(dc, dataCard, headingFont_, smallFont_, "REAL-TIME DATA LINK",
        dataLive ? "Streaming" : "Idle",
        "TX " + FormatCount(snapshot.sentMessages) + "  /  RX " + FormatCount(snapshot.receivedMessages)
            + "  /  " + DataAge(snapshot.elapsedMilliseconds, snapshot.lastFeedbackMilliseconds),
        dataLive ? Cyan : SecondaryText);

    const int contentTop = statusTop + statusHeight + 16;
    const int contentBottom = height - padding;
    const int contentGap = 16;
    const int leftWidth = static_cast<int>((width - (2 * padding) - contentGap) * 0.47);
    const RECT leftPanel = MakeRect(padding, contentTop, padding + leftWidth, contentBottom);
    const RECT actuatorPanel = MakeRect(leftPanel.right + contentGap, contentTop, width - padding, contentBottom);
    FillRoundedRectangle(dc, leftPanel, Panel, Border);
    FillRoundedRectangle(dc, actuatorPanel, Panel, Border);

    DrawTextValue(dc, headingFont_, "6DOF PLATFORM",
        MakeRect(leftPanel.left + 18, leftPanel.top + 10, leftPanel.right - 18, leftPanel.top + 42), PrimaryText);
    DrawTextValue(dc, smallFont_, "LIVE ACTUATOR GEOMETRY",
        MakeRect(leftPanel.left + 18, leftPanel.top + 36, leftPanel.right - 18, leftPanel.top + 60), SecondaryText);

    const int diagramLeft = leftPanel.left + 18;
    const int diagramRight = leftPanel.right - 18;
    const int diagramTop = leftPanel.top + 64;
    const int diagramBottom = std::min(leftPanel.top + 345, leftPanel.bottom - 205);
    const int centerX = (diagramLeft + diagramRight) / 2;
    const int baseCenterY = diagramBottom - 40;
    const int topCenterY = diagramTop + 84;
    const double yawRadians = snapshot.yawDegrees * Pi / 180.0;

    std::array<POINT, 6> basePoints{};
    std::array<POINT, 6> platformPoints{};
    for (int index = 0; index < 6; ++index) {
        const double angle = (-90.0 + index * 60.0) * Pi / 180.0;
        basePoints[index].x = centerX + static_cast<LONG>(std::cos(angle) * 150.0);
        basePoints[index].y = baseCenterY + static_cast<LONG>(std::sin(angle) * 48.0);

        const double platformAngle = angle + yawRadians * 0.35;
        const double xOffset = std::cos(platformAngle) * 118.0;
        const double perspectiveY = std::sin(platformAngle) * 38.0;
        const double attitudeY = (xOffset / 118.0) * snapshot.rollDegrees * 0.7
            + std::sin(platformAngle) * snapshot.pitchDegrees * 0.7;
        platformPoints[index].x = centerX + static_cast<LONG>(xOffset);
        platformPoints[index].y = topCenterY + static_cast<LONG>(perspectiveY + attitudeY);
    }

    for (int index = 0; index < 6; ++index) {
        const float position = snapshot.currentPositions[index];
        const int thickness = snapshot.positionFeedback
            ? 3 + static_cast<int>(std::max(0.0f, std::min(4.0f, position / 100.0f)))
            : 3;
        HPEN legPen = CreatePen(PS_SOLID, thickness, snapshot.positionFeedback ? Cyan : RGB(74, 89, 103));
        HGDIOBJ previousPen = SelectObject(dc, legPen);
        MoveToEx(dc, basePoints[index].x, basePoints[index].y, nullptr);
        LineTo(dc, platformPoints[index].x, platformPoints[index].y);
        SelectObject(dc, previousPen);
        DeleteObject(legPen);

        const int labelX = (basePoints[index].x + platformPoints[index].x) / 2;
        const int labelY = (basePoints[index].y + platformPoints[index].y) / 2;
        RECT labelRect = MakeRect(labelX - 12, labelY - 11, labelX + 12, labelY + 11);
        HBRUSH labelBrush = CreateSolidBrush(PanelRaised);
        HGDIOBJ oldBrush = SelectObject(dc, labelBrush);
        HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, labelRect.left, labelRect.top, labelRect.right, labelRect.bottom);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(labelBrush);
        DrawTextValue(dc, smallFont_, std::to_string(index + 1), labelRect, PrimaryText,
            DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }

    HBRUSH baseBrush = CreateSolidBrush(RGB(44, 59, 74));
    HPEN basePen = CreatePen(PS_SOLID, 2, RGB(91, 112, 130));
    HGDIOBJ oldBrush = SelectObject(dc, baseBrush);
    HGDIOBJ oldPen = SelectObject(dc, basePen);
    Polygon(dc, basePoints.data(), static_cast<int>(basePoints.size()));
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(basePen);
    DeleteObject(baseBrush);

    HBRUSH platformBrush = CreateSolidBrush(RGB(31, 103, 120));
    HPEN platformPen = CreatePen(PS_SOLID, 2, Cyan);
    oldBrush = SelectObject(dc, platformBrush);
    oldPen = SelectObject(dc, platformPen);
    Polygon(dc, platformPoints.data(), static_cast<int>(platformPoints.size()));
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(platformPen);
    DeleteObject(platformBrush);

    const int poseTop = diagramBottom + 8;
    DrawTextValue(dc, smallFont_, "AIRCRAFT ATTITUDE",
        MakeRect(leftPanel.left + 18, poseTop, leftPanel.right - 18, poseTop + 24), SecondaryText);
    const int poseWidth = (leftPanel.right - leftPanel.left - 52) / 3;
    const char* poseLabels[] = { "PITCH", "ROLL", "YAW" };
    const double poseValues[] = { snapshot.pitchDegrees, snapshot.rollDegrees, snapshot.yawDegrees };
    for (int index = 0; index < 3; ++index) {
        const int x = leftPanel.left + 18 + index * (poseWidth + 8);
        RECT poseRect = MakeRect(x, poseTop + 25, x + poseWidth, poseTop + 83);
        FillRoundedRectangle(dc, poseRect, PanelRaised, Border);
        DrawTextValue(dc, smallFont_, poseLabels[index], MakeRect(x + 10, poseRect.top + 3, poseRect.right - 8, poseRect.top + 23), SecondaryText);
        DrawTextValue(dc, valueFont_, snapshot.motionAvailable ? FormatNumber(poseValues[index], 1) + " deg" : "--.- deg",
            MakeRect(x + 10, poseRect.top + 22, poseRect.right - 8, poseRect.bottom - 3), PrimaryText);
    }

    const int activityTop = poseTop + 96;
    if (activityTop < leftPanel.bottom - 38) {
        DrawTextValue(dc, smallFont_, "SYSTEM ACTIVITY",
            MakeRect(leftPanel.left + 18, activityTop, leftPanel.right - 18, activityTop + 22), SecondaryText);
        const int availableRows = std::max(1, (leftPanel.bottom - activityTop - 27) / 20);
        const int firstEvent = std::max(0, static_cast<int>(snapshot.events.size()) - availableRows);
        int row = 0;
        for (int index = firstEvent; index < static_cast<int>(snapshot.events.size()); ++index, ++row) {
            const DashboardEvent& event = snapshot.events[index];
            const std::string line = FormatElapsed(event.elapsedMilliseconds) + "  " + event.message;
            DrawTextValue(dc, smallFont_, line,
                MakeRect(leftPanel.left + 18, activityTop + 23 + row * 20,
                    leftPanel.right - 18, activityTop + 43 + row * 20), EventColor(event.level));
        }
    }

    DrawTextValue(dc, headingFont_, "ACTUATORS",
        MakeRect(actuatorPanel.left + 18, actuatorPanel.top + 10, actuatorPanel.right - 18, actuatorPanel.top + 42), PrimaryText);
    DrawTextValue(dc, smallFont_, "CURRENT POSITION  /  AMBER MARKER = TARGET",
        MakeRect(actuatorPanel.left + 18, actuatorPanel.top + 36, actuatorPanel.right - 18, actuatorPanel.top + 60), SecondaryText);

    const int gridLeft = actuatorPanel.left + 14;
    const int gridTop = actuatorPanel.top + 66;
    const int gridRight = actuatorPanel.right - 14;
    const int gridBottom = actuatorPanel.bottom - 14;
    const int columnGap = 12;
    const int rowGap = 12;
    const int cardWidth = (gridRight - gridLeft - columnGap) / 2;
    const int cardHeight = (gridBottom - gridTop - (2 * rowGap)) / 3;
    for (int index = 0; index < 6; ++index) {
        const int column = index % 2;
        const int row = index / 2;
        const int left = gridLeft + column * (cardWidth + columnGap);
        const int top = gridTop + row * (cardHeight + rowGap);
        const RECT card = MakeRect(left, top, left + cardWidth, top + cardHeight);
        DrawActuatorCard(dc, card, headingFont_, bodyFont_, smallFont_, index + 1,
            snapshot.currentPositions[index], snapshot.targetPositions[index], snapshot.speeds[index],
            snapshot.positionFeedback);
    }
}

void HmiWindow::CreateFonts() {
    titleFont_ = CreateFontA(-25, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    headingFont_ = CreateFontA(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    valueFont_ = CreateFontA(-17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Consolas");
    bodyFont_ = CreateFontA(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    smallFont_ = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
}

void HmiWindow::DestroyFonts() {
    HFONT* fonts[] = { &titleFont_, &headingFont_, &bodyFont_, &valueFont_, &smallFont_ };
    for (HFONT* font : fonts) {
        if (*font != nullptr) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
}
