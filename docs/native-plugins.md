# Native plugins

The game's Linux engine loads native code from the game folder: a Source
server plugin is a `.so` named by a `.vdf` file in `addons/`. On the Mac,
the runtime can load an arm64 dylib in its place, so a plugin can be written
for this port and dropped where the engine looks for the Linux one. It runs
in the game's process with full access to the guest, which is the same trust
a `.so` in `addons/` asks for on Linux.

The contract is [`runtime/native_plugin.h`](../runtime/native_plugin.h); the
loader is `runtime/native_plugin.c`. Tests: `tests/checks_native_plugin.c`,
with the fixture plugin `tests/native_plugin_fixture.c`.

## How a plugin is found

When the guest `dlopen()`s a path that is no recompiled module, and the file
at that path (after the runtime's usual path mapping) is a Mach-O image, the
runtime loads it with the host's `dlopen` and calls its
`rt_native_plugin_init(host, plugin)`. Any Mach-O at a path the engine opens
qualifies: the file keeps the name the engine asks for (`portal2-fpv.so`, for
example), only its content changes.

- `init` returns 0 to accept. A plugin should refuse (non-zero) a host whose
  `version` or `size` is older than what it was built for.
- A refused plugin stays loaded but is never handed out: every later
  `dlopen` of that path returns 0 (with a `dlerror` message, except with
  `RTLD_NOLOAD`) and `init` does not run again. Hooks it set are cleared.
- A plugin already loaded is found by its path first, so reopening it works
  even after its file was removed or replaced.
- `dlopen` with `RTLD_NOLOAD` returns the handle of a plugin already loaded,
  else 0, and never loads anything.
- `dlsym` on a plugin's handle returns what the plugin gave to
  `export_symbol`; `dlclose` only counts down.

## Lifetime

A plugin is never unloaded, as the recompiled modules are not: after the
engine's last `dlclose` (`plugin_unload`), the dylib stays mapped, the
functions it `define`d stay callable and its hooks keep firing. A plugin that
the engine unloads must stop its own work itself (in its `Unload` callback
for a Source server plugin), for example by removing its hooks. Loading it
again (`plugin_load`) reuses the same instance: `init` is not called a second
time.

## Hooks

- **Present** and **Reset** fire only on the Metal renderer (native
  Direct3D 9). With `P2_RENDERER=opengl` there is no Direct3D device and
  these hooks never run.
- **Events** fires on each `SDL_PollEvent` of the game, on its main thread,
  also in the menus, as a guest call.
- A hook and its `user` pointer are set together: setting or replacing one
  from any thread is safe, also while the render thread presents. Removing
  or replacing a hook does not wait for a call already running on another
  thread (Present runs on the render thread): a plugin that frees the state
  its hook uses must first make sure that hook is not running, for example
  with its own lock or by freeing it from the hook's own thread.
- `release_mouse(1)` holds back the game's relative mouse mode, window grab
  and cursor warps; `release_mouse(0)` applies the game's last request again.

## Platforms

Mac only. On iOS the fallback is compiled out: the app loads only code signed
with it, so a dylib among the game's data could not load anyway, and a
`dlopen` of such a path fails as for any missing module.

## Compatibility

`RT_PLUGIN_HOST_VERSION` makes `RtPluginHost` a public ABI. A change keeps
the layout of the existing fields and adds new ones at the end (a plugin can
check `size`), or bumps the version when an existing field changes meaning.
