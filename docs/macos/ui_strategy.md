# Strategia interfejsu użytkownika dla natywnego portu Notepad++ na macOS

## Cel
Zaprojektować interfejs użytkownika zgodny z wytycznymi Apple Human Interface Guidelines (HIG),
zapewniający użytkownikom macOS wrażenie pracy z natywną aplikacją przy zachowaniu kompatybilności
z istniejącą logiką edytora i rozszerzeniami Notepad++.

## Analiza opcji technologicznych

### Qt 6.7+ z Liquid (Qt Quick + Qt Design Studio)
- **Zalety**: wbudowane wsparcie dla motywu Liquid Glass (translucency, efekt szklanego interfejsu, blur),
  komponenty zgodne z wytycznymi macOS oraz gotowe kontrolki dla scenariuszy desktopowych.
- **Nowoczesne API**: możliwość wykorzystania Qt Quick Controls 3, systemu animacji i komponentów Material/Universal,
  przy jednoczesnym dostosowaniu wyglądu do Liquid Glass oraz pełnym wsparciu dla HiDPI i trybu ciemnego.
- **Integracja**: Qt oferuje mostki do AppKit (NSApplicationDelegate, Touch Bar, Services), co umożliwia implementację
  funkcji specyficznych dla macOS bez utraty produktywności.
- **Wyzwania**: konieczność utrzymania licencji Qt (LGPL/commercial), portowania istniejących widoków Win32 do Qt Widgets
  lub Qt Quick, a także utworzenia warstwy zgodności dla wtyczek oczekujących komunikatów Win32.

### Natywne Cocoa (AppKit)
- **Zalety**: pełna kontrola nad komponentami systemowymi, brak zależności od zewnętrznego toolkitu.
- **Wyzwania**: większy koszt implementacji interfejsu i animacji Liquid Glass, konieczność użycia Objective-C++.
- **Zastosowanie**: wykorzystać jako uzupełnienie Qt dla funkcji wymagających bezpośredniego dostępu do API systemowych
  (Services, NSDocumentController), implementowanych w formie cienkich mostków.

## Strategia rekomendowana
1. **Rdzeń UI w Qt**: budujemy główne okno, menubar i panele w Qt 6.7/6.8, wykorzystując Qt Quick oraz Liquid Components
   (np. `LiquidWindow`, `GlassContainer`) do uzyskania efektu przezroczystych paneli i dynamicznych animacji.
2. **Warstwa `npp::ui`**: projektujemy interfejsy abstrakcyjne (okna, panele dokowane, menedżer skrótów), których
   implementacją referencyjną będzie Qt. Aktualny kod Win32 otrzymuje adapter implementujący te interfejsy,
   co pozwoli przepinać moduły stopniowo.
3. **Scintilla**: integrujemy komponent poprzez `QScintilla` lub warstwę hostującą istniejący widok Scintilli w `QWidget`
   z translacją zdarzeń do Qt. Zapewniamy wsparcie dla trybu dark/light i HiDPI dzięki stylowaniu Qt.
4. **Integracja systemowa**: funkcje macOS specyficzne (Dock menu, usługi, pliki ostatnio otwierane) udostępniamy przez
   klasę pomostową (`MacBridge`) implementującą interfejsy `npp::platform`, korzystając z Objective-C++.
5. **UX**: wykorzystujemy Liquid Glass do budowy panelu preferencji, dockowanego edytora oraz dynamicznych podpowiedzi.
   Tworzymy Design System (kolory, typografia, spacing) możliwy do współdzielenia między Qt a dokumentacją.
6. **Testy UI**: uruchamiamy zestaw testów w Qt Test i XCTest (dla funkcji natywnych) obejmujący m.in. dynamiczne animacje,
   stany paneli dokowanych i skróty Command.

## Plan migracji UI
1. **Definicja interfejsów**: utworzyć moduł `npp::ui` z klasami bazowymi (`Application`, `Window`, `Dock`, `MenuModel`).
2. **Adapter Win32**: zaimplementować adapter Qt → Win32, aby istniejące komponenty mogły korzystać z nowego API
   zanim gotowa będzie implementacja Qt.
3. **Bootstrap Qt**: skonfigurować projekt CMake do budowy wariantu Qt (targets, moc, rcc, qml), przygotować szablon
   aplikacji z Liquid Glass.
4. **Migracja widoków**: przenieść kolejno: główne okno + tabbar, panele boczne (docki), dialog preferencji, plugin manager.
5. **Wtyczki**: zaprojektować warstwę translacji komunikatów (Win32 message ↔ Qt signal/slot) oraz SDK QML/Qt Widget.
6. **Usługi macOS**: osadzić integracje NSDocumentController, Touch Bar i Services jako moduły dodatkowe.

## Wnioski
- Strategia Qt z Liquid Glass łączy szybkość rozwoju z nowoczesnym wyglądem i pozwala dostarczyć interfejs
  o jakości natywnej przy zachowaniu multiplatformowej bazy kodu.
- Utrzymanie cienkich mostków do AppKit zapewnia dostęp do funkcji systemowych bez komplikowania głównej warstwy UI.
- Kluczowe jest zdefiniowanie modułu `npp::ui` i planu migracji pluginów, aby zachować kompatybilność i wysoką jakość UX.
