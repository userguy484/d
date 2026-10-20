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

bool ManualMap(HANDLE hProcess, const char* dllPath) {
    char fullPath[MAX_PATH];
    GetFullPathNameA(dllPath, MAX_PATH, fullPath, NULL);

    void* allocMem = VirtualAllocEx(hProcess, nullptr, strlen(fullPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProcess, allocMem, fullPath, strlen(fullPath) + 1, nullptr);

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    void* loadLibraryAddr = GetProcAddress(hKernel32, "LoadLibraryA");

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, allocMem, 0, nullptr);
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        return true;
    }
    return false;
}

int main() {
    std::string processName;
    std::cout << "=== WESODA EXECUTOR GUI ===\n";
    std::cout << "Enter target process (notepad.exe or RobloxPlayerBeta.exe): ";
    std::getline(std::cin, processName);

    DWORD pid = GetProcessId(processName);
    if (pid == 0) {
        std::cout << "[-] Process not found.\n";
        system("pause");
        return 1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "[-] Access Denied. Run as Admin.\n";
        system("pause");
        return 1;
    }

    std::cout << "[*] Injecting dd.dll...\n";
    if (ManualMap(hProcess, "dd.dll")) {
        std::cout << "[+] Injection successful!\n";
    } else {
        std::cout << "[-] Injection failed.\n";
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }
    CloseHandle(hProcess);

    // --- WAIT FOR DLL TO SETUP PIPE ---
    std::cout << "[*] Connecting to internal pipe...\n";
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    int retries = 20; // Try for 2 seconds (20 * 100ms)
    
    while (retries > 0) {
        hPipe = CreateFileA("\\\\.\\pipe\\WesodaPipe", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) {
            break;
        }
        Sleep(100); // Wait 100ms before trying again
        retries--;
    }

    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cout << "[-] Could not connect to pipe. Injection might have failed.\n";
        system("pause");
        return 1;
    }
    std::cout << "[+] Connected!\n";

    while (true) {
        std::cout << "\nEnter Lua script (or type 'exit' to quit):\n> ";
        std::string script;
        std::getline(std::cin, script);

        if (script == "exit") break;
        
        DWORD written;
        WriteFile(hPipe, script.c_str(), script.length(), &written, NULL);
        std::cout << "[*] Script sent to internal DLL.\n";
    }

    CloseHandle(hPipe);
    return 0;
}
