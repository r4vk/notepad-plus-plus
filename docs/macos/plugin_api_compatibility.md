# Warstwa kompatybilności API wtyczek dla macOS

## 1. Stan obecny
- Komunikacja z wtyczkami jest obecnie oparta o **HWND + SendMessage** (`NPPM_*`, `WM_COPYDATA`, `SCI_*`).
- Wtyczki otrzymują uchwyt `HWND` okna Notepad++ (pole `nppHandle` w strukturze `NppData`) oraz uchwyty kontrolek Scintilla (pola `scintillaMainHandle`, `scintillaSecondHandle`).
- Część funkcji pomocniczych (`setInfo`, `getName`, `beNotified`) oczekuje struktur zdefiniowanych w `Notepad_plus_msgs.h` oraz `plugin.h`. Struktury te zawierają typy ściśle powiązane z WinAPI (`HWND`, `HMENU`, `POINT`, `RECT`).
- Zarządzanie menu wtyczek oraz rejestracja poleceń używa `insertMenuItem`, `SetMenuItemInfo`, `CreatePopupMenu` itp. – wtyczki zakładają obecność klasycznego menu Windows.
- Mechanizm komunikacji rozszerzony o `NPPM_GETPLUGINSCONFIGDIR` oraz zapis w rejestrze (poprzez API Notepad++) podlega migracji do `PreferencesStore`.

## 2. Cele portu na macOS
1. **Zachować kompatybilność binarną dla wtyczek rozwijanych wspólnie (C/C++)**, umożliwiając im wysyłanie tych samych komunikatów logicznych.
2. **Udostępnić alternatywę dla `HWND`**: wtyczki powinny otrzymywać abstrakcję (np. uchwyt `npp::ui::WindowHandle`, identyfikator kanału IPC) zamiast natywnych uchwytów Windows.
3. **Zmapować komunikaty NPPM/SCI** na wywołania API Qt/AppKit**, tak aby logika wtyczek była wykonywana bez modyfikacji kodu lub z minimalnymi poprawkami.
4. **Zapewnić mechanizm hostowania wtyczek** (ładownie dylib, sandbox, podpisywanie) zgodny z wymaganiami Apple.

## 3. Kluczowe grupy komunikatów i wymagane adaptacje
| Grupa | Przykłady | Wymagane działania |
|-------|-----------|--------------------|
| Informacyjne | `NPPM_GETCURRENTSCINTILLA`, `NPPM_GETCURRENTLANGTYPE` | Zwrot identyfikatorów widoków (np. UUID powierzchni Scintilla) zamiast fizycznych `HWND`. Warstwa compatibility tłumaczy na obiekty Scintilla embedowane w Qt. |
| Zarządzanie oknami | `NPPM_MODELESSDIALOG`, `NPPM_DMMREGASDCKDLG` | Zaprojektować adapter `npp::ui::DockingBridge` odzwierciedlający rejestrację paneli. Wtyczki rejestrują dialog poprzez strukturę niezależną od WinAPI (np. `DockablePanelDescriptor`). |
| Pliki/ścieżki | `NPPM_GETPLUGINSCONFIGDIR`, `NPPM_GETNPPDIRECTORY` | Backend już wieloplatformowy (`PathProvider`). Warstwa compatibility jedynie przekazuje ścieżki. |
| Session/Buffer | `NPPM_GETFULLPATHFROMBUFFERID`, `NPPM_SAVECURRENTSESSION` | Bez zmian – logika w `NppParameters` i `Buffer` jest neutralna. |
| Menu/polecenia | `setCommand`, `setMenuItemCheck` | Wymaga mapowania na `QAction`/`NSMenuItem`. Plan: `PluginsManager` utrzymuje tablicę poleceń i wystawia adapter aktualizujący menu menubar/docked toolbar. |
| Wiadomości Scintilla | `SendMessage(scintillaHandle, SCI_*, ...)` | ScintillaQt respektuje te komunikaty. Konieczne zapewnienie, że wtyczki otrzymają poprawny wskaźnik do instancji Scintilla (np. `ScintillaGateway`). |
| Notyfikacje | `NppData`, `SCNotification`, `NPPN_*` | Struktury pozostaną, ale pola z typami WinAPI będą „shimmowane” (np. `HWND` jako alias typu `void*`; na macOS wskazuje na obiekt proxy). |

## 4. Proponowana architektura warstwy kompatybilności
```
macOS Plugin Host
 ├─ PluginLoader (dlopen + sandbox checks)
 ├─ MessageBridge
 │    ├─ translateNppmMessage()
 │    └─ translateDockingMessage()
 ├─ UiBridge (menubar, docking, dialogs)
 └─ ScintillaGateway (udostępnia wskaźniki/identyfikatory widoków)
```
- **PluginLoader** – odpowiada za podpisywanie, katalog `~/Library/Application Support/Notepad++/plugins`, izolację zapisów i mapowanie nazw.
- **MessageBridge** – interceptuje `SendMessage` imitowane poprzez eksport `notepad_plus_msgs_mac.h`; pod spodem wywołuje odpowiednie funkcje Qt/AppKit.
- **UiBridge** – przedstawia menu wtyczek jako `QAction`/`QMenu`, synchronizuje stan (check/enable) i obsługuje skróty klawiaturowe (Command/Option).
- **ScintillaGateway** – zapewnia, że `ScintillaPtr` przekazywany do wtyczek jest ważny (wraper wokół `ScintillaEditView` w Qt).

## 5. Harmonogram wdrożenia
1. **Stub + definicje nagłówków** – przygotować `notepad_plus_msgs_mac.h` utrzymujący te same wartości `NPPM_*`, ale redefiniujący typy do neutralnych (`using HWND = void*;`). Zapewnić zestaw testów kompatybilności (kompilacja wtyczki „HelloWorld”).
2. **Implementacja MessageBridge** – odwzorować najczęściej używane komunikaty (`NPPM_GETCURRENTSCINTILLA`, `NPPM_DOOPEN`, `NPPM_SAVEFILE`). Dodać logi ostrzegające o niezaimplementowanych wiadomościach.
3. **UI Bridge** – zmapować `setCommand`, `ShortcutKey`, `setPluginNameWithFocus` na ekosystem Qt (menubar + Command pallette). Wymaga definicji konwencji `PluginCommandId = std::uint32_t`.
4. **Docking/Panel Bridge** – udostępnić wtyczkom możliwość rejestracji paneli (`DockableDialog::setModule` → `QDockWidget`). Zapewnić serializację położenia.
5. **Dokumentacja** – przygotować przewodnik portowania wtyczek (sekcja w `docs/macos/preferences_plugin_api.md` + nowy rozdział w User Manual).

## 6. Ryzyka i działania zaradcze
- **Zależność od HWND** – część wtyczek rzutuje `HWND` na inne struktury (np. zakłada, że to `HWND` okna głównego). Rozwiązanie: alias `HWND` wskazuje na obiekt proxy implementujący minimalne API (`SendMessage`, `GetMenu`) i zgłasza ostrzeżenia, gdy funkcja jest nieobsługiwana.
- **Hooki WinAPI** – wtyczki, które instalują hooki (`SetWindowsHookEx`) nie będą kompatybilne. Zalecamy komunikat w dokumentacji + API raportujące brak wsparcia (`NPPM_PLATFORM_QUERY`).
- **Różnice w menubarze** – macOS używa globalnego menu. Wtyczki muszą być ostrzeżone, że pozycje zostaną zmapowane do sekcji „Plugins”.
- **Kodowanie ścieżek** – wszystkie ścieżki przekazywane do wtyczek powinny być UTF-8 (na macOS). Konieczne jest oznaczenie w dokumentacji.

## 7. Następne kroki operacyjne
- [x] Utworzyć nagłówek `include/notepad_plus_msgs_portable.h` z neutralnymi typami i aliasami. → dostarczono w SDK jako `include/npp/NotepadPlusMsgs.h` wraz z aliasami macOS.
- [ ] Przygotować prototyp `MessageBridge` z obsługą 10 najczęściej używanych komunikatów (wg statystyk pluginów).
- [x] Zaimplementować przykład wtyczki działającej na macOS (ładującej Scintillę i menu). → `HelloMacPlugin` rejestruje komendę menu i demonstruje integrację status bara.
- [ ] Dodać test integracyjny (ładowanie wtyczki `NppExport`) w pipeline macOS.
