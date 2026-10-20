#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

#include "lua.h"
#include "lualib.h"
#include "Luau/Compiler.h"

static int custom_httpget(lua_State* L) {
    const char* url = lua_tostring(L, 1);
    std::string response = "Downloaded data from: " + std::string(url ? url : "unknown");
    lua_pushstring(L, response.c_str());
    return 1; 
}

void RegisterAPI(lua_State* L) {
    // Register HttpGet
    lua_pushcfunction(L, custom_httpget, "HttpGet");
    lua_setglobal(L, "HttpGet");

    // Create the 'game' table directly instead of a function
    lua_newtable(L); 
    lua_pushstring(L, "Fake Roblox Game Instance");
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
            std::cout << "[+] Wesoda Internal Executor Initialized.\n";

            lua_State* L = luaL_newstate();
            if (!L) {
                std::cout << "[-] Failed to create Luau state.\n";
                return 0;
            }
            
            luaL_openlibs(L);

            std::string testScript = 
                "print('Hello from internal C++ executor!')\n"
                "local data = HttpGet('https://api.github.com')\n"
                "print(data)\n"
                "print('Game Name: ' .. game.Name)\n";
            
            ExecuteScript(L, testScript);
            
            return 0;
        }, nullptr, 0, nullptr);
        break;
    }
    return TRUE;
}
