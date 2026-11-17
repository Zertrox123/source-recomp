# Puzzle Maker on macOS

Puzzle Maker needs both a mounted SDK content tree and native map compilers.
Creating a writable SDK directory alone does not fix VMF export: the Linux
filesystem rejects absolute writes outside its mounted search paths before
calling fopen. `make_app.py` and `run_game.py` add an `SDKCONTENT` search path
to the user-data copy of `portal2/gameinfo.txt`, preserving the Steam original
and any existing user overrides. The custom path ID keeps GAME asset priority
unchanged. The usual overlay redirects exported VMF/BSP files to user data.

Build with your own Linux depot and game files:

```sh
python3 tools/gameprep.py --game portal2
python3 tools/build_game.py --game portal2 --release
python3 tools/build_puzzlemaker.py
python3 tools/make_app.py --game portal2
```

`setup.sh` also builds the compilers. Packaging includes
`Contents/MacOS/portal2-map-compiler` and its matching images under
`Contents/Resources/map-compiler/prep`. Keep these private, like the game binary.
The game intercepts child launches of `vbsp_linux`, `vvis_linux`, and `vrad_linux`
and executes the native host with the existing arguments, pipes, and user-data
root. VBSP has a separate layout because its ET_EXEC address range overlaps the
game's libraries. VVIS and VRAD use their `launchable_dll_1` interfaces in that
host. ELF COPY relocations are applied after loading all images and imported
objects, before constructors run. Static compiler BSS stays below the mmap area.

For a repository diagnostic run, the host is found next to `portal2-native`,
with images in `build/tools/vbsp/prep`. `P2_MAP_COMPILER` and
`P2_MAP_COMPILER_IMAGES` override these paths. `P2_TRACE_FILES=preview.vmf`
traces actual file opens; it cannot show a write that the guest filesystem
rejects before fopen. Compiler child launches produce `[map-compiler]` lines.

Chambers and saves remain in their existing directories. These changes do not
migrate or delete them.

Compiler completion also needs SIGCHLD delivery. The runtime defers the host signal to a guest call boundary, invokes the registered guest handler, and restores CPU registers. The game handler reaps the compiler and records its exit status; accepting registration without delivering the signal leaves the editor waiting on geometry. Process completion also invalidates parent overlay lookups after external writes.


## Validation

The original fix based on upstream `1250f32` was confirmed in-game on Apple
Silicon: Build/Play exported the VMF, completed VBSP, VVIS and HDR VRAD, and
loaded a playable chamber (about 38.5 seconds total). Chambers and saves
remained in place.

This version is adapted to upstream `8313c0c` and its multi-game build tools.
Automated coverage includes compiler image layout, COPY relocation load order,
SDK mounting and packaging, path canonicalization, temporary filenames,
child-process file visibility, and deferred SIGCHLD through both `sigaction`
and `signal`, including blocked delivery and CPU/mask preservation.
Validation on the adapted branch: 210 Python tests (3 skipped), all 262 runtime
checks, and the same 262 checks under UBSan passed. Preparing the real Linux
compiler depot also succeeded.

The adapted version still needs a fresh end-to-end in-game build before merge.
No game binaries or generated game code belong in the repository.
