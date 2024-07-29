/*
 *  SPDX-FileCopyrightText: 2009 Alan Alpert <alan.alpert@nokia.com>
 *  SPDX-FileCopyrightText: 2010 Ménard Alexis <menard@kde.org>
 *  SPDX-FileCopyrightText: 2010 Marco Martin <mart@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "lingmouiplugin.h"

#include <QIcon>
#if defined(Q_OS_ANDROID)
#include <QResource>
#endif
#include <QQmlContext>
#include <QQuickItem>

#include "platform/styleselector.h"

#ifdef LINGMOUI_BUILD_TYPE_STATIC
#include "loggingcategory.h"
#include <QDebug>
#endif

// This is required for declarative registration to work on Windows.
// This is normally generated by Qt but since we need a manually written plugin
// file, we need to include this ourselves.
extern void qml_register_types_org_kde_lingmoui();
Q_GHS_KEEP_REFERENCE(qml_register_types_org_kde_lingmoui)

// we can't do this in the plugin object directly, as that can live in a different thread
// and event filters are only allowed in the same thread as the filtered object
class LanguageChangeEventFilter : public QObject
{
    Q_OBJECT
public:
    bool eventFilter(QObject *receiver, QEvent *event) override
    {
        if (event->type() == QEvent::LanguageChange && receiver == QCoreApplication::instance()) {
            Q_EMIT languageChangeEvent();
        }
        return QObject::eventFilter(receiver, event);
    }

Q_SIGNALS:
    void languageChangeEvent();
};

LingmoUIPlugin::LingmoUIPlugin(QObject *parent)
    : QQmlExtensionPlugin(parent)
{
    // See above.
    volatile auto registration = &qml_register_types_org_kde_lingmoui;
    Q_UNUSED(registration)

    auto filter = new LanguageChangeEventFilter;
    filter->moveToThread(QCoreApplication::instance()->thread());
    QCoreApplication::instance()->installEventFilter(filter);
    connect(filter, &LanguageChangeEventFilter::languageChangeEvent, this, &LingmoUIPlugin::languageChangeEvent);
}

QUrl LingmoUIPlugin::componentUrl(const QString &fileName) const
{
    return LingmoUI::Platform::StyleSelector::componentUrl(fileName);
}

void LingmoUIPlugin::registerTypes(const char *uri)
{
#if defined(Q_OS_ANDROID)
    QResource::registerResource(QStringLiteral("assets:/android_rcc_bundle.rcc"));
#endif

    Q_ASSERT(QLatin1String(uri) == QLatin1String("org.kde.lingmoui"));

    LingmoUI::Platform::StyleSelector::setBaseUrl(baseUrl());

    if (QIcon::themeName().isEmpty() && !qEnvironmentVariableIsSet("XDG_CURRENT_DESKTOP")) {
#if defined(Q_OS_ANDROID)
        QIcon::setThemeSearchPaths({QStringLiteral("assets:/qml/org/kde/lingmoui"), QStringLiteral(":/icons")});
#else
        QIcon::setThemeSearchPaths({LingmoUI::Platform::StyleSelector::resolveFilePath(QStringLiteral(".")), QStringLiteral(":/icons")});
#endif
        QIcon::setThemeName(QStringLiteral("ocean-internal"));
    } else {
        QIcon::setFallbackSearchPaths(QIcon::fallbackSearchPaths() << LingmoUI::Platform::StyleSelector::resolveFilePath(QStringLiteral("icons")));
    }

    qmlRegisterType(componentUrl(QStringLiteral("Action.qml")), uri, 2, 0, "Action");
    qmlRegisterType(componentUrl(QStringLiteral("AbstractApplicationHeader.qml")), uri, 2, 0, "AbstractApplicationHeader");
    qmlRegisterType(componentUrl(QStringLiteral("AbstractApplicationWindow.qml")), uri, 2, 0, "AbstractApplicationWindow");
    qmlRegisterType(componentUrl(QStringLiteral("ApplicationWindow.qml")), uri, 2, 0, "ApplicationWindow");
    qmlRegisterType(componentUrl(QStringLiteral("OverlayDrawer.qml")), uri, 2, 0, "OverlayDrawer");
    qmlRegisterType(componentUrl(QStringLiteral("ContextDrawer.qml")), uri, 2, 0, "ContextDrawer");
    qmlRegisterType(componentUrl(QStringLiteral("GlobalDrawer.qml")), uri, 2, 0, "GlobalDrawer");
    qmlRegisterType(componentUrl(QStringLiteral("Heading.qml")), uri, 2, 0, "Heading");
    qmlRegisterType(componentUrl(QStringLiteral("PageRow.qml")), uri, 2, 0, "PageRow");

    qmlRegisterType(componentUrl(QStringLiteral("OverlaySheet.qml")), uri, 2, 0, "OverlaySheet");
    qmlRegisterType(componentUrl(QStringLiteral("Page.qml")), uri, 2, 0, "Page");
    qmlRegisterType(componentUrl(QStringLiteral("ScrollablePage.qml")), uri, 2, 0, "ScrollablePage");
    qmlRegisterType(componentUrl(QStringLiteral("SwipeListItem.qml")), uri, 2, 0, "SwipeListItem");
    qmlRegisterType(componentUrl(QStringLiteral("Button.qml")), uri, 2, 0, "Button");
    qmlRegisterType(componentUrl(QStringLiteral("ProgressBar.qml")), uri, 2, 0, "ProgressBar");
    qmlRegisterType(componentUrl(QStringLiteral("Switch.qml")), uri, 2, 0, "Switch");
    qmlRegisterType(componentUrl(QStringLiteral("SwitchIndicator.qml")), uri, 2, 0, "SwitchIndicator");
    qmlRegisterType(componentUrl(QStringLiteral("TabBar.qml")), uri, 2, 0, "TabBar");
    qmlRegisterType(componentUrl(QStringLiteral("TabButton.qml")), uri, 2, 0, "TabButton");
    qmlRegisterType(componentUrl(QStringLiteral("TextArea.qml")), uri, 2, 0, "TextArea");
    qmlRegisterType(componentUrl(QStringLiteral("TextField.qml")), uri, 2, 0, "TextField");
    qmlRegisterType(componentUrl(QStringLiteral("ToolTip.qml")), uri, 2, 0, "ToolTip");
    qmlRegisterType(componentUrl(QStringLiteral("StackView.qml")), uri, 2, 0, "StackView");
    qmlRegisterType(componentUrl(QStringLiteral("Slider.qml")), uri, 2, 0, "Slider");
    qmlRegisterType(componentUrl(QStringLiteral("ScrollBar.qml")), uri, 2, 0, "ScrollBar");
    qmlRegisterType(componentUrl(QStringLiteral("RadioIndicator.qml")), uri, 2, 0, "RadioIndicator");
    qmlRegisterType(componentUrl(QStringLiteral("RadioButton.qml")), uri, 2, 0, "RadioButton");
    qmlRegisterType(componentUrl(QStringLiteral("MenuItem.qml")), uri, 2, 0, "MenuItem");
    qmlRegisterType(componentUrl(QStringLiteral("RadioDelegate.qml")), uri, 2, 0, "RadioDelegate");
    qmlRegisterType(componentUrl(QStringLiteral("Menu.qml")), uri, 2, 0, "Menu");
    qmlRegisterType(componentUrl(QStringLiteral("Frame.qml")), uri, 2, 0, "Frame");
    qmlRegisterType(componentUrl(QStringLiteral("DialogButtonBox.qml")), uri, 2, 0, "DialogButtonBox");
    qmlRegisterType(componentUrl(QStringLiteral("Dialog.qml")), uri, 2, 0, "Dialog");
    qmlRegisterType(componentUrl(QStringLiteral("ComboBox.qml")), uri, 2, 0, "ComboBox");
    qmlRegisterType(componentUrl(QStringLiteral("Control.qml")), uri, 2, 0, "Control");
    qmlRegisterType(componentUrl(QStringLiteral("CheckIndicator.qml")), uri, 2, 0, "CheckIndicator");
    qmlRegisterType(componentUrl(QStringLiteral("CheckBox.qml")), uri, 2, 0, "CheckBox");

    // 2.1
    qmlRegisterType(componentUrl(QStringLiteral("AbstractApplicationItem.qml")), uri, 2, 1, "AbstractApplicationItem");
    qmlRegisterType(componentUrl(QStringLiteral("ApplicationItem.qml")), uri, 2, 1, "ApplicationItem");

    // 2.4
    qmlRegisterType(componentUrl(QStringLiteral("AbstractCard.qml")), uri, 2, 4, "AbstractCard");
    qmlRegisterType(componentUrl(QStringLiteral("Card.qml")), uri, 2, 4, "Card");
    qmlRegisterType(componentUrl(QStringLiteral("CardsListView.qml")), uri, 2, 4, "CardsListView");
    qmlRegisterType(componentUrl(QStringLiteral("CardsLayout.qml")), uri, 2, 4, "CardsLayout");
    qmlRegisterType(componentUrl(QStringLiteral("InlineMessage.qml")), uri, 2, 4, "InlineMessage");

    // 2.5
    qmlRegisterType(componentUrl(QStringLiteral("ListItemDragHandle.qml")), uri, 2, 5, "ListItemDragHandle");
    qmlRegisterType(componentUrl(QStringLiteral("ActionToolBar.qml")), uri, 2, 5, "ActionToolBar");

    // 2.6
    qmlRegisterType(componentUrl(QStringLiteral("AboutPage.qml")), uri, 2, 6, "AboutPage");
    qmlRegisterType(componentUrl(QStringLiteral("LinkButton.qml")), uri, 2, 6, "LinkButton");
    qmlRegisterType(componentUrl(QStringLiteral("UrlButton.qml")), uri, 2, 6, "UrlButton");

    // 2.7
    qmlRegisterType(componentUrl(QStringLiteral("ActionTextField.qml")), uri, 2, 7, "ActionTextField");

    // 2.8
    qmlRegisterType(componentUrl(QStringLiteral("SearchField.qml")), uri, 2, 8, "SearchField");
    qmlRegisterType(componentUrl(QStringLiteral("PasswordField.qml")), uri, 2, 8, "PasswordField");

    // 2.10
    qmlRegisterType(componentUrl(QStringLiteral("ListSectionHeader.qml")), uri, 2, 10, "ListSectionHeader");

    // 2.11
    qmlRegisterType(componentUrl(QStringLiteral("PagePoolAction.qml")), uri, 2, 11, "PagePoolAction");

    // 2.12
    qmlRegisterType(componentUrl(QStringLiteral("PlaceholderMessage.qml")), uri, 2, 12, "PlaceholderMessage");

    // 2.14
    qmlRegisterType(componentUrl(QStringLiteral("FlexColumn.qml")), uri, 2, 14, "FlexColumn");

    // 2.19
    qmlRegisterType(componentUrl(QStringLiteral("AboutItem.qml")), uri, 2, 19, "AboutItem");
    qmlRegisterType(componentUrl(QStringLiteral("NavigationTabBar.qml")), uri, 2, 19, "NavigationTabBar");
    qmlRegisterType(componentUrl(QStringLiteral("NavigationTabButton.qml")), uri, 2, 19, "NavigationTabButton");
    qmlRegisterType(componentUrl(QStringLiteral("Chip.qml")), uri, 2, 19, "Chip");
    qmlRegisterType(componentUrl(QStringLiteral("LoadingPlaceholder.qml")), uri, 2, 19, "LoadingPlaceholder");
    qmlRegisterType(componentUrl(QStringLiteral("StandardItem.qml")), uri, 2, 19, "StandardItem");
    qmlRegisterType(componentUrl(QStringLiteral("Label.qml")), uri, 2, 19, "Label");
    qmlRegisterType(componentUrl(QStringLiteral("RoundedItem.qml")), uri, 2, 19, "RoundedItem");

    // 2.20
    qmlRegisterType(componentUrl(QStringLiteral("SelectableLabel.qml")), uri, 2, 20, "SelectableLabel");
    qmlRegisterType(componentUrl(QStringLiteral("InlineViewHeader.qml")), uri, 2, 20, "InlineViewHeader");
    qmlRegisterType(componentUrl(QStringLiteral("ContextualHelpButton.qml")), uri, 2, 20, "ContextualHelpButton");

}

void LingmoUIPlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(uri);
    connect(this, &LingmoUIPlugin::languageChangeEvent, engine, &QQmlEngine::retranslate);
}

#ifdef LINGMOUI_BUILD_TYPE_STATIC
LingmoUIPlugin &LingmoUIPlugin::getInstance()
{
    static LingmoUIPlugin instance;
    return instance;
}

void LingmoUIPlugin::registerTypes(QQmlEngine *engine)
{
    if (engine) {
        engine->addImportPath(QLatin1String(":/"));
    } else {
        qCWarning(LingmoUILog)
            << "Registering LingmoUI on a null QQmlEngine instance - you likely want to pass a valid engine, or you will want to manually add the "
               "qrc root path :/ to your import paths list so the engine is able to load the plugin";
    }
}
#endif

#include "lingmouiplugin.moc"
#include "moc_lingmouiplugin.cpp"
