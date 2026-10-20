#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

#include "lua.h"
#include "lualib.h"

// Custom function: HttpGet("url")
static int custom_httpget(lua_State* L) {
    const char* url = lua_tostring(L, 1);
    std::string response = "Downloaded data from: " + std::string(url ? url : "unknown");
    lua_pushstring(L, response.c_str());
    return 1; 
}

// Custom property: game
static int get_game(lua_State* L) {
    lua_newtable(L); 
    lua_pushstring(L, "Fake Roblox Game Instance");
    lua_setfield(L, -2, "Name");
    return 1;
}

// Inject the API into the Luau environment
void RegisterAPI(lua_State* L) {
    lua_pushcfunction(L, custom_httpget, "HttpGet");
    lua_setglobal(L, "HttpGet");

    lua_pushcfunction(L, get_game, "game");
    lua_setglobal(L, "game");
}

// Execute the Lua script
void ExecuteScript(lua_State* L, const std::string& script) {
    // Luau's native load function
    int status = luau_load(L, "ExecutorScript", script.c_str(), script.length(), 0);
    if (status != LUA_OK) {
        std::cout << "[Lua Load Error] " << lua_tostring(L, -1) << std::endl;
        lua_pop(L, 1);
        return;
    }

    RegisterAPI(L);

    // Run the script
    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        std::cout << "[Lua Runtime Error] " << lua_tostring(L, -1) << std::endl;
        lua_pop(L, 1);
    }
}

// Spawn a console window for output
void SetupConsole() {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);
}

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            SetupConsole();
            std::cout << "[+] Wesoda Internal Executor Initialized.\n";

            // 1. Initialize the real Luau State
            lua_State* L = luaL_newstate();
            if (!L) {
                std::cout << "[-] Failed to create Luau state.\n";
                return 0;
            }
            
            luaL_openlibs(L);

            // 2. Run a test script
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
