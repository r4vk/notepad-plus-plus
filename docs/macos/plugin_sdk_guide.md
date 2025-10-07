# macOS Plugin SDK Guide

## Cel

Zapewnienie gotowego zestawu nagłówków oraz szablonów budowania, które umożliwią deweloperom kompilację wtyczek Notepad++ na macOS bez zależności od WinAPI. Wersja "preview" koncentruje się na:

- utrzymaniu zgodności z komunikatami `NPPM_*` poprzez nagłówek z aliasami typów Win32,
- dostarczeniu przenośnego interfejsu `PluginAPI.h` oraz przykładowego modułu `HelloMacPlugin`,
- udokumentowaniu katalogów docelowych (`~/Library/Application Support/Notepad++/plugins/<NazwaPluginu>/`),
- przygotowaniu presetów CMake (Ninja oraz Xcode universal arm64/x86_64).

## Wymagania wstępne

- macOS 12 lub nowszy z zainstalowanym Xcode 15 (Command Line Tools),
- CMake 3.22+, Ninja (opcjonalnie) i Python 3 do automatyzacji pakowania,
- Qt 6.x oraz Liquid Glass UI nie są wymagane do kompilacji samej wtyczki, ale będą potrzebne przy integracji UI.

## Kroki budowy

1. `cd tools/macos_plugin_sdk`
2. `cmake --preset xcode-universal`
3. `cmake --build --preset build-xcode-universal`
4. (Opcjonalnie) `cpack --config build/xcode-universal/CPackConfig.cmake`

Instalacja (`cmake --install build/xcode-universal`) utworzy strukturę:

```
HelloMacPlugin/
  HelloMacPlugin.nppplugin
include/
  npp/
  scintilla/
```

Skopiuj `HelloMacPlugin.nppplugin` do `~/Library/Application Support/Notepad++/plugins/HelloMacPlugin/`.

## Użycie w Xcode

- Otwórz `build/xcode-universal/NotepadPlusPlusMacPluginSDK.xcodeproj`
- Scheme `HelloMacPlugin` generuje moduł z sufiksem `.nppplugin`
- Ustaw `INSTALL_PATH` na katalog wtyczki w preferencjach schematu, aby umożliwić Debug → Run z automatycznym kopiowaniem

## API i ograniczenia

- `PluginAPI.h` eksportuje `setInfo`, `getName`, `getFuncsArray`, `beNotified`, `messageProc`, `isUnicode`
- Struktura `NppData` zawiera uchwyty (`void*`) do instancji UI — traktuj je jako nieprzezroczyste tokeny
- Makra `NPPM_*`, `NPPN_*` i struktury (`ToolbarIcons`, `shortcutKey`, `SCNotification`) są dostępne bez WinAPI
- Funkcje wymagające Win32 (np. `NPPM_GETMENUHANDLE` zwracające `HMENU`) zwracają wskaźniki proxy — host macOS zapewni translację

## Dalsze kroki

- Dodanie generatora pakietu `.pkg` oraz podpisywania kodu
- Integracja testów integracyjnych ładowania wtyczek w pipeline CI macOS
- Aktualizacja dokumentacji wtyczek w User Manual po ustabilizowaniu API

## Historia zmian

- `v0.1-preview`: pierwszy drop SDK z nagłówkami, przykładowym pluginem i presetami CMake/Xcode.
