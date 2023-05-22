#include "DroidSans.h"
#include "Helpers.h"
#include "PluginAPI.h"
#include <chrono>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <vector>

typedef struct string {
	union {
		char data[16];
		char *ptr;
	};
	u64 length;
	u64 capacity;
} string;

char *
getStringInternal (string *str) {
	if (str->capacity > 15) return str->ptr;
	else return str->data;
}

typedef struct subtitle {
	int duration;
	char *subtitle;
	char *name;
} subtitle;

extern "C" subtitle get_subtitle (char *);

typedef enum subtitle_state {
	fade_in,
	show,
	fade_out,
} subtitle_state;

typedef struct showing_subtitle {
	subtitle_state state;
	f32 opacity;
	subtitle subtitle;
	std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
} showing_subtitle;

std::vector<showing_subtitle> showing_subtitles;

bool
operator== (showing_subtitle lhs, subtitle const rhs) {
	return strcmp (lhs.subtitle.name, rhs.name) == 0;
}

void
show_subtitle (string *name) {
	subtitle sub = get_subtitle (getStringInternal (name));
	if (!strcmp (sub.subtitle, "")) return;
	showing_subtitles.push_back (showing_subtitle{fade_in, 0.0, sub, std::chrono::high_resolution_clock::now () + std::chrono::milliseconds (sub.duration)});
}

extern LRESULT ImGui_ImplWin32_WndProcHandler (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC originalWndProc;

i64
WndProc (const HWND window, u32 message, u64 wParam, i64 lParam) {
	if (ImGui_ImplWin32_WndProcHandler (window, message, wParam, lParam)) return true;
	return CallWindowProc (originalWndProc, window, message, wParam, lParam);
}

ID3D11DeviceContext *pContext;
ID3D11RenderTargetView *mainRenderTargetView;

f32 font_size;
f32 imguiWidth;
f32 imguiHeight;
f32 imguiPosX;
f32 imguiPosY;
bool resChanged = false;

void
resize (IDXGISwapChain *This) {
	DXGI_SWAP_CHAIN_DESC sd;
	ID3D11Texture2D *pBackBuffer;
	ID3D11Device *pDevice;

	This->GetDevice (__uuidof (ID3D11Device), (void **)&pDevice);
	pDevice->GetImmediateContext (&pContext);

	This->GetBuffer (0, __uuidof (ID3D11Texture2D), (void **)&pBackBuffer);
	pDevice->CreateRenderTargetView (pBackBuffer, 0, &mainRenderTargetView);
	pBackBuffer->Release ();

	This->GetDesc (&sd);
	HWND windowHandle = sd.OutputWindow;

	RECT windowRect;
	GetWindowRect (windowHandle, &windowRect);
	f32 width   = windowRect.right - windowRect.left;
	f32 height  = windowRect.bottom - windowRect.top;
	imguiWidth  = width / 2.5;
	imguiHeight = font_size * 5;
	imguiPosX   = width / 2;
	imguiPosY   = height / 1.25;
	resChanged  = true;
}

VTABLE_HOOK (HRESULT, __stdcall, IDXGISwapChain, ResizeBuffers, u32 BufferCount, u32 Width, u32 Height, DXGI_FORMAT NewFormat, u32 SwapChainFlags) {
	HRESULT res = originalIDXGISwapChainResizeBuffers (This, BufferCount, Width, Height, NewFormat, SwapChainFlags);
	resize (This);
	return res;
}

VTABLE_HOOK (HRESULT, __stdcall, IDXGISwapChain, ResizeTarget, void *params) {
	HRESULT res = originalIDXGISwapChainResizeTarget (This, params);
	resize (This);
	return res;
}

VTABLE_HOOK (i32, __stdcall, IDXGISwapChain, Present, u32 sync, u32 flags) {
	static bool inited = false;
	if (!inited) {
		DXGI_SWAP_CHAIN_DESC sd;
		ID3D11Texture2D *pBackBuffer;
		ID3D11Device *pDevice;

		This->GetDevice (__uuidof (ID3D11Device), (void **)&pDevice);
		pDevice->GetImmediateContext (&pContext);

		This->GetBuffer (0, __uuidof (ID3D11Texture2D), (void **)&pBackBuffer);
		pDevice->CreateRenderTargetView (pBackBuffer, 0, &mainRenderTargetView);
		pBackBuffer->Release ();

		This->GetDesc (&sd);
		HWND windowHandle = sd.OutputWindow;
		originalWndProc   = (WNDPROC)SetWindowLongPtrA (windowHandle, GWLP_WNDPROC, (i64)WndProc);

		font_size = 17.0;
		try {
			auto config = toml::parse ("Mods/Plugins/subtitles.toml");
			font_size   = toml::find_or<f32> (config, "font_size", 17.0);
		} catch (...) {}

		ImGui::CreateContext ();
		ImGuiIO &io    = ImGui::GetIO ();
		io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
		io.Fonts->AddFontFromMemoryCompressedTTF (DroidSans_compressed_data, DroidSans_compressed_size, font_size);
		ImGui_ImplWin32_Init (windowHandle);
		ImGui_ImplDX11_Init (pDevice, pContext);

		RECT windowRect;
		GetWindowRect (windowHandle, &windowRect);
		f32 width   = windowRect.right - windowRect.left;
		f32 height  = windowRect.bottom - windowRect.top;
		imguiWidth  = width / 2.5;
		imguiHeight = font_size * 5;
		imguiPosX   = width / 2;
		imguiPosY   = height / 1.25;

		inited = true;
	}

	ImGui_ImplDX11_NewFrame ();
	ImGui_ImplWin32_NewFrame ();
	ImGui::NewFrame ();

	ImGui::SetNextWindowSize (ImVec2 (imguiWidth, imguiHeight), resChanged ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos (ImVec2 (imguiPosX, imguiPosY), resChanged ? ImGuiCond_Always : ImGuiCond_FirstUseEver, ImVec2 (0.5, 0.5));
	ImGui::SetNextWindowBgAlpha (0.7);
	if (ImGui::Begin ("Subtitles", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse)) {
		ImGui::PushTextWrapPos (0.0);
		for (auto &sub : showing_subtitles) {
			if (sub.end_time <= std::chrono::high_resolution_clock::now ()) sub.state = fade_out;
			switch (sub.state) {
			case fade_in:
				ImGui::TextColored (ImVec4 (1.0, 1.0, 1.0, sub.opacity), "%s\n", sub.subtitle.subtitle);
				sub.opacity += 0.1;
				if (sub.opacity >= 1.0) sub.state = show;
				break;
			case show: ImGui::TextWrapped ("%s\n", sub.subtitle.subtitle); break;
			case fade_out:
				ImGui::TextColored (ImVec4 (1.0, 1.0, 1.0, sub.opacity), "%s\n", sub.subtitle.subtitle);
				sub.opacity -= 0.1;
				if (sub.opacity <= 0.0) {
					auto it = std::find (showing_subtitles.begin (), showing_subtitles.end (), sub.subtitle);
					if (it != showing_subtitles.end ()) showing_subtitles.erase (it);
				}
				break;
			}
		}
		ImGui::PopTextWrapPos ();
	}
	ImGui::End ();

	ImGui::EndFrame ();
	ImGui::Render ();
	pContext->OMSetRenderTargets (1, &mainRenderTargetView, 0);
	ImGui_ImplDX11_RenderDrawData (ImGui::GetDrawData ());
	resChanged = false;

	return originalIDXGISwapChainPresent (This, sync, flags);
}

VTABLE_HOOK (i32, __stdcall, IDXGIFactory2, CreateSwapChain, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain) {
	i32 result = originalIDXGIFactory2CreateSwapChain (This, pDevice, pDesc, ppSwapChain);
	if (FAILED (result)) return result;

	if (ppSwapChain && *ppSwapChain) {
		INSTALL_VTABLE_HOOK (IDXGISwapChain, *ppSwapChain, Present, 8);
		INSTALL_VTABLE_HOOK (IDXGISwapChain, *ppSwapChain, ResizeBuffers, 13);
		INSTALL_VTABLE_HOOK (IDXGISwapChain, *ppSwapChain, ResizeTarget, 14);
	}

	return result;
}

HOOK (i32, __stdcall, D3D11CreateDevice, PROC_ADDRESS ("d3d11.dll", "D3D11CreateDevice"), IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext) {
	i32 result = originalD3D11CreateDevice (pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
	if (FAILED (result)) return result;

	ID3D11Device *pDevice = *ppDevice;
	IDXGIDevice2 *pDXGIDevice;
	IDXGIAdapter *pDXGIAdapter;
	IDXGIFactory2 *pIDXGIFactory;
	pDevice->QueryInterface (__uuidof (IDXGIDevice2), (void **)&pDXGIDevice);
	pDXGIDevice->GetParent (__uuidof (IDXGIAdapter), (void **)&pDXGIAdapter);
	pDXGIAdapter->GetParent (__uuidof (IDXGIFactory2), (void **)&pIDXGIFactory);
	INSTALL_VTABLE_HOOK (IDXGIFactory2, pIDXGIFactory, CreateSwapChain, 10);

	return result;
}

HOOK (void *, __fastcall, PlayVoice, 0x14047c9d0, void *a1, string *name, void *a3) {
	show_subtitle (name);
	return originalPlayVoice (a1, name, a3);
}

HOOK (void *, __fastcall, PlaySurroundVoice, 0x14047cba0, void *a1, string *name, void *a3, void *a4, i32 a5, u32 a6) {
	show_subtitle (name);
	return originalPlaySurroundVoice (a1, name, a3, a4, a5, a6);
}

extern "C" {
__declspec (dllexport) bool __fastcall EML5_Load (PluginInfo *info) {
	INSTALL_HOOK (D3D11CreateDevice);
	INSTALL_HOOK (PlayVoice);
	INSTALL_HOOK (PlaySurroundVoice);

	// debug stuff
	// AllocConsole ();
	// freopen ("CONOUT$", "w", stdout);

	info->infoVersion = PluginInfo::MaxInfoVer;
	info->name        = "Subtitles";
	info->version     = PLUG_VER (1, 0, 0, 0);
	return 1;
}
}
