// Renderer bring-up smoke: drives one D3D9 Clear + pixel readback + Present
// through the entire TRANSLATION stack — libdxvk_d3d9.a → Vulkan → MoltenVK
// (statically linked) → Metal — onto a dedicated CAMetalLayer sublayer.
//
// The DXVK build carries a custom "iOS" WSI backend whose window handle IS a
// CAMetalLayer* (selected via DXVK_WSI_DRIVER). Readback happens BEFORE
// Present (SWAPEFFECT_DISCARD leaves the backbuffer undefined after), so the
// reported pixel proves rendering and the Present HRESULT proves the
// swapchain/presentation path.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <exception>

// windows_base.h defines `interface` as a macro (COM-ism) — kill it before
// any ObjC @interface is parsed. BOOL is handled by the __OBJC__ guard the
// iOS WSI work added to windows_base.h itself.
#undef interface
#import <Foundation/Foundation.h>

static const char *kisak_d3d9_smoke_inner(void *metalLayer);

extern "C" const char *kisak_d3d9_smoke(void *metalLayer)
{
    // DXVK signals unrecoverable setup problems with C++ exceptions
    // (DxvkError does not derive from std::exception) — keep them from
    // unwinding into Swift/UIKit.
    try {
        return kisak_d3d9_smoke_inner(metalLayer);
    } catch (const std::exception &e) {
        static char ebuf[256];
        snprintf(ebuf, sizeof(ebuf), "FAIL std::exception: %s", e.what());
        return ebuf;
    } catch (...) {
        return "FAIL non-std C++ exception out of DXVK";
    }
}

static const char *kisak_d3d9_smoke_inner(void *metalLayer)
{
    static char buf[512];

    // DXVK logs to stderr, which devicectl's console does not capture —
    // tee it into the sandbox where the marker-file tooling can pull it.
    NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    freopen([docs stringByAppendingPathComponent:@"dxvk_stderr.log"].fileSystemRepresentation, "w", stderr);
    setvbuf(stderr, nullptr, _IONBF, 0);

    setenv("DXVK_WSI_DRIVER", "iOS", 1);
    setenv("DXVK_LOG_LEVEL", "info", 1);
    setenv("DXVK_LOG_PATH", "none", 1); // no file logs; app cwd is read-only

    NSLog(@"KISAK_D3D9 breadcrumb: calling Direct3DCreate9");
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    NSLog(@"KISAK_D3D9 breadcrumb: Direct3DCreate9 -> %p", (void *)d3d);
    if (!d3d)
        return "FAIL Direct3DCreate9 returned null";

    D3DADAPTER_IDENTIFIER9 ident = {};
    d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &ident);

    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 480;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = (HWND)metalLayer;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    NSLog(@"KISAK_D3D9 breadcrumb: calling CreateDevice (hwnd=CAMetalLayer %p)", metalLayer);
    IDirect3DDevice9 *dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                   (HWND)metalLayer,
                                   D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                   &pp, &dev);
    NSLog(@"KISAK_D3D9 breadcrumb: CreateDevice -> 0x%08X dev=%p", (unsigned)hr, (void *)dev);
    if (FAILED(hr) || !dev) {
        snprintf(buf, sizeof(buf), "FAIL CreateDevice hr=0x%08X (adapter=%s)",
                 (unsigned)hr, ident.Description);
        d3d->Release();
        return buf;
    }

    // Clear to a distinctive color, read the pixel back, THEN present.
    const D3DCOLOR kClear = D3DCOLOR_XRGB(186, 85, 211); // 0x00BA55D3
    const HRESULT clearHr = dev->Clear(0, nullptr, D3DCLEAR_TARGET, kClear, 1.0f, 0);

    unsigned pixel = 0;
    HRESULT readHr = E_FAIL;
    IDirect3DSurface9 *backbuffer = nullptr;
    if (SUCCEEDED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer))) {
        D3DSURFACE_DESC desc = {};
        backbuffer->GetDesc(&desc);
        IDirect3DSurface9 *sysmem = nullptr;
        if (SUCCEEDED(dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                       D3DPOOL_SYSTEMMEM, &sysmem, nullptr))) {
            readHr = dev->GetRenderTargetData(backbuffer, sysmem);
            if (SUCCEEDED(readHr)) {
                D3DLOCKED_RECT lr = {};
                if (SUCCEEDED(sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                    const unsigned char *center = (const unsigned char *)lr.pBits
                        + (desc.Height / 2) * lr.Pitch + (desc.Width / 2) * 4;
                    pixel = *(const unsigned *)center;
                    sysmem->UnlockRect();
                }
            }
            sysmem->Release();
        }
        backbuffer->Release();
    }

    HRESULT presentHr = dev->Present(nullptr, nullptr, nullptr, nullptr);

    // This exact success string is an earned marker in the simulator lane.
    // It deliberately proves only this renderer smoke, not an engine frame.
    constexpr unsigned kExpectedPixel = 0xFFBA55D3u;
    if (SUCCEEDED(clearHr) && SUCCEEDED(readHr)
        && pixel == kExpectedPixel && SUCCEEDED(presentHr)) {
        return "DXVK D3D9 OK — CreateDevice, clear/readback=0xFFBA55D3, Present";
    }

    snprintf(buf, sizeof(buf),
             "FAIL adapter=%s clear=0x%08X read=0x%08X px=0x%08X present=0x%08X",
             ident.Description, (unsigned)clearHr, (unsigned)readHr,
             pixel, (unsigned)presentHr);

    // Keep device+d3d alive: the swapchain owns the layer's Metal state and
    // this is a one-shot smoke — teardown ordering is bring-up work, not
    // smoke-test work.
    return buf;
}
