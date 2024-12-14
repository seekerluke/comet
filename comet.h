#ifndef COMET_H
#define COMET_H

#include "raylib.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

typedef struct Engine
{
    lua_State* L;
    bool script_active;
} Engine;

// helpers
#define cmt_register_input(L, input, name) lua_pushinteger(L, input); lua_setglobal(L, name)

#endif //COMET_H
