# Zrealizowane działania portu macOS

- Utworzono moduł `Platform/PathProvider` zapewniający wieloplatformową obsługę katalogów użytkownika (AppData/Application Support) oraz narzędzie `ensureDirectoryExists` do bezpiecznego tworzenia struktur katalogów.
- Zastąpiono w `NppParameters` bezpośrednie odwołania do `SHGetFolderPath` i ręcznych wywołań `CreateDirectory` integracją z nowym modułem platformowym i przygotowano fallbacki dla środowiska macOS.
- Dostosowano `PluginsAdmin` oraz testowy kod `FunctionList` do korzystania z nowego interfejsu ścieżek, ograniczając dalsze zależności od stałych `CSIDL` i przygotowując grunt pod kompilację poza Windows.
- Opracowano dokument `docs/macos/architecture_assessment.md` klasyfikujący moduły PowerEditor, Scintilla i Lexilla pod kątem przenośności oraz mapujący zależności od Win32 na odpowiedniki macOS wraz z analizą ryzyk dla portu; zadania w sekcji analitycznej planu zostały zaktualizowane jako ukończone.
- Opracowano dokument `docs/macos/build_system_plan.md` opisujący podział projektów CMake, cele platformowe (Windows/macOS), plan tworzenia pakietu `.app` oraz integrację CI, jednocześnie potwierdzając dostępność backendu Cocoa w Scintilli i neutralny platformowo kod Lexilli.
- Dodano moduł `Platform/FileSystem` zapewniający wieloplatformowe operacje na ścieżkach (łączenie, weryfikacja i usuwanie plików/katalogów) oraz włączono go do konfiguracji CMake i projektu Visual Studio.
- Przebudowano obsługę katalogu chmury w `NppParameters`, zastępując bezpośrednie wywołania WinAPI nowym API platformowym `combinePath`, `ensureDirectoryExists`, `fileExists` i `removeFile`, aby ujednolicić przygotowanie ścieżek pod macOS.
- Przygotowano dokument `docs/macos/ui_strategy.md` porównujący Cocoa i toolkity wieloplatformowe oraz opisujący plan migracji UI z warstwą abstrakcji `npp::ui` i mostkami eventów.
- Opracowano roadmapę `docs/macos/port_roadmap.md` z fazami iteracyjnego portu, kamieniami milowymi i wskaźnikami jakości, aktualizując plan `macOS_convert_todo.md` o zakończone zadanie harmonogramu.
- Zrefaktoryzowano tworzenie katalogów w `Buffer.cpp`, `Notepad_plus.cpp`, `Notepad_plus_Window.cpp` i `NppIO.cpp`, zastępując wywołania WinAPI funkcjami `npp::platform::ensureDirectoryExists` i `combinePath`, aby przygotować kod do budowy poza Windows.
- Uaktualniono strategię UI w `docs/macos/ui_strategy.md`, wybierając Qt 6.x z motywem Liquid Glass jako warstwę prezentacji oraz dodając nowe zadania w `macOS_convert_todo.md` dotyczące modułu `npp::ui` i prototypu Qt.
