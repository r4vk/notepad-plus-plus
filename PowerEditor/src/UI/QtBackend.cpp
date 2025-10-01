#include "UI/QtBackend.h"

#include "UI/Application.h"
#include "UI/Command.h"
#include "UI/Docking.h"
#include "UI/Factory.h"
#include "UI/Menu.h"
#include "UI/Shortcut.h"
#include "UI/Theme.h"
#include "UI/Window.h"
#include "UI/ScintillaTheme.h"

#if defined(NPP_UI_WITH_QT)

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDockWidget>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QKeySequence>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QScreen>
#include <QStatusBar>
#include <QString>
#include <QWidget>

#include "Scintilla.h"
#include "SciLexer.h"
#include "ScintillaEdit.h"

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
            editor.send(SCI_SETCARETLINEVISIBLE, 1);
            editor.send(SCI_SETCARETLINEBACK, toColorRef(scheme.lineHighlight));
            editor.send(SCI_SETCARETLINEBACKALPHA, static_cast<int>(scheme.lineHighlight.alpha * 255.0F));

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
                    action_->setShortcut(QKeySequence(modifiers | QKeySequence::fromString(key).toCombined()));
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

                for (const auto& item : items_)
                {
                    if (item.type != MenuItemType::Submenu)
                    {
                        continue;
                    }

                    QMenu* menu = menuBar->addMenu(QString::fromStdWString(item.title));
                    buildMenu(*menu, item.children);
                    if (applicationMenuId_ == item.identifier)
                    {
                        menuBar->setNativeMenuBar(true);
                    }
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
                adapters_.erase(identifier);
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
            auto qtApp = std::make_unique<QApplication>(argc, argv);
            qtApp->setApplicationName(QString::fromStdWString(config.applicationName));
            qtApp->setOrganizationName(QString::fromStdWString(config.organizationName));

            auto nativeWindow = std::make_unique<QMainWindow>();
            nativeWindow->setObjectName("npp.mac.preview");
            nativeWindow->resize(1100, 720);
            nativeWindow->setWindowTitle(QString::fromStdWString(config.applicationName));
            nativeWindow->statusBar()->showMessage("Qt prototype loaded");

            auto commandRegistry = std::make_unique<QtCommandRegistry>(nativeWindow.get());
            auto shortcutMap = std::make_unique<QtShortcutMap>(*commandRegistry);
            auto menuModel = std::make_unique<QtMenuModel>(*nativeWindow, *commandRegistry);
            auto themeProvider = std::make_unique<QtThemeProvider>();
            auto dockingHost = std::make_shared<QtDockingHost>(*nativeWindow);

            auto* centralEditor = new ScintillaEdit(nativeWindow.get());
            centralEditor->setObjectName("central.editor");
            nativeWindow->setCentralWidget(centralEditor);

            centralEditor->send(SCI_SETCODEPAGE, SC_CP_UTF8);
            centralEditor->send(SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWR);
            centralEditor->send(SCI_SETBUFFEREDDRAW, 1);
            centralEditor->send(SCI_SETCARETSTYLE, CARETSTYLE_LINE);
            centralEditor->send(SCI_SETMOUSEDWELLTIME, 350);
            centralEditor->send(SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION);
            centralEditor->send(SCI_SETMULTIPLESELECTION, 1);
            centralEditor->send(SCI_SETADDITIONALSELECTIONTYPING, 1);
            centralEditor->send(SCI_SETSCROLLWIDTHTRACKING, 1);
            centralEditor->send(SCI_SETLEXER, SCLEX_NULL);

            QFont editorFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
            editorFont.setPointSizeF(13.0);
            const QByteArray fontNameUtf8 = editorFont.family().toUtf8();
            centralEditor->sends(SCI_STYLESETFONT, STYLE_DEFAULT, fontNameUtf8.constData());
            centralEditor->send(SCI_STYLESETSIZEFRACTIONAL, STYLE_DEFAULT, static_cast<int>(editorFont.pointSizeF() * 100.0));
            centralEditor->send(SCI_STYLECLEARALL);

            const auto scheme = scintilla::deriveColorScheme(themeProvider->palette());
            applyColorScheme(*centralEditor, scheme);

            const float devicePixelRatio = QGuiApplication::primaryScreen() ? static_cast<float>(QGuiApplication::primaryScreen()->devicePixelRatio()) : 1.0F;
            centralEditor->send(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
            centralEditor->send(SCI_SETMARGINWIDTHN, 0, 48);

            auto editorController = std::make_unique<QtScintillaController>(*centralEditor, *themeProvider);
            editorController->updateZoom(devicePixelRatio);

            const char* const sample =
                "// Liquid Glass Mac preview\n"
                "#include <iostream>\n\n"
                "int main() {\n"
                "    std::cout << \"Hello, Scintilla on macOS!\" << std::endl;\n"
                "    return 0;\n"
                "}\n";
            centralEditor->sends(SCI_SETTEXT, 0, sample);

            dockingHost->registerPanel({"function_list",
                                        L"Function List",
                                        std::nullopt,
                                        true,
                                        DockArea::Right,
                                        {}});

            const auto newDocumentCommand = commandRegistry->registerCommand({
                "cmd.file.new",
                L"New",
                L"Create a new document",
                false,
                {},
                []() {}
            });
            const CommandId newDocumentCommandId = newDocumentCommand->identifier();

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
                                {"menu.file.separator",
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

            menu.push_back({"menu.view",
                            MenuItemType::Submenu,
                            L"View",
                            std::nullopt,
                            {
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
