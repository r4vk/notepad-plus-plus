# Testy integracyjne NSDocumentController

## Cel
Zapewnić, że wariant macOS aplikacji Notepad++ poprawnie integruje się z `NSDocumentController`
– otwierając pliki przeciągnięte na ikonę Docka, z menu Finder oraz poprzez AppleScript –
oraz że logika dokumentów (multi-tab, restore session) działa identycznie jak na Windows.

## Zakres przypadków testowych
1. **Podstawowe otwarcie** – `NSDocumentController.shared.openDocument` przekazuje plik do
   warstwy `npp::ui::Application`, a nowa karta otrzymuje poprawną nazwę, zawartość i aktywuje
   się w widoku A.
2. **Wiele dokumentów** – otwarcie listy (`openDocument(withContentsOf:display:)`) skutkuje
   utworzeniem odpowiedniej liczby kart; kolejność kart zgodna z kolejnością przekazaną przez
   system.
3. **Ponowne otwarcie** – dokument oznaczony jako niedawno zamknięty (`reopenDocument`) trafia do
   właściwego widoku oraz przywraca kursor / historię pozycji.
4. **Multi-view** – plik przeciągnięty na panel B ląduje w odpowiednim `TabManager`; sprawdzenie
   aktywacji panelu i synchronizacji między widokami.
5. **Przywracanie po awarii** – integracja `NSDocumentController` z mechanizmem restore state
   (`application:shouldRestoreWindowState:`). Test weryfikuje zapis snapshotu i odtworzenie kart.
6. **Drag & drop** – przeciągnięcie plików na ikonę Docka i do obszaru notatnika; sprawdzenie
   obsługi typów UTType (txt, log, source) oraz reakcji, gdy plik jest już otwarty (aktywacja
   istniejącej karty).

## Architektura testów
- Framework: **XCTest** (`xcodebuild test`), docelowo osadzony w projekcie
  `tests/integration/NotepadPPMacTests.xcodeproj`.
- Test host: mini wrapper `NotepadPPTestHost.app` uruchamiający wariant Qt + `NSApplication`
  (ściągnięcie `NotepadPlusPlusPreview.app` z artefaktu CI lub lokalny build z `cmake`).
- Mostek: scenariusze sterowane przez `XCTestCase` z wykorzystaniem `expectation` i
  `NotificationCenter` (`NSApplication.didFinishLaunchingNotification`, sygnały
  `npp::platform::SystemServices`).

## Przykładowy szkic testu
```swift
final class NSDocumentControllerTests: XCTestCase {
    var app: XCUIApplication!

    override func setUpWithError() throws {
        continueAfterFailure = false
        app = XCUIApplication(bundleIdentifier: "org.notepadplusplus.preview")
        app.launchArguments = ["--uitest", "--disable-plugins"]
        app.launch()
    }

    func testOpenDocumentCreatesTab() throws {
        let fileURL = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("test.txt")
        try "Hello, macOS".write(to: fileURL, atomically: true, encoding: .utf8)

        let controller = NSDocumentController.shared
        let expectation = self.expectation(description: "Document opened")

        NotificationCenter.default.addObserver(forName: NSNotification.Name("npp.ui.tabOpened"),
                                               object: nil,
                                               queue: .main) { notification in
            if let title = notification.userInfo?["title"] as? String, title == "test.txt" {
                expectation.fulfill()
            }
        }

        controller.openDocument(withContentsOf: fileURL, display: true) { _, _, error in
            if let error = error { XCTFail(error.localizedDescription) }
        }

        wait(for: [expectation], timeout: 5.0)
    }
}
```

## Integracja w CI
1. Dodaj workflow GitHub Actions `ci-macos-integration.yml`, który:
   - buduje `NotepadPlusPlusPreview.app` (z Qt backendem),
   - generuje projekt `tests/integration/NotepadPPMacTests.xcodeproj` (skrypt CMake + template),
   - uruchamia `xcodebuild test -scheme NotepadPPMacTests -destination 'platform=macOS'`.
2. Osłab wymagania sandbox (runner `macos-13` potrzebuje dostępu do GUI; użyj `xcodebuild` z
   symulowanym display `CGPreFlightPortServer` lub tryb `NSApplicationActivationPolicyProhibited`).
3. Publikuj raporty JUnit (`-resultBundlePath`) i artefakty (logi, screenshoty z narzędzia
   `xcrun simctl io`).

## Lokalne uruchamianie
```bash
cmake -S tests/integration -B build/integration -G Xcode \
      -DNPP_APP_BUNDLE=~/Projects/notepad-plus-plus/build/macos/NotepadPlusPlusPreview.app
cmake --build build/integration --config Debug --target NotepadPPMacTests
xcodebuild test -project build/integration/NotepadPPMacTests.xcodeproj \
               -scheme NotepadPPMacTests -destination 'platform=macOS'
```

## Wymagania środowiskowe
- macOS 13+ z uprawnieniami do automatyzacji (System Settings → Privacy & Security → Accessibility).
- Zainstalowany `Qt 6.8` (ten sam wariant co w aplikacji) oraz frameworki testowe (`XCTest`).
- `brew install applesimutils` (opcjonalnie) – do sprzątania oczekujących procesów.

## Dalsze kroki
- Zaimplementować `npp::ui::DocumentEvents` wysyłające powiadomienia o otwarciu/zamknięciu kart,
  aby ułatwić asercje.
- Rozszerzyć testy o scenariusze przeciągania plików (`XCUIElement.dragAndDrop`) i wielokrotnego
  otwierania.
- Dodać testy regresyjne weryfikujące, że otwarcie pliku już otwartego aktywuje istniejącą kartę,
  zamiast tworzyć duplikat.
- Przygotować fixtures z dużymi plikami i sprawdzić wydajność (`measure` w XCTest).
