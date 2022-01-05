#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "MinHook/MinHook.h"
#if _WIN64 
#pragma comment(lib, "MinHook/libMinHook.x64.lib")
#else
#pragma comment(lib, "MinHook/libMinHook.x86.lib")
#endif

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

// Globals
HINSTANCE dll_handle;

typedef long(__stdcall* present)(IDXGISwapChain*, UINT, UINT);
present p_present;
present p_present_target;
bool get_present_pointer()
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = GetForegroundWindow();
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	IDXGISwapChain* swap_chain;
	ID3D11Device* device;

	const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(
		NULL, 
		D3D_DRIVER_TYPE_HARDWARE, 
		NULL, 
		0, 
		feature_levels, 
		2, 
		D3D11_SDK_VERSION, 
		&sd, 
		&swap_chain, 
		&device, 
		nullptr, 
		nullptr) == S_OK)
	{
		void** p_vtable = *reinterpret_cast<void***>(swap_chain);
		swap_chain->Release();
		device->Release();
		//context->Release();
		p_present_target = (present)p_vtable[8];
		return true;
	}
	return false;
}

WNDPROC oWndProc;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (true && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool init = false;
HWND window = NULL;
ID3D11Device* p_device = NULL;
ID3D11DeviceContext* p_context = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;
static long __stdcall detour_present(IDXGISwapChain* p_swap_chain, UINT sync_interval, UINT flags) {
	if (!init) {
		if (SUCCEEDED(p_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&p_device)))
		{
			p_device->GetImmediateContext(&p_context);
			DXGI_SWAP_CHAIN_DESC sd;
			p_swap_chain->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			p_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
			p_device->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
			ImGui_ImplWin32_Init(window);
			ImGui_ImplDX11_Init(p_device, p_context);
			init = true;
		}
		else
			return p_present(p_swap_chain, sync_interval, flags);
	}
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	ImGui::ShowDemoWindow();

	ImGui::EndFrame();
	ImGui::Render();

	p_context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return p_present(p_swap_chain, sync_interval, flags);
}

DWORD __stdcall EjectThread(LPVOID lpParameter) {
	Sleep(100);
	FreeLibraryAndExitThread(dll_handle, 0);
	Sleep(100);
	return 0;
}


//"main" loop
int WINAPI main()
{

	if (!get_present_pointer()) 
	{
		return 1;
	}

	MH_STATUS status = MH_Initialize();
	if (status != MH_OK)
	{
		return 1;
	}

	if (MH_CreateHook(reinterpret_cast<void**>(p_present_target), &detour_present, reinterpret_cast<void**>(&p_present)) != MH_OK) {
		return 1;
	}

	if (MH_EnableHook(p_present_target) != MH_OK) {
		return 1;
	}

	while (true) {
		Sleep(50);
		
		if (GetAsyncKeyState(VK_NUMPAD0) & 1) {

		}

		if (GetAsyncKeyState(VK_NUMPAD1)) {
			break;
		}
	}

	//Cleanup
	if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK) {
		return 1;
	}
	if (MH_Uninitialize() != MH_OK) {
		return 1;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	if (mainRenderTargetView) { mainRenderTargetView->Release(); mainRenderTargetView = NULL; }
	if (p_context) { p_context->Release(); p_context = NULL; }
	if (p_device) { p_device->Release(); p_device = NULL; }
	SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)(oWndProc));

	CreateThread(0, 0, EjectThread, 0, 0, 0);

	return 0;
}



BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		dll_handle = hModule;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main, NULL, 0, NULL);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{

	}
	return TRUE;
}