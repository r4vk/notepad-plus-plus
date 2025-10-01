# Strategia skrótów klawiaturowych na macOS

## Cel
Zapewnić zgodność skrótów Notepad++ z konwencjami macOS przy zachowaniu możliwości migracji ustawień z Windows oraz elastyczności dla wtyczek i użytkowników.

## Model
- Nowa warstwa `npp::ui::ShortcutMap` obsługuje neutralne skróty niezależne od platformy.
- Funkcje `normalizeWindowsShortcut` i `denormalizeShortcut` tłumaczą wpisy `Ctrl+` na macOS (`Command`, `Option`).
- Backend Qt wykorzystuje Scintilla/Qt do przypisywania `QKeySequence` i stosuje Command/Option według Apple HIG.

## Zasady
1. `Ctrl` z Windows → `Command` na macOS.
2. `Alt` → `Option`, `Ctrl+Alt` → `Command+Option`.
3. `Shift` pozostaje `Shift`, `Ctrl+Shift` → `Command+Shift`.
4. Podczas eksportu do Windows skrótów macOS → `Ctrl`/`Alt` zachowane dla kompatybilności.

## Implementacja
- `PowerEditor/src/UI/ShortcutNormalization.cpp` implementuje konwersję stringów (np. `Ctrl+Alt+F`) na neutralne `Shortcut` i z powrotem.
- `QtShortcutMap::importWindowsBinding` używa nowej funkcji `normalizeWindowsShortcut`, dzięki czemu definicje `ShortcutMapper` Windows mogą być wykorzystane.
- `tests/unit/ui/test_shortcut_normalization.cpp` walidują logikę konwersji.

## Dalsze kroki
- Integracja z `PreferencesStore` w celu przechowywania skrótów użytkownika macOS.
- Aktualizacja dokumentacji użytkownika (skrótów) o odpowiedniki macOS.
- Audyt skrótów wtyczek; przygotowanie adaptera `PluginCommand` dla konwersji.
- Weryfikacja testami UI (Qt Test) rzeczywistego wyzwalania komend przy użyciu Command/Option.
