# SlangCompiler

Host-side shader compiler tool. It wraps the [Slang](https://github.com/shader-slang/slang) compiler and emits the
compiled shader along with the engine's reflection blob (`.keshrf`).

Slang is vendored as prebuilt binaries in `external/slang/`, and is loaded dynamically.
Current version: **2026.20**.

## Vendored files

```
external/slang/
├── include/                       Slang headers
└── lib/<platform>/<arch>/         platform: win32 | linux | macos, arch: x64 | aarch64
```

Only the binaries that the tool actually needs are kept:

| Platform | Files                                                                          | Role                                               |
|----------|--------------------------------------------------------------------------------|----------------------------------------------------|
| Windows  | `slang-compiler.lib`                                                           | Import library the tool links against              |
|          | `slang-compiler.dll`                                                           | The compiler                                       |
|          | `slang-glslang.dll`                                                            | Downstream loaded on demand, required for SPIR-V   |
| Linux    | `libslang-compiler.so.<version>`, `libslang-glslang-<release>.so`              | Compiler, SPIR-V downstream                        |
| macOS    | `libslang-compiler.<version>.dylib`, `libslang-glslang-<release>.dylib`        | Compiler, SPIR-V downstream                        |

Not needed, so intentionally not vendored: `gfx`, `slang-rt` (CPU/host-callable targets), `slang-llvm` (same, and
very large), `slang-glsl-module` (GLSL input only).

On Windows, the DXIL target additionally needs `dxcompiler.dll` and `dxil.dll`. They are taken from
`External/DirectXCompiler`, not from the Slang release.

At build time, CMake copies the required DLLs next to `SlangCompiler.exe` (Windows only for now), and installs them
alongside it.

## Updating the binaries

1. Pick a release on <https://github.com/shader-slang/slang/releases> and download one archive per platform and
   architecture (Windows, Linux and macOS, each for x86_64 and aarch64).
2. Replace `external/slang/include/` with the `include/` folder of any one archive (headers are identical across
   platforms).
3. For each archive, replace the files of the matching `external/slang/lib/<platform>/<arch>/` folder with the ones
   listed in the table above, taken from the archive's `bin/` and `lib/` folders. Remove the old versions: file names
   on Linux and macOS contain the version.
4. Update the version-dependent file names in `CMakeLists.txt` (`SLANG_LIB_NAME`, macOS and Linux) and the version
   mentioned at the top of this file.
5. Check that the list of required binaries hasn't changed, see below.
6. Rebuild `SlangCompiler`, then compile at least one shader for each target (`spirv`, `dxil`, and `metal` on macOS)
   from a directory that doesn't have Slang or the Vulkan SDK in the `PATH`.

### Checking which binaries are required

Slang splits the compiler into a few optional libraries, and that split has changed between releases (the compiler
used to be `slang.dll`, and is now `slang-compiler.dll`). To re-check after an update, copy the built executable into
an empty folder with only the candidate libraries, then compile a shader:

- A missing downstream shows up as `failed to load downstream compiler '...'` or `failed to load dynamic library '...'`.
- On Windows, `dumpbin /dependents <dll>` shows what each DLL imports, and `dumpbin /exports <dll>` shows what it
  forwards.

TODO: document the Linux and macOS steps once they have been validated on those platforms (the minimal set above was only
verified on Windows x64).
