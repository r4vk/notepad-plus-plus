# Plan portu Notepad++ do natywnej aplikacji macOS

## Analiza i planowanie strategiczne
- "Przygotować przegląd architektury PowerEditor/src oraz zależności w katalogach scintilla i lexilla, aby sklasyfikować elementy przenośne i ściśle związane z Win32 (+)"
- "Sporządzić listę funkcji systemowych Windows (shell, rejestr, COM, powiadomienia, udostępnianie plików, drukowanie), z których korzysta Notepad++ i wskazać ich odpowiedniki w macOS (+)"
- "Przygotować dokument z analizą ryzyka i potencjalnych ograniczeń funkcjonalnych podczas portu do macOS (np. brak wsparcia dla niektórych pluginów) (+)"
- "Zdecydować o strategii interfejsu użytkownika: Qt 6.x + Liquid Glass jako warstwa prezentacji z mostkami do AppKit (+)"
- "Opracować harmonogram iteracyjnego portu (etapy: przygotowanie core, interfejs, integracje systemowe, beta) oraz kryteria wejścia/wyjścia dla każdego etapu (+)"

## Przygotowanie środowiska budowy i zależności
- "Zaprojektować nowy system budowania obsługujący macOS (preferencyjnie CMake), łączący kompilację PowerEditor, Scintilla, Lexilla i zależności zewnętrzne (+)"
- "Zweryfikować, czy wersje Scintilla i Lexilla w repozytorium mają aktualne wsparcie dla Cocoa/Qt i w razie potrzeby zaplanować aktualizację lub dostosowanie (+)"
- "Zidentyfikować biblioteki zastępujące Win32 API (np. Core Foundation, AppKit, LaunchServices) oraz ustalić sposób ich linkowania dla architektur x86_64 i arm64 (+)"
- "Przygotować konfigurację środowiska developerskiego (Xcode, clang, narzędzia CLI) oraz skrypty bootstrap dla deweloperów macOS (+)"
- "Skonfigurować pipeline GitHub Actions/CI uruchamiający testy warstwy platformowej na hostach macOS (+)"
- "Rozszerzyć pipeline macOS o budowę pakietu .app i publikację artefaktów (+)"

## Refaktoryzacja warstwy bazowej i abstrakcje systemowe
- "Wydzielić moduły logiki niezależnej od UI (edytor, zarządzanie plikami, ustawienia) z PowerEditor/src do bibliotek możliwych do wielokrotnego użycia"
- "Zaprojektować API warstwy abstrakcji dla usług systemowych (okna, zdarzenia, schowek, monitoring plików, drukowanie) i wprowadzić implementację referencyjną dla Windows jako krok pośredni"
- "Zastąpić bezpośrednie wywołania Win32 (HWND, SendMessage, WNDPROC, SHFileOperation itp.) odpowiednim interfejsem pośredniczącym"
- "Utworzyć moduł zarządzania preferencjami i profilem użytkownika oparty na ścieżkach macOS (~/Library/Application Support/Notepad++) (+)"
- "Wprowadzić moduł platformowy dla ścieżek użytkownika (npp::platform::PathProvider) i podpiąć go w NppParameters (+)"
- "Zastąpić w NppParameters użycie PathFileExists/WinAPI tworzeniem katalogów przez platform::ensureDirectoryExists (+)"
- "Abstrahować kopiowanie plików w konfiguracji Notepad++ przez npp::platform::copyFile (+)"
- "Wydzielić moduł informacji o systemie (npp::platform::SystemInfo) i podpiąć go pod Debug Info (+)"
- "Zaplanować migrację funkcji rejestru Windows na pliki konfiguracyjne/CFPreferences w macOS (+)"
- "Utworzyć moduł `npp::platform::PreferencesStore` z backendami Windows/macOS i zrefaktoryzować istniejące odwołania do rejestru"
- "Przygotować migrator ustawień importujący `config.xml` oraz eksport `.reg` do nowych preferencji"
- "Udokumentować API preferencji dla twórców wtyczek i przygotować przykładowe wdrożenia"
- "Przygotować alternatywę dla wbudowanego managera aktualizacji opartego o WinInet (np. wykorzystanie URLSession/CFNetwork)"

## Interfejs użytkownika i doświadczenie użytkownika
- "Przeprowadzić inwentaryzację elementów UI zależnych od Win32 (Docking Manager, ToolBar, TreeView, rebar, status bar) oraz określić ich odpowiedniki w Cocoa/toolkicie wieloplatformowym"
- "Zaprojektować moduł `npp::ui` z interfejsami Qt dla okien, paneli dokowanych i menubarów (Liquid Glass)"
- "Przygotować bibliotekę stylów i tokenów projektowych Liquid Glass w Qt Quick/Qt Widgets"
- "Zbudować prototyp głównego okna Qt z integracją Scintilli i panelem dokowanym w konwencji Liquid Glass"
- "Zaprojektować makietę interfejsu macOS uwzględniającą menubar zgodny z HIG, pasek narzędzi NSToolbar, panel preferencji oraz widoki wielodokumentowe"
- "Dostosować komponent edytora (Scintilla) do renderowania w kontenerze macOS (SCIModule dla Cocoa) wraz z obsługą HiDPI i trybu ciemnego systemowego"
- "Zaimplementować obsługę skrótów klawiaturowych według standardów macOS (Command/Option) z możliwością mapowania istniejących ustawień"
- "Przenieść menedżer kart dokumentów na implementację kompatybilną z NSTabView/NSSegmentedControl przy zachowaniu funkcji multi-view"
- "Zapewnić wsparcie dla paneli dokowanych (Function List, Project Panel, Document Map) z użyciem NSWindow/NSPanel oraz logiki rozkładu podobnej do Windows"

## Integracja z funkcjami systemowymi macOS
- "Opracować mechanizm rejestrowania rozszerzeń plików i obsługi protokołów poprzez LaunchServices"
- "Przystosować monitoring zmian plików do FSEvents oraz File System Events API"
- "Dostosować operacje przeciągnij-i-upuść oraz otwieranie wielu dokumentów za pomocą NSDocumentController"
- "Zaimplementować integrację z usługami udostępniania (Quick Look, Services menu) oraz otwieranie plików z Finder/spotlight"
- "Przygotować obsługę drukowania i podglądu wydruku w oparciu o NSPrintOperation"
- "Zaadaptować system powiadomień (toastów) do Notification Center"
- "Zastąpić funkcje związane z paskiem zadań i ikoną w zasobniku odpowiednikami w Dock i menu statusowym"

## Architektura wtyczek i rozszerzeń
- "Zmapować istniejące API wtyczek (message-based, Win32 HWND) i zaprojektować nową warstwę kompatybilności dla macOS"
- "Przygotować SDK wtyczek dla macOS (nagłówki, przykładowe projekty Xcode/CMake, dokumentacja budowy)"
- "Zdefiniować strategię dystrybucji wtyczek (katalog w ~/Library/Application Support, podpisywanie binarne, sandboxing)"
- "Zweryfikować możliwość portowania najpopularniejszych wtyczek i zaplanować działania wspierające deweloperów społeczności"

## Dystrybucja, bezpieczeństwo i utrzymanie
- "Zaprojektować strukturę pakietu .app (Info.plist, zasoby, ikonografia) oraz proces budowy DMG instalacyjnego"
- "Przygotować proces podpisywania kodu i notarizacji Apple wraz z automatyzacją w CI"
- "Opracować mechanizm aktualizacji (np. Sparkle) kompatybilny z polityką bezpieczeństwa Notepad++"
- "Zdefiniować politykę zbierania logów i raportów błędów specyficznych dla macOS"
- "Zaktualizować dokumentację użytkownika i dewelopera o instrukcje instalacji, skróty klawiszowe i ograniczenia platformowe"

## Plan testów i jakości
- "Wybrać framework testów jednostkowych (GoogleTest / Catch2) z konfiguracją multiplatformową i integracją z CTest (+)"
- "Stworzyć zestaw testów jednostkowych dla modułu zarządzania dokumentami (otwieranie/zapisywanie, kodowania, EOL, BOM)"
- "Przygotować testy jednostkowe dla parsera makr i silnika odtwarzania (w tym obsługa skrótów macOS)"
- "Zaimplementować testy dla logiki wyszukiwania/zamiany z wykorzystaniem Boost.Regex oraz weryfikacji działań w trybach wielokursorowych"
- "Dodać testy konfiguracji i serializacji ustawień (TinyXML, pliki konfiguracyjne, migracja z Windows)"
- "Przygotować testy dla menedżera sesji i przywracania dokumentów po awarii"
- "Opracować testy jednostkowe dla zarządzania wtyczkami (ładowanie, rejestrowanie poleceń, obsługa błędów)"
- "Stworzyć testy dla funkcji auto-uzupełniania i listy funkcji (parsowanie, cache, aktualizacja podczas edycji)"
- "Dodać testy dla warstwy abstrakcji systemowej (np. mocki FileWatcher, Clipboard, Notifications)"
- "Zaprojektować testy regresyjne dla konwersji kodowań oraz funkcji formatowania tekstu (indentacja, tabulatory, trim)"
- "Ustanowić automatyczne testy integracyjne otwierające pliki poprzez NSDocumentController (np. przy użyciu XCTest/UIAutomation)"
- "Przygotować skrypty do testów wydajnościowych (czas otwierania plików, zużycie pamięci) porównujących wersję Windows i macOS"

## Dokumentacja i komunikacja
- "Zebrać wymagania komunikacyjne (ogłoszenia społeczności, wpisy na blogu, FAQ) i przygotować plan informowania użytkowników o stanie prac"
- "Aktualizować CONTRIBUTING.md o wytyczne dla deweloperów macOS (style kodowania, wymagane narzędzia, proces PR)"
- "Przygotować szablony zgłoszeń błędów i feature requestów specyficznych dla macOS"
