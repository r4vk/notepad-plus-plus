# Integracja Scintilli z kontenerem macOS

## Cel
Zapewnić, że edytor Scintilla renderuje się poprawnie w natywnej aplikacji macOS portowanej na Qt oraz zachowuje spójność wizualną z motywem Liquid Glass, obsługując jednocześnie HiDPI i dynamiczny tryb ciemny systemu.

## Zakres
- Warstwa `npp::ui::scintilla` dostarcza neutralny model kolorów i metryk (`ScintillaTheme.h/.cpp`) wykorzystywany przez backendy.
- Implementacja Qt w `QtBackend.cpp` zastępuje placeholder `QPlainTextEdit` rzeczywistą instancją `ScintillaEdit` (Qt port Scintilli).
- Sterowanie motywem odbywa się poprzez `QtScintillaController`, który nasłuchuje zmian `ThemeProvider` i aplikuje je poprzez komunikaty Scintilli (`SCI_STYLESET*`, `SCI_SETCARETLINE*`).
- Skalowanie HiDPI ustalane jest na podstawie `devicePixelRatio` i odwzorowane na poziom zoomu (`SCI_SETZOOM`).

## HiDPI
- Funkcja `computeMetrics` mapuje współczynnik urządzenia na zoom (np. 2.0 → +20 poziomów). 
- Backend Qt odczytuje bieżące `QScreen::devicePixelRatio()` i aktualizuje Scintillę, zapewniając ostrość na ekranach Retina.

## Tryb ciemny
- `deriveColorScheme` miksuje tokeny Liquid Glass tak, aby zapewnić kontrast i dopasowanie do HIG (kolory tła, selekcji, numerów linii).
- Aktualizacje motywu propagowane są przez obserwatora `ThemeObserver`; w przyszłości kontroler zostanie połączony z `SystemAppearance` dla natywnych powiadomień.

## Dalsze kroki
- Zmapować dodatkowe style (składnia, zaznaczenia, minimapa) na tokeny Liquid Glass.
- Obsłużyć dynamiczne przełączanie ekranów HiDPI (nasłuch `QWindow::screenChanged`).
- Rozszerzyć kontroler o integrację `SystemServices::preferences()` w celu zapisywania zoomu i motywu użytkownika.
