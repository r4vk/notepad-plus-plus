# Strategia interfejsu użytkownika dla natywnego portu Notepad++ na macOS

## Cel
Zaprojektować interfejs użytkownika zgodny z wytycznymi Apple Human Interface Guidelines (HIG),
zapewniający użytkownikom macOS wrażenie pracy z natywną aplikacją przy zachowaniu kompatybilności
z istniejącą logiką edytora i rozszerzeniami Notepad++.

## Analiza opcji technologicznych

### Natywne Cocoa (AppKit)
- **Zalety**: pełna zgodność z HIG, dostęp do nowoczesnych API (Dark Mode, Touch Bar, Spotlight),
najlepsza integracja z systemem, możliwość wykorzystania NSDocument/NSToolbar/NSMenu.
- **Wyzwania**: konieczność zaimplementowania od zera warstwy okien i kontrolek, przygotowanie mostków
do istniejącego kodu C++ (Objective-C++/Swift-C++), aktualizacja menedżera dokowania i paneli bocznych.
- **Wpływ na wtyczki**: wymagane API kompatybilności mapujące komunikaty Win32 na nowy system eventów;
wtyczki mogą potrzebować wrapperów dylib lub procesu hostującego.

### Toolkit wieloplatformowy (Qt/wxWidgets)
- **Zalety**: szybsza migracja UI, gotowe komponenty okienkowe, jedno źródło dla wielu platform.
- **Wyzwania**: dodatkowa warstwa abstrakcji zwiększająca rozmiar binarny, konieczność dopasowania stylu do macOS,
ograniczenia licencyjne (Qt) oraz większy nakład pracy na adaptację pluginów wykorzystujących Win32.
- **Wpływ na wtyczki**: konieczna przebudowa komunikacji do nowego mechanizmu sygnałów/zdarzeń, ryzyko
istnienia dwóch klas pluginów (Windows vs. macOS) z różnymi API.

### Strategia rekomendowana
1. **Warstwa prezentacji** oparta o Cocoa (NSApplication, NSWindow, NSView) z obudowaniem istniejących
   kontrolerów C++ przy użyciu Objective-C++.
2. **Scintilla** w trybie Cocoa (`ScintillaEditView` z backendem `PlatCocoa`) jako komponent edytora.
3. **Mostki eventów**: interfejs `npp::ui::Window` translacyjny między komunikatami Win32 a strukturami Cocoa,
   implementowany początkowo dla Windows (adapter) i macOS (docelowo).
4. **Panel dokowany i widoki boczne**: mapowanie na `NSWindowController` + `NSViewController` z obsługą split view,
   zachowując API pluginów dzięki warstwie kompatybilności (renderowanie w NSView osadzonym w kontenerze).
5. **Menu i skróty**: centralne zarządzanie przez `NSMenu` i `NSMenuItem`, odwzorowanie skrótów w konfiguracji,
   umożliwiające równoczesne mapowania Windows/Command.
6. **UI testing**: adopcja XCTest UI dla krytycznych przepływów (otwieranie dokumentów, dockowanie paneli).

## Plan migracji UI
1. **Warstwa podstawowa**: stworzyć moduł `npp::ui` definiujący abstrakcyjne interfejsy okien, kontrolek, zasobów.
2. **Implementacja Windows (adapter)**: wykorzystać istniejący kod Win32 do implementacji interfejsów, aby
   sukcesywnie przepinać komponenty na nową warstwę bez zmian funkcjonalnych.
3. **Implementacja macOS**: zbudować odpowiedniki bazujące na AppKit, z naciskiem na dokumenty i panele.
4. **Portowanie paneli**: kolejno przenosić `Function List`, `Project Panel`, `Document Map`, `Plugins Admin`.
5. **Dostosowanie UX**: wprowadzić menubar zgodny z macOS, preferencje jako `NSWindow` w stylu "toolbar tabs",
   integrację z systemowym Dark Mode oraz włączenie usług udostępniania.

## Wnioski
- Wybór natywnego Cocoa zapewnia najlepszą akceptację przez użytkowników macOS oraz kontrolę nad integracją
  systemową przy akceptowalnym koszcie implementacji.
- Etap przejściowy z adapterem Win32 minimalizuje ryzyko regresji i umożliwia testowanie równoległe.
- Potrzebne będą inwestycje w dokumentację dla twórców wtyczek oraz narzędzia generujące bindingi do nowego API UI.
