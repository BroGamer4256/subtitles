#include "DroidSans.h"
#include "Helpers.h"
#include "PluginAPI.h"
#include <chrono>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <string>
#include <thread>
#include <unordered_map>
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
	std::chrono::milliseconds duration;
	std::string subtitle;
	std::string name;
} subtitle;

// Doesn't work with char *, wtf?
std::unordered_map<std::string, subtitle> subtitles;

typedef enum subtitle_state {
	fade_in,
	show,
	fade_out,
} subtitle_state;

typedef struct showing_subtitle {
	subtitle_state state;
	f32 opacity;
	subtitle subtitle;
} showing_subtitle;

std::unordered_map<std::string, showing_subtitle> showing_subtitles;

void
wait_remove_sub (subtitle sub) {
	std::this_thread::sleep_for (sub.duration);
	showing_subtitles[sub.name].state = fade_out;
}

void
show_subtitle (string *name) {
	subtitle sub = subtitles[getStringInternal (name)];
	if (sub.subtitle.empty ()) { return; }
	showing_subtitles[sub.name] = showing_subtitle{ fade_in, 0.0, sub };
	std::thread thread (wait_remove_sub, sub);
	thread.detach ();
}

extern LRESULT ImGui_ImplWin32_WndProcHandler (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC originalWndProc;

i64
WndProc (const HWND window, u32 message, u64 wParam, i64 lParam) {
	if (ImGui_ImplWin32_WndProcHandler (window, message, wParam, lParam)) return true;
	return CallWindowProc (originalWndProc, window, message, wParam, lParam);
}

VTABLE_HOOK (i32, __stdcall, IDXGISwapChain, Present, u32 sync, u32 flags) {
	static ID3D11DeviceContext *pContext;
	static ID3D11RenderTargetView *mainRenderTargetView;

	static f32 imguiWidth;
	static f32 imguiHeight;
	static f32 imguiPosX;
	static f32 imguiPosY;

	static bool inited = false;
	if (!inited) {
		DXGI_SWAP_CHAIN_DESC sd;
		ID3D11Texture2D *pBackBuffer;
		ID3D11Device *pDevice;

		This->GetDevice (__uuidof(ID3D11Device), (void **)&pDevice);
		pDevice->GetImmediateContext (&pContext);

		This->GetBuffer (0, __uuidof(ID3D11Texture2D), (void **)&pBackBuffer);
		pDevice->CreateRenderTargetView (pBackBuffer, 0, &mainRenderTargetView);
		pBackBuffer->Release ();

		This->GetDesc (&sd);
		HWND windowHandle = sd.OutputWindow;
		originalWndProc   = (WNDPROC)SetWindowLongPtrA (windowHandle, GWLP_WNDPROC, (i64)WndProc);

		auto config   = toml::parse ("Mods/Plugins/subtitles.toml");
		f32 font_size = toml::find_or<f32> (config, "font_size", 17.0);

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
		imguiWidth  = width / 3;
		imguiHeight = font_size * 5;
		imguiPosX   = width / 2;
		imguiPosY   = height / 1.25;

		inited = true;
	}

	ImGui_ImplDX11_NewFrame ();
	ImGui_ImplWin32_NewFrame ();
	ImGui::NewFrame ();

	ImGui::SetNextWindowSize (ImVec2 (imguiWidth, imguiHeight), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos (ImVec2 (imguiPosX, imguiPosY), ImGuiCond_FirstUseEver, ImVec2 (0.5, 0.5));
	if (ImGui::Begin ("Subtitles", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse)) {
		ImGui::PushTextWrapPos (0.0);
		for (auto &kv : showing_subtitles) {
			switch (kv.second.state) {
			case fade_in:
				ImGui::TextColored (ImVec4 (1.0, 1.0, 1.0, kv.second.opacity), "%s\n", kv.second.subtitle.subtitle.c_str ());
				kv.second.opacity += 0.1;
				if (kv.second.opacity >= 1.0) { kv.second.state = show; }
				break;
			case show: ImGui::TextWrapped ("%s\n", kv.second.subtitle.subtitle.c_str ()); break;
			case fade_out:
				ImGui::TextColored (ImVec4 (1.0, 1.0, 1.0, kv.second.opacity), "%s\n", kv.second.subtitle.subtitle.c_str ());
				kv.second.opacity -= 0.1;
				if (kv.second.opacity <= 0.0) { showing_subtitles.erase (kv.first); }
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

	return originalIDXGISwapChainPresent (This, sync, flags);
}

VTABLE_HOOK (i32, __stdcall, IDXGIFactory2, CreateSwapChain, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain) {
	i32 result = originalIDXGIFactory2CreateSwapChain (This, pDevice, pDesc, ppSwapChain);
	if (FAILED (result)) return result;

	if (ppSwapChain && *ppSwapChain) INSTALL_VTABLE_HOOK (IDXGISwapChain, *ppSwapChain, Present, 8);

	return result;
}

HOOK (i32, __stdcall, D3D11CreateDevice, PROC_ADDRESS ("d3d11.dll", "D3D11CreateDevice"), IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType,
      HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device **ppDevice,
      D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext) {
	i32 result = originalD3D11CreateDevice (pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel,
	                                        ppImmediateContext);
	if (FAILED (result)) return result;

	ID3D11Device *pDevice = *ppDevice;
	IDXGIDevice2 *pDXGIDevice;
	IDXGIAdapter *pDXGIAdapter;
	IDXGIFactory2 *pIDXGIFactory;
	pDevice->QueryInterface (__uuidof(IDXGIDevice2), (void **)&pDXGIDevice);
	pDXGIDevice->GetParent (__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);
	pDXGIAdapter->GetParent (__uuidof(IDXGIFactory2), (void **)&pIDXGIFactory);
	INSTALL_VTABLE_HOOK (IDXGIFactory2, pIDXGIFactory, CreateSwapChain, 10);

	return result;
}

HOOK (void *, __fastcall, PlayVoice, 0x14047c9d0, void *a1, string *name, void *a3) {
	show_subtitle (name);
	return originalPlayVoice (a1, name, a3);
}

HOOK (void *, __fastcall, PlaySurroundVoice, 0x14047cba0, void *a1, string *name, void *a3, void *a4, u32 *a5, u32 *a6) {
	show_subtitle (name);
	return originalPlaySurroundVoice (a1, name, a3, a4, a5, a6);
}

extern "C" {
__declspec(dllexport) bool __fastcall EML5_Load (PluginInfo *info) {
	INSTALL_HOOK (D3D11CreateDevice);
	INSTALL_HOOK (PlayVoice);
	INSTALL_HOOK (PlaySurroundVoice);

	FILE *file   = fopen ("Mods/Plugins/subtitles.csv", "r");
	i32 buf_size = 255;
	char buffer[buf_size];
	while (fgets (buffer, buf_size, file)) {
		buffer[strcspn (buffer, "\n")] = 0;
		char *time                     = strtok (buffer, ",");
		std::chrono::milliseconds ms   = std::chrono::milliseconds (atoi (time));

		char *internal_name      = strtok (0, ",");
		char *subtitle_s         = strtok (0, ",");
		subtitles[internal_name] = subtitle{ ms, subtitle_s, internal_name };
	}
	fclose (file);

	info->infoVersion = PluginInfo::MaxInfoVer;
	info->name        = "Subtitles";
	info->version     = PLUG_VER (1, 0, 0, 0);
	return 1;
}
}
