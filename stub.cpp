#include <stddef.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

// Empty stubs to trick the linker
extern "C" {
    void lua_pushstring(lua_State* L, const char* s) {}
    void lua_newtable(lua_State* L) {}
    void lua_setfield(lua_State* L, int idx, const char* k) {}
    void lua_pushcfunction(lua_State* L, lua_CFunction f, const char* name) {}
    void lua_setglobal(lua_State* L, const char* name) {}
    const char* lua_tostring(lua_State* L, int idx) { return ""; }
    void lua_pop(lua_State* L, int n) {}
    int lua_pcall(lua_State* L, int nargs, int nresults, int errfunc) { return 0; }
    int luaL_loadbuffer(lua_State* L, const char* buff, size_t sz, const char* name) { return 0; }
    const char* luaL_checkstring(lua_State* L, int narg) { return ""; }
    lua_State* luaL_newstate(void) { return nullptr; }
    void luaL_openlibs(lua_State* L) {}
}
