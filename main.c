#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

#ifdef DEBUG
#include "debug_client.h"
#else
#include "bindings.h"
#endif

#endif

#include <string.h>
#include <assert.h>

#include "comet.h"

void main_loop(void* arg)
{
    Engine* engine = arg;
    lua_State* L = engine->L;

    BeginDrawing();

    if (L != NULL && engine->script_active)
    {
        lua_getglobal(L, "update");
        if (lua_isfunction(L, -1))
        {
            if (lua_pcall(L, 0, 0, 0))
            {
                printf("Lua error: %s\n", lua_tostring(L, -1));
                engine->script_active = false;
            }
        }
        else
        {
            lua_pop(L, 1);
        }
    }

    DrawFPS(0, 0);

    EndDrawing();

    if (L != NULL && engine->script_active)
        assert(lua_gettop(L) == 0);
}

int main(void)
{
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(600, 450, "game");

    Engine engine = {NULL, false};

#ifdef __EMSCRIPTEN__

#ifdef DEBUG
    initialise_debug_client(&engine);
#else
    // initialise Lua immediately when running a release build
    initialise_lua(&engine);
    run_lua_main(&engine);
#endif

    emscripten_set_main_loop_arg(main_loop, &engine, 0, 1);
#else
    // FIXME: desktop is not supported yet
    initialise_lua(&engine);
    run_lua_main(&engine);

    while (!WindowShouldClose()) main_loop(&engine);
#endif

    if (engine.L != NULL)
    {
        lua_close(engine.L);
        engine.L = NULL;
    }

    CloseWindow();
    return 0;
}
