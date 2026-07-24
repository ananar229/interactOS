/*
 * PROJECT:     interactOS win32k.sys benchmark tool
 * PURPOSE:     User-mode timing workloads for win32k.sys (window creation,
 *              GDI blit, message throughput). Prints elapsed time / ops-per-second
 *              per workload; no kernel changes required.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <wine/debug.h>

WINE_DEFAULT_DEBUG_CHANNEL(win32kbench);

#define DEFAULT_ITERATIONS 10000
#define BENCH_CLASS_NAME L"Win32kBenchWndClass"
#define WM_BENCH_NOP  (WM_USER + 1)
#define WM_BENCH_ECHO (WM_USER + 2)

static double
QpcElapsedMs(LARGE_INTEGER Start, LARGE_INTEGER End, LARGE_INTEGER Freq)
{
    return (double)(End.QuadPart - Start.QuadPart) * 1000.0 / (double)Freq.QuadPart;
}

/* Raw OutputDebugStringA output from an unregistered console EXE was found
 * not to reach the captured COM1 stream (unlike ERR()/WARN() from
 * WINE_DEFAULT_DEBUG_CHANNEL-declared components, which reliably do) — use
 * the same channel infrastructure win32k/user32 diagnostics already rely on. */
static void
BenchLog(const char *Format, ...)
{
    char buf[512];
    va_list args;

    va_start(args, Format);
    _vsnprintf(buf, sizeof(buf) - 1, Format, args);
    va_end(args);
    buf[sizeof(buf) - 1] = '\0';

    printf("%s", buf);
    ERR("%s", buf);
}

static void
ReportResult(const char *Label, ULONG Count, double ElapsedMs)
{
    double perSec = (ElapsedMs > 0.0) ? (Count * 1000.0 / ElapsedMs) : 0.0;
    BenchLog("  %-32s %8lu ops in %10.2f ms  (%10.1f ops/sec)\n",
             Label, (unsigned long)Count, ElapsedMs, perSec);
}

static LRESULT CALLBACK
BenchWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
        case WM_BENCH_NOP:
            return 0;
        case WM_BENCH_ECHO:
            return (LRESULT)wParam;
        default:
            return DefWindowProcW(hWnd, Msg, wParam, lParam);
    }
}

static BOOL
RegisterBenchClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = BenchWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = BENCH_CLASS_NAME;

    return RegisterClassExW(&wc) != 0;
}

/* Workload 1: window creation/destruction throughput, plus a regression
 * variant with a deliberately invalid hwndParent that exercises the
 * error path fixed for the co_UserCreateWindowEx window-station
 * reference leak. Success criterion here is "completes N iterations
 * without hang/crash", not a numeric leak count. */
static void
BenchWindows(HINSTANCE hInstance, ULONG Iterations)
{
    LARGE_INTEGER freq, start, end;
    ULONG i, created = 0, failed = 0;
    HWND hWnd;

    BenchLog("[windows] %lu iterations\n", (unsigned long)Iterations);
    QueryPerformanceFrequency(&freq);

    QueryPerformanceCounter(&start);
    for (i = 0; i < Iterations; i++)
    {
        hWnd = CreateWindowExW(0, BENCH_CLASS_NAME, L"bench",
                                WS_OVERLAPPED, 0, 0, 100, 100,
                                NULL, NULL, hInstance, NULL);
        if (hWnd)
        {
            created++;
            DestroyWindow(hWnd);
        }
        else
        {
            failed++;
        }
    }
    QueryPerformanceCounter(&end);
    ReportResult("CreateWindowExW/DestroyWindow", Iterations, QpcElapsedMs(start, end, freq));
    BenchLog("  created=%lu failed=%lu\n", (unsigned long)created, (unsigned long)failed);

    /* Regression variant: invalid hwndParent forces the error path. */
    created = 0;
    failed = 0;
    QueryPerformanceCounter(&start);
    for (i = 0; i < Iterations; i++)
    {
        hWnd = CreateWindowExW(0, BENCH_CLASS_NAME, L"bench",
                                WS_CHILD, 0, 0, 100, 100,
                                (HWND)0xDEADBEEF, NULL, hInstance, NULL);
        if (hWnd)
        {
            created++;
            DestroyWindow(hWnd);
        }
        else
        {
            failed++;
        }
    }
    QueryPerformanceCounter(&end);
    ReportResult("CreateWindowExW(invalid parent)", Iterations, QpcElapsedMs(start, end, freq));
    BenchLog("  created=%lu failed=%lu  (expect failed==N; no crash/hang => WinSta-leak fix holds)\n",
           (unsigned long)created, (unsigned long)failed);
}

/* Workload 2: GDI blit throughput (BitBlt/StretchBlt) between two DIB
 * sections, exercising the eng/dib path touched by the stretchblt.c
 * NULL-deref fix. */
static void
BenchBlit(ULONG Iterations, LONG Width, LONG Height, WORD Bpp)
{
    LARGE_INTEGER freq, start, end;
    HDC screenDc, srcDc, dstDc;
    HBITMAP srcBmp, dstBmp, oldSrc, oldDst;
    BITMAPINFO bmi;
    PVOID srcBits = NULL, dstBits = NULL;
    ULONG i;

    BenchLog("[blit] %lu iterations, %ldx%ld @%u bpp\n",
           (unsigned long)Iterations, Width, Height, Bpp);

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = Width;
    bmi.bmiHeader.biHeight = -Height; /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = Bpp;
    bmi.bmiHeader.biCompression = BI_RGB;

    screenDc = GetDC(NULL);
    srcDc = CreateCompatibleDC(screenDc);
    dstDc = CreateCompatibleDC(screenDc);

    srcBmp = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &srcBits, NULL, 0);
    dstBmp = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &dstBits, NULL, 0);
    if (!srcBmp || !dstBmp)
    {
        BenchLog("  CreateDIBSection failed, skipping blit workload\n");
        goto cleanup;
    }
    if (srcBits)
        memset(srcBits, 0xA5, (size_t)Width * Height * (Bpp / 8));

    oldSrc = SelectObject(srcDc, srcBmp);
    oldDst = SelectObject(dstDc, dstBmp);

    QueryPerformanceFrequency(&freq);

    QueryPerformanceCounter(&start);
    for (i = 0; i < Iterations; i++)
        BitBlt(dstDc, 0, 0, Width, Height, srcDc, 0, 0, SRCCOPY);
    QueryPerformanceCounter(&end);
    ReportResult("BitBlt (1:1)", Iterations, QpcElapsedMs(start, end, freq));

    QueryPerformanceCounter(&start);
    for (i = 0; i < Iterations; i++)
        StretchBlt(dstDc, 0, 0, Width / 2, Height / 2, srcDc, 0, 0, Width, Height, SRCCOPY);
    QueryPerformanceCounter(&end);
    ReportResult("StretchBlt (2:1 downscale)", Iterations, QpcElapsedMs(start, end, freq));

    SelectObject(srcDc, oldSrc);
    SelectObject(dstDc, oldDst);
    DeleteObject(srcBmp);
    DeleteObject(dstBmp);

cleanup:
    DeleteDC(srcDc);
    DeleteDC(dstDc);
    ReleaseDC(NULL, screenDc);
}

typedef struct _MSG_WORKER_CTX
{
    HANDLE ReadyEvent;
    HANDLE DoneEvent;
    HWND hWnd;
    ULONG NopCount;
} MSG_WORKER_CTX, *PMSG_WORKER_CTX;

static DWORD WINAPI
MsgWorkerThread(LPVOID Param)
{
    PMSG_WORKER_CTX ctx = (PMSG_WORKER_CTX)Param;
    MSG msg;

    /* Force message-queue creation before signalling ready. */
    PeekMessageW(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    ctx->hWnd = CreateWindowExW(0, BENCH_CLASS_NAME, L"bench-msg",
                                 WS_OVERLAPPED, 0, 0, 10, 10,
                                 NULL, NULL, GetModuleHandleW(NULL), NULL);
    SetEvent(ctx->ReadyEvent);

    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        if (msg.message == WM_BENCH_NOP)
        {
            ctx->NopCount++;
            if (ctx->NopCount == 0) /* wrapped: treat as done-signal misuse guard */
                break;
        }
        else if (msg.message == WM_QUIT)
        {
            break;
        }
        DispatchMessageW(&msg);
    }

    if (ctx->hWnd)
        DestroyWindow(ctx->hWnd);
    SetEvent(ctx->DoneEvent);
    return 0;
}

/* Workload 3: PostMessage (async) and SendMessage (sync, cross-thread)
 * throughput between two threads, exercising msgqueue.c and the
 * co_IntDoSendMessage path (message.c:458). */
static void
BenchMessages(ULONG Iterations)
{
    LARGE_INTEGER freq, start, end;
    MSG_WORKER_CTX ctx;
    HANDLE hThread;
    ULONG i;

    BenchLog("[msg] %lu iterations\n", (unsigned long)Iterations);

    ZeroMemory(&ctx, sizeof(ctx));
    ctx.ReadyEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    ctx.DoneEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!ctx.ReadyEvent || !ctx.DoneEvent)
    {
        BenchLog("  CreateEvent failed, skipping message workload\n");
        return;
    }

    hThread = CreateThread(NULL, 0, MsgWorkerThread, &ctx, 0, NULL);
    if (!hThread)
    {
        BenchLog("  CreateThread failed, skipping message workload\n");
        return;
    }
    WaitForSingleObject(ctx.ReadyEvent, INFINITE);

    if (!ctx.hWnd)
    {
        BenchLog("  worker's CreateWindowExW failed, skipping message workload\n");
        PostThreadMessageW(GetThreadId(hThread), WM_QUIT, 0, 0);
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        CloseHandle(ctx.ReadyEvent);
        CloseHandle(ctx.DoneEvent);
        return;
    }

    QueryPerformanceFrequency(&freq);

    /* SendMessage: synchronous, blocks until the worker's WNDPROC returns. */
    QueryPerformanceCounter(&start);
    for (i = 0; i < Iterations; i++)
        SendMessageW(ctx.hWnd, WM_BENCH_ECHO, i, 0);
    QueryPerformanceCounter(&end);
    ReportResult("SendMessageW (sync)", Iterations, QpcElapsedMs(start, end, freq));

    /* PostMessage: async, measure post-loop time, then wait for drain. */
    QueryPerformanceCounter(&start);
    for (i = 0; i < Iterations; i++)
        PostMessageW(ctx.hWnd, WM_BENCH_NOP, i, 0);
    QueryPerformanceCounter(&end);
    ReportResult("PostMessageW (post-loop only)", Iterations, QpcElapsedMs(start, end, freq));

    /* Drain: post a WM_QUIT and wait for the worker to finish processing
     * everything queued ahead of it, to see full round-trip throughput. */
    QueryPerformanceCounter(&start);
    PostMessageW(ctx.hWnd, WM_QUIT, 0, 0);
    WaitForSingleObject(ctx.DoneEvent, INFINITE);
    QueryPerformanceCounter(&end);
    ReportResult("PostMessageW (full drain)", Iterations, QpcElapsedMs(start, end, freq));

    CloseHandle(hThread);
    CloseHandle(ctx.ReadyEvent);
    CloseHandle(ctx.DoneEvent);
}

static void
Usage(void)
{
    BenchLog("win32kbench - win32k.sys timing workloads\n");
    BenchLog("usage: win32kbench <windows|blit|msg|all> [iterations]\n");
}

int main(int argc, char **argv)
{
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    ULONG iterations = DEFAULT_ITERATIONS;
    const char *mode;

    if (argc < 2)
    {
        Usage();
        return 1;
    }
    mode = argv[1];
    if (argc >= 3)
        iterations = (ULONG)strtoul(argv[2], NULL, 10);
    if (iterations == 0)
        iterations = DEFAULT_ITERATIONS;

    if (!RegisterBenchClass(hInstance))
    {
        BenchLog("RegisterClassExW failed, aborting\n");
        return 1;
    }

    BenchLog("===WIN32KBENCH START=== (iterations=%lu)\n", (unsigned long)iterations);

    if (!strcmp(mode, "windows") || !strcmp(mode, "all"))
        BenchWindows(hInstance, iterations);
    if (!strcmp(mode, "blit") || !strcmp(mode, "all"))
        BenchBlit(iterations, 256, 256, 32);
    if (!strcmp(mode, "msg") || !strcmp(mode, "all"))
        BenchMessages(iterations);

    BenchLog("===WIN32KBENCH DONE===\n");
    return 0;
}
