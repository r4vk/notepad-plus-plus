# Rejestracja rozszerzeń i protokołów (LaunchServices)

## Cel
Umożliwić Notepad++ dla macOS integrację z systemem poprzez LaunchServices, tak aby:
- dokumenty o wskazanych rozszerzeniach (np. `.txt`, `.ini`, `.log`, `.npp-session`) otwierały się
  w Notepad++,
- aplikacja pojawiała się w menu „Open With”, Finderze oraz Spotlight,
- obsługiwane były niestandardowe protokoły (`notepadpp://`),
- rejestracja/de-rejestracja przebiegała bez udziału instalatora MSI (CLI + `.app`).

## Architektura rozwiązania
1. **Warstwa abstrakcji** `npp::platform::FileAssociationService`
   - Metody `registerFileTypes(const std::vector<FileAssociation>&)`,
     `unregisterFileTypes(...)`, `registerURLSchemes(...)`.
   - Implementacja macOS używa `LSSetDefaultRoleHandlerForContentType`, `UTTypeCreateAllIdentifiersForTag`.
   - Backend Windows pozostaje w aktualnym module `RegExt`.

2. **Manifest `.plist`**
   - Sekcja `CFBundleDocumentTypes` generowana z `FileAssociation` (UTType, ikony, rola `Editor`).
   - Sekcja `CFBundleURLTypes` dla protokołów.
   - Generator CMake (`tools/macos/generate_document_types.py`) synchronizuje JSON → `Info.plist`.

3. **CLI dla developerów**
   - Skrypt `tools/macos/register_file_types.sh` uruchamia `osascript`/`plutil`, wywołuje
     `LSRegisterURL` na ścieżce `.app`. Używany w `postbuild` oraz instrukcjach QA.

4. **CI**
   - Workflow `ci-macos-launchservices.yml`: po zbudowaniu `.app` wykonuje rejestrację, otwiera
     plik testowy (`open test.txt`) i weryfikuje logi (`mdls -name kMDItemCFBundleIdentifier`).
   - Po testach wyrejestrowuje (`lsregister -u`).

## Scenariusze testowe
1. **Open With**: utworzyć plik `.txt`, zarejestrować `.app`, sprawdzić `LSGetApplicationForInfo`.
2. **Default editor**: ustawić Notepad++ jako domyślny dla `.log` i otworzyć plik poprzez `open`.
3. **Spotlight**: indeks `mdfind "kMDItemContentTypeTree == 'public.plain-text' && kMDItemCFBundleIdentifier == 'org.notepadplusplus.preview'"`.
4. **Protocol handler**: `open notepadpp://open?path=/tmp/demo.txt` powinno delegować do modułu
   otwierania dokumentów (Cocoa → `npp::ui::Application`).
5. **Cleanup**: test integracyjny weryfikuje, że po wyrejestrowaniu `LSCopyApplicationForURL`
   zwraca aktualny edytor domyślny (lub `nil`).

## Implementacja krok po kroku
1. Zdefiniować struktury `FileAssociation`, `URLScheme` w `Platform/ApplicationRegistration.h`.
2. Utworzyć implementację `FileAssociationServiceMac` (Objective-C++) korzystającą z LaunchServices.
3. Dodać generator JSON → `Info.plist` oraz wprowadzić docelowe rozszerzenia do repo (`docs/macos/file_associations.json`).
4. Rozszerzyć rozdział instalacyjny (`BUILD.md`) o instrukcję rejestracji CLI i wymagane uprawnienia.
5. Przygotować testy:
   - jednostkowe (mock LaunchServices, `LSSetDefaultRoleHandlerForContentType` stub),
   - integracyjne (XCTest, patrz `nsdocumentcontroller_tests.md`).

## Wymagania bezpieczeństwa
- Rejestracja protokołów oznacza kontrolę nad linkami – wymagana walidacja parametrów
  (tylko ścieżki lokalne, brak automatycznego wykonywania skryptów).
- Operacje LaunchServices wymagają podpisanego `.app`; CI musi używać tymczasowych certyfikatów
  (Developer ID / ad-hoc) do testów.
- Na macOS Sonoma `lsregister` wymaga uprawnień SIP dla katalogów systemowych – proces powinien
  ograniczać się do katalogu użytkownika.

## Dalsze kroki
- Przygotować minimalną implementację `FileAssociationServiceMac` (Objective-C++) i podpiąć do
  nowej warstwy `npp::platform::SystemServices`.
- Dodać testy integracyjne w pipeline (`ci-macos-launchservices.yml`).
- Zaktualizować dokumentację użytkownika z instrukcją ustawiania Notepad++ jako domyślnego edytora.
