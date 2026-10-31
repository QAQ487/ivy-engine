/**===========================================================================

 File   :  [renderer.cpp]

 Author : Terry Yeung
 Date   : 2025/9/29
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "renderer.h"
#include "engine/core/frame_timer.h"
#include "engine/scene/render_settings.h"
#include <d3d11sdklayers.h> // InfoQueue
#include <wrl/client.h> // For ComPtr
#include <cstdio>

 // Globals
static D3D_FEATURE_LEVEL g_FeatureLevel = D3D_FEATURE_LEVEL_11_0;

static ID3D11Device* g_D3DDevice = nullptr;
static ID3D11DeviceContext* g_ImmediateContext = nullptr;
static IDXGISwapChain* g_SwapChain = nullptr;
static ID3D11RenderTargetView* g_RenderTargetView = nullptr;
static ID3D11DepthStencilView* g_DepthStencilView = nullptr;

static ID3D11Texture2D* g_PostRTTexture = nullptr;
static ID3D11RenderTargetView* g_PostRTV = nullptr;
static ID3D11ShaderResourceView* g_PostSRV = nullptr;

static ID3D11RasterizerState* g_RasterizerState = nullptr;
static ID3D11RasterizerState* g_RasterizerStateCullFront = nullptr;
static ID3D11BlendState* g_BlendState = nullptr;
static ID3D11DepthStencilState* g_DepthStateEnable = nullptr;
static ID3D11DepthStencilState* g_DepthStateDisable = nullptr;

static HWND g_hWnd = nullptr;
static D3D11_VIEWPORT g_CurrentViewport = {};

static float g_BackBufferWidth = (float)SCREEN_WIDTH;
static float g_BackBufferHeight = (float)SCREEN_HEIGHT;
static float g_ClientWidth = (float)SCREEN_WIDTH;
static float g_ClientHeight = (float)SCREEN_HEIGHT;
static BOOL g_Windowed = TRUE;

// Saved windowed geometry for borderless fullscreen toggle (not DXGI exclusive).
static DWORD g_SavedStyle = 0;
static DWORD g_SavedExStyle = 0;
static WINDOWPLACEMENT g_SavedPlacement = { sizeof(WINDOWPLACEMENT) };
static bool g_HasSavedPlacement = false;

static HRESULT CreateDepthBuffer(UINT width, UINT height) {
	if (!g_D3DDevice) return E_FAIL;

	if (g_DepthStencilView) {
		g_DepthStencilView->Release();
		g_DepthStencilView = nullptr;
	}

	ID3D11Texture2D* depthTexture = nullptr;
	D3D11_TEXTURE2D_DESC td = {};
	td.Width = width;
	td.Height = height;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	td.SampleDesc.Count = 1;
	td.SampleDesc.Quality = 0;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	HRESULT hr = g_D3DDevice->CreateTexture2D(&td, nullptr, &depthTexture);
	if (FAILED(hr) || !depthTexture)
		return hr;

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};
	dsvd.Format = td.Format;
	dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

	hr = g_D3DDevice->CreateDepthStencilView(depthTexture, &dsvd, &g_DepthStencilView);
	depthTexture->Release();
	return hr;
}

static HRESULT CreateRenderTargetFromSwapChain() {
	if (!g_SwapChain || !g_D3DDevice) return E_FAIL;

	if (g_RenderTargetView) {
		g_RenderTargetView->Release();
		g_RenderTargetView = nullptr;
	}

	ID3D11Texture2D* pBackBuffer = nullptr;
	HRESULT hr = g_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	if (FAILED(hr) || !pBackBuffer)
		return hr;

	hr = g_D3DDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_RenderTargetView);
	pBackBuffer->Release();
	return hr;
}

static void PostRT_ReleaseResources() {
	if (g_PostSRV) { g_PostSRV->Release(); g_PostSRV = nullptr; }
	if (g_PostRTV) { g_PostRTV->Release(); g_PostRTV = nullptr; }
	if (g_PostRTTexture) { g_PostRTTexture->Release(); g_PostRTTexture = nullptr; }
}

HRESULT PostRT_Resize(UINT w, UINT h) {
	if (!g_D3DDevice || !g_SwapChain || w == 0 || h == 0)
		return E_INVALIDARG;

	PostRT_ReleaseResources();

	ID3D11Texture2D* pBackBuffer = nullptr;
	HRESULT hr = g_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	if (FAILED(hr) || !pBackBuffer)
		return FAILED(hr) ? hr : E_FAIL;

	D3D11_TEXTURE2D_DESC bbDesc = {};
	pBackBuffer->GetDesc(&bbDesc);
	pBackBuffer->Release();

	D3D11_TEXTURE2D_DESC td = {};
	td.Width = w;
	td.Height = h;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = bbDesc.Format;
	td.SampleDesc.Count = 1;
	td.SampleDesc.Quality = 0;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	hr = g_D3DDevice->CreateTexture2D(&td, nullptr, &g_PostRTTexture);
	if (FAILED(hr) || !g_PostRTTexture)
		return hr;

	hr = g_D3DDevice->CreateRenderTargetView(g_PostRTTexture, nullptr, &g_PostRTV);
	if (FAILED(hr) || !g_PostRTV) {
		PostRT_ReleaseResources();
		return hr;
	}

	hr = g_D3DDevice->CreateShaderResourceView(g_PostRTTexture, nullptr, &g_PostSRV);
	if (FAILED(hr) || !g_PostSRV) {
		PostRT_ReleaseResources();
		return hr;
	}

	return S_OK;
}

HRESULT PostRT_Initialize() {
	return PostRT_Resize((UINT)g_BackBufferWidth, (UINT)g_BackBufferHeight);
}

void PostRT_Shutdown() {
	PostRT_ReleaseResources();
}

ID3D11RenderTargetView* PostRT_GetRTV() {
	return g_PostRTV;
}

ID3D11ShaderResourceView* PostRT_GetSRV() {
	return g_PostSRV;
}

void Renderer_BindPostTarget() {
	if (!g_ImmediateContext || !g_PostRTV)
		return;

	g_ImmediateContext->OMSetRenderTargets(1, &g_PostRTV, g_DepthStencilView);

	const DirectX::XMFLOAT4& cc = RenderSettings_GetConst().clearColor;
	const float clearColor[4] = { cc.x, cc.y, cc.z, cc.w };
	g_ImmediateContext->ClearRenderTargetView(g_PostRTV, clearColor);
	if (g_DepthStencilView)
		g_ImmediateContext->ClearDepthStencilView(g_DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void Renderer_BindBackBuffer() {
	if (!g_ImmediateContext || !g_RenderTargetView)
		return;
	g_ImmediateContext->OMSetRenderTargets(1, &g_RenderTargetView, g_DepthStencilView);
}

static void ReadClientSize(HWND hWnd, UINT& width, UINT& height) {
	width = SCREEN_WIDTH;
	height = SCREEN_HEIGHT;
	if (!hWnd) return;

	RECT rect = {};
	if (::GetClientRect(hWnd, &rect)) {
		const int w = rect.right - rect.left;
		const int h = rect.bottom - rect.top;
		if (w > 0 && h > 0) {
			width = (UINT)w;
			height = (UINT)h;
		}
	}
}

ID3D11Device* GetDevice(void) {
	return g_D3DDevice;
}

ID3D11DeviceContext* GetDeviceContext(void) {
	return g_ImmediateContext;
}

void SetDepthEnable(bool Enable) {
	if (!g_ImmediateContext) return;
	if (Enable)
		g_ImmediateContext->OMSetDepthStencilState(g_DepthStateEnable, 0);
	else
		g_ImmediateContext->OMSetDepthStencilState(g_DepthStateDisable, 0);
}

void SetCullFront(bool enable) {
	if (!g_ImmediateContext) return;
	if (enable && g_RasterizerStateCullFront)
		g_ImmediateContext->RSSetState(g_RasterizerStateCullFront);
	else if (g_RasterizerState)
		g_ImmediateContext->RSSetState(g_RasterizerState);
}

float GetBackBufferWidth() {
	return g_BackBufferWidth;
}

float GetBackBufferHeight() {
	return g_BackBufferHeight;
}

float GetClientWidth() {
	return g_ClientWidth;
}

float GetClientHeight() {
	return g_ClientHeight;
}

void Renderer_ClientToBackbuffer(float clientX, float clientY, float& bufX, float& bufY) {
	if (g_ClientWidth > 0.0f && g_ClientHeight > 0.0f) {
		bufX = clientX * (g_BackBufferWidth / g_ClientWidth);
		bufY = clientY * (g_BackBufferHeight / g_ClientHeight);
	}
	else {
		bufX = clientX;
		bufY = clientY;
	}
}

void Renderer_BackbufferToClient(float bufX, float bufY, float& clientX, float& clientY) {
	if (g_BackBufferWidth > 0.0f && g_BackBufferHeight > 0.0f) {
		clientX = bufX * (g_ClientWidth / g_BackBufferWidth);
		clientY = bufY * (g_ClientHeight / g_BackBufferHeight);
	}
	else {
		clientX = bufX;
		clientY = bufY;
	}
}

HRESULT Renderer_UpdateClientSize() {
	if (!g_hWnd) return S_OK;

	UINT width = 0;
	UINT height = 0;
	ReadClientSize(g_hWnd, width, height);
	g_ClientWidth = (float)width;
	g_ClientHeight = (float)height;
	return S_OK;
}

HRESULT Renderer_ResizeBuffers(UINT width, UINT height) {
	if (!g_SwapChain || !g_ImmediateContext || width == 0 || height == 0)
		return E_INVALIDARG;

	if ((UINT)g_BackBufferWidth == width && (UINT)g_BackBufferHeight == height)
		return S_OK;

#if defined(DEBUG) || defined(_DEBUG)
	std::fprintf(stderr,
		"Renderer: resizing backbuffer %ux%u -> %ux%u (client %ux%u)\n",
		(UINT)g_BackBufferWidth, (UINT)g_BackBufferHeight,
		width, height,
		(UINT)g_ClientWidth, (UINT)g_ClientHeight);
#endif

	g_ImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);

	// Must release all views that reference swap-chain buffers before ResizeBuffers
	// (DXGI_ERROR_INVALID_CALL / 0x887A0001 if outstanding refs remain).
	PostRT_ReleaseResources();
	if (g_RenderTargetView) {
		g_RenderTargetView->Release();
		g_RenderTargetView = nullptr;
	}
	if (g_DepthStencilView) {
		g_DepthStencilView->Release();
		g_DepthStencilView = nullptr;
	}

	HRESULT hr = g_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) {
		// Recover previous-size views so OM is not left unbound.
		CreateRenderTargetFromSwapChain();
		CreateDepthBuffer((UINT)g_BackBufferWidth, (UINT)g_BackBufferHeight);
		PostRT_Resize((UINT)g_BackBufferWidth, (UINT)g_BackBufferHeight);
		if (g_RenderTargetView)
			g_ImmediateContext->OMSetRenderTargets(1, &g_RenderTargetView, g_DepthStencilView);
		return hr;
	}

	g_BackBufferWidth = (float)width;
	g_BackBufferHeight = (float)height;

	hr = CreateRenderTargetFromSwapChain();
	if (FAILED(hr)) return hr;

	hr = CreateDepthBuffer(width, height);
	if (FAILED(hr)) return hr;

	hr = PostRT_Resize(width, height);
	if (FAILED(hr)) return hr;

	g_ImmediateContext->OMSetRenderTargets(1, &g_RenderTargetView, g_DepthStencilView);
	ResetRenderViewport();
	return S_OK;
}

HRESULT Renderer_ResizeToClient() {
	Renderer_UpdateClientSize();

	const UINT width = (UINT)g_ClientWidth;
	const UINT height = (UINT)g_ClientHeight;
	if (width == 0 || height == 0)
		return S_OK;

#if defined(DEBUG) || defined(_DEBUG)
	if (width != (UINT)g_BackBufferWidth || height != (UINT)g_BackBufferHeight) {
		std::fprintf(stderr,
			"Renderer: client/backbuffer mismatch %ux%u vs %ux%u -- syncing\n",
			width, height,
			(UINT)g_BackBufferWidth, (UINT)g_BackBufferHeight);
	}
#endif

	return Renderer_ResizeBuffers(width, height);
}

BOOL Renderer_IsWindowed() {
	return g_Windowed;
}

HRESULT Renderer_SetWindowMode(BOOL bWindow) {
	if (!g_hWnd)
		return E_FAIL;

	if (g_Windowed == bWindow)
		return S_OK;

	if (!bWindow) {
		// Enter borderless windowed fullscreen on the nearest monitor.
		g_SavedStyle = (DWORD)::GetWindowLong(g_hWnd, GWL_STYLE);
		g_SavedExStyle = (DWORD)::GetWindowLong(g_hWnd, GWL_EXSTYLE);
		g_SavedPlacement = {};
		g_SavedPlacement.length = sizeof(WINDOWPLACEMENT);
		g_HasSavedPlacement = (::GetWindowPlacement(g_hWnd, &g_SavedPlacement) != FALSE);

		HMONITOR mon = ::MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = {};
		mi.cbSize = sizeof(mi);
		if (!::GetMonitorInfo(mon, &mi))
			return E_FAIL;

		::SetWindowLong(g_hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		::SetWindowPos(
			g_hWnd,
			HWND_TOP,
			mi.rcMonitor.left,
			mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_SHOWWINDOW);

		g_Windowed = FALSE;
	}
	else {
		// Restore previous windowed style and placement.
		if (g_SavedStyle != 0)
			::SetWindowLong(g_hWnd, GWL_STYLE, (LONG)g_SavedStyle);
		if (g_SavedExStyle != 0 || g_HasSavedPlacement)
			::SetWindowLong(g_hWnd, GWL_EXSTYLE, (LONG)g_SavedExStyle);

		if (g_HasSavedPlacement) {
			::SetWindowPlacement(g_hWnd, &g_SavedPlacement);
		}
		else {
			::ShowWindow(g_hWnd, SW_RESTORE);
		}

		::SetWindowPos(
			g_hWnd,
			nullptr,
			0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

		g_Windowed = TRUE;
	}

	// WM_SIZE from SetWindowPos also resizes; call explicitly so callers
	// (e.g. before message pump processes WM_SIZE) still sync the backbuffer.
	return Renderer_ResizeToClient();
}

void SetRenderViewport(float x, float y, float width, float height) {
	if (!g_ImmediateContext) return;

	if (width < 1.0f) width = 1.0f;
	if (height < 1.0f) height = 1.0f;

	g_CurrentViewport = {};
	g_CurrentViewport.TopLeftX = x;
	g_CurrentViewport.TopLeftY = y;
	g_CurrentViewport.Width = width;
	g_CurrentViewport.Height = height;
	g_CurrentViewport.MinDepth = 0.0f;
	g_CurrentViewport.MaxDepth = 1.0f;

	g_ImmediateContext->RSSetViewports(1, &g_CurrentViewport);
}

void ResetRenderViewport() {
	SetRenderViewport(0.0f, 0.0f, GetBackBufferWidth(), GetBackBufferHeight());
}

float GetRenderViewportWidth() {
	return g_CurrentViewport.Width > 0.0f ? g_CurrentViewport.Width : GetBackBufferWidth();
}

float GetRenderViewportHeight() {
	return g_CurrentViewport.Height > 0.0f ? g_CurrentViewport.Height : GetBackBufferHeight();
}

HWND GetWindowHandle() {
	return g_hWnd;
}

HRESULT InitRenderer(HINSTANCE /*hInstance*/, HWND hWnd, BOOL bWindow) {
	HRESULT hr = S_OK;

	g_hWnd = hWnd;
	g_Windowed = bWindow;

	UINT clientW = SCREEN_WIDTH;
	UINT clientH = SCREEN_HEIGHT;
	ReadClientSize(hWnd, clientW, clientH);
	g_ClientWidth = (float)clientW;
	g_ClientHeight = (float)clientH;
	g_BackBufferWidth = (float)clientW;
	g_BackBufferHeight = (float)clientH;

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Width = clientW;
	sd.BufferDesc.Height = clientH;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = bWindow ? TRUE : FALSE;

	UINT createFlags = 0;
#if defined(_DEBUG)
	createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	hr = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createFlags,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&sd,
		&g_SwapChain,
		&g_D3DDevice,
		&g_FeatureLevel,
		&g_ImmediateContext);
	if (FAILED(hr)) return hr;

	// Disable DXGI's default Alt+Enter exclusive fullscreen (breaks ImGui / RTV).
	{
		IDXGIFactory* factory = nullptr;
		if (SUCCEEDED(g_SwapChain->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory))) && factory) {
			factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
			factory->Release();
		}
	}

#if defined(_DEBUG)
	{
		Microsoft::WRL::ComPtr<ID3D11InfoQueue> infoQueue;
		if (SUCCEEDED(g_D3DDevice->QueryInterface(__uuidof(ID3D11InfoQueue), reinterpret_cast<void**>(infoQueue.GetAddressOf())))) {
			infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);

			D3D11_MESSAGE_SEVERITY denySeverities[] = {
				D3D11_MESSAGE_SEVERITY_INFO
			};
			D3D11_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumSeverities = _countof(denySeverities);
			filter.DenyList.pSeverityList = denySeverities;
			infoQueue->AddStorageFilterEntries(&filter);
		}
	}
#endif

	hr = CreateRenderTargetFromSwapChain();
	if (FAILED(hr) || !g_RenderTargetView) {
		MessageBox(NULL, "Failed to create RenderTargetView.", "Error", MB_OK | MB_ICONERROR);
		return hr;
	}

	hr = CreateDepthBuffer(clientW, clientH);
	if (FAILED(hr) || !g_DepthStencilView) {
		MessageBox(NULL, "Failed to create DepthStencilView.", "Error", MB_OK | MB_ICONERROR);
		return hr;
	}

	hr = PostRT_Initialize();
	if (FAILED(hr) || !g_PostRTV) {
		MessageBox(NULL, "Failed to create PostRT.", "Error", MB_OK | MB_ICONERROR);
		return hr;
	}

	g_ImmediateContext->OMSetRenderTargets(1, &g_RenderTargetView, g_DepthStencilView);
	ResetRenderViewport();

	D3D11_RASTERIZER_DESC rd = {};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_BACK;
	rd.DepthClipEnable = TRUE;
	rd.MultisampleEnable = FALSE;

	hr = g_D3DDevice->CreateRasterizerState(&rd, &g_RasterizerState);
	if (FAILED(hr)) {
		MessageBox(NULL, "Failed to create RasterizerState.", "Error", MB_OK | MB_ICONERROR);
		return hr;
	}
	g_ImmediateContext->RSSetState(g_RasterizerState);

	rd.CullMode = D3D11_CULL_FRONT;
	hr = g_D3DDevice->CreateRasterizerState(&rd, &g_RasterizerStateCullFront);
	if (FAILED(hr)) {
		MessageBox(NULL, "Failed to create RasterizerState (front cull).", "Error", MB_OK | MB_ICONERROR);
		return hr;
	}

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = g_D3DDevice->CreateBlendState(&blendDesc, &g_BlendState);
	if (FAILED(hr)) {
		MessageBox(NULL, "Failed to create BlendState.", "Error", MB_OK | MB_ICONERROR);
		return hr;
	}
	const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	g_ImmediateContext->OMSetBlendState(g_BlendState, blendFactor, 0xffffffff);

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthStencilDesc.StencilEnable = FALSE;
	hr = g_D3DDevice->CreateDepthStencilState(&depthStencilDesc, &g_DepthStateEnable);
	if (FAILED(hr)) return hr;

	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	hr = g_D3DDevice->CreateDepthStencilState(&depthStencilDesc, &g_DepthStateDisable);
	if (FAILED(hr)) return hr;

	g_ImmediateContext->OMSetDepthStencilState(g_DepthStateEnable, 0);

#if defined(DEBUG) || defined(_DEBUG)
	std::fprintf(stderr,
		"Renderer: init client=%ux%u backbuffer=%ux%u windowed=%d\n",
		clientW, clientH, clientW, clientH, bWindow ? 1 : 0);
#endif

	return S_OK;
}

void UninitRenderer(void) {
	if (g_ImmediateContext) g_ImmediateContext->ClearState();

	if (g_DepthStateDisable) { g_DepthStateDisable->Release(); g_DepthStateDisable = nullptr; }
	if (g_DepthStateEnable) { g_DepthStateEnable->Release();  g_DepthStateEnable = nullptr; }
	if (g_BlendState) { g_BlendState->Release();        g_BlendState = nullptr; }
	if (g_RasterizerState) { g_RasterizerState->Release();   g_RasterizerState = nullptr; }
	if (g_RasterizerStateCullFront) { g_RasterizerStateCullFront->Release(); g_RasterizerStateCullFront = nullptr; }

	if (g_DepthStencilView) { g_DepthStencilView->Release();  g_DepthStencilView = nullptr; }
	if (g_RenderTargetView) { g_RenderTargetView->Release();  g_RenderTargetView = nullptr; }
	PostRT_Shutdown();
	if (g_SwapChain) { g_SwapChain->Release();         g_SwapChain = nullptr; }

	if (g_ImmediateContext) { g_ImmediateContext->Release();  g_ImmediateContext = nullptr; }
	if (g_D3DDevice) { g_D3DDevice->Release();         g_D3DDevice = nullptr; }

	g_hWnd = nullptr;
}

void Clear(void) {
	if (!g_ImmediateContext) return;
	const DirectX::XMFLOAT4& cc = RenderSettings_GetConst().clearColor;
	const float ClearColor[4] = { cc.x, cc.y, cc.z, cc.w };
	if (g_RenderTargetView) g_ImmediateContext->ClearRenderTargetView(g_RenderTargetView, ClearColor);
	if (g_DepthStencilView) g_ImmediateContext->ClearDepthStencilView(g_DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void Present(void) {
	FrameTimer_Begin(FrameTimerStage::Present);
	if (g_SwapChain) g_SwapChain->Present(0, 0);
	FrameTimer_End(FrameTimerStage::Present);
}


#if defined(_DEBUG) && defined(GAME_RUNTIME)
#include "libraries/directxtex/DirectXTex.h"
#pragma comment(lib, "windowscodecs.lib")

bool Renderer_CaptureBackBufferPNG(const wchar_t* filePath, uint32_t* outW, uint32_t* outH) {
    if (!g_SwapChain || !g_D3DDevice || !g_ImmediateContext || !filePath) return false;
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = g_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    if (FAILED(hr) || !pBackBuffer) return false;

    D3D11_TEXTURE2D_DESC desc = {};
    pBackBuffer->GetDesc(&desc);
    if (outW) *outW = desc.Width;
    if (outH) *outH = desc.Height;

    DirectX::ScratchImage scratch;
    hr = DirectX::CaptureTexture(g_D3DDevice, g_ImmediateContext, pBackBuffer, scratch);
    pBackBuffer->Release();
    if (FAILED(hr)) return false;

    const DirectX::Image* img = scratch.GetImage(0, 0, 0);
    if (!img) return false;

    hr = DirectX::SaveToWICFile(*img, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), filePath);
    return SUCCEEDED(hr);
}
#endif
