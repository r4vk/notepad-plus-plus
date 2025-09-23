# macOS system services mapping

## Cel dokumentu
Dokument identyfikuje odpowiedniki dla kluczowych zależności Win32 wykorzystywanych przez Notepad++ oraz opisuje strategię ich użycia podczas portowania aplikacji do natywnego środowiska macOS. Zestawienie stanowi podstawę do implementacji warstwy abstrakcji `npp::platform` i planowania integracji z frameworkiem Qt 6 + Liquid Glass.

## Macierz usług systemowych
| Obszar Win32 | Zakres użycia w Notepad++ | Odpowiednik macOS | Uwagi implementacyjne |
| --- | --- | --- | --- |
| Shell32 / ShlObj (`CSIDL`, `SHGetFolderPath`, `SHFileOperation`) | Rozwiązywanie ścieżek profilu użytkownika, kosz, operacje plikowe | **Foundation / AppKit** (`NSSearchPathForDirectoriesInDomains`, `NSFileManager`, `NSURL`), uzupełniająco `CFURL` | Abstrakcja ścieżek (`PathProvider`) odwołuje się do `NSSearchPath*`; operacje przenoszenia/usuwania plików zrealizować przez `NSFileManager` lub `std::filesystem` z fallbackiem na Cocoa. |
| `GetTempPath`, `GetTempFileName` | Tworzenie plików tymczasowych przy zapisie chronionych plików, autozapisy | **Foundation** (`NSTemporaryDirectory`, `NSFileManager::temporaryDirectory`) | Wprowadzono `KnownDirectory::Temporary`; kolejnym krokiem jest generator bezpiecznych nazw (`mkstemp`) i obsługa autozapisu w katalogu `~/Library/Application Support/Notepad++/Temp`. |
| User32 (`HWND`, `SendMessage`, `TrackPopupMenu`, rejestracja klas okien) | Główne okno, dokowanie paneli, komunikacja z wtyczkami | **AppKit** (`NSWindow`, `NSView`, `NSMenu`, `NSNotificationCenter`) + warstwa Qt | Interfejs `npp::ui` powinien owijać elementy Qt i wystawiać metody stylizowane na macOS; komunikacja z wtyczkami wymaga mostu zdarzeń zamiast `SendMessage`. |
| GDI (`HBITMAP`, `HDC`, `CreateCompatibleDC`) | Ikony, renderowanie pasków narzędzi, miniatury | **Core Graphics**, `NSImage`, `NSBitmapImageRep` | Przy renderingu korzystać z `QImage`/`QPixmap` i konwersji do `NSImage` przy integracji z Dockiem i schowkiem. |
| Rejestr Windows (`RegGetValue`, `SHGetValue`) | Ustawienia aplikacji i część wtyczek | **Core Foundation** (`CFPreferences`), pliki `.plist` | Konfigurację przenosimy do katalogu `~/Library/Preferences` poprzez moduł preferencji (planowane). |
| COM / OLE (`IDataObject`, `IDropTarget`) | Schowek, Drag & Drop, shell integrations | **NSPasteboard**, `NSDraggingDestination`, `UTType` | Wymagany adapter w warstwie `npp::ui`, wtyczki muszą korzystać z nowego API. |
| Powiadomienia systemowe (`Shell_NotifyIcon`) | Ikona w zasobniku, komunikaty toast | **UserNotifications**, `NSStatusBar` | Notyfikacje przenosimy do `UNUserNotificationCenter`, wskaźnik statusu do `NSStatusItem`. |
| File System Watchers (`ReadDirectoryChangesW`) | Monitoring zmian plików i auto-odświeżanie | **FSEvents**, `DispatchSourceFileSystemObject` | Interfejs `IFileWatcher` powinien zostać przeniesiony na implementację w oparciu o FSEvents z fallbackiem `std::filesystem::last_write_time`. |
| Drukowanie (`StartDoc`, `EndDoc`) | Podgląd i druk dokumentów | **AppKit / PDFKit** (`NSPrintOperation`, `NSPrintInfo`) | Warstwa Qt oferuje `QPrinter`; integracja z natywnym dialogiem przez `NSPrintPanel`. |
| Networking (`WinInet`, `WinHTTP`) | Menedżer aktualizacji, Plugins Admin | **CFNetwork** (`NSURLSession`), `QtNetwork` | Aktualizator planujemy oprzeć o `NSURLSession`/Sparkle, Plugins Admin może korzystać z `QtNetwork`. |

## Biblioteki i pakiety zewnętrzne
- **Qt 6.6+ (Widgets + Quick Controls)** – podstawowa warstwa UI/UX Liquid Glass, integracja z Cocoa zapewnia natywne menu i paski narzędzi.
- **Sparkle 2.x** – aktualizacje i notarizacja pakietów `.app`.
- **GoogleTest / Catch2** – framework testów jednostkowych (sekcja testowa planu).
- **fmt** – bezpieczne formatowanie ciągów tekstowych w nowych modułach platformowych.
- **nlohmann/json** (opcjonalnie) – przyszła serializacja ustawień i integracji pluginów w formacie JSON, jeśli TinyXML okaże się niewystarczające na macOS.

## Dalsze kroki
1. Rozszerzyć moduł `npp::platform` o obsługę: schowka, powiadomień, FSEvents oraz generowania bezpiecznych nazw plików tymczasowych.
2. Zaimplementować warstwę preferencji z wykorzystaniem `CFPreferences` i `QSettings` (backend `NativeFormat`).
3. Przygotować szkic API `npp::ui` mapujący główne okno, menu i panele dokowane na komponenty Qt/AppKit.
4. Zweryfikować dostępność bibliotek Sparkle i GoogleTest w pipeline CI, a następnie uwzględnić je w konfiguracji CMake.
