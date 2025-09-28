# Plan systemu budowania Notepad++ dla macOS

## Cele
- Zapewnić w pełni natywne binaria macOS (x86_64 oraz arm64) z możliwością tworzenia uniwersalnych pakietów `.app`.
- Wydzielić wspólne komponenty (PowerEditor core, Scintilla, Lexilla, biblioteki pomocnicze) do wielokrotnego użycia w projektach CMake.
- Umożliwić łatwą integrację z CI/CD (GitHub Actions, lokalne środowiska) oraz narzędziami Apple (Xcode, codesign, notarization).

## Architektura CMake
1. **Top-level CMake**
   - Utrzymać obecny katalog `PowerEditor/src` jako główny projekt, ale podzielić źródła na logiczne biblioteki (`pe-core`, `pe-ui`, `platform-win`, `platform-macos`).
   - Użyć bibliotek typu `OBJECT` dla komponentów współdzielonych, dzięki czemu cele Windows i macOS otrzymają identyczny zestaw plików bez duplikacji.
   - Zastosować presety CMake (`CMakePresets.json`) dla konfiguracji `windows-msvc`, `macos-x86_64`, `macos-arm64` oraz `macos-universal` z odpowiednimi flagami kompilatora.

2. **Konfiguracja platformowa**
   - Zdefiniować cel `notepadplusplus_win` łączący aktualne moduły Win32 wraz z zasobami.
   - Dodać nowy cel `notepadplusplus_macos` zależny od bibliotek `platform-macos`, `scintilla-cocoa`, `lexilla-core` oraz wspólnych modułów edytora.
   - Wprowadzić interfejsowe targety (`npp::platform`, `npp::core`) eksportujące wymagane katalogi nagłówków i flagi (`/std:c++20` vs `-std=c++20`, `UNICODE`, `NPP_PLATFORM_COCOA`).

3. **Integracja zasobów i pakietu `.app`**
   - Dla macOS użyć modułu `BundleUtilities` oraz niestandardowego skryptu `tools/macos/create_app_bundle.cmake` generującego strukturę `Notepad++.app` (Resources, MacOS).
   - Osadzić pliki `Info.plist`, ikony `.icns` i zasoby tłumaczeń za pomocą `set_source_files_properties(... MACOSX_PACKAGE_LOCATION)`.

4. **Obsługa konfiguracji i kompilacji równoległej**
   - Zapewnić wsparcie dla `RelWithDebInfo` (domyślne w CI) oraz `Debug`/`Release`.
   - Dla uniwersalnych binariów wykorzystać `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` i zależne od architektury katalogi pośrednie (`build-macos-arm64`, `build-macos-x86_64`).

## Zależności zewnętrzne
- **Scintilla** – repozytorium zawiera katalog `cocoa` z gotowym backendem opartym na Cocoa (`ScintillaCocoa.mm`, `PlatCocoa.mm`). Plan zakłada zbudowanie osobnej biblioteki `scintilla-cocoa` zdefiniowanej w `scintilla/cocoa/CMakeLists.txt` i linkowanej do aplikacji macOS.
- **Lexilla** – moduł parserów nie posiada platformowo zależnego kodu, co pozwala na kompilację wspólną (`lexilla/src`, `lexilla/lexlib`). Wystarczy zapewnić, że ustawienia CMake udostępniają nagłówki i symbole eksportowane identycznie jak na Windows.
- **Boost.Regex** – dla macOS rekomendowane jest przejście na wbudowaną implementację dostarczaną przez systemową bibliotekę `libc++` lub wykorzystanie `Hunter`/`FetchContent` do pobierania biblioteki w ramach procesu CMake.
- **Uchardet, TinyXML, inne** – moduły są w repozytorium i mogą być budowane jako `OBJECT` lub `STATIC` w ramach głównego projektu, z odpowiednimi flagami kompilatora dla macOS (`-fvisibility=hidden`, `-Werror` w CI).

## CI/CD i automatyzacja
1. Przygotować workflow GitHub Actions `ci-macos.yml` z macOS runnerem (`macos-13`) wykonującym kroki:
   - instalacja narzędzi (`brew install ninja boost`),
   - konfiguracja poprzez `cmake --preset macos-universal`,
   - kompilacja i uruchomienie testów (`ctest`).
2. Artefakty: spakowana aplikacja `.app` oraz archiwum `.dSYM` dla symboli debugowania.
3. W dalszym kroku dodać automatyczne podpisywanie (tymczasowo zidentyfikowane jako krok manualny w planie) i notarizację.

## Kroki następne
- Wydzielenie kodu zależnego od Win32 do dedykowanych plików źródłowych i implementacji interfejsów `npp::platform`.
- Przygotowanie minimalnego front-endu Cocoa wykorzystującego Scintillę w kontrolce `NSView`.
- Zaimplementowanie presetów CMake i migracja istniejących skryptów budowy Windows do nowej struktury targetów.
- Konfiguracja testów jednostkowych (`GoogleTest`) uruchamianych w `ctest` oraz w CI macOS.

