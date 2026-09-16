/**===========================================================================

 File   :  [main.cpp]

 Author : Terry Yeung
 Date   : 2025/9/29
 -----------------------------------------------------------------------------

 ===========================================================================**/
#define _CRTDBG_MAP_ALLOC // enable CRT memory leak checking
#include <stdlib.h>       // for _CrtDumpMemoryLeaks, getenv
#include <crtdbg.h>       // for _CrtDumpMemoryLeaks
#include <thread>

#include "main.h"

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
#include "engine/input/mouse.h"
#include "engine/input/controller.h"
#include "debug_camera.h"
#include "engine/debug_draw/debug_text.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "engine/core/engine_gui.h"

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

#ifndef GAME_RUNTIME
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif


/**===========================================================================
   WinMain - application entry point
===========================================================================**/
int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ LPSTR /*lpCmdLine*/, _In_ int nCmdShow) {
	_CrtDumpMemoryLeaks(); // dump memory leaks at process exit (if any)

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

	// Initialize application systems
	if (FAILED(Initialize(hInstance, hWnd, true))) {
		return -1; // initialization failed
	}

	// Show and update window
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

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
					printf("FPS: %.1f\n", g_fps);
				}
			}

			elapsed_time = current_time - exec_last_time;

			// Cap update to ~60 FPS
			if (elapsed_time >= (1.0 / 60.0)) {
				exec_last_time = current_time;
				Update(current_time, elapsed_time);
				Draw();
				keycopy();
				frame_count++;
			}
		}
	} // while

	Finalize();

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
	EffectSystem::Update(elapsed_time);

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
	const double scaled_dt = fixed_dt * static_cast<double>(ts);
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
void Draw(void) {
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
				L"rom/texture/consolab_ascii_512.png",
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

	// FPS debug text (bottom-left, half default size)
#if defined(DEBUG) || defined(_DEBUG)
	const float screenWf = GetBackBufferWidth();
	const float screenHf = GetBackBufferHeight();
	const UINT screenW = static_cast<UINT>(screenWf);
	const UINT screenH = static_cast<UINT>(screenHf);
	constexpr float kFpsCharSpacing = 16.0f;
	constexpr float kFpsLineSpacing = 16.0f;
	constexpr float kFpsMargin = 8.0f;

	std::stringstream ss;
	ss << "FPS:" << static_cast<int>(g_fps);
	const std::string fpsText = ss.str();

	hal::DebugText dt(GetDevice(), GetDeviceContext(),
		L"rom/texture/consolab_ascii_512.png",
		screenW, screenH,
		0.0f, 0.0f,
		0, 0,
		kFpsLineSpacing, kFpsCharSpacing);

	float fpsTextW = 0.0f;
	float fpsTextH = 0.0f;
	dt.MeasureText(fpsText.c_str(), fpsTextW, fpsTextH);
	dt.SetOffset(kFpsMargin, screenHf - fpsTextH - kFpsMargin);
	dt.SetText(fpsText.c_str(), { 0.1f, 0.5f, 0.2f, 1.0f });
	dt.Draw();
	dt.Clear();
#endif

	AudioManager::Instance().DrawDebug();

	// Present the rendered frame
	Present();
}

/**===========================================================================
   Window procedure - handles Windows messages
===========================================================================**/
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	// Alt+Enter: borderless windowed fullscreen (handle before ImGui so it always works).
	if (uMsg == WM_SYSKEYDOWN && wParam == VK_RETURN && (lParam & (1 << 29))) {
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
		g_inSizeMove = true;
		return 0;

	case WM_EXITSIZEMOVE:
		g_inSizeMove = false;
		g_needResyncTime = true; // request timer resync after resizing/moving
		Renderer_ResizeToClient();
#ifndef GAME_RUNTIME
		EngineGUI_OnBackbufferResized();
#endif
		return 0;

	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) {
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
		Renderer_ResizeToClient();
#ifndef GAME_RUNTIME
		EngineGUI_OnBackbufferResized();
#endif
		return 0;

	case WM_CLOSE: // window close requested
#ifdef _DEBUG
		if (getenv("IVY_SKIP_CLOSE_CONFIRM")) {
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
		PostQuitMessage(0); // signal message loop to quit
		break;
	}

	// Default message handling
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}