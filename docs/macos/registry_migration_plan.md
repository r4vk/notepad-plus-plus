# Migracja funkcji rejestru Windows na preferencje macOS

## Kontekst
Notepad++ opiera część konfiguracji i funkcji integracyjnych na rejestrze Windows.
W szczególności dotyczy to:

- zapisywania globalnych preferencji aplikacji (np. ostatnie położenie okien, ustawienia drukarki, lista ostatnio otwieranych plików),
- zarządzania zachowaniem programu podczas restartu systemu (`RegisterApplicationRestart`, klucze polityki w `HKCU`),
- rozszerzeń shellowych (`HKCR`), wpisów aktualizacji i integracji z protokołami URL,
- konfiguracji i manifestów wtyczek, które przyjmują ścieżki lub ustawienia z rejestru.

Port na macOS musi zastąpić te mechanizmy systemem preferencji opartym na `CFPreferences`, plikach `.plist`
oraz katalogach `~/Library/Application Support/Notepad++` i `~/Library/Preferences`.

## Cele migracji

1. **Przeniesienie ustawień aplikacji** do formatu przyjaznego macOS, tak aby preferencje były zapisywane
   w `~/Library/Preferences/com.notepad-plus-plus.app.plist` (preferencje globalne) oraz w dedykowanych
   plikach XML/JSON w `~/Library/Application Support/Notepad++` dla danych strukturalnych (np. listy sesji).
2. **Ujednolicenie API konfiguracji**, które umożliwi wspólne użycie przez Windows (np. pliki ini/xml)
   oraz macOS (`CFPreferences`) bez bezpośrednich odwołań do rejestru.
3. **Zapewnienie kompatybilności wstecznej** – import istniejących ustawień z `config.xml` i z eksportu rejestru,
   aby użytkownik przenoszący się z Windows zachował dotychczasową konfigurację.
4. **Minimalizacja ryzyka** dla wtyczek, które polegają na kluczach rejestru, poprzez przygotowanie warstwy
   zgodności oraz dokumentacji migracyjnej.

## Zakres i priorytety

| Priorytet | Obszar | Opis | Działania |
|-----------|--------|------|-----------|
| Wysoki | Preferencje główne | Ustawienia przechowywane dziś w `HKCU\\Software\\Notepad++` | <ul><li>Zaprojektować interfejs `npp::platform::PreferencesStore` z backendami `WindowsRegistry` i `ApplePreferences`.</li><li>Udokumentować mapowanie kluczy rejestru na pliki `.plist` lub XML.</li><li>Wprowadzić migrator, który podczas pierwszego uruchomienia macOS odczyta `config.xml` / eksport rejestru i zapisze dane w nowym formacie.</li></ul> |
| Średni | Polityki restartu i crash recovery | Klucze sterujące automatycznym restartem | <ul><li>Przenieść flagi z rejestru do pliku `~/Library/Application Support/Notepad++/Policies.json`.</li><li>Na Windows zachować możliwość tworzenia pliku `noRestartAutomatically.xml` jako wspólnego mechanizmu.</li></ul> |
| Średni | Integracja shellowa | Rejestracja rozszerzeń plików, protokołów | <ul><li>Przenalizować istniejące narzędzia `MISC/RegExt` i przygotować odpowiedniki `LaunchServices`.</li><li>Udostępnić narzędzie CLI/Xcode project generujące profile `.plist`.</li></ul> |
| Niski | Wtyczki | Dodatkowe klucze używane przez pluginy | <ul><li>Przygotować przewodnik dla autorów wtyczek z rekomendacją migracji na pliki konfiguracyjne.</li><li>Rozważyć adapter API udostępniający dostęp do `PreferencesStore` zamiast rejestru.</li></ul> |

## Plan działania

1. **Inwentaryzacja**
   - Użyć statycznej analizy (`rg "Reg"`) i dokumentacji, aby wypisać wszystkie miejsca korzystające z `RegOpenKeyEx`, `RegQueryValueEx`, `SHGetValue`.
   - Udokumentować format danych (string, DWORD, struktury binarne) i częstotliwość użycia.
2. **Projekt interfejsu**
   - Zdefiniować API kluczy przestrzeni (`PreferencesDomain`, `PreferencesKey`) oraz operacji (odczyt, zapis, enumeracja, usuwanie).
   - Zaprojektować obsługę sekcji (np. `Scintilla`, `Recent Files`) oraz transakcji/commitów.
3. **Implementacja backendu macOS**
   - Użyć `CFPreferences` i `CFPropertyList` do przechowywania typów prostych.
   - Dla danych złożonych (listy sesji) wykorzystać pliki JSON/XML w `Application Support` z operacjami atomowymi (`NSFileManager`, `std::filesystem`).
   - Zapewnić walidację danych i backup automatyczny.
4. **Migracja kodu**
   - Utworzyć moduł `Platform/Preferences` z wersją Windows opartą na plikach XML (stopniowe wygaszanie rejestru), aby uniezależnić logikę od WinAPI.
   - Zrefaktoryzować `NppParameters`, `DoLocalConf`, `PluginManager` i inne moduły, aby korzystały z nowego interfejsu.
   - Przygotować testy jednostkowe symulujące migrację oraz weryfikujące poprawność zapisu/odczytu.
5. **Obsługa wtyczek i dokumentacja**
   - Zapewnić API C/C++ dostępne dla pluginów: funkcje `NPPM_PREFERENCES_GET/SET` odwołujące się do `PreferencesStore`.
   - Przygotować przewodnik migracyjny i przykładowy kod.
6. **Walidacja i QA**
   - Utworzyć zestaw testów regresyjnych sprawdzających brak regresji w ustawieniach (porównanie snapshotów).
   - Dodać scenariusze do pipeline CI (Windows i macOS) pokrywające migrację oraz równoległe zapisy ustawień.

## Wymagania testowe

- Testy jednostkowe `tests/unit/preferences/test_preferences_store.cpp` pokrywające operacje CRUD na obu backendach.
- Testy integracyjne uruchamiające Notepad++ w trybie headless (jeśli dostępne) lub harness startowy, aby zweryfikować migrację ustawień z istniejących plików/rejestru.
- Testy systemowe sprawdzające poprawną rejestrację rozszerzeń plików i protokołów poprzez narzędzia `LaunchServices` (macOS) oraz analogiczną dezinstalację.

## Ryzyka i mitigacje

- **Różnice w typach danych** – `CFPreferences` operuje na `CFPropertyList`. Konieczne jest mapowanie struktur z rejestru. *Mitigacja*: format JSON/PropertyList z walidacją schematu.
- **Wtyczki nieprzystosowane do nowego API** – zapewnić okres przejściowy z aliasami, dokumentację i przykład migracji.
- **Równoczesny dostęp** – macOS preferencje są buforowane; należy wymusić `CFPreferencesAppSynchronize` po zapisie. W Windows zastosować blokady plików.
- **Import ustawień** – przygotować narzędzie CLI konwertujące export `.reg` na pliki `.plist` oraz testy konwersji.

## Rezultaty

Realizacja planu umożliwi całkowite odejście od rejestru Windows i stworzy wspólną warstwę przechowywania
ustawień dla Windows i macOS. Dzięki temu:

- budowa na macOS będzie pozbawiona zależności od WinAPI,
- preferencje staną się przenośne pomiędzy systemami,
- poprawimy bezpieczeństwo (brak zapisu w HKLM) i zgodność z politykami Apple (sandbox, notarizacja),
- wtyczki otrzymają nowoczesny, stabilny interfejs na lata.
