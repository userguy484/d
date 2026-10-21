#include <windows.h>
#include <iostream>
#include <string>
#include "lua.h"
#include "lualib.h"
#include "Luau/Compiler.h"

static int custom_httpget(lua_State* L) {
    const char* url = lua_tostring(L, 1);
    lua_pushstring(L, url ? url : "unknown");
    return 1; 
}

void RegisterAPI(lua_State* L) {
    lua_pushcfunction(L, custom_httpget, "HttpGet");
    lua_setglobal(L, "HttpGet");
    lua_newtable(L); 
    lua_pushstring(L, "Roblox Game Instance");
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
            lua_State* L = luaL_newstate();
            luaL_openlibs(L);

            HANDLE hPipe = CreateNamedPipeA("\\\\.\\pipe\\WesodaPipe", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 8192, 8192, 0, NULL);
            
            while (true) {
                ConnectNamedPipe(hPipe, NULL);
                char buffer[8192];
                DWORD bytesRead;
                if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
                    buffer[bytesRead] = '\0';
                    ExecuteScript(L, std::string(buffer));
                }
                DisconnectNamedPipe(hPipe);
            }
            return 0;
        }, nullptr, 0, nullptr);
        break;
    }
    return TRUE;
}
