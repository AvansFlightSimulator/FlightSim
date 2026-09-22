#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <string>

class DashboardModel;
class MotionController;
struct DashboardSnapshot;

class HmiWindow {
public:
    HmiWindow(
        DashboardModel& model,
        MotionController& motion,
        const std::string& targetName,
        const std::string& bindIp,
        int port);
    ~HmiWindow();

    HmiWindow(const HmiWindow&) = delete;
    HmiWindow& operator=(const HmiWindow&) = delete;

    bool Create(HINSTANCE instance, int showCommand);
    bool ProcessControlMessage(MSG& message);

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SourceControlProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void Paint();
    void Render(HDC deviceContext, const RECT& client, const DashboardSnapshot& snapshot);
    void CreateFonts();
    void DestroyFonts();
    bool CreateControls(HINSTANCE instance);
    void RefreshControls();
    void ExecuteManualInput();
    void ExecuteActuatorInput();
    bool ReadInputValue(HWND control, double& value);
    void DrawSourceControl(HDC dc);
    void DrawControl(const DRAWITEMSTRUCT& item);

    DashboardModel& model_;
    MotionController& motion_;
    std::string targetName_;
    std::string endpoint_;
    HWND window_ = nullptr;
    HWND sourceControl_ = nullptr;
    HWND angleControls_[3]{};
    HWND positionControls_[6]{};
    HWND executeControl_ = nullptr;
    WNDPROC originalSourceProcedure_ = nullptr;
    HBRUSH fieldBrush_ = nullptr;
    HBRUSH disabledFieldBrush_ = nullptr;
    std::string inputStatus_;
    HFONT titleFont_ = nullptr;
    HFONT headingFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT valueFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    RECT diagramBounds_{};
    POINT orbitMouse_{};
    bool orbitDragging_ = false;
    double orbitAzimuth_ = 35.0;
    double orbitElevation_ = 25.0;
    double orbitZoom_ = 1.0;
};
