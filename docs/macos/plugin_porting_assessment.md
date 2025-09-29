# Ocena możliwości portowania wtyczek społeczności na macOS

## 1. Cel dokumentu
Określić stan najpopularniejszych wtyczek Notepad++, zidentyfikować bariery portowania na macOS oraz zaproponować działania wspierające społeczność deweloperów.

## 2. Metodologia
- Lista wtyczek na podstawie `nppPluginList` i statystyk pobrań.
- Analiza kodu źródłowego (język, zależności, kompilacja CMake/Visual Studio).
- Ocena użycia WinAPI (GUI, schowek, rejestr, shell, hooking).

## 3. Wtyczki priorytetowe
| Wtyczka | Główne funkcje | Technologie | Bariery | Priorytet |
|---------|----------------|-------------|---------|-----------|
| ComparePlus | Porównywanie plików, wizualizacja różnic | C++, Win32, Scintilla | Docking API, GDI kolorowanie | Wysoki |
| NppExport | Eksport do RTF/HTML/LaTeX | C++, WinAPI shell | ShellExecute, GDI | Średni |
| NppFTP | FTP/SFTP klient | C++, WinSock, OpenSSL | WinSock, MDI okna | Wysoki |
| JSToolNpp | Formatowanie JSON/JavaScript | C++, WinAPI | Minimalne (MessageBox, menu) | Średni |
| XML Tools | Walidacja, XSLT | C++, libxml2, WinAPI | MSXML zależności, COM | Wysoki |
| MarkdownViewer++ | Podgląd Markdown | C++, WebView (Trident/Edge) | Brak WebView na macOS | Wysoki |
| CSScriptNpp | Uruchamianie skryptów C# | .NET, WinForms | Brak .NET Framework, UI WinForms | Niski |
| PythonScript | Embedded Python | Python, WinAPI | PyWin32, COM | Średni |
| DSpellCheck | Spellchecking | Hunspell, WinAPI | Docking, COM | Średni |

## 4. Klasyfikacja barier
1. **UI zależne od Win32** – wtyczki używające `DialogBox`, `CreateWindowEx`, `DockingManager` (ComparePlus, NppFTP, DSpellCheck).
2. **Zależności systemowe Windows** – WinSock (NppFTP), COM/MSXML (XML Tools), .NET/WinForms (CSScriptNpp).
3. **Komponenty renderujące Windows** – WebBrowser/Trident/Edge (MarkdownViewer++, Explorer++).
4. **Integracja z rejestrem/WinAPI** – NppExport, JSTool (MessageBox).

## 5. Działania wspierające
- **Warstwa `DockingManagerBridge`** (w planie) – zapewnić wczesny prototyp i dokumentację migracji UI dla ComparePlus/NppFTP.
- **Biblioteki cross-platformowe** – rekomendacja: libcurl zamiast WinSock, libxml2/built-in parser zamiast MSXML/COM, Qt WebEngine jako zamiennik WebView.
- **Pakiet SDK macOS** – przygotować repo `npp-mac-plugin-sdk` (CMake + przykład wtyczki „HelloMac”) z instrukcją budowania universal2.
- **Narzędzie diagnostyczne** – script `plugin_winapi_audit.py` analizujący `.cpp` pod kątem WinAPI; generuje raporty dla twórców.
- **Kanały komunikacji** – wpis w blogu, wątek na forum, dedykowany kanał Discord dla portu macOS.
- **Program „early access”** – umożliwić autorom wtyczek testy na wersjach alpha Notepad++ macOS (dokument NDA, zgłoszenia błędów w GitHub).

## 6. Plan wdrożenia wsparcia
| Etap | Działanie | Termin |
|------|-----------|--------|
| P0 | Opublikować SDK macOS (nagłówki, script `build_plugin.sh`) | T+2 tygodnie |
| P1 | Przygotować poradnik migracyjny („Minimal Win32 shims” + `PreferencesStore`) | T+3 tygodnie |
| P2 | Warsztat online dla autorów ComparePlus/NppFTP | T+4 tygodnie |
| P3 | Zbiórka feedbacku, iteracja dokumentacji | T+6 tygodni |

## 7. Ryzyka
- **Brak zasobów u autorów wtyczek** – minimalizować przez gotowe szablony i wsparcie CI.
- **Rozbieżności w API** – krytyczne, jeśli `SendMessage` nie zostanie w pełni odwzorowany; wniosek: priorytet dla `MessageBridge`.
- **Różnice w renderowaniu (WebView)** – rozważyć dedykowany „Notepad++ Web Preview” plugin na Qt WebEngine do zastąpienia zależności od IE/Edge.

## 8. Następne kroki
- [ ] Stworzyć repozytorium `notepad-plus-plus/notepadpp-mac-plugin-sdk` (CMake + przykład ComparePlus minimal).
- [ ] Skontaktować się z maintainerami ComparePlus, NppFTP, MarkdownViewer++ – przekazać plan i zebrać oczekiwania.
- [ ] Zaplanować sesję Q&A na forum + zebrać pytania.
