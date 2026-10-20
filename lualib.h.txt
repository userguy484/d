#pragma once
#include "lua.h"
extern "C" {
    lua_State* luaL_newstate(void);
    void luaL_openlibs(lua_State* L);
}
