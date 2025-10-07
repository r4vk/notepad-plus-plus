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
- "Zaprojektować API warstwy abstrakcji dla usług systemowych (okna, zdarzenia, schowek, monitoring plików, drukowanie) i wprowadzić implementację referencyjną dla Windows jako krok pośredni (+)"
- "Zastąpić bezpośrednie wywołania Win32 (HWND, SendMessage, WNDPROC, SHFileOperation itp.) odpowiednim interfejsem pośredniczącym"
- "Utworzyć moduł zarządzania preferencjami i profilem użytkownika oparty na ścieżkach macOS (~/Library/Application Support/Notepad++) (+)"
- "Wprowadzić moduł platformowy dla ścieżek użytkownika (npp::platform::PathProvider) i podpiąć go w NppParameters (+)"
- "Zastąpić w NppParameters użycie PathFileExists/WinAPI tworzeniem katalogów przez platform::ensureDirectoryExists (+)"
- "Abstrahować kopiowanie plików w konfiguracji Notepad++ przez npp::platform::copyFile (+)"
- "Wydzielić moduł informacji o systemie (npp::platform::SystemInfo) i podpiąć go pod Debug Info (+)"
- "Utworzyć moduł `npp::platform::PreferencesStore` z backendami Windows/macOS" (+)
- "Zrefaktoryzować istniejące odwołania do rejestru, aby korzystały z `PreferencesStore` (+)"

## Interfejs użytkownika i doświadczenie użytkownika
- "Zaprojektować moduł `npp::ui` z interfejsami Qt dla okien, paneli dokowanych i menubarów (Liquid Glass) (+)"
- "Zbudować prototyp głównego okna Qt z integracją Scintilli i panelem dokowanym w konwencji Liquid Glass (+)"
- "Zaprojektować makietę interfejsu macOS uwzględniającą menubar zgodny z HIG, pasek narzędzi NSToolbar, panel preferencji oraz widoki wielodokumentowe (+)"
- "Dostosować komponent edytora (Scintilla) do renderowania w kontenerze macOS (SCIModule dla Cocoa) wraz z obsługą HiDPI i trybu ciemnego systemowego (+)"
- "Zaimplementować obsługę skrótów klawiaturowych według standardów macOS (Command/Option) z możliwością mapowania istniejących ustawień (+)"
- "Przenieść menedżer kart dokumentów na implementację kompatybilną z NSTabView/NSSegmentedControl przy zachowaniu funkcji multi-view (+)"
- "Zapewnić wsparcie dla paneli dokowanych (Function List, Project Panel, Document Map) z użyciem NSWindow/NSPanel oraz logiki rozkładu podobnej do Windows"

## Integracja z funkcjami systemowymi macOS
- "Opracować mechanizm rejestrowania rozszerzeń plików i obsługi protokołów poprzez LaunchServices (+)"
- "Przystosować monitoring zmian plików do FSEvents oraz File System Events API (+) — wdrożono strażnika FSEvents w SystemServices z filtrowaniem subkatalogów"
- "Dostosować operacje przeciągnij-i-upuść oraz otwieranie wielu dokumentów za pomocą NSDocumentController" — przygotowano kolejkę `DocumentOpenQueue` w `SystemServices` z obsługą wielu ścieżek i testami jednostkowymi, aby umożliwić integrację z NSDocumentController i ujednolicić przetwarzanie żądań otwarcia
- "Zaimplementować integrację z usługami udostępniania (Quick Look, Services menu) oraz otwieranie plików z Finder/spotlight (+) — dodano kolejkę `SharingCommandQueue` w `SystemServices` wraz z testami i obsługą w override, aby przekazywać żądania Quick Look/Services do warstwy UI"
- "Przygotować obsługę drukowania i podglądu wydruku w oparciu o NSPrintOperation (+) — dodano usługę `PrintService` w `SystemServices` z implementacją Windows korzystającą z istniejącego `Printer`, refaktoryzowano `Notepad_plus::filePrint` do korzystania z nowej abstrakcji i przygotowano stubbing na macOS/testach"
- "Zaadaptować system powiadomień (toastów) do Notification Center (+)"
- "Zastąpić funkcje związane z paskiem zadań i ikoną w zasobniku odpowiednikami w Dock i menu statusowym (+) — dodano `StatusItemService` w `SystemServices` wraz z implementacją Windows/stubem testowym oraz zrefaktoryzowano kod Notepad++ do korzystania z abstrakcji, przygotowując integrację z Dock/NSStatusItem"

## Architektura wtyczek i rozszerzeń
- "Przygotować SDK wtyczek dla macOS (nagłówki, przykładowe projekty Xcode/CMake, dokumentacja budowy) (+) — dodano pakiet `tools/macos_plugin_sdk` z przenośnymi nagłówkami, presetami CMake/Xcode i przykładową wtyczką HelloMac"

## Dystrybucja, bezpieczeństwo i utrzymanie
- "Zaprojektować strukturę pakietu .app (Info.plist, zasoby, ikonografia) oraz proces budowy DMG instalacyjnego"
- "Przygotować proces podpisywania kodu i notarizacji Apple wraz z automatyzacją w CI"
- "Opracować mechanizm aktualizacji (np. Sparkle) kompatybilny z polityką bezpieczeństwa Notepad++"
- "Zdefiniować politykę zbierania logów i raportów błędów specyficznych dla macOS"
- "Zaktualizować dokumentację użytkownika i dewelopera o instrukcje instalacji, skróty klawiszowe i ograniczenia platformowe"

## Plan testów i jakości
- "Wybrać framework testów jednostkowych (GoogleTest / Catch2) z konfiguracją multiplatformową i integracją z CTest (+)"
- "Stworzyć zestaw testów jednostkowych dla modułu zarządzania dokumentami (otwieranie/zapisywanie, kodowania, EOL, BOM) (+)"
- "Przygotować testy jednostkowe dla parsera makr i silnika odtwarzania (w tym obsługa skrótów macOS)"
- "Zaimplementować testy dla logiki wyszukiwania/zamiany z wykorzystaniem Boost.Regex oraz weryfikacji działań w trybach wielokursorowych"
- "Dodać testy konfiguracji i serializacji ustawień (TinyXML, pliki konfiguracyjne, migracja z Windows)"
- "Przygotować testy dla menedżera sesji i przywracania dokumentów po awarii"
- "Opracować testy jednostkowe dla zarządzania wtyczkami (ładowanie, rejestrowanie poleceń, obsługa błędów)"
- "Stworzyć testy dla funkcji auto-uzupełniania i listy funkcji (parsowanie, cache, aktualizacja podczas edycji)"
- "Dodać testy dla warstwy abstrakcji systemowej (np. mocki FileWatcher, Clipboard, Notifications) (+) — dodano mechanizm ScopedSystemServicesOverride i test mocków dla Clipboard oraz FileWatcher"
- "Zaprojektować testy regresyjne dla konwersji kodowań oraz funkcji formatowania tekstu (indentacja, tabulatory, trim)"
- "Ustanowić automatyczne testy integracyjne otwierające pliki poprzez NSDocumentController (np. przy użyciu XCTest/UIAutomation) (+)"
- "Przygotować skrypty do testów wydajnościowych (czas otwierania plików, zużycie pamięci) porównujących wersję Windows i macOS"

## Dokumentacja i komunikacja
- "Zebrać wymagania komunikacyjne (ogłoszenia społeczności, wpisy na blogu, FAQ) i przygotować plan informowania użytkowników o stanie prac"
- "Aktualizować CONTRIBUTING.md o wytyczne dla deweloperów macOS (style kodowania, wymagane narzędzia, proces PR) (+)"
- "Przygotować szablony zgłoszeń błędów i feature requestów specyficznych dla macOS"
