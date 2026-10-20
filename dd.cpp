#include <windows.h>
#include <iostream>
#include <string>
#include "lua.h"
#include "lualib.h"
#include "Luau/Compiler.h"

// --- Luau API ---
static int custom_httpget(lua_State* L) {
    const char* url = lua_tostring(L, 1);
    std::string response = "Downloaded: " + std::string(url ? url : "unknown");
    lua_pushstring(L, response.c_str());
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
    if (status != LUA_OK) {
        std::cout << "[Lua Load Error] " << lua_tostring(L, -1) << std::endl;
        lua_pop(L, 1);
        return;
    }
    RegisterAPI(L);
    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        std::cout << "[Lua Runtime Error] " << lua_tostring(L, -1) << std::endl;
        lua_pop(L, 1);
    }
}

void SetupConsole() {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            SetupConsole();
            std::cout << "[+] Internal Executor Loaded.\n";

            lua_State* L = luaL_newstate();
            luaL_openlibs(L);

            // Create Named Pipe to talk to the External GUI
            HANDLE hPipe = CreateNamedPipeA("\\\\.\\pipe\\WesodaPipe", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
            
            while (true) {
                std::cout << "[*] Waiting for External GUI to connect...\n";
                ConnectNamedPipe(hPipe, NULL);
                std::cout << "[+] GUI Connected! Waiting for script...\n";

                char buffer[8192];
                DWORD bytesRead;
                if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
                    buffer[bytesRead] = '\0';
                    std::cout << "[*] Executing script from GUI...\n";
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
