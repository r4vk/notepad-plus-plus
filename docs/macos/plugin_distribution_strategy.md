# Strategia dystrybucji wtyczek Notepad++ na macOS

## 1. Stan obecny (Windows)
- **Katalog instalacyjny:** wtyczki instalowane globalnie w `Notepad++/plugins/<PluginName>` oraz konfiguracje w `plugins/config`.
- **Plugins Admin:** korzysta z listy `nppPluginList.json`/`nppPluginList.dll`, podpisanej i hostowanej przez projekt; instalacje realizuje narzędzie `gup.exe` (pobranie ZIP, weryfikacja podpisu, rozpakowanie do katalogu `plugins`).
- **Bezpieczeństwo:** kontrola podpisów bibliotek DLL (WinTrust), opcjonalne SHA-256 w manifestach.
- **Aktualizacje:** `gup.exe` zarządza instalacją/usuwaniem/aktualizacją oraz restartuje Notepad++.

## 2. Wymagania macOS
1. **Ścieżki macOS:** wszystkie wtyczki i ich pliki konfiguracyjne muszą być przechowywane w `~/Library/Application Support/Notepad++/plugins/<PluginName>` (użytkownik) oraz opcjonalnie w katalogu aplikacji (`NotepadPlusPlus.app/Contents/Plugins`) dla paczki systemowej.
2. **Podpisywanie i notarization:** każda binarka `.dylib` musi być podpisana identyfikatorem Developer ID oraz poddana notarization Apple. Instalacja niezaufanych binariów musi być blokowana.
3. **Architektura uniwersalna:** wtyczki powinny dostarczać binaria `universal2` (x86_64 + arm64). Plugins Admin musi sprawdzać architekturę przed instalacją.
4. **Sandbox / uprawnienia:** docelowo Notepad++ może działać bez sandboxu, ale wtyczki muszą respektować lokalizacje zapisu i nie modyfikować katalogu aplikacji. Instalator wtyczek powinien mieć możliwość manipulacji wyłącznie w `Application Support` i `Caches`.
5. **Interfejs użytkownika:** użytkownik powinien instalować/usuwać wtyczki z poziomu okna Plugins Admin (Qt), z powiadomieniami zgodnymi z HIG.
6. **Kompatybilność:** zachowanie API wtyczek (NPPM_*) – dystrybucja musi umożliwiać wtyczkom dostarczanie dodatkowych zasobów (ikony, języki).

## 3. Proponowana architektura dystrybucji
```
Plugin Catalog Service
 ├─ plugin_catalog.json (HTTPS + podpis Ed25519)
 ├─ manifesty wtyczek (.json per plugin)
 └─ archiwa universal2 (ZIP lub PKG podpisane)

Plugins Admin (macOS)
 ├─ ApplicationDownloader (NSURLSession / CFNetwork)
 ├─ SignatureValidator (codesign --verify, SecStaticCodeCheckValidity)
 ├─ Installer (rozpakowanie / instalacja w Application Support)
 ├─ PostInstall Hooks (kopiowanie zasobów, migracja config)
 └─ Rollback Manager (kopie poprzednich wersji)
```

## 4. Manifest wtyczki (propozycja)
```json
{
  "identifier": "org.notepadpp.plugin.example",
  "displayName": "Example Plugin",
  "version": "1.2.3",
  "minimumNppVersion": "9.0.0",
  "architectures": ["universal2"],
  "download": {
    "url": "https://plugins.notepad-plus-plus.org/mac/example-1.2.3.zip",
    "sha256": "..."
  },
  "signature": "base64-ed25519",
  "install": {
    "bundle": "Example.dylib",
    "resources": [".config", "assets"]
  }
}
```
- Manifest podpisany kluczem Ed25519 (projektem). Plugins Admin weryfikuje podpis oraz sumę SHA-256 po pobraniu archiwum.
- Archiwum zawiera strukturę docelową `Example.dylib`, `config/`, `assets/`. Po rozpakowaniu trafia do `~/Library/Application Support/Notepad++/plugins/Example/`.

## 5. Proces instalacji
1. Użytkownik wybiera wtyczkę -> Plugins Admin pobiera manifest z `plugin_catalog.json` (cache w `%USER%/Library/Caches`).
2. Weryfikacja podpisu manifestu (Ed25519) oraz sumy SHA-256.
3. Pobranie archiwum ZIP (`NSURLSession`).
4. `SignatureValidator` wykonuje:
   - `codesign --verify --deep --strict` na rozpakowanym `.dylib`.
   - `spctl --assess --type execute` z opcją `-v` w trybie dry-run (logi dla diagnostyki).
5. Installer rozpakowuje archiwum do katalogu docelowego, wykonując backup starej wersji (rename -> `.backup`).
6. Prefetch plików zasobów (np. `.xib`, `.json`).
7. Odświeżenie menu wtyczek; jeśli wtyczka wymaga restartu, prezentowany jest dialog (NSAlert) z opcją natychmiastowego restartu.

## 6. Aktualizacje i usuwanie
- **Aktualizacja:** `plugin_catalog.json` zawiera flagi `latestVersion`. Plugins Admin porównuje z lokalnym `version.json`. Pobranie różnicy przebiega jak instalacja, z zachowaniem kopii poprzedniej wersji.
- **Usuwanie:** przeniesienie katalogu wtyczki do `~/Library/Application Support/Notepad++/plugins/_Trash/Example_<timestamp>` i odświeżenie menu. Tylko po akceptacji użytkownika.
- **Rollback:** na wypadek błędu instalacji system przywraca ostatnią kopię z `_Trash`.

## 7. Pipeline publikacji (CI)
- Workflow GitHub Actions (macOS runner):
  1. Budowa wtyczki w trybie release (universal2).
  2. `codesign --sign "Developer ID Application"`.
  3. Notary service (`xcrun notarytool submit`).
  4. Uzupełnienie manifestu JSON (hash, rozmiar).
  5. Publikacja artefaktów na CDN (np. GitHub Releases, Cloudflare R2).
  6. Re-gen `plugin_catalog.json`, podpis Ed25519 (klucz przechowywany w secrets).

## 8. Integracja z istniejącym Windows Plugins Admin
- Repozytorium listy wtyczek może być wspólne, ale sekcja macOS powinna mieć własne pola (`platforms: ["windows", "macos"]`).
- Wtyczki mogą dostarczać różne pakiety na Windows i macOS; manifest główny wskazuje oddzielne URL dla każdej platformy.
- Plugins Admin na macOS ignoruje wpisy Windows-only.

## 9. Wymagania dodatkowe
- **Przenośne biblioteki:** wtyczki nie mogą linkować dynamicznie do bibliotek spoza systemu (poza wbudowanymi). Wymóg: statyczne linkowanie lub bundling w katalogu wtyczki.
- **Sandbox** (opcjonalnie): w przyszłości, jeżeli Notepad++ zostanie zsandboxowany, wtyczki muszą używać mechanizmów App Sandbox Entitlements (wymaga nowej dystrybucji `.plugin`).
- **Telemetria:** Plugins Admin powinien logować wynik instalacji (sukces/błąd) w lokalnym logu diagnostycznym.
- **UX:** integracja z dark mode, pokazywanie rozmiaru pliku, daty wydania.

## 10. Plan wdrożenia
| Faza | Zakres | Rezultaty |
|------|--------|-----------|
| P1 – Manifesty i katalog | Zdefiniować schemat JSON, generator katalogu, podpis ED25519 | Wspólny algorytm w repo `infra/plugins`. |
| P2 – Narzędzia deweloperskie | Udostępnić `tools/validate_plugin_manifest.py`, przykład manifestu, dokument „How-To” | Walidacja lokalna manifestów przez autorów wtyczek. |
| P3 – Installer macOS | Implementacja `ApplicationDownloader`, `SignatureValidator`, rozpakowywanie ZIP, backup | Działający Plugins Admin na macOS z instalacją testowej wtyczki. |
| P4 – Notary pipeline | Przygotować instrukcje dla autorów wtyczek, przykładowe workflow GitHub Actions | Dokumentacja + sample repo. |
| P5 – CI integracja | Automatyczne testy instalacji (macOS runner, headless) | Walidacja katalogu i instalacji. |
| P6 – Komunikacja | Ogłoszenia, dokumentacja dla devów, migracja istniejących wtyczek | Aktualizacja user manual + blog post. |

## 11. Następne kroki
- [ ] Przygotować wzorcowy manifest + narzędzie walidujące (`validate_plugin_manifest.py`).
- [ ] Zaimplementować moduł `ApplicationDownloader` oraz `SignatureValidator` w Plugins Admin (macOS path).
- [ ] Opracować szablon GitHub Actions dla autorów (build + notarization + publikacja).
- [ ] Skonfigurować serwer katalogu (HTTPS, HSTS, cert pinning).
