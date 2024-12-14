#include <stdio.h>
#include <emscripten/websocket.h>
#include <ftw.h>
#include "util/b64.h"

#include "bindings.h"

static EM_BOOL test_socket_open(int eventType, const EmscriptenWebSocketOpenEvent* websocketEvent, void* userData)
{
    printf("opened socket connection in Emscripten\n");
    return EM_TRUE;
}

static EM_BOOL test_socket_error(int eventType, const EmscriptenWebSocketErrorEvent* websocketEvent, void* userData)
{
    printf("error in socket connection\n");
    return EM_TRUE;
}

static EM_BOOL test_socket_close(int eventType, const EmscriptenWebSocketCloseEvent* websocketEvent, void* userData)
{
    printf("closed socket connection in Emscripten\n");
    return EM_TRUE;
}

int remove_callback(const char *file_path, const struct stat *sb, int type_flag, struct FTW *ftw_buffer)
{
    if (remove(file_path) == 0)
        printf("File \"%s\" was removed\n", file_path);
    else
        printf("\"%s\" could not be removed.\n", file_path);

    return 0;
}

static EM_BOOL test_socket_message(int eventType, const EmscriptenWebSocketMessageEvent* websocketEvent, void* userData)
{
    if (websocketEvent->isText)
    {
        char* str = (char*)websocketEvent->data;

        // restart the lua VM upon receiving this message
        if (strcmp(str, "restart_lua") == 0)
        {
            Engine* engine = userData;
            if (engine->L != NULL)
            {
                lua_close(engine->L);
                engine->L = NULL;
            }
            initialise_lua(engine);
            run_lua_main(engine);
            return EM_TRUE;
        }

        // expected format for string is "<event_kind>,<file_path>,<contents>,<contents_length>,<text_or_binary>"
        // event_kind of type "remove" only provides "<event_kind>,<file_path>", need to check the rest for NULL
        // contents_length is ignored for text files, but it's still required for the hacky parser below
        const char* event_kind = strtok(str, ",");
        const char* file_path = strtok(NULL, ",");

        if (strcmp(event_kind, "remove") == 0)
        {
            // removing a directory should also remove everything inside it recursively
            if (DirectoryExists(file_path))
            {
                nftw(file_path, remove_callback, 64, FTW_DEPTH | FTW_PHYS);
            }
            else
            {
                if (remove(file_path) == 0)
                    printf("File \"%s\" was removed\n", file_path);
                else
                    printf("\"%s\" could not be removed.\n", file_path);
            }

            return EM_TRUE;
        }

        // you can safely parse contents after remove event has been processed and returned
        const char* contents = strtok(NULL, ",");
        const char* contents_length = strtok(NULL, ",");
        const char* text_or_binary = strtok(NULL, ",");
        if (contents == NULL || contents_length == NULL || text_or_binary == NULL)
        {
            printf("Received invalid message data format\n");
            return EM_FALSE;
        }

        // get directory path without file name, make the directory
        char dir_name[512] = {0};
        const size_t position = strrchr(file_path, '/') - file_path;
        strncpy(dir_name, file_path, position);
        MakeDirectory(dir_name);

        char* decoded = (char*)b64_decode(contents, strlen(contents));

        if (text_or_binary[0] == '0')
        {
            if (!SaveFileText(file_path, decoded))
            {
                printf("Could not save text file \"%s\"\n", file_path);
            }
        }
        else if (text_or_binary[0] == '1')
        {
            const int size = (int)strtol(contents_length, NULL, 0);
            if (!SaveFileData(file_path, decoded, size))
            {
                printf("Could not save binary file \"%s\"\n", file_path);
            }
        }
        else
        {
            printf("Invalid value for text_or_binary\n");
        }

        free(decoded);
        decoded = NULL;
    }
    else
    {
        printf("binary message received\n");
    }
    return EM_TRUE;
}

bool initialise_debug_client(Engine* engine)
{
    if (emscripten_websocket_is_supported())
    {
        EmscriptenWebSocketCreateAttributes attr = {"ws://0.0.0.0:8000/", NULL, EM_TRUE};
        const EMSCRIPTEN_WEBSOCKET_T socket = emscripten_websocket_new(&attr);
        if (socket < 0)
        {
            printf("WebSocket could not be created\n");
            return false;
        }

        emscripten_websocket_set_onopen_callback(socket, engine, test_socket_open);
        emscripten_websocket_set_onerror_callback(socket, engine, test_socket_error);
        emscripten_websocket_set_onclose_callback(socket, engine, test_socket_close);
        emscripten_websocket_set_onmessage_callback(socket, engine, test_socket_message);
    }
    else
    {
        printf("Cannot start Comet reload client, WebSockets are not supported on this browser\n");
        return false;
    }

    return true;
}