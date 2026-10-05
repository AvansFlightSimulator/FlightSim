#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "TelemetryStore.h"
#include <future>

// A read-only HMI page. Pausing/replaying graphs never changes motion control.
class DiagnosticsView {
public:
    explicit DiagnosticsView(TelemetryStore& store) : store_(store) {}
    bool Create(HWND parent, HINSTANCE instance);
    void Show(bool visible, int width, int height);
    void Resize(int width, int height);
private:
    struct Replay { std::vector<TelemetryFrame> frames; std::string error; };
    static LRESULT CALLBACK Procedure(HWND, UINT, WPARAM, LPARAM);
    LRESULT Handle(UINT, WPARAM, LPARAM);
    void Paint();
    void RefreshFiles();
    void Update();
    void Graph(HDC dc, RECT rect, const char* title, const std::vector<std::size_t>& signals);
    TelemetryStore& store_;
    HWND window_ = nullptr;
    HWND axis_ = nullptr;
    HWND files_ = nullptr;
    HWND details_ = nullptr;
    bool showDetails_ = false;
    HFONT font_ = nullptr;
    bool live_ = true;
    double end_ = 30;
    double span_ = 30;
    double cursor_ = -1;
    int actuator_ = 0;
    std::vector<TelemetryFrame> frames_;
    std::future<Replay> loading_;
    std::string message_;
};
