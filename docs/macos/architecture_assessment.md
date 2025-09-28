# Przegląd architektury i analiza portu macOS

## 1. Inwentaryzacja komponentów

### 1.1 PowerEditor/src
- **Wejście aplikacji i rdzeń** – `winmain.cpp`, `Notepad_plus.cpp`, `NppBigSwitch.cpp` i `Notepad_plus_Window.cpp` obsługują pętlę komunikatów Win32, inicjalizację COM (`CoInitialize`) oraz uruchamianie procesów przez `ShellExecute`, co wiąże logikę startową z API Windows.【F:PowerEditor/src/winmain.cpp†L1-L71】【F:PowerEditor/src/NppIO.cpp†L148-L156】【F:PowerEditor/src/NppCommands.cpp†L176-L235】
- **Warstwa UI zależna od Win32** – katalog `WinControls` zawiera kontrolki z bezpośrednimi `#include <windows.h>` oraz implementacje `HWND/WNDPROC`, obejmujące m.in. dokowanie, pasek kart, mapę dokumentu i menedżer okien.【F:PowerEditor/src/WinControls/Window.h†L1-L44】【F:PowerEditor/src/WinControls/DockingWnd/DockingManager.h†L1-L46】
- **Funkcje systemowe i narzędzia** – moduły z katalogu `MISC` udostępniają funkcje powłoki (`SHFileOperation`), kryptografię, integrację z rejestrem oraz menedżera wtyczek z nagłówkami Win32, co wymaga abstrakcji przy portowaniu.【F:PowerEditor/src/MISC/Common/Common.cpp†L1343-L1356】【F:PowerEditor/src/MISC/PluginsManager/Notepad_plus_msgs.h†L1-L79】
- **Komponent Scintilla i logika edycji** – katalog `ScintillaComponent` oraz biblioteka `TinyXml`, `Utf8_16` i `json` są w większości przenośne, ponieważ opierają się na API Scintilla i standardowych bibliotekach, choć dialogi `.rc` wymagają zamiany na rozwiązania macOS.【F:PowerEditor/src/ScintillaComponent/ScintillaCtrls.h†L1-L60】【F:PowerEditor/src/TinyXml/tinyXmlA/tinyxmlA.h†L1-L53】
- **Warstwa abstrakcji** – nowy katalog `Platform` zawiera `PathProvider`, który może być rozszerzany o implementacje macOS i stanowi punkt wejścia do dalszego wygaszania bezpośrednich wywołań Win32.【F:PowerEditor/src/Platform/PathProvider.cpp†L1-L146】

**Klasyfikacja przenośności (PowerEditor/src)**

| Obszar | Główne pliki/katalogi | Ocena przenośności |
| --- | --- | --- |
| Logika edytora i model dokumentu | `ScintillaComponent/`, `Utf8_16.cpp`, `EncodingMapper.*`, `lastRecentFileList.*` | Wysoka – wymaga jedynie adaptacji UI |
| Konfiguracja i serializacja | `Parameters.*`, `TinyXml/`, `json/` | Średnia – zależna od ścieżek użytkownika i rejestru |
| Interfejs użytkownika | `WinControls/`, `Notepad_plus*.cpp`, zasoby `.rc` | Niska – ściśle związana z Win32 |
| Integracje systemowe | `MISC/Common`, `NppIO.cpp`, `NppCommands.cpp` | Niska – liczne API Windows |

### 1.2 Scintilla
- Repozytorium zawiera katalogi platform (`cocoa`, `gtk`, `qt`, `win32`) oraz wspólny kod rdzenia w `src/` i nagłówki w `include/`, co ułatwia budowę wariantu Cocoa i wykorzystanie istniejącego backendu renderującego.【F:scintilla/README†L9-L33】【F:scintilla/README†L83-L92】【F:scintilla/cocoa/ScintillaView.mm†L1-L60】
- Część funkcji edytora (np. rysowanie) jest już obsługiwana w `cocoa/`, jednak integracja z Notepad++ wymaga warstwy klejącej zamiast obecnych wrapperów Win32 w `ScintillaComponent`.

### 1.3 Lexilla
- Dostarcza leksersy i analizatory składni w katalogach `lexers/`, `lexlib/` i `src/`, które są niezależne od platformy, ale obecny proces budowy (skrypty `zipsrc.bat`, `delbin.bat`) jest ukierunkowany na Windows i wymaga alternatywnych skryptów dla macOS.【F:lexilla/README†L1-L60】

## 2. Mapowanie zależności od Windows na odpowiedniki macOS

| Obszar funkcjonalny | Obecne API Windows | Proponowane podejście na macOS |
| --- | --- | --- |
| Uruchamianie procesów i integracja z powłoką | `ShellExecute`, `ShellExecuteEx` w `NppCommands.cpp`, `NppIO.cpp`, `Common.cpp` | `NSWorkspace`/`LSOpenCFURLRef` do otwierania plików i URL oraz `NSTask` do uruchamiania poleceń |
| Foldery użytkownika i uprawnienia | `SHFileOperation`, specjalne foldery `CSIDL` w `Common.cpp` | `FileManager`/`NSURL` oraz katalogi `~/Library/...` udostępniane przez `FileManager.urls(for:in:)` |
| Monitorowanie systemu plików | `ReadDirectoryChangesW` i własne wątki w `WinControls/ReadDirectoryChanges` | `FSEvents` lub `DispatchSourceFileSystemObject` z integracją przez CFRunLoop |
| Schowek i drag&drop | Bezpośrednie komunikaty `WM_COPYDATA`, `DragAcceptFiles` w `WinControls` | `NSPasteboard`, `NSDraggingDestination` i `NSDocumentController` |
| Powiadomienia i zasobnik systemowy | `Shell_NotifyIcon`, okna typu `HWND` w `TrayIconControler` | `NSUserNotificationCenter`/`UNUserNotificationCenter` oraz `NSStatusItem` |
| Pętla zdarzeń i okna | `HWND`, `WndProc`, `CreateWindowEx` w `Notepad_plus_Window.cpp` i `WinControls` | Natywne `NSWindow`, `NSView` lub warstwa abstrakcji z użyciem Cocoa/SwiftUI |
| COM i integracja z Explorerem | `CoInitialize`, `IShellLink` w `NppIO.cpp` | Zamiana na API LaunchServices, `CFURL`, ewentualnie `NSUserActivity` |
| Aktualizacje sieciowe | `wininet.h` w `Notepad_plus.cpp` i zależności linkera | `URLSession`, `CFNetwork` lub integracja z frameworkiem Sparkle |
| Drukowanie | `PrintDlg`, `StartDoc` (implikowane w `MISC/Print`) | `NSPrintOperation`, `PMPrintSession` |

## 3. Analiza ryzyk i ograniczeń

1. **Warstwa UI i docking** – `WinControls/DockingWnd`, `TabBar`, `SplitterContainer` i dialogi `.rc` wymagają kompletnego przepisania na Cocoa, co wiąże się z ryzykiem regresji w zarządzaniu wieloma widokami i panelami pomocniczymi.【F:PowerEditor/src/WinControls/DockingWnd/Docking.h†L1-L88】【F:PowerEditor/src/WinControls/TabBar/TabBar.h†L1-L80】
2. **API wtyczek** – Nagłówek `Notepad_plus_msgs.h` wymusza użycie `HWND`, `UINT`, `WPARAM/LPARAM`, a wiele komunikatów wykorzystuje identyfikatory zasobów Win32, co oznacza konieczność zdefiniowania warstwy kompatybilności lub nowego SDK dla macOS.【F:PowerEditor/src/MISC/PluginsManager/Notepad_plus_msgs.h†L31-L120】
3. **Integracja systemowa i bezpieczeństwo** – Funkcje takie jak podpisywanie plików (`verifySignedfile.cpp`), UAC (`allowPrivilegeMessages` w `winmain.cpp`) oraz uruchamianie z uprawnieniami administratora (`runas` w `NppIO.cpp`) nie mają bezpośrednich odpowiedników i wymagają zaprojektowania alternatywnych ścieżek zgodnych z politykami bezpieczeństwa Apple.【F:PowerEditor/src/winmain.cpp†L19-L44】【F:PowerEditor/src/MISC/Common/verifySignedfile.cpp†L1-L60】【F:PowerEditor/src/NppIO.cpp†L723-L784】
4. **Proces budowania i dystrybucji** – Istniejące pliki projektu (`visual.net/notepadPlus.vcxproj`, skrypty `.bat`) są dedykowane Windows; należy przygotować konfigurację CMake/Xcode, pipeline CI oraz proces podpisywania i notarizacji dla pakietu `.app`.【F:PowerEditor/visual.net/notepadPlus.vcxproj†L1-L20】
5. **Testy i regresje** – Brak testów jednostkowych w repozytorium powoduje wysokie ryzyko regresji podczas refaktoryzacji; priorytetem jest wybór frameworku (Catch2/GoogleTest) i pokrycie krytycznej logiki (zarządzanie dokumentami, ustawienia, integracje pluginów).【F:macOS_convert_todo.md†L66-L91】

Dokument stanowi podstawę do aktualizacji planu `macOS_convert_todo.md` i podjęcia kolejnych iteracji portu.
