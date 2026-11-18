// main.cpp
// Application WinMain entry and main loop.

#include "engine/scene/scene_loader.h"
#include "engine/core/frame_capture.h"
#include "engine/testing/runtime_test_bridge.h"
#include "engine/ecs/game_object.h"
#include "engine/ecs/rigidbody_component.h"
#include "engine/core/content_path.h"
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
#include "engine/ecs/prefab_runtime.h"
#include "src/app/runtime_test_world_apply.h"
#include "src/app/runtime_test_editor_apply.h"
#include <shellapi.h> // CommandLineToArgvW (bridge --scene); links via shell32.lib below
static ivy::testing::Bridge* g_pRuntimeTestBridge = nullptr;
static bool g_runtimeTestSceneArgFatal = false;
#endif

#if defined(_DEBUG) && defined(GAME_RUNTIME)
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#pragma comment(lib, "dbghelp.lib")

namespace {
static bool CrashTraceEnabled() {
    char val[4]{};
    return GetEnvironmentVariableA("IVY_RUNTIME_TEST_CRASH_TRACE", val, 4) > 0 && val[0] == '1';
}

static LONG WINAPI CrashTraceUnhandledExceptionFilter(EXCEPTION_POINTERS* pEx) {
    static volatile LONG s_inHandler = 0;
    if (InterlockedCompareExchange(&s_inHandler, 1, 0) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (!CrashTraceEnabled()) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    HANDLE hProcess = GetCurrentProcess();
    HMODULE hModule = GetModuleHandleA(nullptr);
    uintptr_t base = reinterpret_cast<uintptr_t>(hModule);

    DWORD code = pEx && pEx->ExceptionRecord ? pEx->ExceptionRecord->ExceptionCode : 0;
    void* addr = pEx && pEx->ExceptionRecord ? pEx->ExceptionRecord->ExceptionAddress : nullptr;
    uintptr_t addrVal = reinterpret_cast<uintptr_t>(addr);
    uintptr_t rva = (addrVal >= base) ? (addrVal - base) : 0;

    fprintf(stderr, "\n=== [IVY_CRASH_TRACE] UNHANDLED EXCEPTION ===\n");
    fprintf(stderr, "ExceptionCode: 0x%08X\n", code);
    fprintf(stderr, "ExceptionAddress: 0x%p (ModuleBase: 0x%p, RVA: 0x%llX)\n", addr, reinterpret_cast<void*>(base), static_cast<unsigned long long>(rva));

    char searchPath[MAX_PATH * 2] = { 0 };
    char exePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH)) {
        char* lastSlash = strrchr(exePath, '\\');
        if (lastSlash) *lastSlash = '\0';
        strcpy_s(searchPath, exePath);
    }
    char symPath[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableA("IVY_RUNTIME_TEST_SYMBOL_PATH", symPath, MAX_PATH) > 0) {
        if (searchPath[0]) strcat_s(searchPath, ";");
        strcat_s(searchPath, symPath);
    }
    SymInitialize(hProcess, searchPath[0] ? searchPath : nullptr, TRUE);

    void* frames[32] = { nullptr };
    USHORT captured = RtlCaptureStackBackTrace(0, 32, frames, nullptr);
    fprintf(stderr, "Call stack (%u frames):\n", captured);

    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] = { 0 };
    PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(buffer);
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    for (USHORT i = 0; i < captured; ++i) {
        uintptr_t frameAddr = reinterpret_cast<uintptr_t>(frames[i]);
        uintptr_t frameRva = (frameAddr >= base) ? (frameAddr - base) : 0;

        DWORD64 displacement = 0;
        BOOL hasSym = SymFromAddr(hProcess, frameAddr, &displacement, pSymbol);

        IMAGEHLP_LINE64 line = { 0 };
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD lineDisplacement = 0;
        BOOL hasLine = SymGetLineFromAddr64(hProcess, frameAddr, &lineDisplacement, &line);

        if (hasSym && hasLine) {
            fprintf(stderr, "  [%02u] 0x%p (RVA: 0x%llX) %s+0x%llX [%s:%u]\n",
                i, reinterpret_cast<void*>(frameAddr), static_cast<unsigned long long>(frameRva),
                pSymbol->Name, static_cast<unsigned long long>(displacement),
                line.FileName, line.LineNumber);
        } else if (hasSym) {
            fprintf(stderr, "  [%02u] 0x%p (RVA: 0x%llX) %s+0x%llX\n",
                i, reinterpret_cast<void*>(frameAddr), static_cast<unsigned long long>(frameRva),
                pSymbol->Name, static_cast<unsigned long long>(displacement));
        } else {
            fprintf(stderr, "  [%02u] 0x%p (RVA: 0x%llX) <unknown>\n",
                i, reinterpret_cast<void*>(frameAddr), static_cast<unsigned long long>(frameRva));
        }
    }
    fprintf(stderr, "=== [IVY_CRASH_TRACE] END TRACE ===\n\n");
    fflush(stderr);

    SymCleanup(hProcess);
    return EXCEPTION_CONTINUE_SEARCH;
}

struct MainStaticDestructionMarker {
    MainStaticDestructionMarker() {
        if (CrashTraceEnabled()) {
            fprintf(stderr, "[IVY_CRASH_TRACE] main.cpp: static constructor reached\n");
            fflush(stderr);
        }
    }
    ~MainStaticDestructionMarker() {
        if (CrashTraceEnabled()) {
            fprintf(stderr, "[IVY_CRASH_TRACE] main.cpp: static destruction reached\n");
            fflush(stderr);
        }
    }
};
static MainStaticDestructionMarker s_mainStaticMarker;
} // namespace
#endif
#if defined(_DEBUG) && !defined(GAME_RUNTIME)
#include "tools/play_scene_switch_check.h"
#endif
/**===========================================================================

 File   :  [main.cpp]

 Author : Terry Yeung
 Date   : 2025/9/29
 -----------------------------------------------------------------------------

 ===========================================================================**/
#define _CRTDBG_MAP_ALLOC // enable CRT memory leak checking
#include <stdlib.h>       // for _CrtDumpMemoryLeaks
#include <crtdbg.h>       // for _CrtDumpMemoryLeaks
#include <sstream>
#include <string>
#include <thread>
#include <functional>

#include "main.h"

#include "engine/core/env_var.h"
#include "engine/audio/audio_manager.h"
#include "engine/rendering/d3d11/renderer.h"
#include "engine/rendering/d3d11/shader.h"
#include "engine/rendering/d3d11/post_process.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/rendering/sprite/sprite.h"
#include "engine/rendering/sprite/line_sprite.h"
#include "engine/rendering/model/model_util.h"
#include "engine/rendering/effect/effect_system.h"
#include "engine/input/keyboard.h"
#include "engine/input/key_logger.h"
#include "scene_manager.h"
#include "game_scene.h"
#include "fade.h"
#include "scene_transition.h"
#include "engine/core/system_timer.h"
#include "engine/core/job_system.h"
#include "engine/core/debug_system.h"
#include "engine/core/frame_timer.h"
#include "engine/core/render_thread.h"
#include "engine/input/mouse.h"
#include "engine/input/controller.h"
#include "debug_camera.h"
#include "camera/third_person_camera.h"
#include "engine/debug_draw/debug_text.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "engine/core/engine_gui.h"
#include "engine/core/play_fps_overlay.h"
#include "engine/rendering/culling/frustum_cull.h"
#include "engine/rendering/culling/shadow_cascade_cull.h"
#include "engine/rendering/culling/sprite_cull.h"
#include "engine/rendering/culling/lod_state.h"

#ifndef GAME_RUNTIME
#include "imgui/imgui.h"
#endif

 /**---------------------------------------------------------------------------
	Forward declarations and globals
  ---------------------------------------------------------------------------**/
#pragma	comment (lib, "d3d11.lib")
#pragma	comment (lib, "d3dcompiler.lib")
#pragma	comment (lib, "winmm.lib")
#pragma	comment (lib, "dxguid.lib")
#pragma	comment (lib, "dinput8.lib")
#pragma	comment (lib, "shell32.lib")

  // Effekseer
#if defined(_DEBUG) // Debug
#pragma comment (lib, "Effekseerd.lib")
#pragma comment (lib, "EffekseerRendererDX11d.lib")
#else // Release
#pragma comment (lib, "Effekseer.lib")
#pragma comment (lib, "EffekseerRendererDX11.lib")
#endif

/**---------------------------------------------------------------------------
  Window class and title
---------------------------------------------------------------------------**/
#define	CLASS_NAME "HEW_Window"
#define	WINDOW_CAPTION "Ivy Engine"

double g_fps = 0.0;

// Whether the window is currently in a size/move modal loop (WM_ENTERSIZEMOVE .. WM_EXITSIZEMOVE)
static volatile bool g_inSizeMove = false;
// Whether the main loop should resynchronize timing (set after exiting size/move)
static volatile bool g_needResyncTime = false;

/**---------------------------------------------------------------------------
  Function prototypes
---------------------------------------------------------------------------**/
// Window procedure
LRESULT	CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Initialization and teardown
HRESULT	Initialize(HINSTANCE hInstance, HWND hWnd, BOOL bWindow);
void Finalize(void);

// Main update/draw loop helpers
void Update(double current_time, double elapsed_time);
void Draw(void);

static bool EnvDisableRenderThread() {
	const auto v = GetEnv("IVY_DISABLE_RENDER_THREAD");
	return v && *v == "1";
}

static void JoinRenderBeforeDeviceTouch() {
	RenderThread_Join();
}

static void WaitForRenderThreadWithMessages() {
	HANDLE done = RenderThread_DoneEvent();
	if (!done) {
		RenderThread_Join();
		return;
	}

	for (;;) {
		if (!RenderThread_IsBusy()
			&& WaitForSingleObject(done, 0) == WAIT_OBJECT_0) {
			RenderThread_Join();
			return;
		}

		const DWORD wait = MsgWaitForMultipleObjects(1, &done, FALSE, INFINITE, QS_ALLINPUT);
		if (wait == WAIT_OBJECT_0) {
			RenderThread_Join();
			return;
		}
		if (wait == WAIT_OBJECT_0 + 1) {
			MSG msg;
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) {
					PostQuitMessage(static_cast<int>(msg.wParam));
					RenderThread_Join();
					return;
				}
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			continue;
		}
		RenderThread_Join();
		return;
	}
}

static void DrawWorld(void);
static void PresentFrame(void);

static void SubmitDraw() {
	if (RenderThread_IsRunning()) {
		FrameTimer_Begin(FrameTimerStage::JoinWait);
		RenderThread_Kick(&DrawWorld);
		WaitForRenderThreadWithMessages();
		FrameTimer_End(FrameTimerStage::JoinWait);
#ifndef GAME_RUNTIME
		EngineGUI_Draw();
#endif
		PresentFrame();
		return;
	}
	DrawWorld();
#ifndef GAME_RUNTIME
	EngineGUI_Draw();
#endif
	PresentFrame();
}

#ifndef GAME_RUNTIME
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif


/**===========================================================================
   WinMain - application entry point
===========================================================================**/
int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ LPSTR /*lpCmdLine*/, _In_ int nCmdShow) {
	_CrtDumpMemoryLeaks(); // dump memory leaks at process exit (if any)
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
    // Capture raw --scene text; honored only after the bridge is enabled.
    std::string runtimeTestSceneArgRaw;
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            for (int i = 1; i + 1 < argc; ++i) {
                if (wcscmp(argv[i], L"--scene") != 0)
                    continue;
                const wchar_t* wpath = argv[i + 1];
                const int bytes = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, nullptr, 0, nullptr, nullptr);
                if (bytes > 1) {
                    runtimeTestSceneArgRaw.assign(static_cast<std::size_t>(bytes - 1), '\0');
                    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, runtimeTestSceneArgRaw.data(), bytes, nullptr, nullptr);
                } else {
                    runtimeTestSceneArgRaw = "";
                    g_runtimeTestSceneArgFatal = true;
                }
                break;
            }
            LocalFree(argv);
        }
    }
#endif
    // Boot assets belong to the executable, regardless of the launcher's cwd.
    wchar_t executablePath[32768]{};
    const DWORD pathLength=GetModuleFileNameW(nullptr,executablePath,32768);
    if(pathLength==0||pathLength>=32768)return -1;
    for(DWORD i=pathLength;i>0;--i) {
        if(executablePath[i-1]==L'\\'||executablePath[i-1]==L'/') {
            executablePath[i-1]=L'\0';
            if(!SetCurrentDirectoryW(executablePath))return -1;
            break;
        }
    }

	// Set per-monitor DPI awareness BEFORE any window is created so that
	// Windows reports mouse coordinates without any DPI virtualization.
	// (Calling this after CreateWindow would leave the window in system-DPI-aware
	//  mode, causing a coordinate offset in windowed mode on high-DPI displays.)
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);

	// Register the window class
	WNDCLASS	wc;
	ZeroMemory(&wc, sizeof(WNDCLASS)); // clear structure
	wc.lpfnWndProc = WndProc;          // window procedure callback
	wc.lpszClassName = CLASS_NAME;     // class name
	wc.hInstance = hInstance;          // instance handle
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);    // default arrow cursor
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1); // background brush
	RegisterClass(&wc);	// register class with Windows

	// Default client size is the design resolution (1920x1080). Clamp so the
	// outer window fits the primary monitor work area (16:10 / smaller laptops).
	const DWORD windowStyle = WS_OVERLAPPEDWINDOW;
	int clientW = SCREEN_WIDTH;
	int clientH = SCREEN_HEIGHT;
    if(GetEnv("IVY_FRAME_CAPTURE")) {clientW=1280;clientH=720;}

	HMONITOR primaryMon = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	if (GetMonitorInfo(primaryMon, &mi)) {
		const int workW = mi.rcWork.right - mi.rcWork.left;
		const int workH = mi.rcWork.bottom - mi.rcWork.top;

		RECT outer = { 0, 0, clientW, clientH };
		AdjustWindowRect(&outer, windowStyle, FALSE);
		const int outerW = outer.right - outer.left;
		const int outerH = outer.bottom - outer.top;
		if (outerW > workW || outerH > workH) {
			const float scaleX = (float)workW / (float)outerW;
			const float scaleY = (float)workH / (float)outerH;
			const float scale = (scaleX < scaleY) ? scaleX : scaleY;
			clientW = (int)((float)clientW * scale);
			clientH = (int)((float)clientH * scale);
			if (clientW < 640) clientW = 640;
			if (clientH < 360) clientH = 360;
		}
	}

	RECT rc = { 0, 0, clientW, clientH };
	AdjustWindowRect(&rc, windowStyle, FALSE);

	// Create the main window (resizable / maximizable)
	HWND	hWnd = CreateWindow(
		CLASS_NAME,	// window class
		WINDOW_CAPTION,	// window title
		windowStyle,
		CW_USEDEFAULT,      // position
		CW_USEDEFAULT,
		rc.right - rc.left, // width
		rc.bottom - rc.top, // height
		NULL,
		NULL,
		hInstance,
		NULL
	);

#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
    ivy::testing::Bridge runtimeTestBridge;
    bool Renderer_CaptureBackBufferPNG(const wchar_t* filePath, uint32_t* outW, uint32_t* outH);
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (CrashTraceEnabled()) {
        SetUnhandledExceptionFilter(CrashTraceUnhandledExceptionFilter);
        fprintf(stderr, "[IVY_CRASH_TRACE] SetUnhandledExceptionFilter installed\n");
        fflush(stderr);
    }
#endif
    const bool runtimeTestEnabled = runtimeTestBridge.StartFromEnvironment();
    if (runtimeTestEnabled) {
        runtimeTestBridge.SetCaptureHandler(&Renderer_CaptureBackBufferPNG);
        runtimeTestBridge.SetWorldOpHandler(&RuntimeTestWorld_Apply);
        runtimeTestBridge.SetEditorOpHandler(&RuntimeTestEditor_Apply);
        g_pRuntimeTestBridge = &runtimeTestBridge;
        if (g_runtimeTestSceneArgFatal || (!runtimeTestSceneArgRaw.empty())) {
            std::string normalized, err;
            if (g_runtimeTestSceneArgFatal ||
                !ivy::testing::ValidateRuntimeTestSceneArg(runtimeTestSceneArgRaw, &normalized, &err)) {
                fprintf(stderr, "[runtime_test_bridge] invalid --scene: %s\n",
                    err.empty() ? "empty path" : err.c_str());
                fflush(stderr);
                return 2;
            }
            ivy::testing::RuntimeTestSceneOverride_Set(std::move(normalized));
        }
    } else if (!runtimeTestSceneArgRaw.empty() || g_runtimeTestSceneArgFatal) {
        fprintf(stderr, "[runtime_test_bridge] ignoring --scene (bridge not enabled)\n");
        fflush(stderr);
        ivy::testing::RuntimeTestSceneOverride_Clear();
        g_runtimeTestSceneArgFatal = false;
    }
#endif
	// Initialize application systems
	if (FAILED(Initialize(hInstance, hWnd, true))) {
		return -1; // initialization failed
	}

	// Show and update window
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// Boot preload (runtime) Presents on WinMain inside Initialize. Start the
	// render thread only after that exclusive-context work is done. Editor ImGui
	// stays on WinMain after the join (see SubmitDraw).
	if (!EnvDisableRenderThread())
		RenderThread_Start();

	// Main message / game loop
	MSG	msg;
	ZeroMemory(&msg, sizeof(MSG));

	double exec_last_time = 0.0;
	double fps_last_time = 0.0;
	double current_time = 0.0;
	double elapsed_time = 0.0;
	ULONG frame_count = 0;

	exec_last_time = fps_last_time = SystemTimer_GetTime();

	while (1) {
		// Process Windows messages if any
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				break;
			}
			else {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else {
			// If the window is being resized/moved, yield CPU and skip updating/drawing
			if (g_inSizeMove) {
				Sleep(1);
				continue;
			}

			// If we need to resync the timing after a resize/move, reset timers
			if (g_needResyncTime) {
				current_time = SystemTimer_GetTime();
				exec_last_time = current_time;
				fps_last_time = current_time;
				frame_count = 0;
				g_needResyncTime = false;
				// Yield to the OS briefly
				Sleep(0);
				continue;
			}

			current_time = SystemTimer_GetTime();

			// Compute FPS every second
			{
				double fps_elapsed = current_time - fps_last_time;
				if (fps_elapsed >= 1.0) {
					g_fps = frame_count / fps_elapsed;
					fps_last_time = current_time;
					frame_count = 0;
					FrameTimer_FlushPrint(g_fps, RenderThread_IsRunning());
				}
			}

			elapsed_time = current_time - exec_last_time;

			// Cap update to ~60 FPS
			if (elapsed_time >= (1.0 / 60.0)) {
				exec_last_time = current_time;
				FrameCapture_BeginFrame();
				FrameTimer_BeginFrame();
				JobSystem_BeginFrame();
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
                const bool runSim = !runtimeTestEnabled || runtimeTestBridge.ShouldRunSimTick();
#else
                const bool runSim = true;
#endif
				FrameTimer_Begin(FrameTimerStage::Update);
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
                if(runtimeTestEnabled) {
                    // Drain world/editor ops even while paused so pause->set_transform->impulse->step works.
                    runtimeTestBridge.MaybeApplyWorldOps();
                    runtimeTestBridge.MaybeApplyEditorOps();
                    if (runSim) {
                        runtimeTestBridge.PollAndApplyInput(EngineGUI_IsPlaying()
                            && !EngineGUI_IsSceneAssetPreloadActive() && !SceneLoader_GetCurrentScenePath().empty());
                    } else {
                        runtimeTestBridge.ExpireInputDeadlines();
                    }
                }
#endif
                if (runSim)
				    Update(current_time, elapsed_time);
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
                if(runtimeTestEnabled && runSim) {
                    runtimeTestBridge.FinishInputUpdate();
                    runtimeTestBridge.NoteSimFrame();
                    if (EngineGUI_IsPlaying())
                        runtimeTestBridge.NotePhysicsTick();
                }
#endif
#if defined(_DEBUG) && !defined(GAME_RUNTIME)
                PlaySceneSwitchCheck_Tick();
#endif
				FrameTimer_End(FrameTimerStage::Update);
				SubmitDraw();
#if defined(_DEBUG) && !defined(GAME_RUNTIME)
                PlaySceneSwitchCheck_AfterDraw();
#endif
				keycopy();
				FrameTimer_EndFrame();
                FrameCapture_EndFrame();
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
                if (runtimeTestEnabled) {
                    ivy::testing::PerfSample sample{};
                    sample.frameMs = static_cast<float>(FrameTimer_LastMs(FrameTimerStage::Frame));
                    sample.updateMs = static_cast<float>(FrameTimer_LastMs(FrameTimerStage::Update));
                    sample.drawMs = static_cast<float>(FrameTimer_LastMs(FrameTimerStage::Draw));
                    sample.presentMs = static_cast<float>(FrameTimer_LastMs(FrameTimerStage::Present));
                    {
                        const FrustumCullStats cull = FrustumCull_GetLastStats();
                        sample.cullConsidered = cull.considered;
                        sample.cullCulled = cull.culled;
                        sample.cullDrawn = cull.drawn;
                        const ShadowCascadeCullStats sc = ShadowCascadeCull_GetLastStats();
                        sample.shadowCullConsidered = sc.considered;
                        sample.shadowCullCulled = sc.culled;
                        sample.shadowCullDrawn = sc.drawn;
                        const SpriteCullStats spriteCull = SpriteCull_GetLastStats();
                        sample.spriteCullConsidered = spriteCull.considered;
                        sample.spriteCullCulled = spriteCull.culled;
                        sample.spriteCullDrawn = spriteCull.drawn;
                        const LodStats lod = Lod_GetLastStats();
                        sample.lodConsidered = lod.considered;
                        sample.lodCulledByDistance = lod.culledByDistance;
                        sample.lodLevel0 = lod.drawn[0];
                        sample.lodLevel1 = lod.drawn[1];
                        for (int level = 2; level < kLodMaxLevels; ++level)
                            sample.lodLevel2Plus += lod.drawn[level];
                    }
                    runtimeTestBridge.RecordPerfSample(sample);

                    // Update workers and render work have joined. Copy only on WinMain.
                    const std::string scene = SceneLoader_GetCurrentScenePath();
                    const bool preload = EngineGUI_IsSceneAssetPreloadActive();
                    const bool ready = !scene.empty() && !preload;
                    const char* readyDetail = nullptr;
                    if (!ready)
                        readyDetail = scene.empty() ? "scene_path_empty" : "preload_active";
                    else if (!EngineGUI_IsPlaying())
                        readyDetail = "not_playing";
                    std::vector<ivy::testing::ObservedTarget> targets;
                    std::vector<ivy::testing::ListedObject> objects;
                    if (ready && EngineGUI_IsPlaying()) {
                        std::function<void(const GameObject&)> collect = [&](const GameObject& obj) {
                            // PrefabRuntime_Destroy marks pending then SetActive(false); omit the
                            // whole subtree so observe returns found:false before FlushDestroyed.
                            if (PrefabRuntime_IsPendingDestroy(&obj))
                                return;
                            if (!obj.GetName().empty()) {
                                ivy::testing::ObservedTarget t;
                                t.name = obj.GetName();
                                t.active = obj.IsActive();
                                // Prefer Transform component (world pose via GetWorldPose) over
                                // legacy m_position/m_rotation/m_scale, which scripts that write
                                // localPosition directly leave stale after scene load.
                                {
                                    using namespace DirectX;
                                    const XMFLOAT3 legacyPos = obj.GetPosition();
                                    const XMFLOAT3 legacyRot = obj.GetRotation();
                                    const XMFLOAT3 legacyScale = obj.GetScale();
                                    ivy::testing::ObservePose legacy{
                                        {legacyPos.x, legacyPos.y, legacyPos.z},
                                        {legacyRot.x, legacyRot.y, legacyRot.z},
                                        {legacyScale.x, legacyScale.y, legacyScale.z}};
                                    ivy::testing::ObservePose fromTransform = legacy;
                                    const bool hasTransform = obj.GetTransform() != nullptr;
                                    if (hasTransform) {
                                        XMFLOAT3 worldPos{}, worldRot{}, worldScale{};
                                        obj.GetWorldPose(worldPos, worldRot, worldScale);
                                        fromTransform = {
                                            {worldPos.x, worldPos.y, worldPos.z},
                                            {worldRot.x, worldRot.y, worldRot.z},
                                            {worldScale.x, worldScale.y, worldScale.z}};
                                    }
                                    const auto pose = ivy::testing::ResolveObservedPose(
                                        hasTransform, legacy, fromTransform);
                                    ivy::testing::AssignObservedPose(t, pose);
                                    // Match Transform::GetLocalMatrix ZYX Euler convention.
                                    const XMMATRIX R =
                                        XMMatrixRotationZ(XMConvertToRadians(pose.rotation.z)) *
                                        XMMatrixRotationY(XMConvertToRadians(pose.rotation.y)) *
                                        XMMatrixRotationX(XMConvertToRadians(pose.rotation.x));
                                    XMFLOAT4 qf{};
                                    XMStoreFloat4(&qf, XMQuaternionRotationMatrix(R));
                                    t.quatX = qf.x; t.quatY = qf.y; t.quatZ = qf.z; t.quatW = qf.w;
                                }
                                if (const auto* rc = obj.GetComponent<RigidbodyComponent>()) {
                                    const auto& body = rc->Body();
                                    const auto vel = body.GetVelocity();
                                    t.velX = vel.x; t.velY = vel.y; t.velZ = vel.z;
                                    const auto ang = body.GetAngularVelocity();
                                    t.hasAngularVelocity = true;
                                    t.angVelX = ang.x; t.angVelY = ang.y; t.angVelZ = ang.z;
                                    t.hasRigidbody = true;
                                    t.kinematic = body.IsKinematic();
                                    t.mass = body.GetMass();
                                    t.drag = body.GetDrag();
                                    t.angularDrag = body.GetAngularDrag();
                                    t.friction = body.GetFriction();
                                    t.useGravity = body.UseGravity();
                                    const auto invI = body.GetLocalInverseInertia();
                                    t.hasLocalInverseInertia = true;
                                    t.invInertiaX = invI.x;
                                    t.invInertiaY = invI.y;
                                    t.invInertiaZ = invI.z;
                                }
                                targets.push_back(std::move(t));

                                ivy::testing::ListedObject listed;
                                listed.name = obj.GetName();
                                for (const auto& kv : obj.GetComponents()) {
                                    if (kv.second && kv.second->GetTypeName())
                                        listed.components.push_back(kv.second->GetTypeName());
                                }
                                objects.push_back(std::move(listed));
                            }
                            for (const auto& child : obj.GetChildren()) {
                                if (child) collect(*child);
                            }
                        };
                        for (const auto& node : SceneLoader_GetSceneNodes()) {
                            if (node) collect(*node);
                        }
                    }
                    ivy::testing::EditorPlayModeKind bridgePlayMode =
                        ivy::testing::EditorPlayModeKind::None;
                    if (EngineGUI_IsPlaying()) {
                        bridgePlayMode = (EngineGUI_GetPlayMode() == PlayMode::Fullscreen)
                            ? ivy::testing::EditorPlayModeKind::Fullscreen
                            : ivy::testing::EditorPlayModeKind::Viewport;
                    }
                    if (runtimeTestBridge.PublishAndTakeQuit(ready, EngineGUI_IsPlaying(), scene,
                            std::move(targets), std::move(objects), readyDetail, bridgePlayMode)) {
#ifdef GAME_RUNTIME
                        EngineGUI_RequestExitGame();
                        // RequestExitGame is deferred to EngineGUI_Update. While the
                        // bridge is paused, ShouldRunSimTick is false and Update is
                        // skipped, so drain the exit here on WinMain (posts WM_APP+48).
                        EngineGUI_Update(0.0);
#else
                        // Editor: RequestExitGame only ReturnToStart()s. Bridge quit
                        // must DestroyWindow (and StopPlay first if still playing).
                        EngineGUI_RequestBridgeHostQuit();
#endif
                    }
                }
#endif
				frame_count++;
			}
		}
	} // while

	ThirdPersonCamera_Shutdown();

#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (CrashTraceEnabled()) {
        fprintf(stderr, "[IVY_CRASH_TRACE] before runtimeTestBridge.Stop()\n");
        fflush(stderr);
    }
#endif
    g_pRuntimeTestBridge = nullptr;
    runtimeTestBridge.Stop();
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (CrashTraceEnabled()) {
        fprintf(stderr, "[IVY_CRASH_TRACE] after runtimeTestBridge.Stop()\n");
        fprintf(stderr, "[IVY_CRASH_TRACE] before Finalize()\n");
        fflush(stderr);
    }
#endif
#endif
	Finalize();
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (CrashTraceEnabled()) {
        fprintf(stderr, "[IVY_CRASH_TRACE] after Finalize()\n");
        fflush(stderr);
    }
#endif

	return (int)msg.wParam;
}

/**===========================================================================
   Initialize all subsystems
===========================================================================**/
HRESULT	Initialize(HINSTANCE hInstance, HWND hWnd, BOOL bWindow) {
	InitRenderer(hInstance, hWnd, bWindow); // initialize DirectX renderer

	if (!Shader_Initialize(GetDevice(), GetDeviceContext())) {
		PostQuitMessage(0);
	}
	else {
		Texture_Initialize(GetDevice(), GetDeviceContext()); // initialize texture system
		Sprite_Initialize(GetDevice(), GetDeviceContext());  // initialize sprite renderer
		LineSprite_Initialize(GetDevice(), GetDeviceContext());
	}

	SystemTimer_Initialize();
	KeyLogger_Initialize(); // keyboard logger
	Mouse_Initialize(); // mouse input
	Controller_Initialize(); // controller input
	DebugSystem::Init();
	AudioManager::Instance().Initialize();
	AudioManager::Instance().SetMasterVolume(0.3f);

	{
		unsigned int hc = std::thread::hardware_concurrency();
		int workers = (hc > 1) ? static_cast<int>(hc) - 1 : 1;
		if (workers > 8) workers = 8;
		JobSystem_Init(workers);
	}

	// Start the initial scene (Editor-first: start directly in GameScene)
	SceneManager::Instance().ChangeScene(new GameScene());
	Fade_Initialize(); // initialize fade system
	PostProcess_Initialize();
	SceneTransition_Initialize();
	EffectSystem::Initialize();

	// Debug camera settings
	DebugCamera_SetMoveSpeed(6.0f);
	DebugCamera_SetBoostMultiplier(3.0f);
	DebugCamera_SetMouseSensitivity(0.12f);

	return	S_OK;
}

/**===========================================================================
   Finalize and release subsystems
===========================================================================**/
void Finalize(void) {

	RenderThread_Stop();
    FrameCapture_Shutdown();
	JobSystem_Shutdown();
	DebugSystem::Shutdown();

	// Tear down the active scene while D3D/UI subsystems are still valid.
	// Without this, ~SceneManager runs after UninitRenderer() and corrupts the heap.
	SceneManager::Instance().SetDeleteSceneAfterFinalize(false);
	SceneManager::Instance().DestroyCurrentScene();

	SceneTransition_Finalize();
	Fade_Finalize();
	PostProcess_Shutdown();
	Controller_Finalize();
	EffectSystem::Finalize();
	LineSprite_Finalize();
	Sprite_Finalize();
	Texture_Finalize();
	ModelUtil_ReleaseTextureCache();
	Shader_Finalize();
	UninitRenderer();
	AudioManager::Instance().Finalize();
}

/**===========================================================================
   Update - called at a fixed-ish tick from the main loop
===========================================================================**/
void Update(double current_time, double elapsed_time) {
	// Update input loggers
	KeyLogger_Update(current_time);

	// Update controllers
	Controller_Update();
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
	// Consumer probe reads public getters after Controller_Update, once per
	// engine Update, while injection from PollAndApplyInput is still live.
	ivy::testing::RuntimeTestControllerProbe_SampleFromGetters();
#endif

#ifndef GAME_RUNTIME
	// Editor-only debug hotkeys (never in IvyEngineRuntime / shipped build)
	if (KeyLogger_IsTrigger(KK_F11))
		DebugSystem::ToggleCollisionViz();
	if (KeyLogger_IsTrigger(KK_F7))
		DebugSystem::CycleTimeScale();
	if (KeyLogger_IsTrigger(KK_F12))
		DebugSystem::StepOneFrame();
#endif

	// Update global effect system (uses elapsed_time)
	if(!SceneLoader_Is2D()) EffectSystem::Update(elapsed_time);

	// Update fade system
	Fade_Update();

	// Use a fixed timestep for scene/physics updates
	const double fixed_dt = 1.0 / 60.0;
	const double max_dt = 1.0 / 15.0; // clamp very large dt to avoid instability

	// Clamp elapsed_time into reasonable bounds
	if (elapsed_time > max_dt) elapsed_time = max_dt;
	if (elapsed_time < 0.0)   elapsed_time = 0.0;

	DebugSystem::UpdateTimeScalePulse(static_cast<float>(elapsed_time * 1000.0));
	const float ts = DebugSystem::GetTimeScale();
	const double scaled_dt = (SceneLoader_Uses2DPhysics()?elapsed_time:fixed_dt) * static_cast<double>(ts);
	PostProcess_Update(static_cast<float>(elapsed_time * static_cast<double>(ts)));

#ifndef GAME_RUNTIME
	// Editor-only PostFX showcase: F9 = RGB burst, F10 = toggle mosaic
	if (KeyLogger_IsTrigger(KK_F9))
		PostProcess_TriggerBurst(0.012f, 0.45f);
	if (KeyLogger_IsTrigger(KK_F10)) {
		float rgb = 0.0f;
		float mosaic = 0.0f;
		PostProcess_GetEffects(rgb, mosaic);
		PostProcess_SetEffects(rgb, (mosaic > 0.0f) ? 0.0f : 40.0f);
	}
#endif

	SceneTransition_Update(elapsed_time);

	if (Fade_IsFadeEnd() && !SceneTransition_BlocksSceneUpdate()) {
		// Freeze skips sim Update, but deferred scene-load flush / preload tick
		// lives inside EngineGUI_Update ? keep that path alive while busy.
		if (!DebugSystem::IsFrozen()) {
			SceneManager::Instance().Update(scaled_dt);
		} else if (DebugSystem::ConsumeFrameStep()) {
			SceneManager::Instance().Update(fixed_dt);
		} else if (EngineGUI_IsSceneAssetPreloadActive()) {
			SceneManager::Instance().Update(0.0);
		}
	}

	DebugSystem::RefreshOverlay();

	AudioManager::Instance().Update();
}

/**===========================================================================
   Draw - called each frame after Update
===========================================================================**/
static void PresentFrame(void) {
#if defined(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE)
	if (g_pRuntimeTestBridge) {
		g_pRuntimeTestBridge->MaybeExecuteCapture();
	}
#endif
	Present();
#ifndef GAME_RUNTIME
    EngineGUI_NotifyFramePresented();
#endif
}

static void DrawWorld(void) {
    FrameCapture_BeginDraw();
	FrameTimer_Begin(FrameTimerStage::Draw);

	// Clear the backbuffer and depth/stencil
	Clear();

	// Draw the current scene (includes Effekseer + post-process resolve before UI)
	SceneManager::Instance().Draw();

	// Draw fade overlay on top of scene
	Fade_Draw();
	SceneTransition_Draw();

	// DebugSystem overlay (all builds, including Release GAME_RUNTIME)
	{
		const char* overlay = DebugSystem::GetOverlayText();
		if (overlay && overlay[0]) {
			const float screenWf = GetBackBufferWidth();
			const float screenHf = GetBackBufferHeight();
			const UINT screenW = static_cast<UINT>(screenWf);
			const UINT screenH = static_cast<UINT>(screenHf);
			constexpr float kCharSpacing = 12.0f;
			constexpr float kLineSpacing = 14.0f;
			constexpr float kMargin = 8.0f;

			hal::DebugText dbg(GetDevice(), GetDeviceContext(),
				screenW, screenH,
				0.0f, 0.0f,
				0, 0,
				kLineSpacing, kCharSpacing);
			dbg.SetOffset(kMargin, kMargin);
			dbg.SetText(overlay, DebugSystem::GetOverlayColor());
			dbg.Draw();
			dbg.Clear();
		}
	}

	// Play FPS text follows View > Show FPS When Playing in Debug and Release.
	if (PlayFpsOverlay::ShouldDraw(EngineGUI_IsPlaying())) {
		float viewportX, viewportY, viewportWidth, viewportHeight;
		EngineGUI_GetGameViewportRect(viewportX, viewportY, viewportWidth, viewportHeight);
		if (viewportWidth > 0.0f && viewportHeight > 0.0f) {
			const float screenWf = GetBackBufferWidth();
			const float screenHf = GetBackBufferHeight();
			const UINT screenW = static_cast<UINT>(screenWf);
			const UINT screenH = static_cast<UINT>(screenHf);
			constexpr float kFpsCharSpacing = 16.0f;
			constexpr float kFpsLineSpacing = 16.0f;
			constexpr float kFpsMargin = 8.0f;

			std::stringstream ss;
			ss << "FPS:" << static_cast<int>(g_fps);
			{
				const FrustumCullStats cull = FrustumCull_GetLastStats();
				const ShadowCascadeCullStats sc = ShadowCascadeCull_GetLastStats();
				const SpriteCullStats spriteCull = SpriteCull_GetLastStats();
				ss << "\nCULL c:" << cull.considered
				   << " x:" << cull.culled
				   << " d:" << cull.drawn
				   << "\nSCULL c:" << sc.considered
				   << " x:" << sc.culled
				   << " d:" << sc.drawn
				   << "\n2CULL c:" << spriteCull.considered
				   << " x:" << spriteCull.culled
				   << " d:" << spriteCull.drawn;
				const LodStats lod = Lod_GetLastStats();
				int lodFar = 0;
				for (int level = 2; level < kLodMaxLevels; ++level)
					lodFar += lod.drawn[level];
				ss << "\nLOD c:" << lod.considered
				   << " x:" << lod.culledByDistance
				   << " 0:" << lod.drawn[0]
				   << " 1:" << lod.drawn[1]
				   << " 2+:" << lodFar;
			}
			const std::string fpsText = ss.str();

			hal::DebugText dt(GetDevice(), GetDeviceContext(),
				screenW, screenH,
				0.0f, 0.0f,
				0, 0,
				kFpsLineSpacing, kFpsCharSpacing);

			float fpsTextW = 0.0f;
			float fpsTextH = 0.0f;
			dt.MeasureText(fpsText.c_str(), fpsTextW, fpsTextH);
			dt.SetOffset(viewportX + kFpsMargin, viewportY + viewportHeight - fpsTextH - kFpsMargin);
			dt.SetText(fpsText.c_str(), { 0.1f, 0.5f, 0.2f, 1.0f });
			dt.Draw();
			dt.Clear();
		}
	}

	AudioManager::Instance().DrawDebug();

	FrameTimer_End(FrameTimerStage::Draw);
    FrameCapture_EndDraw();
}

void Draw(void) {
	DrawWorld();
#ifndef GAME_RUNTIME
	EngineGUI_Draw();
#endif
	PresentFrame();
}

/**===========================================================================
   Window procedure - handles Windows messages
===========================================================================**/
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	// Alt+Enter: borderless windowed fullscreen (handle before ImGui so it always works).
	if (uMsg == WM_SYSKEYDOWN && wParam == VK_RETURN && (lParam & (1 << 29))) {
		JoinRenderBeforeDeviceTouch();
		Renderer_SetWindowMode(Renderer_IsWindowed() ? FALSE : TRUE);
		return 0;
	}

#ifndef GAME_RUNTIME
	// Allow ImGui to process Windows messages first
	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;
#endif
	switch (uMsg) {
	case WM_ACTIVATEAPP:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		Keyboard_ProcessMessage(uMsg, wParam, lParam);
		break;

	case WM_KEYDOWN: // key pressed
		// Escape is reserved for in-game pause; quit via the window close button.
		Keyboard_ProcessMessage(uMsg, wParam, lParam);
		break;

		// Mouse input messages
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_MOUSELEAVE:
	case WM_MOUSEHOVER:
	case WM_NCMOUSEMOVE:
		Mouse_ProcessMessage(uMsg, wParam, lParam, hWnd);
		break;

		// Enter size/move modal loop (user is resizing or moving the window)
	case WM_ENTERSIZEMOVE:
		JoinRenderBeforeDeviceTouch();
		g_inSizeMove = true;
		return 0;

	case WM_EXITSIZEMOVE:
		JoinRenderBeforeDeviceTouch();
		g_inSizeMove = false;
		g_needResyncTime = true; // request timer resync after resizing/moving
		Renderer_ResizeToClient();
#ifndef GAME_RUNTIME
		EngineGUI_OnBackbufferResized();
#endif
		return 0;

	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) {
			JoinRenderBeforeDeviceTouch();
			Renderer_ResizeToClient();
#ifndef GAME_RUNTIME
			EngineGUI_OnBackbufferResized();
#endif
		}
		return 0;

	case WM_DPICHANGED:
		if (lParam) {
			const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
			SetWindowPos(hWnd, nullptr,
				suggested->left, suggested->top,
				suggested->right - suggested->left,
				suggested->bottom - suggested->top,
				SWP_NOZORDER | SWP_NOACTIVATE);
		}
		JoinRenderBeforeDeviceTouch();
		Renderer_ResizeToClient();
#ifndef GAME_RUNTIME
		EngineGUI_OnBackbufferResized();
#endif
		return 0;

    // Confirmed exit (Runtime ExitGame / bridge quit). Bypasses WM_CLOSE
    // confirmations and Editor unsaved popup — only posted by intentional quit paths.
    case WM_APP + 48:
        JoinRenderBeforeDeviceTouch();
        DestroyWindow(hWnd);
        return 0;
	case WM_CLOSE: // window close requested
		JoinRenderBeforeDeviceTouch();
#ifdef _DEBUG
		if (GetEnv("IVY_SKIP_CLOSE_CONFIRM")) {
			DestroyWindow(hWnd);
			return 0;
		}
#endif
#ifndef GAME_RUNTIME
		if (EngineGUI_IsSceneDirty()) {
			EngineGUI_QueueUnsavedExit();
			return 0; // cancel close; unsaved popup handles decision
		}
#endif
		if (MessageBox(hWnd, "Are you sure you want to exit?", "Confirm",
			MB_OKCANCEL | MB_DEFBUTTON2) == IDOK) {
			DestroyWindow(hWnd);
		}
		else {
			return 0; // user cancelled close
		}
		break;

	case WM_DESTROY: // window is being destroyed
		JoinRenderBeforeDeviceTouch();
		PostQuitMessage(0); // signal message loop to quit
		break;
	}

	// Default message handling
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}