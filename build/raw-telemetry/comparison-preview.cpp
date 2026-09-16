#include <string>
#define private public
#include "HmiWindow.h"
#undef private
#include "../../HmiWindow.cpp"
#include "DashboardModel.h"
#include <fstream>

int main() {
    DashboardModel model;
    model.SetSimulatorConnected(true);
    model.UpdateSimulatorAttitude(-89.9, -179.9);
    model.UpdateSimulatorRudder(-25.0);
    model.UpdateOrientation(30.0, -30.0, 12.5);
    HmiWindow hmi(model, "PLC", "0.0.0.0", 32760);
    hmi.CreateFonts();
    const int width = 1104, height = 681;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!dc || !bitmap) return 1;
    HGDIOBJ previous = SelectObject(dc, bitmap);
    RECT bounds{0, 0, width, height};
    hmi.Render(dc, bounds, model.GetSnapshot());
    GdiFlush();
    BITMAPFILEHEADER file{};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + width * height * 4;
    std::ofstream output("comparison-preview.bmp", std::ios::binary);
    output.write(reinterpret_cast<const char*>(&file), sizeof(file));
    output.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(BITMAPINFOHEADER));
    output.write(static_cast<const char*>(pixels), width * height * 4);
    const bool success = output.good();
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return success ? 0 : 1;
}

