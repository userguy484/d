#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tlhelp32.h>
#include <string>
#include <mutex>
#include <vector>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// --- VEH Globals ---
std::vector<std::pair<uintptr_t, uintptr_t>> managed_pages;
uintptr_t injected_dll_base = 0; // We need to know where our DLL lands

// --- VEH Handler ---
LONG CALLBACK veh_handler(EXCEPTION_POINTERS* pExceptionInfo) {
    auto* er = pExceptionInfo->ExceptionRecord;
    auto* ctx = pExceptionInfo->ContextRecord;

    if (!er || !ctx) return EXCEPTION_CONTINUE_SEARCH;

    // 0xC0000005 is ACCESS_VIOLATION
    if (er->ExceptionCode != 0xC0000005) return EXCEPTION_CONTINUE_SEARCH;
    // Check if it's an EXECUTE fault (8)
    if (er->NumberParameters < 2 || er->ExceptionInformation[0] != 8) return EXCEPTION_CONTINUE_SEARCH;

    uintptr_t fault_addr = (uintptr_t)er->ExceptionAddress;
    if (ctx->Rip != fault_addr) return EXCEPTION_CONTINUE_SEARCH;

    // Check if the fault address is inside Hyperion's encrypted pages
    for (auto& page : managed_pages) {
        if (fault_addr >= page.first && fault_addr <= page.second) {
            // The page is encrypted. Hyperion's VEH usually decrypts it right before we get here.
            // We fake a return address or patch the RIP to jump to our DLL.
            // For this to work perfectly, you must find the exact trampoline offset.
            
            // Pseudo: Patch RIP to point to our injected payload entry point
            // ctx->Rip = injected_dll_base + entry_point_offset;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

// --- Standard UI / Injection Globals ---
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static bool                     g_Running = true;

char processName[128] = "RobloxPlayerBeta.exe";
char scriptBuffer[4096] = "print('Hello from VEH Executor!')";
HANDLE hPipe = INVALID_HANDLE_VALUE;
std::string tempDllPath = "";
bool isConnected = false;
bool isInjecting = false;

std::string outputLog = "";
std::mutex outputMutex;

void AddLog(const std::string& text) {
    std::lock_guard<std::mutex> lock(outputMutex);
    outputLog += text;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void CleanupDeviceD3D() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

DWORD GetProcessId(const std::string& name) {
    PROCESSENTRY32 pe32; pe32.dwSize = sizeof(pe32);
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (name == pe32.szExeFile) { CloseHandle(hSnapshot); return pe32.th32ProcessID; }
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot); return 0;
}

DWORD WINAPI PipeReadThread(LPVOID lpParam) {
    char buffer[4096];
    DWORD bytesRead;
    while (isConnected && hPipe != INVALID_HANDLE_VALUE) {
        BOOL success = ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
        if (success && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            AddLog(buffer);
        } else {
            isConnected = false;
            AddLog("[-] Connection lost to DLL.\n");
            break;
        }
    }
    return 0;
}

DWORD WINAPI InjectThread(LPVOID lpParam) {
    isInjecting = true;
    isConnected = false;
    
    // 1. Setup VEH
    AddLog("[*] Registering VEH...\n");
    PVOID veh_handle = AddVectoredExceptionHandler(1, veh_handler);
    
    // 2. Prep Payload
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    tempDllPath = std::string(tempPath) + "wesoda_payload.dll";
    CopyFileA("dd.dll", tempDllPath.c_str(), FALSE);

    DWORD pid = GetProcessId(std::string(processName));
    if (pid == 0) { AddLog("[-] Process not found!\n"); isInjecting = false; return 0; }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) { AddLog("[-] Failed to open process. Run as Admin!\n"); isInjecting = false; return 0; }

    // 3. Allocate and write payload path
    void* alloc = VirtualAllocEx(hProc, nullptr, tempDllPath.length() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProc, alloc, tempDllPath.c_str(), tempDllPath.length() + 1, nullptr);

    // 4. THE VEH TRICK (Simplified)
    // Instead of CreateRemoteThread (which gets blocked), we find Hyperion's encrypted page
    // and force Roblox to jump to it, triggering our VEH handler.
    AddLog("[*] Triggering VEH Fault (Bypassing Hyperion)...\n");
    
    // In a real executor, you do AOB scanning here to find the Hyperion trampoline address.
    // We'll simulate the remote thread for the sake of the GUI.
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    void* loadLib = GetProcAddress(k32, "LoadLibraryA");
    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, (LPTHREAD_START_ROUTINE)loadLib, alloc, 0, nullptr);
    
    if (hThread == NULL) { 
        AddLog("[-] Injection blocked by Hyperion!\n");
        CloseHandle(hProc); isInjecting = false; return 0; 
    }
    
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread); CloseHandle(hProc);

    // 5. Connect Pipe
    AddLog("[*] Waiting for DLL to connect...\n");
    for (int i = 0; i < 20; i++) {
        hPipe = CreateFileA("\\\\.\\pipe\\WesodaPipe", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) { isConnected = true; break; }
        Sleep(100);
    }
    
    if (isConnected) {
        CreateThread(nullptr, 0, PipeReadThread, nullptr, 0, nullptr);
        AddLog("[+] Connected! Executor is alive inside Roblox.\n");
    } else {
        AddLog("[-] Pipe failed. Injection crashed or was blocked.\n");
    }
    
    isInjecting = false;
    return 0;
}

DWORD WINAPI ExecuteThread(LPVOID lpParam) {
    if (!isConnected || hPipe == INVALID_HANDLE_VALUE) {
        AddLog("[-] Not connected! Inject first.\n");
        return 0;
    }
    AddLog("[*] Sending script to DLL...\n");
    DWORD written;
    WriteFile(hPipe, scriptBuffer, strlen(scriptBuffer), &written, NULL);
    return 0;
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK) return false;
    CreateRenderTarget(); return true;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"WesodaGUI", nullptr };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Wesoda Executor", WS_OVERLAPPEDWINDOW, 100, 100, 700, 600, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) { CleanupDeviceD3D(); UnregisterClassW(wc.lpszClassName, wc.hInstance); return 1; }

    ShowWindow(hwnd, SW_SHOWDEFAULT); UpdateWindow(hwnd);

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io; ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd); ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    MSG msg;
    while (g_Running) {
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_Running = false;
        }
        if (!g_Running) break;

        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Wesoda", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::TextColored(ImVec4(0.0f, 0.6f, 1.0f, 1.0f), "WESODA VEH EXECUTOR");
        ImGui::Separator();
        ImGui::InputText("Process Name", processName, IM_ARRAYSIZE(processName));
        
        if (isInjecting) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Injecting...");
        } else {
            if (ImGui::Button("Inject", ImVec2(100, 30))) { 
                CreateThread(nullptr, 0, InjectThread, nullptr, 0, nullptr); 
            }
        }
        
        ImGui::SameLine();
        if (isConnected) ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Connected");
        else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Status: Disconnected");

        ImGui::Separator();
        ImGui::Text("Script:");
        ImGui::InputTextMultiline("##script", scriptBuffer, IM_ARRAYSIZE(scriptBuffer), ImVec2(-1, 150), ImGuiInputTextFlags_AllowTabInput);
        
        if (ImGui::Button("Execute Script", ImVec2(-1, 30))) { 
            CreateThread(nullptr, 0, ExecuteThread, nullptr, 0, nullptr);
        }

        ImGui::Separator();
        ImGui::Text("Output:");
        
        ImGui::BeginChild("OutputBox", ImVec2(-1, -1), true);
        {
            std::lock_guard<std::mutex> lock(outputMutex);
            ImGui::TextUnformatted(outputLog.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        ImGui::End();

        ImGui::Render();
        const float clear_color[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    CleanupDeviceD3D(); DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
