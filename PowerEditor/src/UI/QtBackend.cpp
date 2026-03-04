#include "UI/QtBackend.h"

#include "UI/Application.h"
#include "UI/Command.h"
#include "UI/Docking.h"
#include "UI/Factory.h"
#include "UI/Menu.h"
#include "UI/Shortcut.h"
#include "UI/ShortcutNormalization.h"
#include "UI/Tabs.h"
#include "UI/Theme.h"
#include "UI/Window.h"
#include "UI/ScintillaTheme.h"

#if defined(NPP_UI_WITH_QT)

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDockWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScreen>
#include <QStatusBar>
#include <QString>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QWidget>

#include "Scintilla.h"
#include "SciLexer.h"
#include "ScintillaEdit.h"
#include "ILexer.h"
#include "Lexilla.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace npp::ui
{
    namespace
    {
        long toColorRef(const Color& color) noexcept
        {
            const auto clamp = [](float component) -> long {
                if (component <= 0.0F)
                {
                    return 0L;
                }
                if (component >= 1.0F)
                {
                    return 255L;
                }
                return static_cast<long>(component * 255.0F + 0.5F);
            };

            return clamp(color.red) | (clamp(color.green) << 8) | (clamp(color.blue) << 16);
        }

        void applyColorScheme(ScintillaEdit& editor, const scintilla::EditorColorScheme& scheme)
        {
            editor.send(SCI_BEGINUNDOACTION);

            editor.send(SCI_STYLESETFORE, STYLE_DEFAULT, toColorRef(scheme.foreground));
            editor.send(SCI_STYLESETBACK, STYLE_DEFAULT, toColorRef(scheme.background));
            editor.send(SCI_STYLECLEARALL);

            editor.send(SCI_STYLESETBACK, STYLE_LINENUMBER, toColorRef(scheme.gutterBackground));
            editor.send(SCI_STYLESETFORE, STYLE_LINENUMBER, toColorRef(scheme.gutterForeground));

            editor.send(SCI_SETSELFORE, 1, toColorRef(scheme.selectionForeground));
            editor.send(SCI_SETSELBACK, 1, toColorRef(scheme.selectionBackground));
            editor.send(SCI_SETSELALPHA, static_cast<int>(scheme.selectionBackground.alpha * 255.0F));

            editor.send(SCI_SETCARETFORE, toColorRef(scheme.caret));
            // Disable caret line highlighting - it makes text unreadable
            editor.send(SCI_SETCARETLINEVISIBLE, 0);

            editor.send(SCI_ENDUNDOACTION);
        }

        class QtScintillaController final : public ThemeObserver
        {
        public:
            QtScintillaController(ScintillaEdit& editor, ThemeProvider& provider)
                : editor_(editor), provider_(provider)
            {
                provider_.addObserver(*this);
                apply(provider_.palette());
            }

            QtScintillaController(const QtScintillaController&) = delete;
            QtScintillaController& operator=(const QtScintillaController&) = delete;

            ~QtScintillaController() override
            {
                provider_.removeObserver(*this);
            }

            void onThemeChanged(const ThemePalette& palette) override
            {
                apply(palette);
            }

            void updateZoom(float devicePixelRatio)
            {
                const auto metrics = scintilla::computeMetrics(devicePixelRatio);
                editor_.send(SCI_SETZOOM, metrics.zoomLevel);
            }

        private:
            void apply(const ThemePalette& palette)
            {
                const auto scheme = scintilla::deriveColorScheme(palette);
                applyColorScheme(editor_, scheme);
            }

            ScintillaEdit& editor_;
            ThemeProvider& provider_;
        };

        class QtCommand final : public Command
        {
        public:
            QtCommand(QObject* parent, CommandDescriptor descriptor)
                : id_(std::move(descriptor.identifier)),
                  title_(descriptor.title),
                  description_(descriptor.description),
                  checkable_(descriptor.checkable),
                  callback_(std::move(descriptor.action))
            {
                action_ = new QAction(QString::fromStdWString(title_), parent);
                action_->setCheckable(checkable_);

                QObject::connect(action_, &QAction::triggered, [this]() {
                    if (callback_)
                    {
                        callback_();
                    }
                });

                if (!descriptor.defaultShortcut.chords.empty())
                {
                    const auto& chord = descriptor.defaultShortcut.chords.front();
                    const auto key = QString::fromStdString(chord.key);
                    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
                    if (any(chord.modifiers & KeyModifier::Command))
                    {
                        modifiers |= Qt::MetaModifier;
                    }
                    if (any(chord.modifiers & KeyModifier::Option))
                    {
                        modifiers |= Qt::AltModifier;
                    }
                    if (any(chord.modifiers & KeyModifier::Control))
                    {
                        modifiers |= Qt::ControlModifier;
                    }
                    if (any(chord.modifiers & KeyModifier::Shift))
                    {
                        modifiers |= Qt::ShiftModifier;
                    }
                    // Qt 6: toCombined() removed, use operator[] instead
                    QKeySequence keySeq = QKeySequence::fromString(key);
                    action_->setShortcut(QKeySequence(modifiers | keySeq[0]));
                }
            }

            ~QtCommand() override = default;

            const CommandId& identifier() const noexcept override
            {
                return id_;
            }

            const UiString& title() const noexcept override
            {
                return title_;
            }

            const UiString& description() const noexcept override
            {
                return description_;
            }

            CommandState state() const noexcept override
            {
                return {action_->isEnabled(), action_->isChecked()};
            }

            void setState(const CommandState& newState) override
            {
                action_->setEnabled(newState.enabled);
                if (action_->isCheckable())
                {
                    action_->setChecked(newState.checked);
                }
            }

            void trigger() override
            {
                action_->trigger();
            }

            QAction* action() const noexcept
            {
                return action_;
            }

        private:
            CommandId id_;
            UiString title_;
            UiString description_;
            bool checkable_;
            std::function<void()> callback_;
            QAction* action_{};
        };

        class QtCommandRegistry final : public CommandRegistry
        {
        public:
            explicit QtCommandRegistry(QObject* parent)
                : parent_(parent)
            {
            }

            std::shared_ptr<Command> registerCommand(CommandDescriptor descriptor) override
            {
                auto command = std::make_shared<QtCommand>(parent_, std::move(descriptor));
                commands_[command->identifier()] = command;
                return command;
            }

            std::shared_ptr<Command> find(const CommandId& identifier) const noexcept override
            {
                const auto it = commands_.find(identifier);
                return it == commands_.end() ? nullptr : it->second;
            }

            void unregister(const CommandId& identifier) override
            {
                commands_.erase(identifier);
            }

            void clear() override
            {
                commands_.clear();
            }

            QAction* actionFor(const CommandId& identifier) const noexcept
            {
                const auto command = std::static_pointer_cast<QtCommand>(find(identifier));
                return command ? command->action() : nullptr;
            }

        private:
            QObject* parent_{};
            std::unordered_map<CommandId, std::shared_ptr<QtCommand>> commands_{};
        };

        class QtShortcutMap final : public ShortcutMap
        {
        public:
            explicit QtShortcutMap(QtCommandRegistry& registry)
                : registry_(registry)
            {
            }

            void bind(const CommandId& identifier, Shortcut shortcut) override
            {
                bindings_[identifier] = shortcut;
                apply(identifier, shortcut);
            }

            std::optional<Shortcut> lookup(const CommandId& identifier) const override
            {
                const auto it = bindings_.find(identifier);
                return it == bindings_.end() ? std::nullopt : std::optional<Shortcut>(it->second);
            }

            void remove(const CommandId& identifier) override
            {
                bindings_.erase(identifier);
                apply(identifier, std::nullopt);
            }

            void clear() override
            {
                for (const auto& [commandId, _] : bindings_)
                {
                    apply(commandId, std::nullopt);
                }
                bindings_.clear();
            }

            void importWindowsBinding(const CommandId& identifier, const std::wstring& windowsDefinition) override
            {
                Shortcut shortcut = normalizeWindowsShortcut({windowsDefinition});
                bind(identifier, std::move(shortcut));
            }

        private:
            static QKeySequence toKeySequence(const ShortcutChord& chord)
            {
                Qt::KeyboardModifiers modifiers = Qt::NoModifier;
                if (any(chord.modifiers & KeyModifier::Command))
                    modifiers |= Qt::MetaModifier;
                if (any(chord.modifiers & KeyModifier::Option))
                    modifiers |= Qt::AltModifier;
                if (any(chord.modifiers & KeyModifier::Control))
                    modifiers |= Qt::ControlModifier;
                if (any(chord.modifiers & KeyModifier::Shift))
                    modifiers |= Qt::ShiftModifier;

                const auto keySequence = QKeySequence(QString::fromStdString(chord.key));
                return QKeySequence(modifiers | keySequence[0]);
            }

            void apply(const CommandId& identifier, const std::optional<Shortcut>& shortcut)
            {
                QAction* action = registry_.actionFor(identifier);
                if (!action)
                {
                    return;
                }

                if (shortcut && !shortcut->chords.empty())
                {
                    action->setShortcut(toKeySequence(shortcut->chords.front()));
                }
                else
                {
                    action->setShortcut(QKeySequence());
                }
            }

            QtCommandRegistry& registry_;
            std::unordered_map<CommandId, Shortcut> bindings_{};
        };

        class QtThemeProvider final : public ThemeProvider
        {
        public:
            ThemePalette palette() const override
            {
                return palette_;
            }

            void addObserver(ThemeObserver& observer) override
            {
                observers_.push_back(&observer);
            }

            void removeObserver(ThemeObserver& observer) override
            {
                observers_.erase(std::remove(observers_.begin(), observers_.end(), &observer), observers_.end());
            }

            void setPalette(const ThemePalette& palette)
            {
                palette_ = palette;
                for (auto* observer : observers_)
                {
                    observer->onThemeChanged(palette_);
                }
            }

        private:
            ThemePalette palette_{ThemeVariant::Light, {0.1F, 0.45F, 0.9F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}, {0.12F, 0.12F, 0.12F, 1.0F}, {0.6F, 0.85F, 0.98F, 1.0F}};
            std::vector<ThemeObserver*> observers_{};
        };

        // Language/Lexer mapping
        struct LanguageInfo
        {
            const char* name;           // Display name
            const char* lexerName;      // Scintilla/Lexilla lexer name
            const char* extensions;     // File extensions (comma separated)
        };

        // Language database - supporting 44 most common languages
        static const std::vector<LanguageInfo> g_languages = {
            // Programming languages
            {"C++", "cpp", "cpp,cxx,cc,h,hpp,hxx,hh"},
            {"C", "cpp", "c"},
            {"C#", "cpp", "cs"},
            {"Java", "cpp", "java"},
            {"JavaScript", "cpp", "js,jsx,mjs"},
            {"TypeScript", "cpp", "ts,tsx"},
            {"Python", "python", "py,pyw"},
            {"Ruby", "ruby", "rb,rbw"},
            {"PHP", "phpscript", "php,php3,php4,php5,phtml"},
            {"Perl", "perl", "pl,pm,pod"},
            {"Go", "cpp", "go"},
            {"Rust", "rust", "rs"},
            {"Swift", "cpp", "swift"},
            {"Kotlin", "cpp", "kt,kts"},
            {"Objective-C", "cpp", "m,mm"},

            // Markup languages
            {"HTML", "html", "html,htm,shtml,shtm,xhtml"},
            {"XML", "xml", "xml,xsd,xsl,xslt,dtd"},
            {"JSON", "json", "json"},
            {"YAML", "yaml", "yaml,yml"},
            {"Markdown", "markdown", "md,markdown"},
            {"CSS", "css", "css"},
            {"SCSS", "css", "scss"},
            {"Less", "css", "less"},

            // Query languages
            {"SQL", "sql", "sql"},

            // Script languages
            {"Bash", "bash", "sh,bash,zsh,ksh"},
            {"PowerShell", "powershell", "ps1,psm1,psd1"},
            {"Batch", "batch", "bat,cmd,nt"},
            {"Lua", "lua", "lua"},
            {"TCL", "tcl", "tcl"},

            // Data/Config formats
            {"INI", "properties", "ini,inf,reg,cfg"},
            {"Properties", "properties", "properties"},
            {"TOML", "properties", "toml"},

            // Assembly
            {"Assembly", "asm", "asm,s"},

            // Fortran
            {"Fortran", "fortran", "f,for,f90,f95,f2k"},

            // Pascal
            {"Pascal", "pascal", "pas,pp,p,inc"},

            // Lisp/Scheme
            {"Lisp", "lisp", "lisp,lsp"},
            {"Scheme", "lisp", "scm,ss"},

            // LaTeX
            {"LaTeX", "latex", "tex,latex,sty"},

            // Diff
            {"Diff", "diff", "diff,patch"},

            // Makefile
            {"Makefile", "makefile", "mk,mak"},

            // CMake
            {"CMake", "cmake", "cmake"},

            // R
            {"R", "r", "r,R"},

            // MATLAB
            {"MATLAB", "matlab", "m"},

            // Plain text (no highlighting)
            {"Plain Text", "null", "txt"}
        };

        // Helper to detect language by file extension
        const LanguageInfo* detectLanguage(const QString& filePath)
        {
            QFileInfo fileInfo(filePath);
            QString ext = fileInfo.suffix().toLower();

            if (ext.isEmpty())
                return nullptr;

            for (const auto& lang : g_languages)
            {
                QStringList extensions = QString(lang.extensions).split(',');
                if (extensions.contains(ext))
                {
                    return &lang;
                }
            }

            return nullptr;
        }

        // Lexilla CreateLexer function (statically linked)
        extern "C" Scintilla::ILexer5* CreateLexer(const char* name);

        // Apply lexer to editor by lexer name
        void applyLexer(ScintillaEdit* editor, const char* lexerName)
        {
            if (!lexerName || std::strcmp(lexerName, "null") == 0)
            {
                // No lexer - plain text
                editor->send(SCI_SETILEXER, 0, 0);
                return;
            }

            // Create lexer using Lexilla (statically linked)
            Scintilla::ILexer5* lexer = CreateLexer(lexerName);
            if (!lexer)
            {
                // Lexer not found, fall back to plain text
                editor->send(SCI_SETILEXER, 0, 0);
                return;
            }

            // Set the lexer
            editor->send(SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(lexer));

            // Set up basic syntax highlighting styles
            // Note: Different lexers use different style numbers
            // For C-like languages (cpp, java, javascript, etc.):
            if (std::strcmp(lexerName, "cpp") == 0)
            {
                // C++ keywords
                const char* cppKeywords =
                    "alignas alignof and and_eq asm atomic_cancel atomic_commit atomic_noexcept auto bitand "
                    "bitor bool break case catch char char8_t char16_t char32_t class compl concept const "
                    "consteval constexpr constinit const_cast continue co_await co_return co_yield decltype "
                    "default delete do double dynamic_cast else enum explicit export extern false float for "
                    "friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator "
                    "or or_eq private protected public reflexpr register reinterpret_cast requires return "
                    "short signed sizeof static static_assert static_cast struct switch synchronized template "
                    "this thread_local throw true try typedef typeid typename union unsigned using virtual "
                    "void volatile wchar_t while xor xor_eq";
                editor->sends(SCI_SETKEYWORDS, 0, cppKeywords);

                // Style settings for C++
                editor->send(SCI_STYLESETFORE, SCE_C_COMMENT, 0x00008000);         // Green
                editor->send(SCI_STYLESETFORE, SCE_C_COMMENTLINE, 0x00008000);     // Green
                editor->send(SCI_STYLESETFORE, SCE_C_COMMENTDOC, 0x00008000);      // Green
                editor->send(SCI_STYLESETFORE, SCE_C_NUMBER, 0x000080FF);          // Orange
                editor->send(SCI_STYLESETFORE, SCE_C_WORD, 0x00FF0000);            // Blue
                editor->send(SCI_STYLESETBOLD, SCE_C_WORD, 1);
                editor->send(SCI_STYLESETFORE, SCE_C_STRING, 0x000000FF);          // Red
                editor->send(SCI_STYLESETFORE, SCE_C_CHARACTER, 0x000000FF);       // Red
                editor->send(SCI_STYLESETFORE, SCE_C_PREPROCESSOR, 0x00808080);    // Gray
                editor->send(SCI_STYLESETFORE, SCE_C_OPERATOR, 0x00000080);        // Dark red
            }
            else if (std::strcmp(lexerName, "python") == 0)
            {
                // Python keywords
                const char* pythonKeywords =
                    "and as assert async await break class continue def del elif else except "
                    "False finally for from global if import in is lambda None nonlocal not "
                    "or pass raise return True try while with yield";
                editor->sends(SCI_SETKEYWORDS, 0, pythonKeywords);

                // Style settings for Python
                editor->send(SCI_STYLESETFORE, SCE_P_COMMENTLINE, 0x00008000);     // Green
                editor->send(SCI_STYLESETFORE, SCE_P_NUMBER, 0x000080FF);          // Orange
                editor->send(SCI_STYLESETFORE, SCE_P_STRING, 0x000000FF);          // Red
                editor->send(SCI_STYLESETFORE, SCE_P_CHARACTER, 0x000000FF);       // Red
                editor->send(SCI_STYLESETFORE, SCE_P_WORD, 0x00FF0000);            // Blue
                editor->send(SCI_STYLESETBOLD, SCE_P_WORD, 1);
                editor->send(SCI_STYLESETFORE, SCE_P_TRIPLE, 0x000000FF);          // Red
                editor->send(SCI_STYLESETFORE, SCE_P_TRIPLEDOUBLE, 0x000000FF);    // Red
                editor->send(SCI_STYLESETFORE, SCE_P_CLASSNAME, 0x000000A0);       // Dark blue
                editor->send(SCI_STYLESETBOLD, SCE_P_CLASSNAME, 1);
                editor->send(SCI_STYLESETFORE, SCE_P_DEFNAME, 0x00007F7F);         // Teal
                editor->send(SCI_STYLESETBOLD, SCE_P_DEFNAME, 1);
            }
            // Add more language-specific styles as needed

            // Force document to be re-lexed
            editor->send(SCI_COLOURISE, 0, -1);
        }

        // XML Pretty Print formatter
        QString formatXML(const QString& xml)
        {
            QString formatted;
            int indent = 0;
            bool inTag = false;
            bool inClosingTag = false;
            QString currentLine;

            for (int i = 0; i < xml.length(); ++i)
            {
                QChar c = xml[i];

                if (c == '<')
                {
                    // Check if it's a closing tag
                    if (i + 1 < xml.length() && xml[i + 1] == '/')
                    {
                        inClosingTag = true;
                        --indent;
                        if (!currentLine.trimmed().isEmpty())
                        {
                            formatted += QString(indent * 2, ' ') + currentLine.trimmed() + '\n';
                            currentLine.clear();
                        }
                    }
                    else if (!currentLine.trimmed().isEmpty())
                    {
                        formatted += QString(indent * 2, ' ') + currentLine.trimmed() + '\n';
                        currentLine.clear();
                    }

                    currentLine += c;
                    inTag = true;
                }
                else if (c == '>')
                {
                    currentLine += c;
                    inTag = false;

                    formatted += QString(indent * 2, ' ') + currentLine.trimmed() + '\n';
                    currentLine.clear();

                    // Check if it's a self-closing tag
                    if (i > 0 && xml[i - 1] == '/')
                    {
                        // Self-closing tag, don't change indent
                    }
                    else if (inClosingTag)
                    {
                        inClosingTag = false;
                    }
                    else
                    {
                        ++indent;
                    }
                }
                else if (!c.isSpace() || inTag)
                {
                    currentLine += c;
                }
                else if (!currentLine.trimmed().isEmpty())
                {
                    currentLine += c;
                }
            }

            if (!currentLine.trimmed().isEmpty())
            {
                formatted += QString(indent * 2, ' ') + currentLine.trimmed() + '\n';
            }

            return formatted;
        }

        // JSON Pretty Print formatter
        QString formatJSON(const QString& json)
        {
            QString formatted;
            int indent = 0;
            bool inString = false;
            bool escape = false;

            for (int i = 0; i < json.length(); ++i)
            {
                QChar c = json[i];

                if (escape)
                {
                    formatted += c;
                    escape = false;
                    continue;
                }

                if (c == '\\' && inString)
                {
                    formatted += c;
                    escape = true;
                    continue;
                }

                if (c == '"')
                {
                    formatted += c;
                    inString = !inString;
                    continue;
                }

                if (inString)
                {
                    formatted += c;
                    continue;
                }

                // Not in string - handle formatting
                if (c == '{' || c == '[')
                {
                    formatted += c;
                    formatted += '\n';
                    ++indent;
                    formatted += QString(indent * 2, ' ');
                }
                else if (c == '}' || c == ']')
                {
                    formatted += '\n';
                    --indent;
                    formatted += QString(indent * 2, ' ');
                    formatted += c;
                }
                else if (c == ',')
                {
                    formatted += c;
                    formatted += '\n';
                    formatted += QString(indent * 2, ' ');
                }
                else if (c == ':')
                {
                    formatted += c;
                    formatted += ' ';
                }
                else if (!c.isSpace())
                {
                    formatted += c;
                }
            }

            return formatted;
        }

        // Document structure for tab management
        struct Document
        {
            TabIdentifier identifier;
            QString filePath;
            ScintillaEdit* editor;
            bool isModified;
            QString language;  // Current language (for syntax highlighting)

            Document(const TabIdentifier& id, ScintillaEdit* ed)
                : identifier(id), editor(ed), isModified(false), language("Plain Text")
            {
            }
        };

        class QtTabManager final : public TabManager
        {
        public:
            QtTabManager(QTabWidget& tabWidget, QMainWindow& window,
                        std::shared_ptr<std::vector<Document>> documents)
                : tabWidget_(tabWidget), window_(window), eventSink_(nullptr),
                  documents_(documents)
            {
                // Configure tab widget
                tabWidget_.setTabsClosable(true);
                tabWidget_.setMovable(true);
                tabWidget_.setDocumentMode(true);

                // Connect signals
                QObject::connect(&tabWidget_, &QTabWidget::currentChanged,
                    [this](int index) {
                        if (index >= 0 && eventSink_) {
                            auto id = getTabIdAt(index);
                            if (id.has_value()) {
                                eventSink_->onSelected(TabSelectionEvent{*id});
                            }
                        }
                    });

                // Handle tab close button (X)
                QObject::connect(&tabWidget_, &QTabWidget::tabCloseRequested,
                    [this](int index) {
                        handleTabClose(index);
                    });

                QObject::connect(tabWidget_.tabBar(), &QTabBar::tabMoved,
                    [this](int from, int to) {
                        if (from < static_cast<int>(documents_->size()) && to < static_cast<int>(documents_->size())) {
                            // Update internal documents vector
                            auto doc = (*documents_)[from];
                            documents_->erase(documents_->begin() + from);
                            documents_->insert(documents_->begin() + to, doc);

                            // Update tabs_ if using TabManager interface
                            if (eventSink_ && from < static_cast<int>(tabs_.size())) {
                                auto id = tabs_[from].identifier;
                                eventSink_->onDragged(id, static_cast<std::uint32_t>(to));
                                auto tab = tabs_[from];
                                tabs_.erase(tabs_.begin() + from);
                                tabs_.insert(tabs_.begin() + to, tab);
                            }
                        }
                    });
            }

            void handleTabClose(int index)
            {
                if (index < 0) return;

                // Check for unsaved changes
                // TODO: Check modification state of tab being closed
                // if (documents_[index].isModified) { ... show dialog ... }

                // If this is the last tab, prevent closing
                if (tabWidget_.count() == 1) {
                    QMessageBox::information(&window_, "Cannot Close",
                        "Cannot close the last tab. Use File → Quit to exit the application.");
                    return;
                }

                // Remove from documents list
                if (index < static_cast<int>(documents_->size())) {
                    documents_->erase(documents_->begin() + index);
                }

                // Remove from tabs_ list if exists
                if (index < static_cast<int>(tabs_.size())) {
                    tabs_.erase(tabs_.begin() + index);
                }

                // Remove tab (this will delete the editor widget)
                tabWidget_.removeTab(index);
            }

            void setEventSink(TabEventSink* sink) override
            {
                eventSink_ = sink;
            }

            void setTabs(std::vector<TabDescriptor> tabs, std::optional<TabIdentifier> active) override
            {
                tabs_ = std::move(tabs);
                rebuildTabs();
                if (active) {
                    setActive(*active);
                }
            }

            void updateMetadata(const TabIdentifier& identifier, const TabMetadata& metadata) override
            {
                for (size_t i = 0; i < tabs_.size(); ++i) {
                    if (tabs_[i].identifier == identifier) {
                        tabs_[i].metadata = metadata;
                        updateTabAt(i);
                        break;
                    }
                }
            }

            void setActive(const TabIdentifier& identifier) override
            {
                for (size_t i = 0; i < tabs_.size(); ++i) {
                    if (tabs_[i].identifier == identifier) {
                        tabWidget_.setCurrentIndex(static_cast<int>(i));
                        break;
                    }
                }
            }

            void remove(const TabIdentifier& identifier) override
            {
                for (size_t i = 0; i < tabs_.size(); ++i) {
                    if (tabs_[i].identifier == identifier) {
                        tabWidget_.removeTab(static_cast<int>(i));
                        tabs_.erase(tabs_.begin() + i);
                        break;
                    }
                }
            }

            void move(const TabIdentifier& identifier, std::uint32_t newIndex) override
            {
                for (size_t i = 0; i < tabs_.size(); ++i) {
                    if (tabs_[i].identifier == identifier) {
                        tabWidget_.tabBar()->moveTab(static_cast<int>(i), static_cast<int>(newIndex));
                        auto tab = tabs_[i];
                        tabs_.erase(tabs_.begin() + i);
                        tabs_.insert(tabs_.begin() + newIndex, tab);
                        break;
                    }
                }
            }

            std::uint32_t count() const noexcept override
            {
                return static_cast<std::uint32_t>(tabs_.size());
            }

        private:
            void rebuildTabs()
            {
                // This would be called to rebuild all tabs, but since we're managing
                // tabs dynamically, this is mainly for initialization
                tabWidget_.clear();
                for (size_t i = 0; i < tabs_.size(); ++i) {
                    // Tab widgets are added when documents are created
                }
            }

            void updateTabAt(size_t index)
            {
                if (index >= tabs_.size()) return;

                const auto& tab = tabs_[index];
                QString title = QString::fromStdWString(tab.metadata.title);

                if (tab.metadata.dirty) {
                    title = "• " + title;  // Bullet point for modified files
                }

                tabWidget_.setTabText(static_cast<int>(index), title);

                if (!tab.metadata.tooltip.empty()) {
                    tabWidget_.setTabToolTip(static_cast<int>(index),
                        QString::fromStdWString(tab.metadata.tooltip));
                }
            }

            std::optional<TabIdentifier> getTabIdAt(int index) const
            {
                if (index >= 0 && index < static_cast<int>(tabs_.size())) {
                    return tabs_[index].identifier;
                }
                return std::nullopt;
            }

            QTabWidget& tabWidget_;
            QMainWindow& window_;
            TabEventSink* eventSink_;
            std::vector<TabDescriptor> tabs_;
            std::shared_ptr<std::vector<Document>> documents_;
        };

        class QtMenuModel final : public MenuModel
        {
        public:
            QtMenuModel(QMainWindow& window, QtCommandRegistry& registry)
                : window_(window), registry_(registry)
            {
            }

            void setItems(std::vector<MenuItemDescriptor> items) override
            {
                items_ = std::move(items);
                rebuild();
            }

            void updateState(const MenuId& identifier, bool enabled, bool checked) override
            {
                if (auto* action = registry_.actionFor(identifier))
                {
                    action->setEnabled(enabled);
                    if (action->isCheckable())
                    {
                        action->setChecked(checked);
                    }
                }
            }

            void setApplicationMenu(const MenuId& identifier) override
            {
                applicationMenuId_ = identifier;
                rebuild();
            }

            void rebuild() override
            {
                auto* menuBar = window_.menuBar();
                menuBar->clear();

                // Enable native menu bar for macOS (menu in top bar, not window)
                menuBar->setNativeMenuBar(true);

                for (const auto& item : items_)
                {
                    if (item.type != MenuItemType::Submenu)
                    {
                        continue;
                    }

                    QMenu* menu = menuBar->addMenu(QString::fromStdWString(item.title));
                    buildMenu(*menu, item.children);
                }
            }

        private:
            void buildMenu(QMenu& menu, const std::vector<MenuItemDescriptor>& items)
            {
                for (const auto& descriptor : items)
                {
                    switch (descriptor.type)
                    {
                        case MenuItemType::Separator:
                            menu.addSeparator();
                            break;
                        case MenuItemType::Submenu:
                        {
                            QMenu* submenu = menu.addMenu(QString::fromStdWString(descriptor.title));
                            buildMenu(*submenu, descriptor.children);
                            break;
                        }
                        case MenuItemType::Action:
                        default:
                        {
                            QAction* action = nullptr;
                            if (descriptor.command)
                            {
                                action = registry_.actionFor(*descriptor.command);
                            }

                            if (!action)
                            {
                                action = menu.addAction(QString::fromStdWString(descriptor.title));
                                action->setEnabled(false);
                            }
                            else
                            {
                                action->setText(QString::fromStdWString(descriptor.title));
                                menu.addAction(action);
                            }
                            break;
                        }
                    }
                }
            }

            QMainWindow& window_;
            QtCommandRegistry& registry_;
            std::vector<MenuItemDescriptor> items_{};
            MenuId applicationMenuId_{};
        };

        class QtDockingHost final : public DockingHost
        {
        public:
            explicit QtDockingHost(QMainWindow& window)
                : window_(window)
            {
            }

            void registerPanel(DockablePanelDescriptor descriptor) override
            {
                auto dockWidget = std::make_unique<QDockWidget>(QString::fromStdWString(descriptor.title), &window_);
                dockWidget->setObjectName(QString::fromStdString(descriptor.identifier));
                dockWidget->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

                auto* list = new QListWidget();
                list->addItem("Outline Placeholder");
                list->addItem("Functions");
                list->addItem("TODO: Integrate actual Function List");
                dockWidget->setWidget(list);

                Qt::DockWidgetArea area = Qt::RightDockWidgetArea;
                switch (descriptor.preferredArea)
                {
                    case DockArea::Left:
                        area = Qt::LeftDockWidgetArea;
                        break;
                    case DockArea::Top:
                        area = Qt::TopDockWidgetArea;
                        break;
                    case DockArea::Bottom:
                        area = Qt::BottomDockWidgetArea;
                        break;
                    case DockArea::Floating:
                        dockWidget->setFloating(true);
                        dockWidget->setVisible(descriptor.initiallyVisible);
                        break;
                    case DockArea::Right:
                    default:
                        area = Qt::RightDockWidgetArea;
                        break;
                }

                window_.addDockWidget(area, dockWidget.get());
                dockWidget->setVisible(descriptor.initiallyVisible);

                panels_[descriptor.identifier] = std::move(dockWidget);
            }

            void unregisterPanel(const PanelId& identifier) override
            {
                auto it = panels_.find(identifier);
                if (it != panels_.end())
                {
                    it->second->hide();
                    it->second->deleteLater();
                    panels_.erase(it);
                }
                // adapters_ not defined in this class - possibly removed or not needed
                // adapters_.erase(identifier);
            }

            void setLayout(const DockLayoutSnapshot& snapshot) override
            {
                if (!snapshot.encodedState.empty())
                {
                    window_.restoreState(QByteArray::fromStdString(snapshot.encodedState));
                }
            }

            DockLayoutSnapshot captureLayout() const override
            {
                DockLayoutSnapshot snapshot;
                snapshot.encodedState = window_.saveState().toStdString();
                snapshot.description = L"Qt main window docking layout";
                snapshot.variant = ThemeVariant::Light;
                return snapshot;
            }

            void showPanel(const PanelId& identifier) override
            {
                if (auto it = panels_.find(identifier); it != panels_.end())
                {
                    it->second->show();
                    it->second->raise();
                }
            }

            void hidePanel(const PanelId& identifier) override
            {
                if (auto it = panels_.find(identifier); it != panels_.end())
                {
                    it->second->hide();
                }
            }

            bool isPanelVisible(const PanelId& identifier) const override
            {
                if (auto it = panels_.find(identifier); it != panels_.end())
                {
                    return it->second->isVisible();
                }
                return false;
            }

        private:
            QMainWindow& window_;
            std::map<PanelId, std::unique_ptr<QDockWidget>> panels_{};
        };

        class QtWindow final : public Window
        {
        public:
            explicit QtWindow(std::unique_ptr<QMainWindow> window)
                : window_(std::move(window))
            {
            }

            void show() override
            {
                window_->show();
                window_->raise();
                window_->activateWindow();
            }

            void hide() override
            {
                window_->hide();
            }

            void requestUserAttention() override
            {
                QApplication::alert(window_.get());
            }

            void setTitle(const UiString& title) override
            {
                window_->setWindowTitle(QString::fromStdWString(title));
            }

            UiString title() const override
            {
                return window_->windowTitle().toStdWString();
            }

            void setEventHandlers(WindowEventHandlers handlers) override
            {
                handlers_ = std::move(handlers);
            }

            void setDockingHost(std::shared_ptr<DockingHost> host) override
            {
                dockingHost_ = std::move(host);
            }

            DockingHost* dockingHost() const noexcept override
            {
                return dockingHost_.get();
            }

            void setMinimumContentSize(std::uint32_t width, std::uint32_t height) override
            {
                window_->setMinimumSize(static_cast<int>(width), static_cast<int>(height));
            }

            QMainWindow& native() noexcept
            {
                return *window_;
            }

        private:
            std::unique_ptr<QMainWindow> window_;
            std::shared_ptr<DockingHost> dockingHost_{};
            WindowEventHandlers handlers_{};
        };

        class QtApplication final : public Application
        {
        public:
            QtApplication(std::unique_ptr<QApplication> app,
                          std::unique_ptr<QtWindow> mainWindow,
                          std::unique_ptr<QtCommandRegistry> commandRegistry,
                          std::unique_ptr<QtMenuModel> menuModel,
                          std::unique_ptr<QtShortcutMap> shortcutMap,
                          std::unique_ptr<QtThemeProvider> themeProvider,
                          std::shared_ptr<DockingHost> dockingHost,
                          std::unique_ptr<QtScintillaController> editorController)
                : app_(std::move(app)),
                  mainWindow_(std::move(mainWindow)),
                  commandRegistry_(std::move(commandRegistry)),
                  menuModel_(std::move(menuModel)),
                  shortcutMap_(std::move(shortcutMap)),
                  themeProvider_(std::move(themeProvider)),
                  scintillaController_(std::move(editorController))
            {
                mainWindow_->setDockingHost(std::move(dockingHost));
            }

            Window& mainWindow() noexcept override
            {
                return *mainWindow_;
            }

            CommandRegistry& commands() noexcept override
            {
                return *commandRegistry_;
            }

            MenuModel& menuModel() noexcept override
            {
                return *menuModel_;
            }

            ShortcutMap& shortcuts() noexcept override
            {
                return *shortcutMap_;
            }

            ThemeProvider& theme() noexcept override
            {
                return *themeProvider_;
            }

            void integrateDockingHost(std::shared_ptr<DockingHost> host) override
            {
                mainWindow_->setDockingHost(std::move(host));
            }

            void dispatchPendingEvents() override
            {
                app_->processEvents();
            }

            int run() override
            {
                mainWindow_->show();
                return app_->exec();
            }

        private:
            std::unique_ptr<QApplication> app_;
            std::unique_ptr<QtWindow> mainWindow_;
            std::unique_ptr<QtCommandRegistry> commandRegistry_;
            std::unique_ptr<QtMenuModel> menuModel_;
            std::unique_ptr<QtShortcutMap> shortcutMap_;
            std::unique_ptr<QtThemeProvider> themeProvider_;
            std::unique_ptr<QtScintillaController> scintillaController_;
        };

        ThemePalette toPalette(const ThemePalette& palette)
        {
            return palette;
        }
    }

    void registerQtBackend()
    {
        Factory::registerBackend([](const ApplicationConfig& config) -> BootstrapResult {
            int argc = 0;
            char* argv[] = {nullptr};

            // IMPORTANT: Ensure native menu bar is used on macOS
            // This MUST be set BEFORE QApplication is created
            QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar, false);

            auto qtApp = std::make_unique<QApplication>(argc, argv);
            qtApp->setApplicationName(QString::fromStdWString(config.applicationName));
            qtApp->setOrganizationName(QString::fromStdWString(config.organizationName));

            auto nativeWindow = std::make_unique<QMainWindow>();
            nativeWindow->setObjectName("npp.mac.preview");
            nativeWindow->resize(1100, 720);
            nativeWindow->setWindowTitle(QString::fromStdWString(config.applicationName));

            // IMPORTANT: Set native menu bar BEFORE creating menu
            // This ensures menu appears ONLY in macOS top bar, not in window
            nativeWindow->menuBar()->setNativeMenuBar(true);

            // macOS apps typically don't have toolbars in the window
            // All actions should be accessed via menu bar or keyboard shortcuts

            // Create status bar with labels
            QStatusBar* statusBar = nativeWindow->statusBar();
            QLabel* statusMessage = new QLabel("Ready");
            QLabel* cursorPosLabel = new QLabel("Line: 1, Col: 1");
            QLabel* fileInfoLabel = new QLabel("UTF-8 | LF");

            statusBar->addWidget(statusMessage, 1); // Stretch
            statusBar->addPermanentWidget(cursorPosLabel);
            statusBar->addPermanentWidget(fileInfoLabel);

            auto commandRegistry = std::make_unique<QtCommandRegistry>(nativeWindow.get());
            auto shortcutMap = std::make_unique<QtShortcutMap>(*commandRegistry);
            auto menuModel = std::make_unique<QtMenuModel>(*nativeWindow, *commandRegistry);
            auto themeProvider = std::make_unique<QtThemeProvider>();
            auto dockingHost = std::make_shared<QtDockingHost>(*nativeWindow);

            // Create tab widget for multi-document interface
            auto* tabWidget = new QTabWidget(nativeWindow.get());
            tabWidget->setObjectName("main.tabs");
            nativeWindow->setCentralWidget(tabWidget);

            // Document management - list of all open documents
            auto documents = std::make_shared<std::vector<Document>>();
            auto currentDocIndex = std::make_shared<int>(0);

            // Create tab manager (passing documents for tab close handling)
            auto tabManager = std::make_unique<QtTabManager>(*tabWidget, *nativeWindow, documents);

            // Helper function to create a new editor widget
            auto createEditor = [themeProvider = themeProvider.get()]() {
                auto* editor = new ScintillaEdit();
                editor->send(SCI_SETCODEPAGE, SC_CP_UTF8);
                editor->send(SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DEFAULT);
                editor->send(SCI_SETBUFFEREDDRAW, 1);
                editor->send(SCI_SETCARETSTYLE, CARETSTYLE_LINE);
                editor->send(SCI_SETMOUSEDWELLTIME, 350);
                editor->send(SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION);
                editor->send(SCI_SETMULTIPLESELECTION, 1);
                editor->send(SCI_SETADDITIONALSELECTIONTYPING, 1);
                editor->send(SCI_SETSCROLLWIDTHTRACKING, 1);

                // Font setup
                QFont editorFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
                const QByteArray fontNameUtf8 = editorFont.family().toUtf8();
                editor->sends(SCI_STYLESETFONT, STYLE_DEFAULT, fontNameUtf8.constData());
                editor->send(SCI_STYLESETSIZEFRACTIONAL, STYLE_DEFAULT,
                           static_cast<int>(editorFont.pointSizeF() * 100.0));
                editor->send(SCI_STYLECLEARALL);

                // Color scheme
                const auto scheme = scintilla::deriveColorScheme(themeProvider->palette());
                applyColorScheme(*editor, scheme);

                // Line numbers
                const float devicePixelRatio = QGuiApplication::primaryScreen() ?
                    static_cast<float>(QGuiApplication::primaryScreen()->devicePixelRatio()) : 1.0F;
                editor->send(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
                editor->send(SCI_SETMARGINWIDTHN, 0, 48);

                return editor;
            };

            // Create first document/tab
            auto* centralEditor = createEditor();
            centralEditor->setObjectName("central.editor");

            const char* const sample =
                "// Liquid Glass Mac preview\n"
                "#include <iostream>\n\n"
                "int main() {\n"
                "    std::cout << \"Hello, Scintilla on macOS!\" << std::endl;\n"
                "    return 0;\n"
                "}\n";
            centralEditor->sends(SCI_SETTEXT, 0, sample);

            // Add first tab
            TabIdentifier firstTabId{"tab_0"};
            documents->push_back(Document(firstTabId, centralEditor));
            tabWidget->addTab(centralEditor, "new 1");

            // Store default font size for reset (shared with zoom commands)
            QFont editorFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
            auto defaultFontSize = std::make_shared<float>(editorFont.pointSizeF());

            // Helper to get current editor
            auto getCurrentEditor = [tabWidget, documents]() -> ScintillaEdit* {
                int index = tabWidget->currentIndex();
                if (index >= 0 && index < static_cast<int>(documents->size())) {
                    return (*documents)[index].editor;
                }
                return nullptr;
            };

            // Counter for new tabs
            auto nextTabNumber = std::make_shared<int>(2);

            // Scintilla controller for first editor (theme observer)
            auto editorController = std::make_unique<QtScintillaController>(*centralEditor, *themeProvider);
            const float devicePixelRatio = QGuiApplication::primaryScreen() ?
                static_cast<float>(QGuiApplication::primaryScreen()->devicePixelRatio()) : 1.0F;
            editorController->updateZoom(devicePixelRatio);

            dockingHost->registerPanel({"function_list",
                                        L"Function List",
                                        std::nullopt,
                                        true,
                                        DockArea::Right,
                                        {}});

            // Store current file path and modification state
            // Note: We use raw pointers here because centralEditor is owned by Qt's parent-child system
            // editorPtr now gets current editor from tab widget
            // Note: Commands that need the current editor will call getCurrentEditor()
            // For initialization, use centralEditor (first tab)
            ScintillaEdit* editorPtr = centralEditor;
            QMainWindow* windowPtr = nativeWindow.get();

            // Shared state for file operations (wrapped in shared_ptr for lambda capture)
            auto currentFilePath = std::make_shared<QString>();
            auto isModified = std::make_shared<bool>(false);

            // Helper to check unsaved changes
            auto checkUnsavedChanges = [windowPtr, isModified]() -> bool {
                if (*isModified)
                {
                    auto reply = QMessageBox::question(
                        windowPtr,
                        "Unsaved Changes",
                        "Do you want to save changes to the current document?",
                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                        QMessageBox::Save
                    );

                    if (reply == QMessageBox::Cancel)
                    {
                        return false; // Cancel operation
                    }
                    else if (reply == QMessageBox::Save)
                    {
                        // Save will be handled by caller
                        return true; // Continue but save first
                    }
                    // Discard - continue without saving
                }
                return true; // No changes or discarded
            };

            // ========================================
            // TAB MANAGEMENT COMMANDS
            // ========================================

            // New Tab - Create a new empty tab
            const auto newTabCommand = commandRegistry->registerCommand({
                "cmd.tab.new",
                L"New Tab",
                L"Create a new tab",
                false,
                {},
                [tabWidget, documents, nextTabNumber, createEditor, windowPtr]() {
                    // Create new editor
                    auto* newEditor = createEditor();

                    // Generate tab ID and title
                    QString tabTitle = QString("new %1").arg(*nextTabNumber);
                    TabIdentifier tabId{QString("tab_%1").arg(documents->size()).toStdString()};

                    // Add to documents list
                    documents->push_back(Document(tabId, newEditor));

                    // Add tab to widget
                    int newIndex = tabWidget->addTab(newEditor, tabTitle);
                    tabWidget->setCurrentIndex(newIndex);

                    // Increment counter
                    (*nextTabNumber)++;

                    // Focus the new editor
                    newEditor->setFocus(Qt::OtherFocusReason);
                }
            });
            const CommandId newTabCommandId = newTabCommand->identifier();

            // Close Tab - Close the current tab
            const auto closeTabCommand = commandRegistry->registerCommand({
                "cmd.tab.close",
                L"Close Tab",
                L"Close the current tab",
                false,
                {},
                [tabWidget, documents, windowPtr, checkUnsavedChanges]() {
                    int currentIndex = tabWidget->currentIndex();
                    if (currentIndex < 0) return;

                    // Check for unsaved changes
                    // TODO: Check modification state of current tab
                    // if (!checkUnsavedChanges()) return;

                    // If this is the last tab, create a new one first
                    if (tabWidget->count() == 1) {
                        QMessageBox::information(windowPtr, "Cannot Close",
                            "Cannot close the last tab. Use File → Quit to exit the application.");
                        return;
                    }

                    // Remove from documents list
                    if (currentIndex < static_cast<int>(documents->size())) {
                        documents->erase(documents->begin() + currentIndex);
                    }

                    // Remove tab (this will delete the editor widget)
                    tabWidget->removeTab(currentIndex);
                }
            });
            const CommandId closeTabCommandId = closeTabCommand->identifier();

            // Next Tab - Switch to next tab
            const auto nextTabCommand = commandRegistry->registerCommand({
                "cmd.tab.next",
                L"Next Tab",
                L"Switch to the next tab",
                false,
                {},
                [tabWidget]() {
                    int count = tabWidget->count();
                    if (count <= 1) return;

                    int currentIndex = tabWidget->currentIndex();
                    int nextIndex = (currentIndex + 1) % count;
                    tabWidget->setCurrentIndex(nextIndex);
                }
            });
            const CommandId nextTabCommandId = nextTabCommand->identifier();

            // Previous Tab - Switch to previous tab
            const auto previousTabCommand = commandRegistry->registerCommand({
                "cmd.tab.previous",
                L"Previous Tab",
                L"Switch to the previous tab",
                false,
                {},
                [tabWidget]() {
                    int count = tabWidget->count();
                    if (count <= 1) return;

                    int currentIndex = tabWidget->currentIndex();
                    int prevIndex = (currentIndex - 1 + count) % count;
                    tabWidget->setCurrentIndex(prevIndex);
                }
            });
            const CommandId previousTabCommandId = previousTabCommand->identifier();

            // Go to Tab 1-9 commands
            std::vector<CommandId> goToTabCommandIds;
            for (int i = 1; i <= 9; ++i) {
                QString titleStr = QString("Go to Tab %1").arg(i);
                QString descStr = QString("Switch to tab %1").arg(i);
                auto goToTabCmd = commandRegistry->registerCommand({
                    QString("cmd.tab.goto_%1").arg(i).toStdString(),
                    titleStr.toStdWString(),
                    descStr.toStdWString(),
                    false,
                    {},
                    [tabWidget, i]() {
                        int targetIndex = i - 1;
                        if (targetIndex < tabWidget->count()) {
                            tabWidget->setCurrentIndex(targetIndex);
                        }
                    }
                });
                goToTabCommandIds.push_back(goToTabCmd->identifier());
            }

            // ========================================
            // FILE MANAGEMENT COMMANDS
            // ========================================

            const auto newDocumentCommand = commandRegistry->registerCommand({
                "cmd.file.new",
                L"New",
                L"Create a new document",
                false,
                {},
                [editorPtr, currentFilePath, windowPtr, isModified, checkUnsavedChanges]() {
                    if (!checkUnsavedChanges())
                        return;

                    editorPtr->send(SCI_CLEARALL);
                    currentFilePath->clear();
                    *isModified = false;
                    windowPtr->setWindowTitle("Notepad++");
                }
            });
            const CommandId newDocumentCommandId = newDocumentCommand->identifier();

            const auto openFileCommand = commandRegistry->registerCommand({
                "cmd.file.open",
                L"Open...",
                L"Open an existing file",
                false,
                {},
                [editorPtr, windowPtr, currentFilePath, isModified, checkUnsavedChanges, documents, tabWidget]() {
                    if (!checkUnsavedChanges())
                        return;

                    QString fileName = QFileDialog::getOpenFileName(
                        windowPtr,
                        "Open File",
                        QString(),
                        "All Files (*);;Text Files (*.txt);;C++ Files (*.cpp *.h *.hpp *.cxx);;Python Files (*.py);;JavaScript Files (*.js *.json);;HTML Files (*.html *.htm);;XML Files (*.xml)"
                    );

                    if (!fileName.isEmpty())
                    {
                        QFile file(fileName);
                        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
                        {
                            QByteArray content = file.readAll();
                            editorPtr->send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(content.constData()));
                            *currentFilePath = fileName;
                            *isModified = false;
                            windowPtr->setWindowTitle(QString("Notepad++ - %1").arg(QFileInfo(fileName).fileName()));
                            file.close();

                            // Auto-detect language and apply lexer
                            if (const auto* lang = detectLanguage(fileName))
                            {
                                applyLexer(editorPtr, lang->lexerName);

                                // Update document language in documents list
                                int currentIndex = tabWidget->currentIndex();
                                if (currentIndex >= 0 && currentIndex < static_cast<int>(documents->size()))
                                {
                                    (*documents)[currentIndex].language = QString(lang->name);
                                }
                            }
                        }
                        else
                        {
                            QMessageBox::critical(windowPtr, "Error", "Failed to open file: " + fileName);
                        }
                    }
                }
            });
            const CommandId openFileCommandId = openFileCommand->identifier();

            const auto saveFileCommand = commandRegistry->registerCommand({
                "cmd.file.save",
                L"Save",
                L"Save the current file",
                false,
                {},
                [editorPtr, windowPtr, currentFilePath, isModified]() {
                    if (currentFilePath->isEmpty())
                    {
                        // Trigger Save As if no file path
                        QString fileName = QFileDialog::getSaveFileName(
                            windowPtr,
                            "Save File",
                            QString(),
                            "All Files (*);;Text Files (*.txt);;C++ Files (*.cpp *.h);;Python Files (*.py)"
                        );

                        if (fileName.isEmpty())
                            return;

                        *currentFilePath = fileName;
                    }

                    QFile file(*currentFilePath);
                    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
                    {
                        auto length = editorPtr->send(SCI_GETLENGTH);
                        std::vector<char> buffer(length + 1);
                        editorPtr->send(SCI_GETTEXT, length + 1, reinterpret_cast<sptr_t>(buffer.data()));

                        QTextStream out(&file);
                        out << buffer.data();
                        file.close();

                        *isModified = false;
                        windowPtr->setWindowTitle(QString("Notepad++ - %1").arg(QFileInfo(*currentFilePath).fileName()));
                    }
                    else
                    {
                        QMessageBox::critical(windowPtr, "Error", "Failed to save file: " + *currentFilePath);
                    }
                }
            });
            const CommandId saveFileCommandId = saveFileCommand->identifier();

            const auto saveAsFileCommand = commandRegistry->registerCommand({
                "cmd.file.save_as",
                L"Save As...",
                L"Save the current file with a new name",
                false,
                {},
                [editorPtr, windowPtr, currentFilePath, isModified]() {
                    QString fileName = QFileDialog::getSaveFileName(
                        windowPtr,
                        "Save File As",
                        currentFilePath->isEmpty() ? QString() : *currentFilePath,
                        "All Files (*);;Text Files (*.txt);;C++ Files (*.cpp *.h);;Python Files (*.py);;JavaScript Files (*.js);;HTML Files (*.html);;XML Files (*.xml)"
                    );

                    if (fileName.isEmpty())
                        return;

                    QFile file(fileName);
                    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
                    {
                        auto length = editorPtr->send(SCI_GETLENGTH);
                        std::vector<char> buffer(length + 1);
                        editorPtr->send(SCI_GETTEXT, length + 1, reinterpret_cast<sptr_t>(buffer.data()));

                        QTextStream out(&file);
                        out << buffer.data();
                        file.close();

                        *currentFilePath = fileName;
                        *isModified = false;
                        windowPtr->setWindowTitle(QString("Notepad++ - %1").arg(QFileInfo(fileName).fileName()));
                    }
                    else
                    {
                        QMessageBox::critical(windowPtr, "Error", "Failed to save file: " + fileName);
                    }
                }
            });
            const CommandId saveAsFileCommandId = saveAsFileCommand->identifier();

            // Edit commands
            const auto undoCommand = commandRegistry->registerCommand({
                "cmd.edit.undo",
                L"Undo",
                L"Undo the last action",
                false,
                {},
                [editorPtr]() {
                    editorPtr->send(SCI_UNDO);
                }
            });
            const CommandId undoCommandId = undoCommand->identifier();

            const auto redoCommand = commandRegistry->registerCommand({
                "cmd.edit.redo",
                L"Redo",
                L"Redo the last undone action",
                false,
                {},
                [editorPtr]() {
                    editorPtr->send(SCI_REDO);
                }
            });
            const CommandId redoCommandId = redoCommand->identifier();

            const auto cutCommand = commandRegistry->registerCommand({
                "cmd.edit.cut",
                L"Cut",
                L"Cut selected text to clipboard",
                false,
                {},
                [editorPtr]() {
                    editorPtr->send(SCI_CUT);
                }
            });
            const CommandId cutCommandId = cutCommand->identifier();

            const auto copyCommand = commandRegistry->registerCommand({
                "cmd.edit.copy",
                L"Copy",
                L"Copy selected text to clipboard",
                false,
                {},
                [editorPtr]() {
                    editorPtr->send(SCI_COPY);
                }
            });
            const CommandId copyCommandId = copyCommand->identifier();

            const auto pasteCommand = commandRegistry->registerCommand({
                "cmd.edit.paste",
                L"Paste",
                L"Paste text from clipboard",
                false,
                {},
                [editorPtr]() {
                    editorPtr->send(SCI_PASTE);
                }
            });
            const CommandId pasteCommandId = pasteCommand->identifier();

            const auto selectAllCommand = commandRegistry->registerCommand({
                "cmd.edit.select_all",
                L"Select All",
                L"Select all text",
                false,
                {},
                [editorPtr]() {
                    editorPtr->send(SCI_SELECTALL);
                }
            });
            const CommandId selectAllCommandId = selectAllCommand->identifier();

            // Search commands
            auto lastSearchText = std::make_shared<QString>();

            const auto findCommand = commandRegistry->registerCommand({
                "cmd.search.find",
                L"Find...",
                L"Find text in document",
                false,
                {},
                [editorPtr, windowPtr, lastSearchText]() {
                    bool ok;
                    QString text = QInputDialog::getText(
                        windowPtr,
                        "Find",
                        "Find what:",
                        QLineEdit::Normal,
                        *lastSearchText,
                        &ok
                    );

                    if (ok && !text.isEmpty())
                    {
                        *lastSearchText = text;

                        // Get current position
                        auto currentPos = editorPtr->send(SCI_GETCURRENTPOS);
                        auto docLength = editorPtr->send(SCI_GETLENGTH);

                        // Search from current position
                        editorPtr->send(SCI_SETTARGETSTART, currentPos);
                        editorPtr->send(SCI_SETTARGETEND, docLength);
                        editorPtr->send(SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE);

                        QByteArray searchBytes = text.toUtf8();
                        auto pos = editorPtr->send(SCI_SEARCHINTARGET, searchBytes.length(),
                                                   reinterpret_cast<sptr_t>(searchBytes.constData()));

                        if (pos >= 0)
                        {
                            // Found - select it
                            auto targetStart = editorPtr->send(SCI_GETTARGETSTART);
                            auto targetEnd = editorPtr->send(SCI_GETTARGETEND);
                            editorPtr->send(SCI_SETSEL, targetStart, targetEnd);
                            editorPtr->send(SCI_SCROLLCARET);
                        }
                        else
                        {
                            // Not found - wrap around from beginning
                            editorPtr->send(SCI_SETTARGETSTART, 0);
                            editorPtr->send(SCI_SETTARGETEND, currentPos);
                            pos = editorPtr->send(SCI_SEARCHINTARGET, searchBytes.length(),
                                                 reinterpret_cast<sptr_t>(searchBytes.constData()));

                            if (pos >= 0)
                            {
                                auto targetStart = editorPtr->send(SCI_GETTARGETSTART);
                                auto targetEnd = editorPtr->send(SCI_GETTARGETEND);
                                editorPtr->send(SCI_SETSEL, targetStart, targetEnd);
                                editorPtr->send(SCI_SCROLLCARET);
                            }
                            else
                            {
                                QMessageBox::information(windowPtr, "Find", "Text not found: " + text);
                            }
                        }
                    }
                }
            });
            const CommandId findCommandId = findCommand->identifier();

            const auto findNextCommand = commandRegistry->registerCommand({
                "cmd.search.find_next",
                L"Find Next",
                L"Find next occurrence",
                false,
                {},
                [editorPtr, lastSearchText]() {
                    if (lastSearchText->isEmpty())
                        return;

                    auto currentPos = editorPtr->send(SCI_GETCURRENTPOS);
                    auto docLength = editorPtr->send(SCI_GETLENGTH);

                    editorPtr->send(SCI_SETTARGETSTART, currentPos + 1);
                    editorPtr->send(SCI_SETTARGETEND, docLength);
                    editorPtr->send(SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE);

                    QByteArray searchBytes = lastSearchText->toUtf8();
                    auto pos = editorPtr->send(SCI_SEARCHINTARGET, searchBytes.length(),
                                               reinterpret_cast<sptr_t>(searchBytes.constData()));

                    if (pos >= 0)
                    {
                        auto targetStart = editorPtr->send(SCI_GETTARGETSTART);
                        auto targetEnd = editorPtr->send(SCI_GETTARGETEND);
                        editorPtr->send(SCI_SETSEL, targetStart, targetEnd);
                        editorPtr->send(SCI_SCROLLCARET);
                    }
                    else
                    {
                        // Wrap around
                        editorPtr->send(SCI_SETTARGETSTART, 0);
                        editorPtr->send(SCI_SETTARGETEND, currentPos);
                        pos = editorPtr->send(SCI_SEARCHINTARGET, searchBytes.length(),
                                             reinterpret_cast<sptr_t>(searchBytes.constData()));

                        if (pos >= 0)
                        {
                            auto targetStart = editorPtr->send(SCI_GETTARGETSTART);
                            auto targetEnd = editorPtr->send(SCI_GETTARGETEND);
                            editorPtr->send(SCI_SETSEL, targetStart, targetEnd);
                            editorPtr->send(SCI_SCROLLCARET);
                        }
                    }
                }
            });
            const CommandId findNextCommandId = findNextCommand->identifier();

            // View commands - Zoom
            const auto zoomInCommand = commandRegistry->registerCommand({
                "cmd.view.zoom_in",
                L"Zoom In",
                L"Increase font size",
                false,
                {},
                [editorPtr]() {
                    editorPtr->send(SCI_ZOOMIN);
                }
            });
            const CommandId zoomInCommandId = zoomInCommand->identifier();

            const auto zoomOutCommand = commandRegistry->registerCommand({
                "cmd.view.zoom_out",
                L"Zoom Out",
                L"Decrease font size",
                false,
                {},
                [editorPtr]() {
                    editorPtr->send(SCI_ZOOMOUT);
                }
            });
            const CommandId zoomOutCommandId = zoomOutCommand->identifier();

            const auto zoomResetCommand = commandRegistry->registerCommand({
                "cmd.view.zoom_reset",
                L"Reset Zoom",
                L"Reset to default font size",
                false,
                {},
                [editorPtr, defaultFontSize]() {
                    // Reset to exact system font size (not zoom level)
                    editorPtr->send(SCI_STYLESETSIZEFRACTIONAL, STYLE_DEFAULT,
                                  static_cast<int>(*defaultFontSize * 100.0));
                    editorPtr->send(SCI_STYLECLEARALL);
                }
            });
            const CommandId zoomResetCommandId = zoomResetCommand->identifier();

            // Update status bar with cursor position
            auto updateStatusBar = [editorPtr, cursorPosLabel, fileInfoLabel]() {
                auto line = editorPtr->send(SCI_LINEFROMPOSITION, editorPtr->send(SCI_GETCURRENTPOS));
                auto col = editorPtr->send(SCI_GETCOLUMN, editorPtr->send(SCI_GETCURRENTPOS));
                cursorPosLabel->setText(QString("Line: %1, Col: %2").arg(line + 1).arg(col + 1));

                // EOL mode
                auto eolMode = editorPtr->send(SCI_GETEOLMODE);
                QString eolStr = (eolMode == SC_EOL_CRLF) ? "CRLF" : (eolMode == SC_EOL_LF) ? "LF" : "CR";
                fileInfoLabel->setText(QString("UTF-8 | %1").arg(eolStr));
            };

            // Timer to update status bar periodically
            QTimer* statusTimer = new QTimer(nativeWindow.get());
            QObject::connect(statusTimer, &QTimer::timeout, updateStatusBar);
            statusTimer->start(100); // Update every 100ms

            // Track document modifications using Scintilla notifications
            // We connect to textChanged signal (if available) or use periodic check
            QTimer* modCheckTimer = new QTimer(nativeWindow.get());
            auto lastLength = std::make_shared<sptr_t>(editorPtr->send(SCI_GETLENGTH));
            QObject::connect(modCheckTimer, &QTimer::timeout, [editorPtr, isModified, lastLength, windowPtr, currentFilePath]() {
                auto currentLength = editorPtr->send(SCI_GETLENGTH);
                if (currentLength != *lastLength && !*isModified)
                {
                    *isModified = true;
                    // Update window title to show modified state
                    QString title = currentFilePath->isEmpty() ? "Notepad++" : QString("Notepad++ - %1").arg(QFileInfo(*currentFilePath).fileName());
                    windowPtr->setWindowTitle(title + " *");
                }
                *lastLength = currentLength;
            });
            modCheckTimer->start(200); // Check every 200ms

            std::weak_ptr<DockingHost> dockingWeak = dockingHost;
            const auto toggleFunctionListCommand = commandRegistry->registerCommand({
                "cmd.view.function_list",
                L"Toggle Function List",
                L"Show or hide the Function List panel",
                true,
                {},
                [dockingWeak]() {
                    if (auto locked = dockingWeak.lock())
                    {
                        const PanelId panelId = "function_list";
                        if (locked->isPanelVisible(panelId))
                        {
                            locked->hidePanel(panelId);
                        }
                        else
                        {
                            locked->showPanel(panelId);
                        }
                    }
                }
            });
            const CommandId toggleFunctionListCommandId = toggleFunctionListCommand->identifier();
            toggleFunctionListCommand->setState({true, true});

            // ========================================
            // LANGUAGE COMMANDS
            // ========================================

            // Create language selection commands
            std::vector<CommandId> languageCommandIds;
            for (const auto& lang : g_languages)
            {
                QString cmdId = QString("cmd.language.%1").arg(QString(lang.name).toLower().replace(" ", "_"));
                QString title = QString::fromUtf8(lang.name);
                QString desc = QString("Set language to %1").arg(title);

                auto langCmd = commandRegistry->registerCommand({
                    cmdId.toStdString(),
                    title.toStdWString(),
                    desc.toStdWString(),
                    false,
                    {},
                    [getCurrentEditor, documents, tabWidget, lexerName = lang.lexerName, langName = QString(lang.name)]() {
                        auto* editor = getCurrentEditor();
                        if (editor)
                        {
                            applyLexer(editor, lexerName);

                            // Update document language in documents list
                            int currentIndex = tabWidget->currentIndex();
                            if (currentIndex >= 0 && currentIndex < static_cast<int>(documents->size()))
                            {
                                (*documents)[currentIndex].language = langName;
                            }
                        }
                    }
                });
                languageCommandIds.push_back(langCmd->identifier());
            }

            // ========================================
            // FORMATTING COMMANDS
            // ========================================

            // XML Pretty Print
            const auto xmlPrettyPrintCommand = commandRegistry->registerCommand({
                "cmd.format.xml_pretty_print",
                L"XML Pretty Print",
                L"Format XML with proper indentation",
                false,
                {},
                [getCurrentEditor, windowPtr]() {
                    auto* editor = getCurrentEditor();
                    if (!editor) return;

                    // Get all text from editor
                    auto length = editor->send(SCI_GETLENGTH);
                    std::vector<char> buffer(length + 1);
                    editor->send(SCI_GETTEXT, length + 1, reinterpret_cast<sptr_t>(buffer.data()));

                    QString originalText = QString::fromUtf8(buffer.data());
                    QString formattedText = formatXML(originalText);

                    // Replace editor content
                    editor->send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(formattedText.toUtf8().constData()));

                    windowPtr->statusBar()->showMessage("XML formatted", 2000);
                }
            });
            const CommandId xmlPrettyPrintCommandId = xmlPrettyPrintCommand->identifier();

            // JSON Pretty Print
            const auto jsonPrettyPrintCommand = commandRegistry->registerCommand({
                "cmd.format.json_pretty_print",
                L"JSON Pretty Print",
                L"Format JSON with proper indentation",
                false,
                {},
                [getCurrentEditor, windowPtr]() {
                    auto* editor = getCurrentEditor();
                    if (!editor) return;

                    // Get all text from editor
                    auto length = editor->send(SCI_GETLENGTH);
                    std::vector<char> buffer(length + 1);
                    editor->send(SCI_GETTEXT, length + 1, reinterpret_cast<sptr_t>(buffer.data()));

                    QString originalText = QString::fromUtf8(buffer.data());
                    QString formattedText = formatJSON(originalText);

                    // Replace editor content
                    editor->send(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(formattedText.toUtf8().constData()));

                    windowPtr->statusBar()->showMessage("JSON formatted", 2000);
                }
            });
            const CommandId jsonPrettyPrintCommandId = jsonPrettyPrintCommand->identifier();

            std::vector<MenuItemDescriptor> menu;
            menu.push_back({"menu.file",
                            MenuItemType::Submenu,
                            L"File",
                            std::nullopt,
                            {
                                {"menu.file.new",
                                 MenuItemType::Action,
                                 L"New",
                                 std::optional<CommandId>{newDocumentCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.new_tab",
                                 MenuItemType::Action,
                                 L"New Tab",
                                 std::optional<CommandId>{newTabCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.separator_tabs",
                                 MenuItemType::Separator,
                                 L"",
                                 std::nullopt,
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.close_tab",
                                 MenuItemType::Action,
                                 L"Close Tab",
                                 std::optional<CommandId>{closeTabCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.next_tab",
                                 MenuItemType::Action,
                                 L"Next Tab",
                                 std::optional<CommandId>{nextTabCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.previous_tab",
                                 MenuItemType::Action,
                                 L"Previous Tab",
                                 std::optional<CommandId>{previousTabCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.separator_open",
                                 MenuItemType::Separator,
                                 L"",
                                 std::nullopt,
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.open",
                                 MenuItemType::Action,
                                 L"Open...",
                                 std::optional<CommandId>{openFileCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.separator1",
                                 MenuItemType::Separator,
                                 L"",
                                 std::nullopt,
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.save",
                                 MenuItemType::Action,
                                 L"Save",
                                 std::optional<CommandId>{saveFileCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.save_as",
                                 MenuItemType::Action,
                                 L"Save As...",
                                 std::optional<CommandId>{saveAsFileCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.separator2",
                                 MenuItemType::Separator,
                                 L"",
                                 std::nullopt,
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.file.close",
                                 MenuItemType::Action,
                                 L"Close",
                                 std::nullopt,
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard}
                            },
                            true,
                            false,
                            MenuRole::Application});

            menu.push_back({"menu.edit",
                            MenuItemType::Submenu,
                            L"Edit",
                            std::nullopt,
                            {
                                {"menu.edit.undo",
                                 MenuItemType::Action,
                                 L"Undo",
                                 std::optional<CommandId>{undoCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.edit.redo",
                                 MenuItemType::Action,
                                 L"Redo",
                                 std::optional<CommandId>{redoCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.edit.separator1",
                                 MenuItemType::Separator,
                                 L"",
                                 std::nullopt,
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.edit.cut",
                                 MenuItemType::Action,
                                 L"Cut",
                                 std::optional<CommandId>{cutCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.edit.copy",
                                 MenuItemType::Action,
                                 L"Copy",
                                 std::optional<CommandId>{copyCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.edit.paste",
                                 MenuItemType::Action,
                                 L"Paste",
                                 std::optional<CommandId>{pasteCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.edit.separator2",
                                 MenuItemType::Separator,
                                 L"",
                                 std::nullopt,
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.edit.select_all",
                                 MenuItemType::Action,
                                 L"Select All",
                                 std::optional<CommandId>{selectAllCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard}
                            },
                            true,
                            false,
                            MenuRole::Standard});

            menu.push_back({"menu.search",
                            MenuItemType::Submenu,
                            L"Search",
                            std::nullopt,
                            {
                                {"menu.search.find",
                                 MenuItemType::Action,
                                 L"Find...",
                                 std::optional<CommandId>{findCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.search.find_next",
                                 MenuItemType::Action,
                                 L"Find Next",
                                 std::optional<CommandId>{findNextCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard}
                            },
                            true,
                            false,
                            MenuRole::Standard});

            menu.push_back({"menu.view",
                            MenuItemType::Submenu,
                            L"View",
                            std::nullopt,
                            {
                                {"menu.view.zoom_in",
                                 MenuItemType::Action,
                                 L"Zoom In",
                                 std::optional<CommandId>{zoomInCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.view.zoom_out",
                                 MenuItemType::Action,
                                 L"Zoom Out",
                                 std::optional<CommandId>{zoomOutCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.view.zoom_reset",
                                 MenuItemType::Action,
                                 L"Reset Zoom",
                                 std::optional<CommandId>{zoomResetCommandId},
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.view.separator1",
                                 MenuItemType::Separator,
                                 L"",
                                 std::nullopt,
                                 {},
                                 true,
                                 false,
                                 MenuRole::Standard},
                                {"menu.view.docks",
                                 MenuItemType::Action,
                                 L"Function List",
                                 std::optional<CommandId>{toggleFunctionListCommandId},
                                 {},
                                 true,
                                 true,
                                 MenuRole::Standard}
                            },
                            true,
                            false,
                            MenuRole::Standard});

            // Build Language menu
            std::vector<MenuItemDescriptor> languageMenuItems;

            // Add XML and JSON formatters at the top
            languageMenuItems.push_back({
                "menu.language.xml_pretty_print",
                MenuItemType::Action,
                L"XML Pretty Print",
                std::optional<CommandId>{xmlPrettyPrintCommandId},
                {},
                true,
                false,
                MenuRole::Standard
            });
            languageMenuItems.push_back({
                "menu.language.json_pretty_print",
                MenuItemType::Action,
                L"JSON Pretty Print",
                std::optional<CommandId>{jsonPrettyPrintCommandId},
                {},
                true,
                false,
                MenuRole::Standard
            });
            languageMenuItems.push_back({
                "menu.language.separator1",
                MenuItemType::Separator,
                L"",
                std::nullopt,
                {},
                true,
                false,
                MenuRole::Standard
            });

            // Add all language options
            for (size_t i = 0; i < g_languages.size(); ++i)
            {
                QString menuId = QString("menu.language.%1").arg(QString(g_languages[i].name).toLower().replace(" ", "_"));
                languageMenuItems.push_back({
                    menuId.toStdString(),
                    MenuItemType::Action,
                    QString::fromUtf8(g_languages[i].name).toStdWString(),
                    std::optional<CommandId>{languageCommandIds[i]},
                    {},
                    true,
                    false,
                    MenuRole::Standard
                });
            }

            menu.push_back({"menu.language",
                            MenuItemType::Submenu,
                            L"Language",
                            std::nullopt,
                            languageMenuItems,
                            true,
                            false,
                            MenuRole::Standard});

            menuModel->setItems(menu);
            menuModel->setApplicationMenu("menu.file");

            auto qtWindow = std::make_unique<QtWindow>(std::move(nativeWindow));
            auto application = std::make_unique<QtApplication>(std::move(qtApp),
                                                               std::move(qtWindow),
                                                               std::move(commandRegistry),
                                                               std::move(menuModel),
                                                               std::move(shortcutMap),
                                                               std::move(themeProvider),
                                                               dockingHost,
                                                               std::move(editorController));

            BootstrapResult result;
            result.application = std::move(application);
            return result;
        });
    }

    bool isQtBackendAvailable() noexcept
    {
        return true;
    }
}

#else

#include <utility>

namespace npp::ui
{
    void registerQtBackend()
    {
        Factory::registerBackend([](const ApplicationConfig&) -> BootstrapResult {
            BootstrapResult result;
            result.warnings.push_back({L"Qt backend not built in this configuration"});
            return result;
        });
    }

    bool isQtBackendAvailable() noexcept
    {
        return false;
    }
}

#endif
