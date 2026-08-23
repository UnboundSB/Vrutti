#include <node_api.h>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Extension Bridge Linker (Native Addon)
// Provides a fast-path native linking capability between the Node.js Extension Host and the C++ Core.

typedef const char* (*BridgeCallFn)(const char* payload);

napi_value DynamicLinkCall(napi_env env, napi_callback_info info, bool isRequest) {
    size_t argc = 2;
    napi_value args[2];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    
    if (status != napi_ok || argc < 1) {
        napi_throw_type_error(env, NULL, "Wrong number of arguments");
        return NULL;
    }
    
    size_t str_len;
    char method[256];
    napi_get_value_string_utf8(env, args[0], method, sizeof(method), &str_len);
    
    char payload[8192] = "{}";
    if (argc >= 2) {
        napi_get_value_string_utf8(env, args[1], payload, sizeof(payload), &str_len);
    }
    
    std::string methodStr(method);
    size_t slashPos = methodStr.find('/');
    napi_value result;

    if (slashPos == std::string::npos) {
        napi_create_string_utf8(env, "{\"status\":\"fallback\"}", NAPI_AUTO_LENGTH, &result);
        return result;
    }

    std::string component = methodStr.substr(0, slashPos);
    std::string action = methodStr.substr(slashPos + 1);

#ifdef _WIN32
    std::string libName = "vrutti_" + component + ".dll";
    HMODULE hMod = LoadLibraryA(libName.c_str());
    if (!hMod) {
        napi_create_string_utf8(env, "{\"status\":\"fallback\"}", NAPI_AUTO_LENGTH, &result);
        return result;
    }
    auto func = (BridgeCallFn)GetProcAddress(hMod, action.c_str());
#else
    std::string libName = "libvrutti_" + component + ".so";
    void* hMod = dlopen(libName.c_str(), RTLD_LAZY);
    if (!hMod) {
        napi_create_string_utf8(env, "{\"status\":\"fallback\"}", NAPI_AUTO_LENGTH, &result);
        return result;
    }
    auto func = (BridgeCallFn)dlsym(hMod, action.c_str());
#endif

    if (!func) {
#ifdef _WIN32
        FreeLibrary(hMod);
#else
        dlclose(hMod);
#endif
        napi_create_string_utf8(env, "{\"status\":\"fallback\"}", NAPI_AUTO_LENGTH, &result);
        return result;
    }

    // Call the dynamically linked function
    const char* nativeResult = func(payload);
    
    if (nativeResult) {
        napi_create_string_utf8(env, nativeResult, NAPI_AUTO_LENGTH, &result);
    } else {
        napi_create_string_utf8(env, "{\"status\":\"ok\"}", NAPI_AUTO_LENGTH, &result);
    }
    
#ifdef _WIN32
    FreeLibrary(hMod);
#else
    dlclose(hMod);
#endif
    return result;
}

napi_value SendRequestNative(napi_env env, napi_callback_info info) {
    return DynamicLinkCall(env, info, true);
}

napi_value SendNotificationNative(napi_env env, napi_callback_info info) {
    return DynamicLinkCall(env, info, false);
}

napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        { "sendRequestNative", NULL, SendRequestNative, NULL, NULL, NULL, napi_default, NULL },
        { "sendNotificationNative", NULL, SendNotificationNative, NULL, NULL, NULL, napi_default, NULL }
    };
    
    napi_define_properties(env, exports, 2, desc);
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
