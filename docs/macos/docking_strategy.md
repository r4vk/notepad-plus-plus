# Strategia systemu dokowania dla portu macOS

## 1. Stan obecny (Windows)
- Warstwa dokowania opiera się na zestawie klas `WinControls/DockingWnd` (`DockingManager`, `DockingCont`, `DockingSplitter`).
- Każdy panel (Function List, Document Map, Project Panel, Plugins Admin) implementuje własny `StaticDialog` i rejestruje się w `DockingManager` poprzez struktury `tTbData` oraz komunikaty `NPPM_DMMREGASDCKDLG`.
- Layout jest serializowany w `config.xml` (sekcja `<GUIConfig name="DockingManager">`). Pozycje definiują obszar (top/bottom/left/right), stan (docked/floating) oraz proporcje.
- Interakcje UI wymagają WinAPI: `CreateWindowEx`, `DockingSplitter` używa GDI, rebar/pasek kart to custom controls.

## 2. Wymagania dla macOS
1. **Natywne doznanie użytkownika** – zgodność z Human Interface Guidelines: wykorzystanie `NSToolbar`, `NSSplitView`, `NSWindow`/`NSPanel`, zapewnienie animacji i gestów (drag & drop paneli).
2. **Elastyczny layout** – zachowanie możliwości dockowania/odpinania paneli, autoukład, zapamiętywanie konfiguracji.
3. **Kompatybilność z wtyczkami** – wtyczki powinny nadal rejestrować panele poprzez API podobne do `tTbData`, ale implementacja musi tłumaczyć te żądania na komponenty Qt/AppKit.
4. **Integracja z trybem wielu widoków** – Docking powinien współgrać z głównym oknem edytora (widoki A/B, Document Map, Function List) oraz z przyszłą warstwą `npp::ui`.
5. **Testowalność** – możliwość serializacji layoutu i odtwarzania w testach automatycznych.

## 3. Proponowana architektura
```
npp::ui::DockingHost (Qt/Cocoa)           <-- warstwa platformowa
    ├─ manages QMainWindow + QDockWidget wrappers
    ├─ stores layout (QByteArray / plist)
    ├─ exposes signals for plugin docking events
    └─ bridges to Notepad++ core (DockingManagerBridge)

DockingManagerBridge (C++)
    ├─ accepts legacy tTbData registrations
    ├─ maps toolbar icons, captions, callbacks
    ├─ delegates to DockingHost to create/attach dock widgets
    └─ persists layout via PreferencesStore (domain: notepad.gui.docking)
```
- **DockingHost (Qt)** – wykorzystuje `QMainWindow` i `QDockWidget` (z ustawionym `setFeatures` dla pływających paneli), integruje się z `QSplitter/QLayout` i implementuje makiety menu kontekstowego (zamknij, resetuj).
- **DockingHost (AppKit)** – alternatywnie (dla pełnej natywności) można użyć `NSWindowTabGroup`, `NSToolbarItemGroup`, `NSPanel`. Jednak Qt 6.x zapewnia już integrację z Cocoa, co redukuje ryzyko.
- **Bridge** – adapter pomiędzy istniejącymi strukturami (`tTbData`) a nowym API. Wtyczki zgłaszają żądanie rejestracji -> Bridge tworzy `DockWidgetDescriptor` (tytuł, ikonę, QWidget dostarczony przez wtyczkę) -> DockingHost umieszcza panel.

## 4. Migracja danych
- Obecna konfiguracja: `DockingManager` zapisuje listę paneli (id, stan, pozycja) w `config.xml`.
- Planowane podejście: migracja do `PreferencesStore` (`notepad.gui.docking`) – zapis JSON/CBOR opisujący layout niezależny od WinAPI.
- Proces startu macOS:
  1. `DockingManagerBridge` ładuje layout z `PreferencesStore`.
  2. Tworzy panele w DockingHost, ustawiając geometrię (dock area, float rect, visibility).
  3. Jeśli layout nie istnieje – używa wartości domyślnych (np. Function List docked right).

## 5. Interfejs API dla wtyczek
- Zachowujemy istniejący `tTbData`, ale mapujemy pola:
  - `hClient` -> wskaźnik na QWidget dostarczony przez wtyczkę (poprzez wrapper `DockableWidgetAdapter`).
  - `pszName` -> `QString` / `NSString` tytułu panelu.
  - `pszAddInfo` -> metadane (opis) – używane do tooltipów.
  - `iIconMenu` -> identyfikator ikony, mapowany na `QIcon` (pobrany z zasobów wtyczki lub Notepad++).
- Dodatkowe API (`NPPM_DMMFLOATING`, `NPPM_DMMHIDE`, `NPPM_DMMUPDATEDISPINFO`) zostanie odwzorowane na metody `DockingHost` (show/hide, setFloating, setCaption).

## 6. Integracja z `npp::ui`
- `npp::ui::MainWindow` stanie się właścicielem instancji DockingHost.
- `NppParameters` pozostanie źródłem prawdy dla idealnego layoutu (persistencja), ale warstwa `npp::ui` wykona aktualizację runtime.
- Zdarzenia (np. zmiana dockowania) emitują sygnały Qt, które Bridge zamienia na powiadomienia dla Notepad++/wtyczek.

## 7. Harmonogram
| Etap | Zakres | Rezultat |
|------|--------|----------|
| D1 – Projekt API | Zdefiniować `DockingManagerBridge`, `DockWidgetDescriptor`, neutralne struktury layoutu | Nagłówek + dokumentacja (ten plik) |
| D2 – Implementacja Qt prototypu | Prowizoryczny `MainWindow` z 2 panelami (Function List, Document Map) | Dowód działający w `PowerEditor/macos` |
| D3 – Integracja pluginów | Adaptacja `NppDockingManager` do Bridge, migracja `tTbData` | Panele wewnętrzne działają na macOS |
| D4 – Persistencja | Zapis/odczyt layoutu w `PreferencesStore` | Layout zachowywany między uruchomieniami |
| D5 – API pluginów | Dokumentacja i adapter `NPPM_DMM*` | Wtyczki kompatybilne |

## 8. Ryzyka i decyzje otwarte
- **Qt vs. natywne AppKit** – wybór Qt uspójnia kod bazowy, ale może wymagać customizacji, aby spełnić HIG. Opcja mieszana: Qt dla layoutu, a AppKit dla menu kontekstowego.
- **Wydajność** – Docking Qt w połączeniu z dużą liczbą paneli może wymagać optymalizacji (lazy loading paneli).
- **Kompatybilność testów** – potrzebne testy automatyczne sprawdzające layout dockowania przy starcie.
- **Pluginy** – część wtyczek może bezpośrednio manipulować strukturami `DockingManager` (niepubliczne API). Dokumentacja musi wskazać wspierane ścieżki.

## 9. Następne kroki
- [ ] Przygotować szkic `DockingManagerBridge.h` z neutralnymi strukturami.
- [ ] Zaimplementować prototyp DockingHost w Qt (w folderze `PowerEditor/macos`).
- [ ] Zmodyfikować `NppParameters::load` tak, aby odczytywał layout z `PreferencesStore` (not. gui.docking).
- [ ] Opracować checklistę testów UI (przeciąganie paneli, floating, reset layout).
