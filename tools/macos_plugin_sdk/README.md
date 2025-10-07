# Notepad++ macOS Plugin SDK (Preview)

This SDK packages the headers, build presets and a reference project required to compile Notepad++ plugins for the macOS port. The goals are:

- expose the familiar `NPPM_*` messaging surface without depending on Win32 headers,
- provide a portable `PluginAPI` wrapper that maps the Windows exports to the macOS host bridge,
- deliver ready-to-run CMake and Xcode configurations for building universal `*.nppplugin` bundles,
- document the layout expected by the macOS plugin loader.

## Layout

```
include/
  npp/
    PluginAPI.h           # portable function prototypes and shared structs
    NotepadPlusMsgs.h     # full NPPM_* catalogue with macOS-friendly Win32 aliases
  scintilla/
    ...                   # Scintilla public headers required for SCI_* messages
src/
  HelloPlugin.cpp         # sample plugin exposing one command
CMakeLists.txt             # builds HelloMacPlugin and packages the headers
CMakePresets.json          # Ninja + Xcode presets (arm64/x86_64 universal)
```

## Building with CMake (Ninja)

```
cmake --preset ninja-release
cmake --build --preset build-ninja-release
cpack --config build/ninja-release/CPackConfig.cmake
```

The resulting artefacts live under `build/ninja-release/`. Installing the target (`cmake --install`) produces a staging directory:

```
HelloMacPlugin/
  HelloMacPlugin.nppplugin    # loadable module copied to Notepad++ plugins folder
include/
  ...                         # headers for redistribution
```

## Generating an Xcode project

On macOS you can create an Xcode workspace via the preset:

```
cmake --preset xcode-universal
open build/xcode-universal/NotepadPlusPlusMacPluginSDK.xcodeproj
```

The scheme `HelloMacPlugin` builds a universal binary that links against the AppKit framework and emits an `.nppplugin` module suitable for copying into `~/Library/Application Support/Notepad++/plugins/`.

## Packaging

The SDK ships with a CPack configuration that creates a `.tar.gz` archive containing the headers and (optionally) the sample plugin binaries. Run `cpack` from either build tree to produce the distributable. The top-level `LICENSE.txt` replicates the Notepad++ GPLv3 licence for downstream consumers.

## Integrating with the macOS host

- The plugin host will translate `NPPM_*` messages and surface UI updates through the Liquid Glass/Qt bridge.
- `PluginAPI.h` defines `NppData` using opaque window handles (`void*`) on macOS. Plugins should treat these as tokens.
- `SCNotification` structures are delivered unchanged from Scintilla.
- Use wide-character (`wchar_t`) APIs for menu names and command strings. `isUnicode()` must return `true`.

For more background see [`docs/macos/plugin_sdk_guide.md`](../../docs/macos/plugin_sdk_guide.md) and [`docs/macos/plugin_api_compatibility.md`](../../docs/macos/plugin_api_compatibility.md).
