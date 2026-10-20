#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>

DWORD GetProcessId(const std::string& processName) {
    PROCESSENTRY32 processInfo;
    processInfo.dwSize = sizeof(processInfo);
    HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (processesSnapshot == INVALID_HANDLE_VALUE) return 0;

    Process32First(processesSnapshot, &processInfo);
    do {
        if (processName.compare(processInfo.szExeFile) == 0) {
            CloseHandle(processesSnapshot);
            return processInfo.th32ProcessID;
        }
    } while (Process32Next(processesSnapshot, &processInfo));

    CloseHandle(processesSnapshot);
    return 0;
}

int main() {
    std::string processName;
    
    // Hardcoded DLL name (must be in the same folder as injector.exe)
    std::string dllPath = "dd.dll"; 

    std::cout << "Enter target process (e.g., RobloxPlayerBeta.exe or notepad.exe): ";
    std::getline(std::cin, processName);

    // Get absolute path for the DLL so it doesn't fail inside the target process
    char fullPath[MAX_PATH];
    GetFullPathNameA(dllPath.c_str(), MAX_PATH, fullPath, NULL);

    DWORD pid = GetProcessId(processName);
    if (pid == 0) {
        std::cout << "[-] Process not found. Is it running?\n";
        system("pause");
        return 1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "[-] Failed to open process. Run injector as Administrator.\n";
        system("pause");
        return 1;
    }

    void* allocMem = VirtualAllocEx(hProcess, nullptr, strlen(fullPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProcess, allocMem, fullPath, strlen(fullPath) + 1, nullptr);

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    void* loadLibraryAddr = GetProcAddress(hKernel32, "LoadLibraryA");

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, allocMem, 0, nullptr);
    WaitForSingleObject(hThread, INFINITE);

    std::cout << "[+] Injection successful!\n";

    CloseHandle(hThread);
    CloseHandle(hProcess);
    system("pause");
    return 0;
}#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>

DWORD GetProcessId(const std::string& processName) {
    PROCESSENTRY32 processInfo;
    processInfo.dwSize = sizeof(processInfo);
    HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (processesSnapshot == INVALID_HANDLE_VALUE) return 0;

    Process32First(processesSnapshot, &processInfo);
    do {
        if (processName.compare(processInfo.szExeFile) == 0) {
            CloseHandle(processesSnapshot);
            return processInfo.th32ProcessID;
        }
    } while (Process32Next(processesSnapshot, &processInfo));

    CloseHandle(processesSnapshot);
    return 0;
}

int main() {
    std::string processName;
    std::string dllPath;

    std::cout << "Enter target process (e.g., RobloxPlayerBeta.exe or notepad.exe): ";
    std::getline(std::cin, processName);
    
    std::cout << "Enter path to your DLL (e.g., dd.dll): ";
    std::getline(std::cin, dllPath);

    // Get full path for the DLL
    char fullPath[MAX_PATH];
    GetFullPathNameA(dllPath.c_str(), MAX_PATH, fullPath, NULL);

    DWORD pid = GetProcessId(processName);
    if (pid == 0) {
        std::cout << "[-] Process not found.\n";
        system("pause");
        return 1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "[-] Failed to open process. Run as Administrator.\n";
        system("pause");
        return 1;
    }

    void* allocMem = VirtualAllocEx(hProcess, nullptr, strlen(fullPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProcess, allocMem, fullPath, strlen(fullPath) + 1, nullptr);

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    void* loadLibraryAddr = GetProcAddress(hKernel32, "LoadLibraryA");

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, allocMem, 0, nullptr);
    WaitForSingleObject(hThread, INFINITE);

    std::cout << "[+] Injection successful!\n";

    CloseHandle(hThread);
    CloseHandle(hProcess);
    system("pause");
    return 0;
}
