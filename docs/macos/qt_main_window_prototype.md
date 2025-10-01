# Prototyp głównego okna Qt

## Cel
Celem prototypu jest dostarczenie działającego szkieletu okna Notepad++ na macOS zbudowanego na Qt 6.x, które demonstruje integrację z warstwą `npp::ui`, placeholderem edytora (docelowo Scintilla) oraz panelem dokowanym zgodnym z założeniami Liquid Glass.

## Zakres prototypu
- Rejestracja backendu Qt w `npp::ui::Factory` poprzez `registerQtBackend()`.
- Utworzenie głównego okna (`QMainWindow`) z centralnym edytorem (`QPlainTextEdit` jako tymczasowy substytut Scintilli).
- Konfiguracja panelu dokowanego „Function List” z użyciem `QDockWidget` i placeholdera z listą funkcji.
- Wygenerowanie podstawowego menu (File, View) bazującego na `MenuModel`, z akcjami obsługiwanymi przez `CommandRegistry`.
- Wprowadzenie mapowania skrótu i logicznej obsługi polecenia „Toggle Function List”.

## Integracja Scintilli
Scintilla zostanie zintegrowana w kolejnych iteracjach poprzez warstwę hostującą `ScintillaEdit` w `QWidget`. W prototypie centralny widget jest abstrakcją, ale zachowany został interfejs umożliwiający płynną podmianę.

## Liquid Glass i motywy
Prototyp rejestruje `ThemeProvider` z podstawową paletą Liquid Glass. Zmiana palety będzie sterowana przyszłymi sygnałami z warstwy `SystemAppearance`. Panel dokowany oraz menu wykorzystują kolory systemowe Qt, a docelowo będą stylizowane poprzez generator QSS.

## Jak uruchomić prototyp
1. Zainstalować Qt 6.8+ wraz z modułami `QtWidgets`, `QtGui`, `QtCore`.
2. Skonfigurować build z flagą `-DNPP_UI_WITH_QT=ON` (np. przez `add_definitions(-DNPP_UI_WITH_QT)` w docelowym projekcie lub preset w CMake).
3. W module macOS (lub osobnym sandboxie) utworzyć `QApplication`, wywołać `registerQtBackend()` i `Factory::create(config)`.
4. Uruchomić zwrócone `application->run()` – okno zaprezentuje placeholder Scintilli i prawy dock Function List.

## Następne kroki
- Podmienić centralny `QPlainTextEdit` na ScintillaQt/ScintillaEdit hostowaną w `QWidget`.
- Zmapować docelowe panele (Project Panel, Document Map) z wykorzystaniem nowego API dokowania.
- Rozszerzyć `ThemeProvider` o dynamiczne przełączanie Light/Dark oraz export tokenów Liquid Glass.
- Podłączyć `ShortcutMap` do preferencji użytkownika (`PreferencesStore`).
- Włączyć prototyp do pipeline CI (macOS) z wykorzystaniem trybu `offscreen` Qt do walidacji layoutu.
