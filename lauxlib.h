#pragma once
#include <stddef.h>
#include "lua.h"

extern "C" {
    int luaL_loadbuffer(lua_State* L, const char* buff, size_t sz, const char* name);
    const char* luaL_checkstring(lua_State* L, int narg);
}
