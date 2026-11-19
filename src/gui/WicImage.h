#pragma once
// ============================================================
// WicImage.h — reading a PNG or JPEG through Windows (phase 162d)
//
// The app wrote images with WIC (snapshots, window shots, the preview
// stream) and read none. The Moon map needs to read two kinds: the LRO
// albedo map in data/, and the tiles it makes of the owner's own
// sections (phase 162e). WIC is in every Windows, so this adds no
// library.
//
// Decoded to 8-bit grey (the albedo map is grey, and so is a tile from a
// mono camera) or to 32-bit BGRA. Each call starts COM on its thread if
// nothing has, and leaves it as it found it.
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace raduk::gui {

struct WicPixels {
    int width = 0, height = 0;
    int bytesPerPixel = 0;          // 1 grey, 4 BGRA
    std::vector<uint8_t> data;      // rows top down, no padding
    bool Valid() const { return width > 0 && height > 0 && !data.empty(); }
    uint8_t Grey(int x, int y) const { return data[(size_t)y * width * bytesPerPixel + (size_t)x * bytesPerPixel]; }
};

namespace wic_detail {
    template <typename T> struct Ptr {
        T* p = nullptr;
        ~Ptr() { if (p) p->Release(); }
        T** operator&() { return &p; }
        T* operator->() const { return p; }
        explicit operator bool() const { return p != nullptr; }
    };
    inline std::string Hr(const char* what, HRESULT hr) {
        char b[96];
        snprintf(b, sizeof(b), "%s failed (0x%08lX)", what, (unsigned long)hr);
        return b;
    }
}

// False with the reason. `grey` picks 8-bit grey, else BGRA.
inline bool LoadImageFile(const std::wstring& path, bool grey, WicPixels& out, std::string& why) {
    using namespace wic_detail;
    out = WicPixels{};
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool started = SUCCEEDED(co);         // S_OK or S_FALSE: ours to undo
    struct Undo { bool on; ~Undo() { if (on) CoUninitialize(); } } undo{started};

    Ptr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { why = Hr("the WIC factory", hr); return false; }

    Ptr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) { why = Hr("opening the image", hr); return false; }

    Ptr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { why = Hr("reading its first frame", hr); return false; }

    Ptr<IWICFormatConverter> conv;
    hr = factory->CreateFormatConverter(&conv);
    if (FAILED(hr)) { why = Hr("a format converter", hr); return false; }
    hr = conv->Initialize(frame.p, grey ? GUID_WICPixelFormat8bppGray : GUID_WICPixelFormat32bppBGRA,
                          WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { why = Hr("converting its pixels", hr); return false; }

    UINT w = 0, h = 0;
    conv->GetSize(&w, &h);
    if (w == 0 || h == 0) { why = "the image is empty"; return false; }
    const int bpp = grey ? 1 : 4;
    std::vector<uint8_t> px((size_t)w * h * bpp);
    hr = conv->CopyPixels(nullptr, w * bpp, (UINT)px.size(), px.data());
    if (FAILED(hr)) { why = Hr("copying its pixels", hr); return false; }

    out.width = (int)w;
    out.height = (int)h;
    out.bytesPerPixel = bpp;
    out.data = std::move(px);
    return true;
}

// An 8-bit grey picture to a PNG (phase 162e: the Moon map's tiles).
// Written beside the target and moved over it, so a reader never sees half
// a file. False with the reason.
inline bool SaveGreyPng(const std::wstring& path, int w, int h, const uint8_t* grey, std::string& why) {
    using namespace wic_detail;
    if (w <= 0 || h <= 0 || !grey) { why = "nothing to write"; return false; }
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    struct Undo { bool on; ~Undo() { if (on) CoUninitialize(); } } undo{SUCCEEDED(co)};

    Ptr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { why = Hr("the WIC factory", hr); return false; }
    const std::wstring tmp = path + L".tmp";
    {
        Ptr<IWICStream> stream;
        hr = factory->CreateStream(&stream);
        if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(tmp.c_str(), GENERIC_WRITE);
        if (FAILED(hr)) { why = Hr("creating the file", hr); return false; }
        Ptr<IWICBitmapEncoder> enc;
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
        if (SUCCEEDED(hr)) hr = enc->Initialize(stream.p, WICBitmapEncoderNoCache);
        Ptr<IWICBitmapFrameEncode> frame;
        if (SUCCEEDED(hr)) hr = enc->CreateNewFrame(&frame, nullptr);
        if (SUCCEEDED(hr)) hr = frame->Initialize(nullptr);
        if (SUCCEEDED(hr)) hr = frame->SetSize((UINT)w, (UINT)h);
        WICPixelFormatGUID fmt = GUID_WICPixelFormat8bppGray;
        if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&fmt);
        if (SUCCEEDED(hr) && fmt != GUID_WICPixelFormat8bppGray) hr = E_FAIL;
        if (SUCCEEDED(hr)) hr = frame->WritePixels((UINT)h, (UINT)w, (UINT)((size_t)w * h), const_cast<BYTE*>(grey));
        if (SUCCEEDED(hr)) hr = frame->Commit();
        if (SUCCEEDED(hr)) hr = enc->Commit();
        if (FAILED(hr)) { why = Hr("encoding the PNG", hr); DeleteFileW(tmp.c_str()); return false; }
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        why = "could not move the PNG into place";
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

} // namespace raduk::gui
