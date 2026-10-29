#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tlhelp32.h>
#include <string>
#include <mutex>
#include <fstream>
#include <vector>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// --- D3D11 Globals ---
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static bool g_Running = true;

// --- Executor Globals ---
char processName[128] = "notepad.exe";
char scriptBuffer[4096] = "print('Manual Mapper works!')";
HANDLE hPipe = INVALID_HANDLE_VALUE;
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

// --- MANUAL MAPPER LOGIC ---
bool ManualMap(HANDLE hProc, const char* dllPath) {
    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) return false;

    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)buffer.data();
    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(buffer.data() + pDos->e_lfanew);

    // 1. Allocate memory in target process
    void* pTargetBase = VirtualAllocEx(hProc, nullptr, pNt->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pTargetBase) return false;

    // 2. Write Headers
    WriteProcessMemory(hProc, pTargetBase, buffer.data(), pNt->OptionalHeader.SizeOfHeaders, nullptr);

    // 3. Write Sections
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
    for (int i = 0; i < pNt->FileHeader.NumberOfSections; i++) {
        void* pSectionDest = (void*)((uintptr_t)pTargetBase + pSection[i].VirtualAddress);
        void* pSectionSrc = (void*)(buffer.data() + pSection[i].PointerToRawData);
        WriteProcessMemory(hProc, pSectionDest, pSectionSrc, pSection[i].SizeOfRawData, nullptr);
    }

    // Note: A full manual mapper requires resolving the Import Address Table (IAT) and Relocations.
    // For this example, we assume dd.dll has no external dependencies outside of kernel32/user32 which are already loaded.

    // 4. Create Remote Thread to call DllMain
    // Get the entry point
    uintptr_t entryPoint = (uintptr_t)pTargetBase + pNt->OptionalHeader.AddressOfEntryPoint;
    
    // Create a thread that calls the entry point (DllMain) with DLL_PROCESS_ATTACH
    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, (LPTHREAD_START_ROUTINE)entryPoint, pTargetBase, 0, nullptr);
    
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        return true;
    }
    return false;
}

DWORD WINAPI PipeReadThread(LPVOID lpParam) {
    char buffer[4096];
    DWORD bytesRead;
    while (isConnected && hPipe != INVALID_HANDLE_VALUE) {
        if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            AddLog(buffer);
        } else {
            isConnected = false;
            AddLog("[-] Connection lost.\n");
            break;
        }
    }
    return 0;
}

DWORD WINAPI InjectThread(LPVOID lpParam) {
    isInjecting = true;
    isConnected = false;
    if (hPipe != INVALID_HANDLE_VALUE) { CloseHandle(hPipe); hPipe = INVALID_HANDLE_VALUE; }

    AddLog("[*] Reading dd.dll...\n");

    DWORD pid = GetProcessId(std::string(processName));
    if (pid == 0) { AddLog("[-] Process not found!\n"); isInjecting = false; return 0; }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) { AddLog("[-] Failed to open process. Run as Admin!\n"); isInjecting = false; return 0; }

    AddLog("[*] Manually mapping DLL...\n");
    if (ManualMap(hProc, "dd.dll")) {
        AddLog("[+] Manual Map successful!\n");
    } else {
        AddLog("[-] Manual Map failed.\n");
        CloseHandle(hProc); isInjecting = false; return 0;
    }
    CloseHandle(hProc);

    AddLog("[*] Waiting for pipe...\n");
    for (int i = 0; i < 30; i++) {
        hPipe = CreateFileA("\\\\.\\pipe\\WesodaPipe", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) { isConnected = true; break; }
        Sleep(100);
    }
    
    if (isConnected) {
        CreateThread(nullptr, 0, PipeReadThread, nullptr, 0, nullptr);
        AddLog("[+] Connected to DLL!\n");
    } else {
        AddLog("[-] Pipe failed. Ensure dd.dll is in the same folder.\n");
    }
    
    isInjecting = false;
    return 0;
}

DWORD WINAPI ExecuteThread(LPVOID lpParam) {
    if (!isConnected || hPipe == INVALID_HANDLE_VALUE) {
        AddLog("[-] Not connected! Inject first.\n");
        return 0;
    }
    AddLog("[*] Sending script...\n");
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
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Manual Mapper Executor", WS_OVERLAPPEDWINDOW, 100, 100, 700, 600, nullptr, nullptr, wc.hInstance, nullptr);

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

        ImGui::TextColored(ImVec4(0.0f, 0.6f, 1.0f, 1.0f), "MANUAL MAPPER EXECUTOR");
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
