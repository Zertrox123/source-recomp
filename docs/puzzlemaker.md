# Puzzle Maker on macOS

Puzzle Maker needs both a mounted SDK content tree and native map compilers.
Creating a writable SDK directory alone does not fix VMF export: the Linux
filesystem rejects absolute writes outside its mounted search paths before
calling fopen. App packaging, the app launcher and `run_game.py` regenerate the
user-data copy of `portal2/gameinfo.txt` from the Steam file and add an
`SDKCONTENT` search path. The custom path ID keeps GAME asset priority
unchanged. The usual overlay redirects exported VMF/BSP files to user data.

Build with your own Linux depot and game files:

```sh
python3 tools/gameprep.py --game portal2
python3 tools/build_game.py --game portal2 --release
python3 tools/build_puzzlemaker.py
python3 tools/make_app.py --game portal2
```

`./setup.sh --puzzle-maker` does the same. The compilers are opt-in because
they add about ten minutes to the build and 220 MB to the app; without the
option the editor opens, but Build and Play cannot compile the chamber. The
game still installs if the Linux depot has no `vbsp_linux` or the compilers
do not build.
When the build succeeds, packaging includes
`Contents/MacOS/portal2-map-compiler` and its matching images under
`Contents/Resources/map-compiler/prep`. Keep these private, like the game binary.
The game intercepts child launches of `vbsp_linux`, `vvis_linux`, and `vrad_linux`
and executes the native host with the existing arguments, pipes, and user-data
root. VBSP has a separate layout because its ET_EXEC address range overlaps the
game's libraries. VVIS and VRAD use their `launchable_dll_1` interfaces in that
host. ELF COPY relocations are applied after loading all images and imported
objects, before constructors run. The compiler host starts its mmap area at
`0x30000000` to leave room for its static segments.

For a repository diagnostic run, the host is found next to `portal2-native`,
with images in `build/tools/vbsp/prep`. `P2_MAP_COMPILER` and
`P2_MAP_COMPILER_IMAGES` override these paths. `P2_TRACE_FILES=preview.vmf`
traces actual file opens; it cannot show a write that the guest filesystem
rejects before fopen. Compiler child launches produce `[map-compiler]` lines.

Chambers and saves remain in their existing directories. These changes do not
migrate or delete them.

Compiler completion also needs SIGCHLD delivery. The runtime defers the host
signal until the guest next polls, invokes the registered guest handler, and
restores the CPU registers. The game handler reaps the compiler and records its
exit status. Process completion also invalidates parent overlay lookups after
external writes.
