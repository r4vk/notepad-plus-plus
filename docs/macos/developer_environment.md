# Środowisko developerskie macOS dla Notepad++

## Wymagane narzędzia systemowe
- **macOS 13 Ventura** lub nowszy (Intel/Apple Silicon) z aktywnym kontem administratora.
- **Xcode 15 Command Line Tools** (`xcode-select --install`) zapewniające `clang`, `ld`, `make` oraz narzędzia `codesign`/`notarytool`.
- **Homebrew** do instalacji bibliotek dodatkowych (`/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"`).
- **CMake ≥ 3.26** (`brew install cmake`) z obsługą generatora `Ninja` i narzędzia `ctest`.
- **Ninja ≥ 1.11** (`brew install ninja`) – spójny system budowania z konfiguracją CI.
- **Qt 6.6 LTS** (`brew install qt@6` lub instalator Qt Online) z modułami `qtbase`, `qtdeclarative`, `qttools`, `qtimageformats`.
- **Python 3.11** (Homebrew) – niezbędny dla skryptów CMake (generacja kodu, testy).
- **Git** (systemowy lub Homebrew) z zaufanym certyfikatem `nppRoot.crt` do podpisywania wydanych plików aktualizacji.

## Konfiguracja repozytorium
1. Sklonuj projekt z submodułami: `git clone https://github.com/notepad-plus-plus/notepad-plus-plus.git --recurse-submodules`.
2. Wewnątrz katalogu projektu wykonaj `brew bundle --file=docs/macos/Brewfile` (plik zostanie dodany w kolejnych iteracjach) aby zapewnić spójne wersje zależności.
3. Ustaw zmienną środowiskową `Qt6_DIR` na katalog `lib/cmake/Qt6` (np. `export Qt6_DIR="/opt/homebrew/opt/qt6/lib/cmake/Qt6"`).
4. Dodaj `~/Library/Application Support/Notepad++` do listy katalogów wykluczonych z Time Machine, aby uniknąć konfliktów w testach trwałości.
5. Skonfiguruj pętlę podpisywania: `security create-keychain -p <hasło> npp.keychain` oraz import certyfikatu deweloperskiego Apple (instrukcja w planowanym dokumencie o dystrybucji).

## Procedura bootstrap projektu
```bash
# konfiguracja uniwersalnego builda x86_64 + arm64
cmake -S PowerEditor/src -B build/macos-debug \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DQT_PACKAGE_DIR="${Qt6_DIR}" \
  -DNPP_ENABLE_QT_UI=ON

cmake --build build/macos-debug
ctest --test-dir build/macos-debug --output-on-failure
```

## Rekomendowane profile edytora / IDE
- **Qt Creator 11** z zestawem narzędzi Clang i wsparciem dla CMake presets.
- **CLion 2023.3** – możliwość importu konfiguracji CMake oraz integracji z narzędziami do analizy statycznej (`clang-tidy`, `cppcheck`).
- **Visual Studio Code** z rozszerzeniami: `ms-vscode.cmake-tools`, `llvm-vs-code-extensions.vscode-clangd`, `ms-python.python`.

## Walidacja jakości
- `clang-tidy` z profilem `modernize-*`, `readability-*`, `bugprone-*` uruchamianym w CI (GitHub Actions macOS).
- `codesign --verify --deep` po każdej kompilacji pakietu `.app`.
- `ctest` z docelowymi testami GoogleTest (po wdrożeniu) oraz testami integracyjnymi UI (XCTest).
- Monitorowanie wydajności poprzez `Instruments` (Time Profiler, Leaks) dla głównych scenariuszy: otwieranie dużych plików, wyszukiwanie/regEx, praca z wtyczkami.

## Zadania uzupełniające
- Przygotować `docs/macos/Brewfile` z listą pakietów Homebrew.
- Dodać preset `macos-debug` i `macos-release` do `CMakePresets.json`.
- Zaimplementować skrypt `scripts/macos/bootstrap.sh` automatyzujący kroki konfiguracyjne.
- Włączyć `clang-format` (`.clang-format`) i `clang-tidy` w pipeline macOS.
