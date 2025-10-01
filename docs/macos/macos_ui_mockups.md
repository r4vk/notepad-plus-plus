# Makiety interfejsu macOS dla Notepad++

## Założenia projektowe
- Zachowanie zgodności z Apple Human Interface Guidelines (HIG) przy jednoczesnym wykorzystaniu systemu wizualnego Liquid Glass.
- Minimalizacja barier dla długoletnich użytkowników Windows dzięki analogicznym układom funkcji (taby, panele dokowane, pasek statusu).
- Wszechstronność: obsługa konfiguracji jednookienkich i zewnętrznych paneli, trybu jasnego/ciemnego, HiDPI oraz pełnej kontroli nad skrótami Command/Option.
- Spójne wykorzystanie tokenów kolorystycznych i typograficznych z `docs/macos/liquid_glass_tokens.md`.

## Widok główny aplikacji
### Menubar (NSMenu / Qt Native Menu)
- Sekcje: `Notepad++`, `File`, `Edit`, `Search`, `View`, `Run`, `Macros`, `Plugins`, `Window`, `Help`.
- Pozycje `Notepad++` zgodnie z HIG: `About Notepad++`, `Preferences…`, `Services`, `Hide`, `Hide Others`, `Quit`.
- Akcje przeniesione z paska narzędzi Windows (Nowy, Otwórz, Zapisz) pozostają w menu `File` z domyślnymi skrótami Command.
- Menu `View` dzieli elementy na sekcję paneli dokowanych (Function List, Document Map, Project Panels) oraz ustawienia layoutu.

### Pasek narzędzi (NSToolbar / Liquid Toolbar)
- Główne działania: New, Open, Save, Save All, Undo, Redo, Search, Toggle Function List, Toggle Document Map.
- Styl: przezroczysta belka z efektami Liquid Glass; ikony monochromatyczne `lg.icon.primary`, akcent `lg.accent.blue`.
- Adaptacja rozmiaru ikon i odstępów wg `NSToolbarItem` (40 pt minimal). W trybie kompaktowym ukrywa tekst, w standardowym łączy ikonę z podpisem.

### Obszar edycji (MDI)
- Domyślnie: pojedyncze okno z tab barem w stylu macOS (zagnieżdżone `NSTabView` / `QTabBar` stylizowane).
- Tryb multi-view: przycisk w tab barze do rozdzielenia na `split view`, wspierający przeciągnij-i-upuść kart.
- Górny pasek zakładek integruje przyciski `Add Tab`, `Split` oraz menu kontekstowe HIG.
- Pasek statusu: dolna belka z informacją o pozycji kursora, kodowaniu, końcu linii – zachowanie analogiczne do Windows, lecz w stylu macOS (kolory wg `lg.surface` + `lg.text.secondary`).

### Dockowane panele
- `Function List`, `Document Map`, `Project Panel`: implementacja `QDockWidget`/`NSPanel` z płynnymi animacjami Liquid Glass.
- Domyślna pozycja: Function List (prawa strona), Document Map (prawa/floating), Project Panel (lewa). Możliwość oddokowania do osobnych okien.
- Każdy panel posiada pasek tytułowy z przyciskiem `Close`, `Pin`, menu kontekstowe (Tryb koloru, Reset layoutu).
- Layout serializowany do `DockLayoutSnapshot`, zgodny z planem `DockingManagerBridge`.

## Panel preferencji (Preferences)
- Styl `NSWindow` z paskiem tabów segmentowych (kategoryzacja: General, Editing, Language, Theme, Advanced, Shortcuts).
- Sekcja `General`: ustawienia startowe, interfejs (język, motyw). `Theme` – wybór Liquid Glass Light/Dark, intensywność blur.
- `Shortcuts`: tabela `NSTableView` / `QTableView` z kolumnami `Command`, `Shortcut (macOS)`, `Legacy (Windows)` i przyciskiem `Record`.
- Przyciski akcji: `Restore Defaults`, `Export Settings…`, `Import…` w dolnym pasku.
- Integracja z `PreferencesStore` po stronie logiki.

## Okna dodatkowe
- `Find/Replace`: arkusz (`NSPanelSheet`) przyczepiany do głównego okna, zachowujący dotychczasowy układ pól; dodatkowo ikona trybu regex z tooltipem.
- `Macro Recorder`: wysuwany popover z kontrolkami `Record`, `Play`, `Save`.
- `Update Notification`: modalny `NSAlert` z ikoną Liquid Glass i linkami do changelogów.

## Zachowania interakcyjne
- Gesty trackpada: pinch-to-zoom (skalowanie fontu), przesunięcia trzema palcami do przełączania kart.
- Drag-and-drop plików na dock icon – `NSDocumentController` otwiera w nowym tabie.
- Tryb pełnoekranowy: toolbar w trybie `NSToolbarStyleUnifiedCompact`, panele dockowane przenoszone do `overlay` przy krawędziach.

## Dostępność
- Pełne opisy VoiceOver dla toolbaru i paneli; rola `NSToolbarItem` + `NSAccessibilityCustomRotor` dla przełączania kart.
- Wsparcie dla kontrastu: wariant High Contrast z tokenami `lg.accent.orange` / `lg.surface.dark`.
- Dynamic Type: możliwość zwiększania czcionek w tab barze i panelach z preferencji.

## Artefakty do przygotowania
- Pliki figmowe / Sketch (do wykonania przez zespół UX) z warstwami: Main Window (Light/Dark), Preferences, Function List Dock.
- Biblioteka komponentów w Figmie obejmująca: toolbar, tab bar, panel dockowany, listę dokumentów, popover.
- Macros dla Qt Design Studio generujące QML/QSS na podstawie tokenów.

## Następne kroki
1. Przekazać mockupy zespołowi UX do walidacji i stworzenia plików źródłowych.
2. Zdefiniować bibliotekę QML/QSS w oparciu o tokeny Liquid Glass.
3. Przygotować testy UI (Qt Test, XCTest) dla scenariuszy: otwarcie panelu, przełączanie kart, zmiana motywu.
4. Uzgodnić politykę przechowywania layoutów i skrótów w `PreferencesStore`.
