// Native plugin for tests/checks_native_plugin.c, built by the Makefile as
// build/native-plugin-ok.dylib and, with -DREFUSE, as
// build/native-plugin-refuse.dylib (an init that refuses the host, after
// setting a hook, as a plugin refusing an older host would).
#include "native_plugin.h"
#include <string.h>

#ifdef REFUSE
#define NAME "fixture_refuse"
#else
#define NAME "fixture_ok"
#endif

// Read by the test through the host's dlsym (the image is never unloaded).
int fixture_init_calls;
int fixture_events_calls;

static const RtPluginHost *host;

#ifndef REFUSE
static void add(void *cpu, uint32_t args, void *data) {
    (void)data;
    uint32_t a, b;
    memcpy(&a, host->memory + args, 4);
    memcpy(&b, host->memory + args + 4, 4);
    host->set_return(cpu, a + b);
}
#endif

static void events(void *user) { fixture_events_calls += (int)(intptr_t)user; }

int rt_native_plugin_init(const RtPluginHost *table, RtPlugin *plugin) {
    ++fixture_init_calls;
    if (table->version < RT_PLUGIN_HOST_VERSION || table->size < sizeof(RtPluginHost)) return 1;
    host = table;
    table->set_events_hook(plugin, events, (void *)(intptr_t)1);
#ifdef REFUSE
    return 1;
#else
    table->export_symbol(plugin, NAME "_add", table->define(NAME ":add", add, NULL));
    return 0;
#endif
}
