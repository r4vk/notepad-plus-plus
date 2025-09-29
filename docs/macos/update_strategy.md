# Strategia aktualizacji Notepad++ na macOS

## 1. Kontekst i stan obecny (Windows)
- Aktualizacje rdzenia i wtyczek są wykonywane przez zewnętrzne narzędzie `gup.exe` (Generic Updater).
- `PluginsAdmin` przygotowuje środowisko aktualizacji przez:
  - lokalizację `updater/gup.exe` (`_updaterFullPath`) oraz pliku listy wtyczek (`nppPluginList.dll` lub `nppPluginList.json` w trybie debug),
  - weryfikację podpisów cyfrowych modułów przy pomocy `SecurityGuard::checkModule(...)` (WinTrust),
  - serializację parametrów aktualizacji (ścieżki pluginów, adres repozytorium) i zapis do `NppParameters::setWingup*`.
- Podczas zamykania `Notepad++` (zob. `NppBigSwitch.cpp:2944`) proces `gup.exe` jest uruchamiany poprzez klasę `Process` (wraper `CreateProcess`) z przygotowanymi argumentami.
- `gup.exe` odpowiada za:
  - pobranie plików (WinInet/WinHTTP),
  - weryfikację podpisów paczek,
  - rozpakowanie archiwów ZIP i podmianę plików w katalogach aplikacji/wtyczek,
  - restart Notepad++ po zakończeniu.
- Cały przepływ jest silnie zależny od WinAPI (WinInet, WinTrust, CreateProcess, ścieżki `Program Files`).

## 2. Cele dla macOS
1. Zapewnić natywną obsługę aktualizacji aplikacji oraz wtyczek zgodną z wymaganiami Apple (podpisywanie, notarization, sandbox).
2. Wyeliminować zależność od binariów Windows (`gup.exe`) oraz WinInet.
3. Zapewnić przejrzyste API w warstwie platformowej (`npp::platform::Updater`), aby Windows mógł wciąż używać dotychczasowego rozwiązania, a macOS korzystać z nowego backendu.
4. Podnieść poziom bezpieczeństwa (TLS pinning, sprawdzanie podpisów paczek, integralność list aktualizacji).
5. Zintegrować harmonogram aktualizacji z istniejącymi preferencjami (`NppGUI::_autoUpdateOpt`) i umożliwić przenoszenie ustawień między platformami.

## 3. Proponowana architektura
### 3.1 Warstwa abstrakcji
```
npp::platform::Updater
    ├── WindowsUpdaterBackend  (utrzymuje gup.exe / WinInet)
    └── MacUpdaterBackend      (NSURLSession + narzędzia Foundation)
```
- `Updater` udostępnia metody:
  - `checkForUpdates(UpdateKind kind)` – inicjuje sprawdzenie aktualizacji rdzenia lub listy wtyczek,
  - `downloadPackage(const UpdatePackage& pkg, const std::filesystem::path& target)` – pobiera paczkę,
  - `applyUpdate(const UpdatePackage& pkg)` – rozpakowuje i wykonuje kroki migracyjne,
  - `scheduleOnExit(const PendingOperations& ops)` – rejestruje działania do wykonania przy zamknięciu aplikacji.
- `PluginsAdmin` oraz moduł auto-aktualizacji będą korzystały z tych funkcji zamiast bezpośrednio manipulować plikami w katalogu `updater`.

### 3.2 Backend macOS (NSURLSession/CFNetwork)
- **Sieć:** `NSURLSession` z konfiguracją `ephemeralSessionConfiguration`, wsparcie dla HTTP/2, automatyczna obsługa TLS z cert pinning (opcjonalnie wykorzystanie `SecTrustEvaluateWithError`).
- **Podpisy i integralność:**
  - dla pakietów aplikacji – integracja z Sparkle 2.x (dla `.app` + DMG) lub wykorzystanie `SecStaticCodeCheckValidity` i podpisów Notepad++,
  - dla wtyczek – wymagane SHA-256 / podpisy `.pkg`. Plan: publikować archiwa `.pkg` podpisane Developer ID, a klient weryfikuje `SecStaticCodeCheckValidity` przed instalacją.
- **Dekompresja:** użycie `libarchive` (już dostępnej w macOS) poprzez `archive_read_open_filename` / `archive_write_disk` lub `NSFileCoordinator` dla sandboxu.
- **Operacje na plikach:**
  - aktualizacja wtyczek w `~/Library/Application Support/Notepad++/plugins`,
  - aktualizacja aplikacji w katalogu `.app` wymaga restartu i zastąpienia pakietu (Sparkle handle).
- **Harmonogram:**
  - `MacUpdaterBackend` przechowuje stan w `PreferencesStore` (np. `notepad.updater.lastCheck`, `notepad.updater.pendingOps`).
  - integracja z `NSBackgroundActivityScheduler` (jeśli aplikacja będzie sandboxowana) lub wykonywanie przy starcie.

### 3.3 Aktualizacje rdzenia vs wtyczek
- **Rdzeń:** rekomendacja użycia Sparkle 2.x (de facto standard na macOS). Umożliwia:
  - podpisane kanały aktualizacji (Ed25519),
  - automatyczne DMG/ZIP, powiadomienia użytkownika, restart.
- **Wtyczki:** zachować podobną semantykę jak dziś (lista wtyczek, instalacja/aktualizacja/usuwanie).
  - Lista wtyczek (`nppPluginList.json`) będzie pobierana bezpośrednio przez `NSURLSession`.
  - Zamiast restartowania i uruchamiania procesu, backend macOS może:
    1. pobrać paczki, 2. przeprowadzić walidację, 3. rozpakować do katalogu wtyczek, 4. oznaczyć w `PendingOperations` konfigurację wymagającą restartu (Notepad++ wyświetli toast i automatycznie zrestartuje się, gdy użytkownik potwierdzi).

## 4. Aspekty bezpieczeństwa
1. **Transport:** wymusić HTTPS z HSTS, opcjonalnie cert pinning (hash SPKI) dla kanałów aktualizacji.
2. **Podpisy paczek:** wykorzystać Developer ID + notarization (Apple) – brak podpisu → paczka odrzucana.
3. **Lista wtyczek:**
   - publikować manifest z podpisem (Ed25519) lub co najmniej sumami SHA-256,
   - weryfikować sumy po pobraniu pliku.
4. **Ograniczenia sandbox:** docelowo update backend musi działać w ramach przyznanych uprawnień; preferowane katalogi w `Application Support` i `Caches`.
5. **Roll-back:** zachowywać kopię poprzedniej wersji wtyczki (rename `.backup`) i możliwość automatycznego przywrócenia.

## 5. Plan implementacji
| Faza | Zakres | Rezultaty |
|------|--------|-----------|
| **F1 – interfejs i stub** | Zdefiniować `Platform/Updater.*`, dostarczyć stub implementację (w pamięci) + testy jednostkowe. Zrefaktoryzować `PluginsAdmin` i `NppBigSwitch`, aby używały nowego API na Windows (pr delegata). | Kompilacja niezmieniona na Windows, brak funkcjonalności na macOS (stub). |
| **F2 – Sparkle / updater rdzenia** | Zintegrować Sparkle 2.x z projektem macOS (`PowerEditor/macos`). Przygotować feed aktualizacji i proces podpisywania. | Możliwość ręcznej aktualizacji `.app` na macOS. |
| **F3 – wtyczki na macOS** | Implementacja `MacUpdaterBackend`: pobieranie listy, walidacja podpisu, pobranie i rozpakowanie archiwów, zapis stanu. Zaimplementować UI w `PluginsAdmin`, informujący o restartach (NSAlert). | Aktualizacja wtyczek bez `gup.exe`, test jednostkowy symulujący pobranie (HTTP stub). |
| **F4 – bezpieczeństwo i CI** | Automatyzacja podpisywania pakietów w CI (GitHub Actions macOS runners), notarization, testy E2E (pobranie, instalacja). | Zaufany łańcuch aktualizacji, pipeline publikacji artefaktów. |

## 6. Zależności i wymagania dodatkowe
- Dodanie zależności Sparkle (Swift/Objective‑C) – wymaga mostków w CMake/Xcode.
- Narzędzia do weryfikacji podpisów (`codesign`, `spctl`) dostępne systemowo – należy uwzględnić w dokumentacji zespołu.
- Potrzebny jest serwer CDN/HTTPS do hostowania kanału aktualizacji i paczek (Cloudflare/Netlify).
- Wtyczki: uzgodnić z maintainerami format dystrybucji (ZIP + manifest JSON vs signed pkg).

## 7. Otwarte kwestie
1. Czy zachować możliwość aktualizacji w trybie offline (lokalne archiwa)? – potencjalnie flagi CLI.
2. Integracja z istniejącym systemem powiadomień (toast w Windows) → na macOS planowane użycie `NSUserNotification` lub `NotificationCenter` (docelowa migracja w zadaniu powiadomień).
3. Wsparcie dla wtyczek instalowanych ręcznie – czy automatyczny aktualizator powinien je ignorować?
4. Zachowanie w środowiskach z ograniczeniami MDM (firmowe macOS) – należy umożliwić wyłączenie aktualizacji przez preferencje.

## 8. Następne kroki
- [ ] Zaimplementować interfejs `npp::platform::Updater` oraz stub backend.
- [ ] Przygotować proof-of-concept integracji Sparkle (manualne wywołanie aktualizacji).
- [ ] Zaplanować strukturę manifestu wtyczek (JSON + podpis) i pipeline publikacji.
- [ ] Zaktualizować `docs/macos/system_services_matrix.md` po wdrożeniu backendu.
