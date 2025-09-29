# Liquid Glass – Style tokens i biblioteka komponentów (macOS)

## 1. Cel
Zdefiniować zestaw tokenów projektowych i konwencji implementacyjnych dla motywu Liquid Glass w Notepad++ na macOS z wykorzystaniem Qt 6 (Widgets/QML). Dokument stanowi bazę dla deweloperów UI oraz autorów wtyczek, aby zapewnić spójność wizualną.

## 2. Rdzeń design systemu
- **Estetyka:** połączenie szklanych warstw (blur + translucencja) z ostrymi akcentami kolorystycznymi.
- **Ton:** jasny i ciemny wariant, automatyczne dopasowanie do systemowego trybu Light/Dark.
- **Adaptacja:** wszystkie kolorowe tokeny muszą posiadać odpowiednik w macOS Accent Colors.

## 3. Tokeny kolorów
| Token | Jasny tryb | Ciemny tryb | Uwagi |
|-------|------------|-------------|-------|
| `lg.surface.primary` | #F5F7FA (95% op. blur) | #1C1F24 (80% op. blur) | Główne tło okien |
| `lg.surface.secondary` | #FFFFFF E7 | #1F2328 CC | Boczne panele |
| `lg.glass.highlight` | #FFFFFF 50% | #FFFFFF 20% | Nakładki glassmorphism |
| `lg.text.primary` | #131722 | #E9EDF5 | Kolor podstawowy tekstu |
| `lg.text.secondary` | #4B5565 | #B2B8C4 | Metadane, placeholder |
| `lg.accent.blue` | #3C74F6 | #7AA2FF | Domyślny akcent |
| `lg.accent.green` | #3AC27C | #55DDA1 | Status sukcesu |
| `lg.accent.orange` | #F29F4C | #F3B270 | Ostrzeżenia |
| `lg.accent.red` | #F2665D | #F57F79 | Błędy |
| `lg.border.muted` | #D7DCE5 | #2A3036 | Linie separatorów |

## 4. Typografia
| Token | Font | Rozmiar | Zastosowanie |
|-------|------|---------|--------------|
| `lg.font.display` | SF Pro Display / 600 | 18 pt | Tytuły paneli |
| `lg.font.body` | SF Pro Text / 400 | 13 pt | Treść UI |
| `lg.font.caption` | SF Pro Text / 400 | 11 pt | Etykiety, status |
| `lg.font.mono` | SF Mono / 400 | 12 pt | Dane techniczne i logi |

## 5. Odstępy i promienie
- `lg.spacing.xs` = 4 px
- `lg.spacing.sm` = 8 px
- `lg.spacing.md` = 12 px
- `lg.spacing.lg` = 16 px
- `lg.corner.radius` = 9 px (panele), 6 px (przyciski)
- `lg.shadow.glass` = `QGraphicsDropShadowEffect` (0 12 24, alpha 0.18)

## 6. Komponenty referencyjne
1. **Toolbar** – `QToolBar` z `lg.surface.secondary`, ikony monochromatyczne (SF Symbol fallback) + akcent gradientowy.
2. **Panel dockowany** – `QDockWidget` z półprzezroczystym tłem (`lg.surface.primary`), nagłówek `lg.font.display`, przyciski (close/float) w stylu macOS.
3. **Przyciski akcji** – `QPushButton` + `QStyle` override: gradient `lg.accent.*` + glass overlay, focus ring `QPalette::Highlight`.
4. **Listy/TreeView** – `QTreeView` z wierszami 28px, highlight `lg.accent.blue` 40% alpha.
5. **Okno preferencji** – `QTabWidget` lub `QStackedWidget` + `Sidebar` w stylu macOS (ikony SF, spacing `lg.spacing.lg`).

## 7. Implementacja w Qt
- **Centrala stylów:** plik `resources/ui/liquid_glass.qss` generowany ze źródła (JSON tokeny → QSS).
- **Binding do QML (opcjonalnie):** mapowanie tokenów na `Qt.palette` (`ThemeManager` w C++ → `singleton` QML).
- **Dynamiczne przełączanie Light/Dark:** listen na `SystemAppearance::preferredColorScheme()` (już istnieje) i reapply QSS.

## 8. Narzędzie do tokenów
- Plik źródłowy `design/tokens/liquid_glass.json` przechowuje kolory, odstępy, radii.
- Generator (Python) przekształca do: `liquid_glass.qss`, `liquid_glass.qml`, dokumentacji (MarkDown).
- CI waliduje brak duplikatów i kontrast (`WCAG AA` – min 4.5:1 dla tekstu). Plan: użycie `tinycss2 + contrast-ratio` w pipeline macOS.

## 9. Integracja z pluginami
- Ekspozycja tokenów poprzez API `npp::ui::ThemeProvider` (C++). Pluginy mogą zapytać o wartości (np. `getColor("lg.accent.blue")`).
- Dokumentacja stylu + przykładowy `QWidget` w SDK.

## 10. Następne kroki
- [ ] Ustalić definicje gradientów i blur (parametry `QGraphicsBlurEffect`).
- [ ] Przygotować prototyp QSS i referencyjne widgety w `PowerEditor/macos/ui_prototype`.
- [ ] Dodać testy regresyjne (screenshot tests) w CI macOS.
- [ ] Skonsultować finalne kolory z zespołem UX / społecznością.
