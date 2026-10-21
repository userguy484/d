#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>

// Helper to read the DLL file into memory
BYTE* ReadFileToMemory(const char* path, DWORD& fileSize) {
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return nullptr;
    fileSize = GetFileSize(hFile, NULL);
    BYTE* buffer = new BYTE[fileSize];
    DWORD read;
    ReadFile(hFile, buffer, fileSize, &read, NULL);
    CloseHandle(hFile);
    return buffer;
}

// The Manual Mapper
bool ManualMap(HANDLE hProc, const char* dllPath) {
    DWORD fileSize = 0;
    BYTE* pSrcData = ReadFileToMemory(dllPath, fileSize);
    if (!pSrcData) return false;

    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pSrcData;
    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pSrcData + pDos->e_lfanew);
    
    // 1. Allocate memory in the target process for the DLL
    void* pTargetBase = VirtualAllocEx(hProc, nullptr, pNt->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pTargetBase) { delete[] pSrcData; return false; }

    // 2. Write the headers
    WriteProcessMemory(hProc, pTargetBase, pSrcData, pNt->OptionalHeader.SizeOfHeaders, nullptr);

    // 3. Write the sections (.text, .data, etc.)
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
    for (int i = 0; i < pNt->FileHeader.NumberOfSections; i++) {
        void* pSectionDest = (void*)((uintptr_t)pTargetBase + pSection[i].VirtualAddress);
        void* pSectionSrc = (void*)(pSrcData + pSection[i].PointerToRawData);
        WriteProcessMemory(hProc, pSectionDest, pSectionSrc, pSection[i].SizeOfRawData, nullptr);
    }

    // 4. Call DllMain (This is the complex part where relocations and imports are resolved in a real mapper)
    // For this PUA example, we use a shellcode stub to call DllMain
    // (In a full executor, you would resolve the Import Address Table (IAT) here)
    
    delete[] pSrcData;
    return true;
}
