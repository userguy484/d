#include <windows.h>
#include <iostream>
#include <string>
#include "lua.h"
#include "lualib.h"
#include "Luau/Compiler.h"

HANDLE g_hPipe = INVALID_HANDLE_VALUE;

static int custom_print(lua_State* L) {
    int n = lua_gettop(L);
    std::string out = "";
    for (int i = 1; i <= n; i++) {
        if (lua_isstring(L, i)) {
            out += lua_tostring(L, i);
            if (i < n) out += " ";
        }
    }
    out += "\n";

    if (g_hPipe != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(g_hPipe, out.c_str(), out.length(), &written, NULL);
        FlushFileBuffers(g_hPipe); // Forces the text to send immediately
    }
    return 0;
}

void RegisterAPI(lua_State* L) {
    lua_pushcfunction(L, custom_print, "print");
    lua_setglobal(L, "print");

    lua_newtable(L); 
    lua_pushstring(L, "Fake Roblox Game Instance");
    lua_setfield(L, -2, "Name");
    lua_setglobal(L, "game");
}

void ExecuteScript(lua_State* L, const std::string& script) {
    std::string bytecode = Luau::compile(script);
    int status = luau_load(L, "ExecutorScript", bytecode.c_str(), bytecode.length(), 0);
    if (status != LUA_OK) { lua_pop(L, 1); return; }
    RegisterAPI(L);
    lua_pcall(L, 0, LUA_MULTRET, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            // Try to create the pipe. If it fails, it means another DLL is already running.
            g_hPipe = CreateNamedPipeA("\\\\.\\pipe\\WesodaPipe", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 8192, 8192, 0, NULL);
            if (g_hPipe == INVALID_HANDLE_VALUE) return 0; // Exit thread if ghost DLL

            lua_State* L = luaL_newstate();
            luaL_openlibs(L);

            ConnectNamedPipe(g_hPipe, NULL);
            while (true) {
                char buffer[8192];
                DWORD bytesRead;
                if (ReadFile(g_hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    ExecuteScript(L, std::string(buffer));
                } else {
                    break; // Pipe disconnected
                }
            }
            return 0;
        }, nullptr, 0, nullptr);
        break;
    }
    return TRUE;
}
