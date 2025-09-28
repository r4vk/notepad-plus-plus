# Harmonogram iteracyjnego portu Notepad++ na macOS

## Założenia
- Iteracyjne dostarczanie funkcjonalności umożliwiające równoległe testy i feedback społeczności.
- Zachowanie stabilności wersji Windows poprzez warstwę abstrakcji i wspólne testy jednostkowe.
- Wykorzystanie CI/CD do walidacji każdego etapu (buildy macOS, testy jednostkowe, pakiety .app).

## Fazy portu

| Faza | Zakres | Wejście | Wyjście | Kluczowe wskaźniki |
| --- | --- | --- | --- | --- |
| **F1: Fundament platformowy** | Warstwa abstrakcji systemowej (ścieżki, pliki, rejestr → CFPreferences), konfiguracja CMake, bootstrap narzędzi. | Zaktualizowany plan i architektura, działający build Windows. | Kompilacja `libnppcore` na macOS CLI, podstawowe testy jednostkowe konfiguracyjne. | 100% testów konfiguracyjnych zaliczone, build bez ostrzeżeń krytycznych. |
| **F2: Rdzeń edytora** | Integracja Scintilla Cocoa, obsługa dokumentów, kodowania, sesji, autozapisu. | Zbudowany fundament platformowy, działający backend Scintilla. | Uruchamialna aplikacja CLI (bez UI) operująca na dokumentach, testy modułu dokumentów i makr. | Pokrycie testów modułów dokumentów ≥80%, brak regresji w logice plików. |
| **F3: Interfejs użytkownika** | Warstwa `npp::ui`, okno główne AppKit, menu, paski narzędzi, panele dokowane, skróty klawiaturowe. | Stabilny rdzeń edytora, zintegrowana warstwa systemowa. | Wersja alfa `.app` z podstawowym UI, zgodna z HIG, obsługująca wielodokumentowość i preferencje. | Testy UI (XCTest) dla krytycznych scenariuszy, NPS użytkowników alfa ≥50. |
| **F4: Integracje systemowe** | Drag&drop, NSDocumentController, FSEvents, Quick Look, drukowanie, powiadomienia. | Dostępne UI alfa, moduł pluginów w trakcie adaptacji. | Beta `.app` z integracją Finder, Services i drukowania; pipeline CI z notarizacją. | Czas otwarcia pliku ≤1.2x wersji Windows, wszystkie testy integracyjne przechodzą. |
| **F5: Pluginy i ekosystem** | API kompatybilności, SDK macOS, podpisywanie pluginów, dystrybucja. | Stabilna beta, zdefiniowane API eventów. | RC z co najmniej 5 zweryfikowanymi pluginami, dokumentacja dewelopera. | ≥5 pluginów "certified", brak crashów w logach telemetrycznych. |
| **F6: Wydanie GA i utrzymanie** | Optymalizacje, lokalizacja, dokumentacja użytkownika, wsparcie aktualizacji (Sparkle). | API pluginów zamrożone, wyniki beta pozytywne. | Publiczne wydanie GA `.app`, proces aktualizacji i raportowania błędów w miejscu. | Crash-free sessions ≥99%, CSAT ≥4.2/5. |

## Kamienie milowe
1. **M1 – Build CLI**: kompilacja podstawowych modułów na macOS, uruchomienie testów konfiguracji (`F1`).
2. **M2 – Edytor tekstowy**: w pełni funkcjonalny backend Scintilla z obsługą plików i kodowań (`F2`).
3. **M3 – UI alfa**: aplikacja `.app` z podstawową nawigacją i preferencjami (`F3`).
4. **M4 – Integracje**: obsługa dokumentów macOS, drukowania, powiadomień (`F4`).
5. **M5 – Ekosystem pluginów**: SDK + pierwsze zweryfikowane rozszerzenia (`F5`).
6. **M6 – Wydanie GA**: podpisany, notaryzowany pakiet z dokumentacją (`F6`).

## Zarządzanie ryzykiem
- Utrzymanie równoległej zgodności z Windows poprzez wspólny zestaw testów jednostkowych i integracyjnych.
- Regularne przeglądy architektury (co iterację) z zespołem core i społecznością.
- Bufor czasu (10–15%) w każdej fazie na reagowanie na regresje Scintilla/plug-inów.

## Śledzenie postępów
- Dashboard CI raportujący status buildów macOS/Windows, testów i rozmiaru binarnego.
- Kwartalne przeglądy roadmapy i aktualizacja `macOS_convert_todo.md`.
- Rejestr działań (`macOS_convert_tasks_done.md`) utrzymujący chronologię ukończonych elementów oraz odchyleń.
