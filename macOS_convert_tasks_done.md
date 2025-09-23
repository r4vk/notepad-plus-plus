# Zrealizowane działania portu macOS

- Utworzono moduł `Platform/PathProvider` zapewniający wieloplatformową obsługę katalogów użytkownika (AppData/Application Support) oraz narzędzie `ensureDirectoryExists` do bezpiecznego tworzenia struktur katalogów.
- Zastąpiono w `NppParameters` bezpośrednie odwołania do `SHGetFolderPath` i ręcznych wywołań `CreateDirectory` integracją z nowym modułem platformowym i przygotowano fallbacki dla środowiska macOS.
- Dostosowano `PluginsAdmin` oraz testowy kod `FunctionList` do korzystania z nowego interfejsu ścieżek, ograniczając dalsze zależności od stałych `CSIDL` i przygotowując grunt pod kompilację poza Windows.
- Opracowano dokument `docs/macos/architecture_assessment.md` klasyfikujący moduły PowerEditor, Scintilla i Lexilla pod kątem przenośności oraz mapujący zależności od Win32 na odpowiedniki macOS wraz z analizą ryzyk dla portu; zadania w sekcji analitycznej planu zostały zaktualizowane jako ukończone.
- Opracowano dokument `docs/macos/build_system_plan.md` opisujący podział projektów CMake, cele platformowe (Windows/macOS), plan tworzenia pakietu `.app` oraz integrację CI, jednocześnie potwierdzając dostępność backendu Cocoa w Scintilli i neutralny platformowo kod Lexilli.
