// Native plugins (runtime/native_plugin.c): the guest's dlopen / dlsym /
// dlclose on a Mach-O image, with the two dylibs the Makefile builds from
// tests/native_plugin_fixture.c (build/native-plugin-ok.dylib, and
// build/native-plugin-refuse.dylib whose init refuses the host).
#include "checks.h"
#include "modules.h"
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATHS 0x2001e000u   // guest strings
#define STACK 0x20010000u

static uint32_t guest_call_at(uint32_t function, unsigned count, const uint32_t *args) {
    CPU c; rt_cpu_reset(&c); c.esp = STACK;
    for (unsigned i = count; i; --i) PUSH(&c, args[i - 1]);
    rt_call(&c, function, RT_RETURN_SENTINEL);
    assert(c.esp == STACK - 4 * count);
    return c.eax;
}
#define ARGS(...) sizeof((uint32_t[]){__VA_ARGS__}) / 4, (uint32_t[]){__VA_ARGS__}
#define CALL(name, ...) guest_call_at(rt_dynamic_import(name), ARGS(__VA_ARGS__))

static uint32_t guest_string(uint32_t at, const char *text) {
    strcpy(GPTR(at), text);
    return at;
}
static uint32_t fixture_path(uint32_t at, const char *file) {
    char real[PATH_MAX];
    assert(realpath(file, real) && "build the fixture dylibs with make");
    return guest_string(at, real);
}
// The fixture's own counters, through the host's dlsym. RTLD_NOLOAD: NULL
// while the runtime has not loaded that dylib.
static int *fixture_counter(const char *file, const char *name) {
    void *library = dlopen(file, RTLD_NOW | RTLD_NOLOAD);
    if (!library) return NULL;
    int *counter = dlsym(library, name);
    dlclose(library);
    return counter;
}
static int dlerror_says(const char *text) {
    uint32_t message = CALL("dlerror", 0);
    return message && strstr(GPTR(message), text) && CALL("dlerror", 0) == 0;
}

RT_CHECK(native_plugin_open_symbol_close) {
    const char *file = "build/native-plugin-ok.dylib";
    uint32_t path = fixture_path(PATHS, file);
    uint32_t add = guest_string(PATHS + 0x400, "fixture_ok_add");
    uint32_t unknown = guest_string(PATHS + 0x440, "no_such_symbol");
    while (CALL("dlerror", 0)) {}

    // RTLD_NOLOAD before any load: 0, and the dylib is not even mapped.
    assert(CALL("dlopen", path, 4 | 2) == 0 && CALL("dlerror", 0) == 0);
    assert(!fixture_counter(file, "fixture_init_calls"));

    uint32_t handle = CALL("dlopen", path, 2);
    assert(handle >= 0xcd000000u && (handle - 0xcd000000u) % 0x100 == 0);
    int *init_calls = fixture_counter(file, "fixture_init_calls");
    assert(init_calls && *init_calls == 1);
    assert(CALL("dlopen", path, 2) == handle && *init_calls == 1);   // same plugin, initialised once
    assert(CALL("dlopen", path, 4 | 2) == handle && *init_calls == 1);   // RTLD_NOLOAD: loaded, counted

    uint32_t function = CALL("dlsym", handle, add);
    assert(function && guest_call_at(function, ARGS(40, 2)) == 42);   // plugin code called as a guest import
    assert(CALL("dlsym", handle, unknown) == 0 && dlerror_says("no_such_symbol"));

    int *events = fixture_counter(file, "fixture_events_calls");
    int before = *events;
    rt_native_plugins_events();
    assert(*events == before + 1);

    for (int i = 0; i < 3; ++i) assert(CALL("dlclose", handle) == 0);   // three opens
    assert(CALL("dlclose", handle) == UINT32_MAX && dlerror_says("not open"));
    assert(CALL("dlsym", handle, add) == function);   // never unloaded: the code stays callable
    rt_native_plugins_events();
    assert(*events == before + 2);   // and its hooks keep firing (docs/native-plugins.md)
}

RT_CHECK(native_plugin_refused_init) {
    const char *file = "build/native-plugin-refuse.dylib";
    uint32_t path = fixture_path(PATHS + 0x800, file);
    while (CALL("dlerror", 0)) {}

    assert(CALL("dlopen", path, 2) == 0 && dlerror_says("native plugin failed to load"));
    int *init_calls = fixture_counter(file, "fixture_init_calls");
    int *events = fixture_counter(file, "fixture_events_calls");
    assert(init_calls && *init_calls == 1 && events);

    // Refused once, refused for good: no handle, init not run again.
    assert(CALL("dlopen", path, 2) == 0 && dlerror_says("native plugin failed to load"));
    assert(CALL("dlopen", path, 4 | 2) == 0);
    assert(*init_calls == 1);

    int before = *events;
    rt_native_plugins_events();   // the hook set before refusing is cleared
    assert(*events == before);
}

// A loaded plugin is found by its path before its file is read again: it
// reopens after the file is removed.
RT_CHECK(native_plugin_reopen_after_removal) {
    const char *copy = "build/native-plugin-copy.dylib";
    FILE *in = fopen("build/native-plugin-ok.dylib", "rb"), *out = fopen(copy, "wb");
    assert(in && out);
    char buffer[65536];
    for (size_t n; (n = fread(buffer, 1, sizeof(buffer), in));) assert(fwrite(buffer, 1, n, out) == n);
    fclose(in); fclose(out);
    uint32_t path = fixture_path(PATHS + 0xc00, copy);
    uint32_t handle = CALL("dlopen", path, 2);
    assert(handle);
    assert(unlink(copy) == 0);
    assert(CALL("dlopen", path, 2) == handle && CALL("dlopen", path, 4 | 2) == handle);
    for (int i = 0; i < 3; ++i) assert(CALL("dlclose", handle) == 0);
}
