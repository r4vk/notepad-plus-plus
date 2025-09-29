# API preferencji dla twórców wtyczek

## Kontekst i cele
Port macOS rezygnuje z rejestru Windows na rzecz warstwy `npp::platform::PreferencesStore`,
która zapewnia jednolite API do przechowywania ustawień w `~/Library/Application Support/Notepad++`
oraz w plikach `.plist`. Wtyczki powinny korzystać z tej warstwy zamiast odczytów/zapisów do rejestru,
aby zachować kompatybilność między Windows i macOS, uprościć testowanie oraz spełnić wymagania
platform Apple dotyczące sandboxingu.

Celem tego dokumentu jest przedstawienie kontraktu API udostępnianego wtyczkom C/C++ oraz wzorców
użycia, które pozwalają łatwo migrować istniejące rozszerzenia.

## Skrócony przegląd API

### Kluczowe komunikaty Notepad++
| Komunikat | Kierunek | Opis |
|-----------|----------|------|
| `NPPM_PREFERENCES_GET` | wtyczka → Notepad++ | Odczyt wartości z `PreferencesStore` |
| `NPPM_PREFERENCES_SET` | wtyczka → Notepad++ | Zapis wartości do `PreferencesStore` |
| `NPPM_PREFERENCES_REMOVE` | wtyczka → Notepad++ | Usunięcie pojedynczego klucza |
| `NPPM_PREFERENCES_CLEAR` | wtyczka → Notepad++ | Usunięcie całej domeny |

Komunikaty są dostępne od wersji **macOS Preview r2** (oraz analogicznej wersji Windows) i będą
zdefiniowane w `Notepad_plus_msgs.h`. Każdy komunikat działa synchronistycznie i zwraca informację
o powodzeniu operacji (`TRUE`/`FALSE`) lub, w przypadku odczytu, długość danych.

### Struktury danych
```cpp
struct NPP_PREFERENCES_VALUE
{
    const wchar_t* domain;   // np. L"plugins/com.example.awesome"
    const wchar_t* key;      // np. L"autoFormat.enabled"
    void*          buffer;   // wskaźnik na dane (modulowany przez type)
    size_t         sizeInBytes; // rozmiar bufora / danych
    int            type;     // jedna z wartości wyliczonych poniżej
};

enum class NPP_PREFERENCES_TYPE : int
{
    StringUtf16 = 0,
    Int64       = 1,
    Boolean     = 2,
};
```
Wysyłając komunikat wtyczka przekazuje wskaźnik do `NPP_PREFERENCES_VALUE`. Dla operacji odczytu pole
`buffer` powinno wskazywać na bufor, a `sizeInBytes` musi zawierać jego rozmiar. Jeśli bufor jest za
mały, funkcja zwraca wymaganą wielkość (> rozmiaru bufora) i nie modyfikuje danych.

### Domeny nazw
- Domeny tworzą hierarchię z separatorem `/` (np. `plugins/com.example.awesome`).
- Zalecane jest poprzedzenie domeny przestrzenią nazw analogiczną do odwrotnej nazwy domeny internetowej
autorów wtyczki, aby uniknąć kolizji.
- Główne domeny systemowe (`notepad.gui`, `notepad.gui.backup`, …) są zarezerwowane dla rdzenia.

## Przykładowe wdrożenia

### Odczyt i zapis ustawień (C++)
```cpp
#include "Notepad_plus_msgs.h"

namespace
{
    constexpr wchar_t kDomain[] = L"plugins/com.example.awesome";
}

bool getBooleanPreference(HWND npp, const wchar_t* key, bool fallback)
{
    NPP_PREFERENCES_VALUE request{};
    request.domain = kDomain;
    request.key = key;
    request.type = static_cast<int>(NPP_PREFERENCES_TYPE::Boolean);
    request.buffer = &fallback;      // używamy fallback jako bufora wejściowego
    request.sizeInBytes = sizeof(fallback);

    const LRESULT result = ::SendMessage(npp, NPPM_PREFERENCES_GET, 0, reinterpret_cast<LPARAM>(&request));
    if (result == FALSE)
        return fallback;

    return *reinterpret_cast<bool*>(request.buffer);
}

void setBooleanPreference(HWND npp, const wchar_t* key, bool value)
{
    NPP_PREFERENCES_VALUE update{};
    update.domain = kDomain;
    update.key = key;
    update.type = static_cast<int>(NPP_PREFERENCES_TYPE::Boolean);
    update.buffer = &value;
    update.sizeInBytes = sizeof(value);

    ::SendMessage(npp, NPPM_PREFERENCES_SET, 0, reinterpret_cast<LPARAM>(&update));
}
```

### Przechowywanie struktur złożonych
W przypadku obiektów złożonych (np. listy ostatnich projektów) rekomendujemy serializację do JSON
lub XML, a następnie zapis jako UTF‑16 (`StringUtf16`). Należy zadbać o limit wielkości danych
(na macOS preferencje synchronizowane przez CFPreferences nie powinny przekraczać ~64 KB na klucz).

```cpp
std::wstring serializeProjects(const std::vector<Project>& projects)
{
    // ... serializacja do JSON ...
}

void saveProjects(HWND npp, const std::vector<Project>& projects)
{
    const std::wstring payload = serializeProjects(projects);

    NPP_PREFERENCES_VALUE update{};
    update.domain = kDomain;
    update.key = L"projects.recent";
    update.type = static_cast<int>(NPP_PREFERENCES_TYPE::StringUtf16);
    update.buffer = const_cast<wchar_t*>(payload.c_str());
    update.sizeInBytes = (payload.size() + 1) * sizeof(wchar_t);

    ::SendMessage(npp, NPPM_PREFERENCES_SET, 0, reinterpret_cast<LPARAM>(&update));
}
```

### Usuwanie kluczy i czyszczenie domeny
```cpp
void resetPluginPreferences(HWND npp)
{
    NPP_PREFERENCES_VALUE clear{};
    clear.domain = kDomain;
    clear.key = nullptr;

    ::SendMessage(npp, NPPM_PREFERENCES_CLEAR, 0, reinterpret_cast<LPARAM>(&clear));
}
```

## Wzorce projektowe i dobre praktyki
1. **Lazy load + cache** – wtyczki powinny odczytywać preferencje przy starcie/aktywacji i utrzymywać ich
   kopię we własnych strukturach, ograniczając liczbę komunikatów.
2. **Obsługa błędów** – w przypadku `FALSE` należy traktować sytuację jak brak ustawienia i stosować
   wartości domyślne; warto logować ostrzeżenia w konsoli diagnostycznej.
3. **Migracja** – podczas pierwszego uruchomienia wtyczki na macOS można użyć istniejących plików XML
   (np. `plugins/config/<nazwa>.xml`) i przepisać dane do `PreferencesStore`, a następnie usunąć pliki
   źródłowe lub utrzymywać je w trybie tylko do odczytu.
4. **Wielowątkowość** – `SystemServices::preferences()` gwarantuje bezpieczeństwo wątków na poziomie
   pojedynczych operacji; unikaj równoległych zapisów do tego samego klucza z wielu wątków.
5. **Testowalność** – w środowisku testowym można podmienić instancję `SystemServices` na stub (np. przez
   dedykowane funkcje pomocnicze) co pozwala symulować zapis preferencji bez dostępu do dysku.

## Stan wdrożenia i roadmapa
- Interfejs wiadomości zostanie udostępniony równocześnie z wdrożeniem backendu macOS (`CFPreferences`).
- Obecnie moduł `Platform/PreferencesStore` posiada backend Windows (rejestr) oraz stub dla macOS –
  wtyczki mogą już używać API w buildach developerskich; operacje zostaną zapisane w stubie in-memory.
- Po ukończeniu backendu macOS wymagane będzie dodanie podpisów kodu (jeśli wtyczka dystrybuuje
  własne binaria) jednak sam zapis preferencji nie wymaga dodatkowych uprawnień.

## Checklist dla twórców wtyczek
- [ ] Aktualizuj kod, aby używał `NPPM_PREFERENCES_*` zamiast bezpośrednich wywołań rejestru.
- [ ] Zdefiniuj unikalną domenę (`plugins/<org>.<plugin>`).
- [ ] Dodaj fallback wartości domyślnych oraz migrację istniejących ustawień.
- [ ] Uaktualnij dokumentację wtyczki, opisując nową lokalizację ustawień (macOS: `~/Library/Application Support/Notepad++`).
- [ ] Dodaj testy regresyjne sprawdzające zapis/odczyt preferencji (np. w oparciu o stub `SystemServices`).

## Załączniki
- **Przykładowy kod**: fragmenty w niniejszym dokumencie można wkleić do szkieletu wtyczki
  (zob. `PowerEditor/misc/pluginTemplate`).
- **Migracja**: dokument `docs/macos/registry_migration_plan.md` zawiera mapowanie istniejących kluczy
  rejestrowych i strategie migracji.
