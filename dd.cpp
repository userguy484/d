#include <windows.h>
#include <string>
#include "lua.h"
#include "lualib.h"
#include "Luau/Compiler.h"

HANDLE g_hPipe = INVALID_HANDLE_VALUE;

void SendToGUI(const std::string& msg) {
    if (g_hPipe != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(g_hPipe, msg.c_str(), msg.length(), &written, NULL);
        FlushFileBuffers(g_hPipe); // Forces text to update instantly
    }
}

static int custom_print(lua_State* L) {
    int n = lua_gettop(L);
    std::string out = "";
    for (int i = 1; i <= n; i++) {
        if (lua_isstring(L, i) || lua_isnumber(L, i)) {
            out += lua_tostring(L, i);
            if (i < n) out += " ";
        }
    }
    out += "\n";
    SendToGUI(out);
    return 0;
}

void RegisterAPI(lua_State* L) {
    lua_pushcfunction(L, custom_print, "print");
    lua_setglobal(L, "print");
    lua_newtable(L); 
    lua_pushstring(L, "Fake Game");
    lua_setfield(L, -2, "Name");
    lua_setglobal(L, "game");
}

void ExecuteScript(lua_State* L, const std::string& script) {
    std::string bytecode = Luau::compile(script);
    int status = luau_load(L, "Executor", bytecode.c_str(), bytecode.length(), 0);
    
    if (status != LUA_OK) { 
        const char* err = lua_tostring(L, -1);
        if (err) SendToGUI("[-] Compile Error: " + std::string(err) + "\n");
        lua_pop(L, 1); return; 
    }
    
    RegisterAPI(L);
    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        if (err) SendToGUI("[-] Runtime Error: " + std::string(err) + "\n");
        lua_pop(L, 1);
    } else {
        SendToGUI("[+] Script executed.\n");
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        
        // MUTEX: Prevents injecting twice and crashing the game
        HANDLE hMutex = CreateMutexA(NULL, FALSE, "WesodaCleanMutex");
        if (GetLastError() == ERROR_ALREADY_EXISTS) return TRUE; 

        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            lua_State* L = luaL_newstate();
            luaL_openlibs(L);
            RegisterAPI(L);

            // Keep pipe open forever
            while (true) {
                g_hPipe = CreateNamedPipeA("\\\\.\\pipe\\WesodaPipe", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 8192, 8192, 0, NULL);
                if (g_hPipe != INVALID_HANDLE_VALUE) {
                    ConnectNamedPipe(g_hPipe, NULL);
                    SendToGUI("[+] DLL Loaded Successfully!\n");

                    while (true) {
                        char buffer[8192];
                        DWORD bytesRead;
                        if (ReadFile(g_hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                            buffer[bytesRead] = '\0';
                            ExecuteScript(L, std::string(buffer));
                        } else {
                            break; // If injector closes, wait for new connection
                        }
                    }
                    DisconnectNamedPipe(g_hPipe);
                }
                Sleep(500);
            }
            return 0;
        }, nullptr, 0, nullptr);
    }
    return TRUE;
}
