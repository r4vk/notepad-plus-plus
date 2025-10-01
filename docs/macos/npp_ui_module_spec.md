# Specyfikacja modułu `npp::ui`

## Cel i kontekst
Celem modułu `npp::ui` jest zapewnienie wieloplatformowej, nowoczesnej warstwy interfejsu
użytkownika dla Notepad++, ze szczególnym naciskiem na wariant macOS budowany na Qt 6.8+
z wykorzystaniem motywu Liquid Glass. Moduł ma izolować resztę kodu od zależności od
Win32/Qt/AppKit, udostępniając neutralne interfejsy do okien, paneli dokowanych, menubarów,
skrótu klawiaturowych i systemów komend. Implementacje platformowe (Qt, ewentualnie Win32
na etapie migracji) będą rejestrowane poprzez fabrykę i mogą współistnieć podczas okresu
przejściowego.

## Zakres modułu
- Definiuje hierarchię interfejsów: `Application`, `Window`, `DockingHost`, `MenuModel`,
  `Toolbar`, `CommandRegistry`, `ShortcutMap`, `ThemeProvider`.
- Zapewnia mechanizm rejestrowania widoków (Scintilla, panele wtyczek) bez ujawniania
  szczegółów Qt. Klienci operują na `std::shared_ptr` do interfejsów i identyfikatorach typu
  `UiToken` (silnie typizowane struktury).
- Odpowiada za translację zdarzeń klawiatury, myszki i focusu do modelu Notepad++.
- Integruje menubar macOS (globalne `NSMenuBar`) poprzez adapter Qt ⇄ AppKit i reaguje na
  zmiany trybu ciemnego oraz dynamiczne menu usług.
- Utrzymuje spójny system stylów Liquid Glass poprzez `ThemeProvider`, który konsumuje
  tokeny z `docs/macos/liquid_glass_tokens.md`.

Poza zakresem modułu pozostają: logika edytora, zarządzanie plikami, preferencje,
przetwarzanie tekstu (obsługiwane przez istniejące moduły). `npp::ui` może korzystać z
`npp::platform` dla usług systemowych (clipboard, schowek, notyfikacje).

## Architektura i warstwy
```
+--------------------------+
| Notepad++ Core          |
| (edytor, polecenia)     |
+------------+-------------+
             |
             v
+--------------------------+
| npp::ui Interfaces       |  <== ten dokument definiuje kontrakty C++17
+------------+-------------+
             |
  +----------+----------+
  |                     |
  v                     v
Qt6 Liquid Glass    Win32 Adapter (tymczasowy)
(implementacja macOS)| zapewnia ciągłość na Windows
  |                     |
  +----------+----------+
             |
        AppKit mostki
   (NSApplication, TouchBar,
    Services, Dock Menu)
```

- Rdzeń kodu odwołuje się jedynie do nagłówków `npp::ui`. Dostęp do implementacji odbywa
  się przez `Factory` inicjalizowaną w `winmain.cpp` / `mac_main.mm`.
- Implementacja Qt zawiera kod w katalogu `PowerEditor/macos/ui/qt` (planowane), budowany
  wyłącznie, gdy dostępne są biblioteki Qt 6.8+.
- Mostki AppKit dostarczają funkcje wymagające natywnego API (Services, Touch Bar).

## Kluczowe interfejsy
| Interfejs | Odpowiedzialność | Uwagi implementacyjne (Qt) |
|-----------|------------------|-----------------------------|
| `npp::ui::Application` | Konfiguracja globalna aplikacji (event loop, dostęp do zasobów, integracja z AppKit) | Owija `QGuiApplication`. Zapewnia `registerDockingHost`, `setMenuBar`, integrację z `NSApplicationDelegate`. |
| `npp::ui::Window` | Reprezentuje główne okno; zarządza widokami dokumentów, paskami narzędzi, status barem | Mapuje na `QMainWindow` i eksponuje sygnały `focusChanged`, `themeDidChange`. |
| `npp::ui::DockingHost` | Abstrakcja zarządzania panelami dock/floating, wtyczkami | Bazuje na `QDockWidget`, przechowuje layout w formacie niezależnym (`DockLayoutSnapshot`). |
| `npp::ui::MenuModel` | Deklaratywny model menu (hierarchia, akcje, role systemowe) | Buduje `QMenuBar` -> `NSMenu`. Pozwala na dynamiczne zamianę akcji (Services). |
| `npp::ui::Command` | Reprezentuje polecenie mapowane na akcje UI i skróty | Opakowuje `QAction`, zapewnia metadane (UID, grupa, dostępność). |
| `npp::ui::Toolbar` | Warstwa dla `NSToolbar` / pasków w Qt | Umożliwia definiowanie układów Liquid Glass (ikonki monochromatyczne + accent). |
| `npp::ui::ShortcutMap` | Przechowuje skróty Command/Option oraz mapowania z Windows | Konwertuje sekwencje Win32 na macOS (`⌘`, `⌥`, `⌃`). |
| `npp::ui::ThemeProvider` | Dostarcza kolory/typografię Liquid Glass | Wzorzec Observer do powiadamiania o zmianach trybu ciemnego. |

## Założenia projektowe
- **Brak zależności włącznie**: nagłówki `npp::ui` nie dołączają plików Qt; używamy deklaracji
  w przód, `std::unique_ptr` dla PImpl tam, gdzie to konieczne.
- **Idempotencja inicjalizacji**: `Application::bootstrap(const ApplicationConfig&)`
  może być wywołane tylko raz; kolejne próby zwracają błąd (`BootstrapResult`).
- **Bezpieczeństwo**: wszystkie interfejsy używają `std::wstring`/`Utf8String` (alias) do
  tekstów, zapewniamy walidację wejść (np. brak osadzania nielegalnych skrótów).
- **Śledzenie zasobów**: kontrola posiadania obiektów poprzez `std::shared_ptr` do interfejsów;
  implementacje Qt przechowują słabe wskaźniki na kontrolki GUI, aby uniknąć cykli.

## Diagram interakcji (scenariusz startu)
1. `mac_main.mm` tworzy `QtApplicationBackend` i rejestruje go w `npp::ui::Factory`.
2. Rdzeń woła `auto app = npp::ui::Factory::application(config)`.
3. `Application` tworzy `Window`, `DockingHost`, `MenuModel` i injektuje je do `Notepad_plus`.
4. `DockingHost` rejestruje panele (Function List, Document Map) na podstawie layoutu z
   `PreferencesStore`.
5. `ThemeProvider` subskrybuje zmiany `npp::platform::SystemAppearance` i propaguje kolory
do komponentów Qt.

## Integracja z pluginami
- `CommandRegistry` dostarcza API, które mapuje identyfikatory poleceń Windows (`funcItem`)
  na neutralne akcje. Wtyczki przekazują deskryptory poleceń (`CommandDescriptor`).
- Dockowane panele wtyczek przekazują `DockableView`, który implementuje `QWidgetSupplier`.
  Implementacja Qt tworzy `QWidget` on-demand, AppKit (w późniejszym etapie) będzie używać
  `NSView`.
- Eventy `NPPM_` tłumaczymy na metody `DockingHost::requestFocus`, `Window::presentDocument`.

## Bezpieczeństwo i odporność
- Wszystkie akcje UI walidujemy przed wykonaniem (`Command::canExecute()`), by unikać
  dostępu do zwolnionych zasobów.
- `ShortcutMap` filtruje skróty niezgodne z macOS i uniemożliwia rejestrowanie powtarzających
  się sekwencji; preferencje użytkownika przechowywane są w `PreferencesStore` w postaci
  szyfrowanej (AES-GCM) przy użyciu klucza z pęku kluczy macOS (projektowane).
- Layout dokowania serializowany jest do pliku JSON podpisanego sumą SHA-256, aby wykryć
  korupcję danych.

## Strategia testowania
- Testy jednostkowe C++ (Catch2) weryfikują serializację modeli menu, layoutu dokowania,
  mapowanie skrótów oraz poprawność fabryki.
- Testy Qt Test pokrywają interakcje UI (drag & drop paneli, aktualizacja theme).
- XCTest (macOS) sprawdza integrację z NSDocumentController i Services.
- Pipeline CI uruchamia wirtualny serwer okien (Qt `offscreen`), aby wykonać testy
  wizualne/regresyjne.

## Plan implementacji (skrót)
1. **Nagłówki interfejsów** (`include/`) – bieżące zadanie.
2. **Fabryka i IoC** – konfiguracja dostawców implementacji.
3. **Adapter Win32 (tymczasowy)** – pozwala na stopniową migrację.
4. **Backend Qt** – implementacje `QtApplication`, `QtMainWindow`, `QtDockingHost`.
5. **Integracja pluginów** – adapter `CommandRegistry` + `DockingBridge`.
6. **Testy wielowarstwowe** – unit, Qt Test, XCTest.

## Zależności i wymogi wstępne
- Qt 6.8+, moduły: `QtWidgets`, `QtGui`, `QtCore`, `QtMacExtras`, `QtQuickControls2`.
- CMake z obsługą `find_package(Qt6 COMPONENTS Widgets Gui Core QuickControls2)`,
  warunek `if(APPLE)` aktywujący backend.
- Integracja z `npp::platform::SystemAppearance` dla trybów jasny/ciemny.

## Decyzje otwarte
- Czy faza przejściowa wymaga jednoczesnego działania backendów Win32 + Qt? (domyślnie tak,
  z użyciem adapterów).
- Jak daleko idziemy w integracji Liquid Glass (czy renderujemy blur po stronie Qt czy
  AppKit)?
- Czy preferencje skrótów przechowujemy w formacie JSON czy Plist (obecnie skłaniamy się ku
  JSON z `PreferencesStore`).

## Następne kroki
- Utworzyć nagłówki `npp::ui` w katalogu `PowerEditor/src/UI` zgodnie ze specyfikacją.
- Przygotować szkic testów jednostkowych dla layoutu menu i skrótów.
- Rozpocząć implementację fabryki backendów Qt.
- Uzyskać decyzję co do polityki przechowywania skrótów (JSON vs. Plist) zanim zaczniemy
  implementację `ShortcutMap`.
