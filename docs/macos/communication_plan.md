# Plan komunikacji i zaangażowania społeczności dla portu Notepad++ na macOS

## 1. Cele
- Informować użytkowników o postępach i kolejnych etapach portu macOS.
- Utrzymywać transparentność decyzji technicznych (UI, pluginy, dystrybucja).
- Angażować deweloperów społeczności (autorów wtyczek, tłumaczy) w proces testów.
- Zarządzać oczekiwaniami dotyczącymi dostępności funkcji i ograniczeń platformowych.

## 2. Kluczowe grupy odbiorców
1. **Społeczność użytkowników Notepad++** – forum, Reddit, social media.
2. **Autorzy wtyczek** – GitHub, Discord #plugins, mailing list.
3. **Kontrybutorzy rdzenia** – GitHub, dev meetings.
4. **Partnerzy / media** – blog Notepad++, agregatory newsów (Hacker News, Lobsters).

## 3. Kanały komunikacji
| Kanał | Cel | Częstotliwość |
|-------|-----|---------------|
| Blog Notepad++ | Oficjalne ogłoszenia, kamienie milowe | Kiedy zamykamy etap (alpha/beta/RC) |
| Forum Notepad++ (Announcements, Development) | Dyskusje techniczne, feedback QA | Bieżąco (co sprint) |
| GitHub Discussions | Q&A dla deweloperów, roadmap | Aktualizacje sprintów |
| Discord (kanał #mac-port) | Szybka komunikacja z maintainerami | Stały kontakt |
| Newsletter (opcjonalnie) | Zbiorcze podsumowania dla subskrybentów | Co kwartał |

## 4. Harmonogram działań komunikacyjnych
| Faza | Opis | Kanały |
|------|------|--------|
| F0 – Teaser | Ogłoszenie prac nad portem macOS, zaproszenie do udziału | Blog, forum, social |
| F1 – Alpha (CLI/preview) | Publikacja instrukcji buildów CLI, prośba o feedback | GitHub, Discord |
| F2 – UI preview (Liquid Glass) | Screeny, video, focus group UI/UX | Blog, YouTube, forum |
| F3 – Plugin SDK preview | SDK, narzędzia (manifest validator) | Blog, GitHub |
| F4 – Beta | Zaproszenie do testów, zbieranie bugów | GitHub Issues, forum |
| F5 – Release Candidate | Finalna lista zmian, dokumentacja instalacji | Blog, newsletter |
| F6 – Stable Release | Ogłoszenie, instrukcja migracji, FAQ | Wszystkie kanały |

## 5. Kluczowe komunikaty
- **Misja**: „Przenosimy Notepad++ na macOS z zachowaniem lekkości i elastyczności”.
- **Wartość**: natywne doświadczenie, kompatybilność wtyczek, bezpieczeństwo.
- **Transparentność**: publiczny roadmap, raporty postępów co sprint.
- **Współpraca**: zaproszenie do testów, zgłaszania problemów, udział w warsztatach.

## 6. Materiały wspierające
1. Roadmapa (`docs/macos/port_roadmap.md`).
2. Strategia UI (`docs/macos/ui_strategy.md`).
3. Strategia wtyczek/SDK (`docs/macos/plugin_api_compatibility.md`, `docs/macos/plugin_distribution_strategy.md`).
4. FAQ portu macOS (do utworzenia) – odpowiedzi na pytania o pluginy, wymagania sprzętowe.
5. Instrukcja buildów (`docs/macos/developer_environment.md`).
6. Dev blog / changelog (z automatycznym generowaniem). 

## 7. Zaangażowanie społeczności
- **Ankiety UX** – prototypy Liquid Glass, preferencje UI (menubar, skróty). Narzędzia: Google Forms/Typeform.
- **Program Early Adopters** – lista testerska (GitHub Discussions), cotygodniowe syncs.
- **Warsztaty wtyczkowe** – spotkania online (Zoom/Discord) po udostępnieniu SDK.
- **Bug bash events** – sesje testów tematycznych (np. „obsługa plików sieciowych”).

## 8. Zarządzanie ryzykiem komunikacyjnym
| Ryzyko | Mitigacja |
|--------|-----------|
| Opóźnienia w roadmapie | Transparentne raporty sprintowe; aktualizacja planu na blogu. |
| Niekompatybilność wtyczek | Wczesne ogłoszenie API bridge, dokumentacja migracji, wsparcie maintainers. |
| Dezinformacja (fora zewnętrzne) | FAQ + pinned posts; reagowanie na błędne informacje. |
| Zbyt duże oczekiwania | Jasna komunikacja ograniczeń (np. brak Win-only funkcji). |

## 9. Wskaźniki sukcesu
- Liczba aktywnych testerów (cel: 100+ w Beta).
- Liczba wtyczek portowanych do macOS (20+ w ciągu 3 miesięcy od release).
- Zmniejszenie liczby krytycznych błędów zgłoszonych po premierze (<10 w miesiącu).
- Zaangażowanie społeczności: min. 3 warsztaty/wydarzenia online.

## 10. Następne kroki
- [ ] Przygotować komunikat F0 (blog + forum) z roadmapą i ankietą zainteresowania.
- [ ] Utworzyć kanał Discord #mac-port i przypiąć FAQ.
- [ ] Zaprojektować template changelogów (Markdown) dla sprintów macOS.
- [ ] Powołać zespół ds. community (moderator forum, maintainers wtyczek, marketing).
