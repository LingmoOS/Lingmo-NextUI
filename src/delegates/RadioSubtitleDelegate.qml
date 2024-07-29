/*
 * SPDX-FileCopyrightText: 2023 Arjen Hiemstra <ahiemstra@heimr.nl>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.lingmoui.platform as Platform

/**
 * A convenience wrapper combining QtQuick Controls RadioDelegate and IconTitleSubtitle
 *
 * This is an intentionally minimal wrapper that replaces the RadioDelegate's
 * contentItem with an IconTitleSubtitle and adds a subtitle property.
 *
 * If you wish to customize the layout further, create your own `RadioDelegate`
 * subclass with the `contentItem:` property set to the content of your choice.
 * This can include `IconTitleSubtitle` inside a Layout, for example.
 *
 * \note If you don't need a subtitle, use `RadioDelegate` directly.
 *
 * \sa LingmoUI::Delegates::TitleSubtitle
 * \sa LingmoUI::Delegates::IconTitleSubtitle
 */
QQC2.RadioDelegate {
    id: delegate

    // Please see the developer note in ItemDelegate

    /**
     * The subtitle to display.
     */
    property string subtitle

    QQC2.ToolTip.text: text + (subtitle.length > 0 ? "\n\n" + subtitle : "")
    QQC2.ToolTip.visible: (Platform.Settings.tabletMode ? down : hovered) && (contentItem?.truncated ?? false)
    QQC2.ToolTip.delay: Platform.Units.toolTipDelay

    contentItem: IconTitleSubtitle {
        icon: icon.fromControlsIcon(delegate.icon)
        title: delegate.text
        subtitle: delegate.subtitle
        selected: delegate.highlighted || delegate.down
        font: delegate.font
    }
}
