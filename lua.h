#pragma once
extern "C" {
    typedef struct lua_State lua_State;
    typedef int (*lua_CFunction)(lua_State* L);
    
    #define LUA_OK 0
    #define LUA_MULTRET -1
    
    void lua_pushstring(lua_State* L, const char* s);
    void lua_newtable(lua_State* L);
    void lua_setfield(lua_State* L, int idx, const char* k);
    void lua_pushcfunction(lua_State* L, lua_CFunction f, const char* name);
    void lua_setglobal(lua_State* L, const char* name);
    const char* lua_tostring(lua_State* L, int idx);
    void lua_pop(lua_State* L, int n);
    int lua_pcall(lua_State* L, int nargs, int nresults, int errfunc);
}
