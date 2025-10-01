# Strategia managera kart dokumentów na macOS

## Cel
Zastąpić obecny, zależny od Win32 `TabBarPlus` manager kart rozwiązaniem zgodnym z macOS, opartym o `NSTabView` (AppKit) i kompatybilnym z warstwą Qt (`QTabBar`/`QTabWidget`), wspierając funkcje Notepad++: wiele widoków, wskaźniki stanu dokumentu, przeciąganie kart oraz personalizowane ikony.

## Potrzeby UX
- Zachowanie paradygmatu kart znanego z wersji Windows przy jednoczesnym dopasowaniu do HIG (płaskie zakładki, animacje przesunięcia).
- Naturalne skróty (`⌘+1…9`, `⌘+{`/`⌘+}`) oraz gesty trackpada (przesunięcie, przeciągnij-i-upuść).
- Wyróżnienia stanów: unsaved (kropka), read-only (ikona kłódki), monitoring (wskaźnik aktywności).
- Tryb podwójnego widoku (A/B) – segment kontrolny do podziału widoku plus możliwość przenoszenia kart między panelami.

## Architektura
```
npp::ui::TabManager (nowy interfejs)
    ├─ QtTabManager (Qt 6) → QTabBar/QTabWidget z Liquid styling
    └─ MacTabManager (AppKit) → NSTabViewController + custom NSTabBarView

Notepad++ core (DocTabView) → adapter TabManagerBridge → npp::ui::TabManager
```

### Interfejs `TabManager`
- Zdefiniowany w `PowerEditor/src/UI/Tabs.h`.
- Zapewnia operacje ustawiania listy kart, aktualizacji metadanych, aktywacji, przenoszenia i usuwania.
- Wydziela zdarzenia (`TabEventSink`) dla zamknięć, wyborów i drag&drop, aby most pomiędzy UI a logiką dokumentów pozostał neutralny.

### Qt backend
- Wariant referencyjny wykorzysta `QTabBar` z motywem Liquid Glass (gradienty, wskaźnik aktywnej karty).
- Integracja z `QtShortcutMap`, skróty do przełączania kart i multi-view.
- Wsparcie dla ikon (`QIcon` z zestawów Liquid) oraz dynamicznej zmiany tytułów w reakcji na `TabMetadata`.

### AppKit backend
- `NSTabViewController` sterujący `NSTabViewItem`. Dla estetyki Liquid Glass planowany jest customowy pasek (`NSView`) z buttonami stylizowanymi (rounded rect + blur).
- Przeciąganie kart między oknami wspierane poprzez `NSDraggingSource`/`NSDraggingDestination`.
- Ikony i znaczniki stanu renderowane przy użyciu `NSImage` + `NSVisualEffectView`.

## Migracja z Win32 `TabBarPlus`
1. **Adapter**: utworzyć `TabManagerBridge` w `DocTabView`, który będzie mapował zdarzenia na nowy interfejs.
2. **Synchronizacja stanu**: dotychczasowe `TCITEM` → `TabDescriptor`. Bufory otrzymają identyfikatory w postaci GUID/ścieżek.
3. **Ikony**: generator Liquid Glass dostarczy zestawy SF Symbols/NSImage do stylu macOS (np. `doc.circle.fill`).
4. **Multi-view**: `TabManager` jest instancjonowany per widok (A/B); bridging zapewni przenoszenie kart poprzez parę managerów.

## Wyzwania i rozwiązania
- **Double-click**: w macOS standardem jest otwieranie w nowym oknie – zachowujemy dotychczasowe zachowanie (split) z preferencją do przeniesienia do nowego widoku.
- **Ikony**: niestandardowe stany (monitoring) wymagają dodatkowego badge – plan: overlay `NSImage`/`QPixmap` z animowanym gradientem.
- **Dark/Light**: TabManager subscribes do `ThemeProvider` z Liquid Glass, reaguje na zmianę palety (`TabMetadata` + dynamiczne rysowanie).
- **Accessibility**: expose `NSAccessibilityTabGroup`/`QAccessible::Role::PageTabList`, uzupełnić opisy (tytuł + status, np. „README.md — modified”).

## Plan wdrożenia
1. **API**: zakończyć definicję interfejsów (wykonano). Rozbudować `UI/QtBackend` o stub `QtTabManager` korzystający z `QTabWidget`.
2. **Bridge**: przenieść logikę z `TabBarPlus` do `TabManagerBridge`, wstrzykiwać `TabManager` w `Notepad_plus`/`DocTabView`.
3. **Styling**: przygotować zestaw styli Qt (QSS) i AppKit (vibrancy) spójny z Liquid Glass.
4. **Testy**: dodać testy jednostkowe (simulowane zdarzenia) oraz Qt Test sprawdzające drag & drop, skróty, dynamiczne aktualizacje tytułów.

## Następne kroki
- Opracować prototyp `QtTabManager` wykorzystujący nowy interfejs.
- Zaprojektować w Figmie układ kart (Light/Dark) wraz z badge’ami stanu.
- Przygotować adapter notyfikujący wtyczki o zmianach w strukturze kart.
- Uwzględnić w `PreferencesStore` ustawienia ikon, koloru badge i preferencji przenoszenia kart.
