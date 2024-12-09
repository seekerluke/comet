#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#endif

#include "raylib.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <string.h>
#include <math.h>

typedef struct Engine
{
    lua_State* L;
    bool script_active;
} Engine;

// helpers
#define cmt_register_input(L, input, name) lua_pushinteger(L, input); lua_setglobal(L, name)

static Texture2D* cmt_check_image(lua_State* L, const int idx, const char* arg_name)
{
    Texture2D* image = lua_touserdata(L, idx);
    if (image == NULL)
        luaL_error(L, "argument \"%s\" is not an image value", arg_name);
    return image;
}

static Color* cmt_check_color(lua_State* L, const int idx, const char* arg_name)
{
    Color* color = lua_touserdata(L, idx);
    if (color == NULL)
        luaL_error(L, "argument \"%s\" is not a color value", arg_name);
    return color;
}

static Rectangle* cmt_check_rect(lua_State* L, const int idx, const char* arg_name)
{
    Rectangle* rect = lua_touserdata(L, idx);
    if (rect == NULL)
        luaL_error(L, "argument \"%s\" is not a rect value", arg_name);
    return rect;
}

static Camera2D* cmt_check_camera(lua_State* L, const int idx, const char* arg_name)
{
    Camera2D* cam = lua_touserdata(L, idx);
    if (cam == NULL)
        luaL_error(L, "argument \"%s\" is not a camera value", arg_name);
    return cam;
}

// TODO: think about the file structure some more, and how you could move these out of global scope
Rectangle source = { 0 };
Rectangle dest = { 0 };
Vector2 origin = { 0 };
Vector2 position = { 0 };

// internal functions used by bindings
static Rectangle* cmt_rect_new_internal(lua_State* L, const float x, const float y, const float width, const float height)
{
    const Rectangle rect = { x, y, width, height };
    Rectangle* rect_ptr = lua_newuserdata(L, sizeof(Rectangle));
    *rect_ptr = rect;

    luaL_getmetatable(L, "__mt_rect");
    lua_setmetatable(L, -2);

    return rect_ptr;
}

static Color* cmt_color_new_internal(lua_State* L, const int r, const int g, const int b, const int a)
{
    const Color color = { r, g, b, a };
    Color* color_ptr = lua_newuserdata(L, sizeof(Color));
    *color_ptr = color;

    luaL_getmetatable(L, "__mt_color");
    lua_setmetatable(L, -2);

    return color_ptr;
}

static Camera2D* cmt_camera_new_internal(lua_State* L, const float x, const float y, const float rotation, const float zoom)
{
    const Camera2D cam = { -x, -y, 0, 0, rotation, zoom };
    Camera2D* cam_ptr = lua_newuserdata(L, sizeof(Camera2D));
    *cam_ptr = cam;

    luaL_getmetatable(L, "__mt_camera");
    lua_setmetatable(L, -2);

    return cam_ptr;
}

static int cmt_clear_background(lua_State* L)
{
    const Color* color = cmt_check_color(L, 1, "color");
    ClearBackground(*color);
    return 0;
}

static int cmt_data_load_text(lua_State* L)
{
    const char* file_path = luaL_checkstring(L, 1);
    const char* contents = LoadFileText(file_path);
    lua_pushstring(L, contents);
    return 1;
}

static int cmt_image_load(lua_State* L)
{
    const char* file_path = luaL_checkstring(L, 1);
    const Texture2D texture = LoadTexture(file_path);

    Texture2D* texture_ptr = lua_newuserdata(L, sizeof(Texture2D));
    *texture_ptr = texture;

    luaL_getmetatable(L, "__mt_image");
    lua_setmetatable(L, -2);

    return 1;
}

static int cmt_image_index(lua_State* L)
{
    const Texture2D* image = cmt_check_image(L, 1, "image");
    const char* key = luaL_checkstring(L, 2);

    if (strcmp(key, "width") == 0)
    {
        lua_pushinteger(L, image->width);
    }
    else if (strcmp(key, "height") == 0)
    {
        lua_pushinteger(L, image->height);
    }
    else
    {
        return luaL_error(L, "Image has no field \"%s\".", key);
    }

    return 1;
}

static int cmt_image_split_regions(lua_State* L)
{
    const Texture2D* image = cmt_check_image(L, 1, "image");
    const lua_Integer rows = luaL_checkinteger(L, 2);
    const lua_Integer cols = luaL_checkinteger(L, 3);

    const int region_width = image->width / cols;
    const int region_height = image->height / rows;
    const int region_count = rows * cols;


    lua_createtable(L, region_count, 0);
    for (int i = 0; i < region_count; ++i)
    {
        lua_pushinteger(L, i + 1);

        const int x = i * region_width % image->width;
        const int y = (int)floor(i * region_height / image->width) * region_height;
        cmt_rect_new_internal(L, (float)x, (float)y, (float)region_width, (float)region_height);
        lua_settable(L, -3);
    }

    return 1;
}

static int cmt_image_draw(lua_State* L)
{
    const Texture2D* image = cmt_check_image(L, 1, "image");
    const lua_Number x = luaL_checknumber(L, 2);
    const lua_Number y = luaL_checknumber(L, 3);

    source.width = (float)image->width;
    source.height = (float)image->height;

    position.x = (float)x;
    position.y = (float)y;

    DrawTextureRec(*image, source, position, WHITE);
    return 0;
}

static int cmt_image_draw_ex(lua_State* L)
{
    const Texture2D* image = cmt_check_image(L, 1, "image");
    const lua_Number x = luaL_checknumber(L, 2);
    const lua_Number y = luaL_checknumber(L, 3);
    const lua_Number rotation = luaL_checknumber(L, 4);
    const lua_Number scale_x = luaL_checknumber(L, 5);
    const lua_Number scale_y = luaL_checknumber(L, 6);
    const bool flip_x = lua_toboolean(L, 7);
    const bool flip_y = lua_toboolean(L, 8);
    const Color* tint = cmt_check_color(L, 9, "tint");

    if (flip_x)
        source.width = -(float)abs(image->width);
    else
        source.width = (float)image->width;

    if (flip_y)
        source.height = -(float)abs(image->width);
    else
        source.height = (float)image->height;

    dest.x = (float)x;
    dest.y = (float)y;
    dest.width = (float)(image->width * scale_x);
    dest.height = (float)(image->height * scale_y);

    DrawTexturePro(*image, source, dest, origin, rotation, *tint);
    return 0;
}

static int cmt_image_draw_region(lua_State* L)
{
    const Texture2D* image = cmt_check_image(L, 1, "image");
    const lua_Number x = luaL_checknumber(L, 2);
    const lua_Number y = luaL_checknumber(L, 3);
    const Rectangle* region = cmt_check_rect(L, 4, "region");

    source = *region;

    position.x = (float)x;
    position.y = (float)y;

    DrawTextureRec(*image, source, position, WHITE);
    return 0;
}

static int cmt_image_draw_region_ex(lua_State* L)
{
    const Texture2D* image = cmt_check_image(L, 1, "image");
    const lua_Number x = luaL_checknumber(L, 2);
    const lua_Number y = luaL_checknumber(L, 3);
    const lua_Number rotation = luaL_checknumber(L, 4);
    const lua_Number scale_x = luaL_checknumber(L, 5);
    const lua_Number scale_y = luaL_checknumber(L, 6);
    const bool flip_x = lua_toboolean(L, 7);
    const bool flip_y = lua_toboolean(L, 8);
    const Color* tint = cmt_check_color(L, 9, "tint");
    const Rectangle* region = cmt_check_rect(L, 10, "region");

    source = *region;

    if (flip_x)
        source.width = -fabsf(source.width);

    if (flip_y)
        source.height = -fabsf(source.width);

    dest.x = (float)x;
    dest.y = (float)y;
    dest.width = (float)(source.width * scale_x);
    dest.height = (float)(source.height * scale_y);

    DrawTexturePro(*image, source, dest, origin, rotation, *tint);
    return 0;
}

static int cmt_input_key_down(lua_State* L)
{
    const lua_Integer code = luaL_checkinteger(L, 1);
    const bool result = IsKeyDown((int)code);
    lua_pushboolean(L, result);
    return 1;
}

static int cmt_input_key_pressed(lua_State* L)
{
    const lua_Integer code = luaL_checkinteger(L, 1);
    const bool result = IsKeyPressed((int)code);
    lua_pushboolean(L, result);
    return 1;
}

static int cmt_input_key_released(lua_State* L)
{
    const lua_Integer code = luaL_checkinteger(L, 1);
    const bool result = IsKeyReleased((int)code);
    lua_pushboolean(L, result);
    return 1;
}

static int cmt_camera_new(lua_State* L)
{
    float x = 0;
    float y = 0;
    float rotation = 0;
    float zoom = 1;

    if (lua_gettop(L) != 0)
    {
        x = (float)luaL_checknumber(L, 1);
        y = (float)luaL_checknumber(L, 2);
        rotation = (float)luaL_checknumber(L, 3);
        zoom = (float)luaL_checknumber(L, 4);
    }

    cmt_camera_new_internal(L, x, y, rotation, zoom);
    return 1;
}

static int cmt_camera_index(lua_State* L)
{
    const Camera2D* cam = cmt_check_camera(L, 1, "camera");
    const char* key = luaL_checkstring(L, 2);

    if (key[0] == 'x')
    {
        lua_pushnumber(L, -cam->offset.x);
    }
    else if (key[0] == 'y')
    {
        lua_pushnumber(L, -cam->offset.y);
    }
    else if (strcmp(key, "rotation") == 0)
    {
        lua_pushnumber(L, cam->rotation);
    }
    else if (strcmp(key, "zoom") == 0)
    {
        lua_pushnumber(L, cam->zoom);
    }
    else
    {
        return luaL_error(L, "Camera has no field \"%s\".", key);
    }

    return 1;
}

static int cmt_camera_newindex(lua_State* L)
{
    Camera2D* cam = cmt_check_camera(L, 1, "camera");
    const char* key = luaL_checkstring(L, 2);

    if (key[0] == 'x')
    {
        cam->offset.x = -(float)luaL_checknumber(L, 3);
    }
    else if (key[0] == 'y')
    {
        cam->offset.y = -(float)luaL_checknumber(L, 3);
    }
    else if (strcmp(key, "rotation") == 0)
    {
        cam->rotation = (float)luaL_checknumber(L, 3);
    }
    else if (strcmp(key, "zoom") == 0)
    {
        cam->zoom = (float)luaL_checknumber(L, 3);
    }
    else
    {
        return luaL_error(L, "Camera has no field \"%s\".", key);
    }

    return 0;
}

static int cmt_camera_begin(lua_State* L)
{
    const Camera2D* cam = cmt_check_camera(L, 1, "camera");
    BeginMode2D(*cam);
    return 0;
}

static int cmt_camera_end(lua_State* L)
{
    EndMode2D();
    return 0;
}

static int cmt_rect_new(lua_State* L)
{
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

    if (lua_gettop(L) != 0)
    {
        x = (float)luaL_checknumber(L, 1);
        y = (float)luaL_checknumber(L, 2);
        width = (float)luaL_checknumber(L, 3);
        height = (float)luaL_checknumber(L, 4);
    }

    cmt_rect_new_internal(L, x, y, width, height);
    return 1;
}

static int cmt_rect_index(lua_State* L)
{
    const Rectangle* rect = cmt_check_rect(L, 1, "rect");
    const char* key = luaL_checkstring(L, 2);

    if (key[0] == 'x')
    {
        lua_pushnumber(L, rect->x);
    }
    else if (key[0] == 'y')
    {
        lua_pushnumber(L, rect->y);
    }
    else if (strcmp(key, "width") == 0)
    {
        lua_pushnumber(L, rect->width);
    }
    else if (strcmp(key, "height") == 0)
    {
        lua_pushnumber(L, rect->height);
    }
    else
    {
        return luaL_error(L, "Rect has no field \"%s\".", key);
    }

    return 1;
}

static int cmt_rect_newindex(lua_State* L)
{
    Rectangle* rect = cmt_check_rect(L, 1, "rect");
    const char* key = luaL_checkstring(L, 2);

    if (key[0] == 'x')
    {
        rect->x = (float)luaL_checknumber(L, 3);
    }
    else if (key[0] == 'y')
    {
        rect->y = (float)luaL_checknumber(L, 3);
    }
    else if (strcmp(key, "width") == 0)
    {
        rect->width = (float)luaL_checknumber(L, 3);
    }
    else if (strcmp(key, "height") == 0)
    {
        rect->height = (float)luaL_checknumber(L, 3);
    }
    else
    {
        return luaL_error(L, "Rect has no field \"%s\".", key);
    }

    return 0;
}

static int cmt_color_new(lua_State* L)
{
    int r = 255;
    int g = 255;
    int b = 255;
    int a = 255;

    if (lua_gettop(L) != 0)
    {
        r = (int)luaL_checkinteger(L, 1);
        g = (int)luaL_checkinteger(L, 2);
        b = (int)luaL_checkinteger(L, 3);
        a = (int)luaL_checkinteger(L, 4);
    }

    cmt_color_new_internal(L, r, g, b, a);
    return 1;
}

static int cmt_color_index(lua_State* L)
{
    const Color* color = cmt_check_color(L, 1, "color");
    const char* key = luaL_checkstring(L, 2);

    if (key[0] == 'r')
    {
        lua_pushinteger(L, color->r);
    }
    else if (key[0] == 'g')
    {
        lua_pushinteger(L, color->g);
    }
    else if (key[0] == 'b')
    {
        lua_pushinteger(L, color->b);
    }
    else if (key[0] == 'a')
    {
        lua_pushinteger(L, color->a);
    }
    else
    {
        return luaL_error(L, "Color has no field \"%s\".", key);
    }

    return 1;
}

static int cmt_color_newindex(lua_State* L)
{
    Color* color = cmt_check_color(L, 1, "color");
    const char* key = luaL_checkstring(L, 2);

    if (key[0] == 'r')
    {
        color->r = luaL_checkinteger(L, 3);
    }
    else if (key[0] == 'g')
    {
        color->g = luaL_checkinteger(L, 3);
    }
    else if (key[0] == 'b')
    {
        color->b = luaL_checkinteger(L, 3);
    }
    else if (key[0] == 'a')
    {
        color->a = luaL_checkinteger(L, 3);
    }
    else
    {
        return luaL_error(L, "Color has no field \"%s\".", key);
    }

    return 0;
}

static void register_raylib_constants(lua_State* L)
{
    cmt_register_input(L, KEY_W, "KEY_W");
    cmt_register_input(L, KEY_A, "KEY_A");
    cmt_register_input(L, KEY_S, "KEY_S");
    cmt_register_input(L, KEY_D, "KEY_D");
    cmt_register_input(L, KEY_UP, "KEY_UP");
    cmt_register_input(L, KEY_LEFT, "KEY_LEFT");
    cmt_register_input(L, KEY_DOWN, "KEY_DOWN");
    cmt_register_input(L, KEY_RIGHT, "KEY_RIGHT");
    cmt_register_input(L, KEY_SPACE, "KEY_SPACE");
}

void main_loop(void* arg)
{
    Engine* engine = arg;
    lua_State* L = engine->L;

    BeginDrawing();

    ClearBackground(RAYWHITE);

    if (engine->L != NULL && engine->script_active)
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
    }

    DrawFPS(0, 0);

    EndDrawing();
}

#ifdef __EMSCRIPTEN__
#ifdef DEBUG
static EM_BOOL test_socket_open(int eventType, const EmscriptenWebSocketOpenEvent *websocketEvent, void *userData)
{
    printf("opened socket connection in Emscripten\n");
    return EM_TRUE;
}

static EM_BOOL test_socket_error(int eventType, const EmscriptenWebSocketErrorEvent *websocketEvent, void *userData)
{
    printf("error in socket connection\n");
    return EM_TRUE;
}

static EM_BOOL test_socket_close(int eventType, const EmscriptenWebSocketCloseEvent *websocketEvent, void *userData)
{
    printf("closed socket connection in Emscripten\n");
    return EM_TRUE;
}

static EM_BOOL test_socket_message(int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData)
{
    if (websocketEvent->isText)
    {
        Engine* engine = userData;
        lua_State* L = engine->L;

        printf("running code with main VM...\n");
        const char* lua_code = (const char*)websocketEvent->data;
        luaL_dostring(L, lua_code);
        engine->script_active = true;
    }
    else
    {
        printf("binary message received\n");
    }
    return EM_TRUE;
}
#endif
#endif

int main(void)
{
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(600, 450, "game");

    lua_State* L = luaL_newstate();
    Engine engine = { L, false };
    luaL_openlibs(L);

    lua_register(L, "clear_background", cmt_clear_background);
    lua_register(L, "data_load_text", cmt_data_load_text);
    lua_register(L, "image_load", cmt_image_load);
    lua_register(L, "image_split_regions", cmt_image_split_regions);
    lua_register(L, "image_draw", cmt_image_draw);
    lua_register(L, "image_draw_ex", cmt_image_draw_ex);
    lua_register(L, "image_draw_region", cmt_image_draw_region);
    lua_register(L, "image_draw_region_ex", cmt_image_draw_region_ex);
    lua_register(L, "input_key_down", cmt_input_key_down);
    lua_register(L, "input_key_pressed", cmt_input_key_pressed);
    lua_register(L, "input_key_released", cmt_input_key_released);
    lua_register(L, "camera_new", cmt_camera_new);
    lua_register(L, "camera_begin", cmt_camera_begin);
    lua_register(L, "camera_end", cmt_camera_end);
    lua_register(L, "rect_new", cmt_rect_new);
    lua_register(L, "color_new", cmt_color_new);

    // set up metamethods
    if (!luaL_newmetatable(L, "__mt_image"))
        printf("Lua error: Image metatable at __mt_image already exists\n");

    lua_pushcfunction(L, cmt_image_index);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    if (!luaL_newmetatable(L, "__mt_camera"))
        printf("Lua error: Camera metatable at __mt_camera already exists\n");

    lua_pushcfunction(L, cmt_camera_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, cmt_camera_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);

    if (!luaL_newmetatable(L, "__mt_rect"))
        printf("Lua error: Rect metatable at __mt_rect already exists\n");

    lua_pushcfunction(L, cmt_rect_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, cmt_rect_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);

    if (!luaL_newmetatable(L, "__mt_color"))
        printf("Lua error: Color metatable at __mt_color already exists\n");

    lua_pushcfunction(L, cmt_color_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, cmt_color_newindex);
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);

    register_raylib_constants(L);

#ifdef __EMSCRIPTEN__

#ifdef DEBUG
    // run Lua code with the help of websockets in debug mode
    // TODO: validate if sockets are supported on this browser, should fail to create a connection and warn the user
    EmscriptenWebSocketCreateAttributes attr = { "ws://0.0.0.0:8000/", NULL, EM_TRUE };
    const EMSCRIPTEN_WEBSOCKET_T socket = emscripten_websocket_new(&attr);
    if (socket < 0)
    {
        printf("socket could not be created\n");
        return 1;
    }

    emscripten_websocket_set_onopen_callback(socket, &engine, test_socket_open);
    emscripten_websocket_set_onerror_callback(socket, &engine, test_socket_error);
    emscripten_websocket_set_onclose_callback(socket, &engine, test_socket_close);
    emscripten_websocket_set_onmessage_callback(socket, &engine, test_socket_message);
#else
    // run embedded Lua code in production
    if (luaL_loadfile(L, "user/main.lua"))
    {
        printf("Lua error: %s\n", lua_tostring(L, -1));
        engine.script_active = false;
    }

    if (engine.script_active && lua_pcall(L, 0, 0, 0))
    {
        printf("Lua error: %s\n", lua_tostring(L, -1));
        engine.script_active = false;
    }
#endif

    emscripten_set_main_loop_arg(main_loop, &engine, 0, 1);
#else
    while (!WindowShouldClose()) main_loop();
#endif

    lua_close(L);
    CloseWindow();
    return 0;
}
