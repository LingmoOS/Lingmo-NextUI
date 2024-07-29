/*
 *  SPDX-FileCopyrightText: 2019 Marco Martin <mart@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "wheelhandler.h"
#include "settings.h"

#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWheelEvent>

LingmoUIWheelEvent::LingmoUIWheelEvent(QObject *parent)
    : QObject(parent)
{
}

LingmoUIWheelEvent::~LingmoUIWheelEvent()
{
}

void LingmoUIWheelEvent::initializeFromEvent(QWheelEvent *event)
{
    m_x = event->position().x();
    m_y = event->position().y();
    m_angleDelta = event->angleDelta();
    m_pixelDelta = event->pixelDelta();
    m_buttons = event->buttons();
    m_modifiers = event->modifiers();
    m_accepted = false;
    m_inverted = event->inverted();
}

qreal LingmoUIWheelEvent::x() const
{
    return m_x;
}

qreal LingmoUIWheelEvent::y() const
{
    return m_y;
}

QPointF LingmoUIWheelEvent::angleDelta() const
{
    return m_angleDelta;
}

QPointF LingmoUIWheelEvent::pixelDelta() const
{
    return m_pixelDelta;
}

int LingmoUIWheelEvent::buttons() const
{
    return m_buttons;
}

int LingmoUIWheelEvent::modifiers() const
{
    return m_modifiers;
}

bool LingmoUIWheelEvent::inverted() const
{
    return m_inverted;
}

bool LingmoUIWheelEvent::isAccepted()
{
    return m_accepted;
}

void LingmoUIWheelEvent::setAccepted(bool accepted)
{
    m_accepted = accepted;
}

///////////////////////////////

WheelFilterItem::WheelFilterItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setEnabled(false);
}

///////////////////////////////

WheelHandler::WheelHandler(QObject *parent)
    : QObject(parent)
    , m_filterItem(new WheelFilterItem(nullptr))
{
    m_filterItem->installEventFilter(this);

    m_wheelScrollingTimer.setSingleShot(true);
    m_wheelScrollingTimer.setInterval(m_wheelScrollingDuration);
    m_wheelScrollingTimer.callOnTimeout([this]() {
        setScrolling(false);
    });

    m_yScrollAnimation.setEasingCurve(QEasingCurve::OutCubic);

    connect(QGuiApplication::styleHints(), &QStyleHints::wheelScrollLinesChanged, this, [this](int scrollLines) {
        m_defaultPixelStepSize = 20 * scrollLines;
        if (!m_explicitVStepSize && m_verticalStepSize != m_defaultPixelStepSize) {
            m_verticalStepSize = m_defaultPixelStepSize;
            Q_EMIT verticalStepSizeChanged();
        }
        if (!m_explicitHStepSize && m_horizontalStepSize != m_defaultPixelStepSize) {
            m_horizontalStepSize = m_defaultPixelStepSize;
            Q_EMIT horizontalStepSizeChanged();
        }
    });
}

WheelHandler::~WheelHandler()
{
    delete m_filterItem;
}

QQuickItem *WheelHandler::target() const
{
    return m_flickable;
}

void WheelHandler::setTarget(QQuickItem *target)
{
    if (m_flickable == target) {
        return;
    }

    if (target && !target->inherits("QQuickFlickable")) {
        qmlWarning(this) << "target must be a QQuickFlickable";
        return;
    }

    if (m_flickable) {
        m_flickable->removeEventFilter(this);
        disconnect(m_flickable, nullptr, m_filterItem, nullptr);
        disconnect(m_flickable, &QQuickItem::parentChanged, this, &WheelHandler::_k_rebindScrollBars);
    }

    m_flickable = target;
    m_filterItem->setParentItem(target);
    if (m_yScrollAnimation.targetObject()) {
        m_yScrollAnimation.stop();
    }
    m_yScrollAnimation.setTargetObject(target);

    if (target) {
        target->installEventFilter(this);

        // Stack WheelFilterItem over the Flickable's scrollable content
        m_filterItem->stackAfter(target->property("contentItem").value<QQuickItem *>());
        // Make it fill the Flickable
        m_filterItem->setWidth(target->width());
        m_filterItem->setHeight(target->height());
        connect(target, &QQuickItem::widthChanged, m_filterItem, [this, target]() {
            m_filterItem->setWidth(target->width());
        });
        connect(target, &QQuickItem::heightChanged, m_filterItem, [this, target]() {
            m_filterItem->setHeight(target->height());
        });
    }

    _k_rebindScrollBars();

    Q_EMIT targetChanged();
}

void WheelHandler::_k_rebindScrollBars()
{
    struct ScrollBarAttached {
        QObject *attached = nullptr;
        QQuickItem *vertical = nullptr;
        QQuickItem *horizontal = nullptr;
    };

    ScrollBarAttached attachedToFlickable;
    ScrollBarAttached attachedToScrollView;

    if (m_flickable) {
        // Get ScrollBars so that we can filter them too, even if they're not
        // in the bounds of the Flickable
        const auto flickableChildren = m_flickable->children();
        for (const auto child : flickableChildren) {
            if (child->inherits("QQuickScrollBarAttached")) {
                attachedToFlickable.attached = child;
                attachedToFlickable.vertical = child->property("vertical").value<QQuickItem *>();
                attachedToFlickable.horizontal = child->property("horizontal").value<QQuickItem *>();
                break;
            }
        }

        // Check ScrollView if there are no scrollbars attached to the Flickable.
        // We need to check if the parent inherits QQuickScrollView in case the
        // parent is another Flickable that already has a LingmoUI WheelHandler.
        auto flickableParent = m_flickable->parentItem();
        if (flickableParent && flickableParent->inherits("QQuickScrollView")) {
            const auto siblings = flickableParent->children();
            for (const auto child : siblings) {
                if (child->inherits("QQuickScrollBarAttached")) {
                    attachedToScrollView.attached = child;
                    attachedToScrollView.vertical = child->property("vertical").value<QQuickItem *>();
                    attachedToScrollView.horizontal = child->property("horizontal").value<QQuickItem *>();
                    break;
                }
            }
        }
    }

    // Dilemma: ScrollBars can be attached to both ScrollView and Flickable,
    // but only one of them should be shown anyway. Let's prefer Flickable.

    struct ChosenScrollBar {
        QObject *attached = nullptr;
        QQuickItem *scrollBar = nullptr;
    };

    ChosenScrollBar vertical;
    if (attachedToFlickable.vertical) {
        vertical.attached = attachedToFlickable.attached;
        vertical.scrollBar = attachedToFlickable.vertical;
    } else if (attachedToScrollView.vertical) {
        vertical.attached = attachedToScrollView.attached;
        vertical.scrollBar = attachedToScrollView.vertical;
    }

    ChosenScrollBar horizontal;
    if (attachedToFlickable.horizontal) {
        horizontal.attached = attachedToFlickable.attached;
        horizontal.scrollBar = attachedToFlickable.horizontal;
    } else if (attachedToScrollView.horizontal) {
        horizontal.attached = attachedToScrollView.attached;
        horizontal.scrollBar = attachedToScrollView.horizontal;
    }

    // Flickable may get re-parented to or out of a ScrollView, so we need to
    // redo the discovery process. This is especially important for
    // LingmoUI.ScrollablePage component.
    if (m_flickable) {
        if (attachedToFlickable.horizontal && attachedToFlickable.vertical) {
            // But if both scrollbars are already those from the preferred
            // Flickable, there's no need for rediscovery.
            disconnect(m_flickable, &QQuickItem::parentChanged, this, &WheelHandler::_k_rebindScrollBars);
        } else {
            connect(m_flickable, &QQuickItem::parentChanged, this, &WheelHandler::_k_rebindScrollBars, Qt::UniqueConnection);
        }
    }

    if (m_verticalScrollBar != vertical.scrollBar) {
        if (m_verticalScrollBar) {
            m_verticalScrollBar->removeEventFilter(this);
            disconnect(m_verticalChangedConnection);
        }
        m_verticalScrollBar = vertical.scrollBar;
        if (vertical.scrollBar) {
            vertical.scrollBar->installEventFilter(this);
            m_verticalChangedConnection = connect(vertical.attached, SIGNAL(verticalChanged()), this, SLOT(_k_rebindScrollBars()));
        }
    }

    if (m_horizontalScrollBar != horizontal.scrollBar) {
        if (m_horizontalScrollBar) {
            m_horizontalScrollBar->removeEventFilter(this);
            disconnect(m_horizontalChangedConnection);
        }
        m_horizontalScrollBar = horizontal.scrollBar;
        if (horizontal.scrollBar) {
            horizontal.scrollBar->installEventFilter(this);
            m_horizontalChangedConnection = connect(horizontal.attached, SIGNAL(horizontalChanged()), this, SLOT(_k_rebindScrollBars()));
        }
    }
}

qreal WheelHandler::verticalStepSize() const
{
    return m_verticalStepSize;
}

void WheelHandler::setVerticalStepSize(qreal stepSize)
{
    m_explicitVStepSize = true;
    if (qFuzzyCompare(m_verticalStepSize, stepSize)) {
        return;
    }
    // Mimic the behavior of QQuickScrollBar when stepSize is 0
    if (qFuzzyIsNull(stepSize)) {
        resetVerticalStepSize();
        return;
    }
    m_verticalStepSize = stepSize;
    Q_EMIT verticalStepSizeChanged();
}

void WheelHandler::resetVerticalStepSize()
{
    m_explicitVStepSize = false;
    if (qFuzzyCompare(m_verticalStepSize, m_defaultPixelStepSize)) {
        return;
    }
    m_verticalStepSize = m_defaultPixelStepSize;
    Q_EMIT verticalStepSizeChanged();
}

qreal WheelHandler::horizontalStepSize() const
{
    return m_horizontalStepSize;
}

void WheelHandler::setHorizontalStepSize(qreal stepSize)
{
    m_explicitHStepSize = true;
    if (qFuzzyCompare(m_horizontalStepSize, stepSize)) {
        return;
    }
    // Mimic the behavior of QQuickScrollBar when stepSize is 0
    if (qFuzzyIsNull(stepSize)) {
        resetHorizontalStepSize();
        return;
    }
    m_horizontalStepSize = stepSize;
    Q_EMIT horizontalStepSizeChanged();
}

void WheelHandler::resetHorizontalStepSize()
{
    m_explicitHStepSize = false;
    if (qFuzzyCompare(m_horizontalStepSize, m_defaultPixelStepSize)) {
        return;
    }
    m_horizontalStepSize = m_defaultPixelStepSize;
    Q_EMIT horizontalStepSizeChanged();
}

Qt::KeyboardModifiers WheelHandler::pageScrollModifiers() const
{
    return m_pageScrollModifiers;
}

void WheelHandler::setPageScrollModifiers(Qt::KeyboardModifiers modifiers)
{
    if (m_pageScrollModifiers == modifiers) {
        return;
    }
    m_pageScrollModifiers = modifiers;
    Q_EMIT pageScrollModifiersChanged();
}

void WheelHandler::resetPageScrollModifiers()
{
    setPageScrollModifiers(m_defaultPageScrollModifiers);
}

bool WheelHandler::filterMouseEvents() const
{
    return m_filterMouseEvents;
}

void WheelHandler::setFilterMouseEvents(bool enabled)
{
    if (m_filterMouseEvents == enabled) {
        return;
    }
    m_filterMouseEvents = enabled;
    Q_EMIT filterMouseEventsChanged();
}

bool WheelHandler::keyNavigationEnabled() const
{
    return m_keyNavigationEnabled;
}

void WheelHandler::setKeyNavigationEnabled(bool enabled)
{
    if (m_keyNavigationEnabled == enabled) {
        return;
    }
    m_keyNavigationEnabled = enabled;
    Q_EMIT keyNavigationEnabledChanged();
}

void WheelHandler::classBegin()
{
    // Initializes smooth scrolling
    m_engine = qmlEngine(this);
    m_units = m_engine->singletonInstance<LingmoUI::Platform::Units *>("org.kde.lingmoui.platform", "Units");
    m_settings = m_engine->singletonInstance<LingmoUI::Platform::Settings *>("org.kde.lingmoui.platform", "Settings");
    initSmoothScrollDuration();

    connect(m_units, &LingmoUI::Platform::Units::longDurationChanged, this, &WheelHandler::initSmoothScrollDuration);
    connect(m_settings, &LingmoUI::Platform::Settings::smoothScrollChanged, this, &WheelHandler::initSmoothScrollDuration);
}

void WheelHandler::componentComplete()
{
}

void WheelHandler::initSmoothScrollDuration()
{
    if (m_settings->smoothScroll()) {
        m_yScrollAnimation.setDuration(m_units->longDuration());
    } else {
        m_yScrollAnimation.setDuration(0);
    }
}

void WheelHandler::setScrolling(bool scrolling)
{
    if (m_wheelScrolling == scrolling) {
        if (m_wheelScrolling) {
            m_wheelScrollingTimer.start();
        }
        return;
    }
    m_wheelScrolling = scrolling;
    m_filterItem->setEnabled(m_wheelScrolling);
}

bool WheelHandler::scrollFlickable(QPointF pixelDelta, QPointF angleDelta, Qt::KeyboardModifiers modifiers)
{
    if (!m_flickable || (pixelDelta.isNull() && angleDelta.isNull())) {
        return false;
    }

    const qreal width = m_flickable->width();
    const qreal height = m_flickable->height();
    const qreal contentWidth = m_flickable->property("contentWidth").toReal();
    const qreal contentHeight = m_flickable->property("contentHeight").toReal();
    const qreal contentX = m_flickable->property("contentX").toReal();
    const qreal contentY = m_flickable->property("contentY").toReal();
    const qreal topMargin = m_flickable->property("topMargin").toReal();
    const qreal bottomMargin = m_flickable->property("bottomMargin").toReal();
    const qreal leftMargin = m_flickable->property("leftMargin").toReal();
    const qreal rightMargin = m_flickable->property("rightMargin").toReal();
    const qreal originX = m_flickable->property("originX").toReal();
    const qreal originY = m_flickable->property("originY").toReal();
    const qreal pageWidth = width - leftMargin - rightMargin;
    const qreal pageHeight = height - topMargin - bottomMargin;
    const auto window = m_flickable->window();
    const qreal devicePixelRatio = window != nullptr ? window->devicePixelRatio() : qGuiApp->devicePixelRatio();

    // HACK: Only transpose deltas when not using xcb in order to not conflict with xcb's own delta transposing
    if (modifiers & m_defaultHorizontalScrollModifiers && qGuiApp->platformName() != QLatin1String("xcb")) {
        angleDelta = angleDelta.transposed();
        pixelDelta = pixelDelta.transposed();
    }

    const qreal xTicks = angleDelta.x() / 120;
    const qreal yTicks = angleDelta.y() / 120;
    qreal xChange;
    qreal yChange;
    bool scrolled = false;

    // Scroll X
    if (contentWidth > pageWidth) {
        // Use page size with pageScrollModifiers. Matches QScrollBar, which uses QAbstractSlider behavior.
        if (modifiers & m_pageScrollModifiers) {
            xChange = qBound(-pageWidth, xTicks * pageWidth, pageWidth);
        } else if (pixelDelta.x() != 0) {
            xChange = pixelDelta.x();
        } else {
            xChange = xTicks * m_horizontalStepSize;
        }

        // contentX and contentY use reversed signs from what x and y would normally use, so flip the signs

        qreal minXExtent = leftMargin - originX;
        qreal maxXExtent = width - (contentWidth + rightMargin + originX);

        qreal newContentX = qBound(-minXExtent, contentX - xChange, -maxXExtent);
        // Flickable::pixelAligned rounds the position, so round to mimic that behavior.
        // Rounding prevents fractional positioning from causing text to be
        // clipped off on the top and bottom.
        // Multiply by devicePixelRatio before rounding and divide by devicePixelRatio
        // after to make position match pixels on the screen more closely.
        newContentX = std::round(newContentX * devicePixelRatio) / devicePixelRatio;
        if (contentX != newContentX) {
            scrolled = true;
            m_flickable->setProperty("contentX", newContentX);
        }
    }

    // Scroll Y
    if (contentHeight > pageHeight) {
        if (modifiers & m_pageScrollModifiers) {
            yChange = qBound(-pageHeight, yTicks * pageHeight, pageHeight);
        } else if (pixelDelta.y() != 0) {
            yChange = pixelDelta.y();
        } else {
            yChange = yTicks * m_verticalStepSize;
        }

        // contentX and contentY use reversed signs from what x and y would normally use, so flip the signs

        qreal minYExtent = topMargin - originY;
        qreal maxYExtent = height - (contentHeight + bottomMargin + originY);

        qreal newContentY;
        if (m_yScrollAnimation.state() == QPropertyAnimation::Running) {
            m_yScrollAnimation.stop();
            newContentY = std::clamp(m_yScrollAnimation.endValue().toReal() + -yChange, -minYExtent, -maxYExtent);
        } else {
            newContentY = std::clamp(contentY - yChange, -minYExtent, -maxYExtent);
        }

        // Flickable::pixelAligned rounds the position, so round to mimic that behavior.
        // Rounding prevents fractional positioning from causing text to be
        // clipped off on the top and bottom.
        // Multiply by devicePixelRatio before rounding and divide by devicePixelRatio
        // after to make position match pixels on the screen more closely.
        newContentY = std::round(newContentY * devicePixelRatio) / devicePixelRatio;
        if (contentY != newContentY) {
            scrolled = true;
            if (m_wasTouched || !m_engine) {
                m_flickable->setProperty("contentY", newContentY);
            } else {
                m_yScrollAnimation.setEndValue(newContentY);
                m_yScrollAnimation.start(QAbstractAnimation::KeepWhenStopped);
            }
        }
    }

    return scrolled;
}

bool WheelHandler::scrollUp(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_verticalStepSize;
    }
    // contentY uses reversed sign
    return scrollFlickable(QPointF(0, stepSize));
}

bool WheelHandler::scrollDown(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_verticalStepSize;
    }
    // contentY uses reversed sign
    return scrollFlickable(QPointF(0, -stepSize));
}

bool WheelHandler::scrollLeft(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_horizontalStepSize;
    }
    // contentX uses reversed sign
    return scrollFlickable(QPoint(stepSize, 0));
}

bool WheelHandler::scrollRight(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_horizontalStepSize;
    }
    // contentX uses reversed sign
    return scrollFlickable(QPoint(-stepSize, 0));
}

bool WheelHandler::eventFilter(QObject *watched, QEvent *event)
{
    auto item = qobject_cast<QQuickItem *>(watched);
    if (!item || !item->isEnabled()) {
        return false;
    }

    qreal contentWidth = 0;
    qreal contentHeight = 0;
    qreal pageWidth = 0;
    qreal pageHeight = 0;
    if (m_flickable) {
        contentWidth = m_flickable->property("contentWidth").toReal();
        contentHeight = m_flickable->property("contentHeight").toReal();
        pageWidth = m_flickable->width() - m_flickable->property("leftMargin").toReal() - m_flickable->property("rightMargin").toReal();
        pageHeight = m_flickable->height() - m_flickable->property("topMargin").toReal() - m_flickable->property("bottomMargin").toReal();
    }

    // The code handling touch, mouse and hover events is mostly copied/adapted from QQuickScrollView::childMouseEventFilter()
    switch (event->type()) {
    case QEvent::Wheel: {
        // QQuickScrollBar::interactive handling Matches behavior in QQuickScrollView::eventFilter()
        if (m_filterMouseEvents) {
            if (m_verticalScrollBar) {
                m_verticalScrollBar->setProperty("interactive", true);
            }
            if (m_horizontalScrollBar) {
                m_horizontalScrollBar->setProperty("interactive", true);
            }
        }
        QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);

        // Can't use wheelEvent->deviceType() to determine device type since on Wayland mouse is always regarded as touchpad
        // https://invent.kde.org/qt/qt/qtwayland/-/blob/e695a39519a7629c1549275a148cfb9ab99a07a9/src/client/qwaylandinputdevice.cpp#L445
        // and we can only expect a touchpad never generates the same angle delta as a mouse

        // mouse wheel can also generate angle delta like 240, 360 and so on when scrolling very fast
        // only checking wheelEvent->angleDelta().y() because we only animate for contentY
        m_wasTouched = (std::abs(wheelEvent->angleDelta().y()) != 0 && std::abs(wheelEvent->angleDelta().y()) % 120 != 0);
        // NOTE: On X11 with libinput, pixelDelta is identical to angleDelta when using a mouse that shouldn't use pixelDelta.
        // If faulty pixelDelta, reset pixelDelta to (0,0).
        if (wheelEvent->pixelDelta() == wheelEvent->angleDelta()) {
            // In order to change any of the data, we have to create a whole new QWheelEvent from its constructor.
            QWheelEvent newWheelEvent(wheelEvent->position(),
                                      wheelEvent->globalPosition(),
                                      QPoint(0, 0), // pixelDelta
                                      wheelEvent->angleDelta(),
                                      wheelEvent->buttons(),
                                      wheelEvent->modifiers(),
                                      wheelEvent->phase(),
                                      wheelEvent->inverted(),
                                      wheelEvent->source());
            m_lingmouiWheelEvent.initializeFromEvent(&newWheelEvent);
        } else {
            m_lingmouiWheelEvent.initializeFromEvent(wheelEvent);
        }

        Q_EMIT wheel(&m_lingmouiWheelEvent);

        if (m_lingmouiWheelEvent.isAccepted()) {
            return true;
        }

        bool scrolled = false;
        if (m_scrollFlickableTarget || (contentHeight <= pageHeight && contentWidth <= pageWidth)) {
            // Don't use pixelDelta from the event unless angleDelta is not available
            // because scrolling by pixelDelta is too slow on Wayland with libinput.
            QPointF pixelDelta = m_lingmouiWheelEvent.angleDelta().isNull() ? m_lingmouiWheelEvent.pixelDelta() : QPoint(0, 0);
            scrolled = scrollFlickable(pixelDelta, m_lingmouiWheelEvent.angleDelta(), Qt::KeyboardModifiers(m_lingmouiWheelEvent.modifiers()));
        }
        setScrolling(scrolled);

        // NOTE: Wheel events created by touchpad gestures with pixel deltas will cause scrolling to jump back
        // to where scrolling started unless the event is always accepted before it reaches the Flickable.
        bool flickableWillUseGestureScrolling = !(wheelEvent->source() == Qt::MouseEventNotSynthesized || wheelEvent->pixelDelta().isNull());
        return scrolled || m_blockTargetWheel || flickableWillUseGestureScrolling;
    }

    case QEvent::TouchBegin: {
        m_wasTouched = true;
        if (!m_filterMouseEvents) {
            break;
        }
        if (m_verticalScrollBar) {
            m_verticalScrollBar->setProperty("interactive", false);
        }
        if (m_horizontalScrollBar) {
            m_horizontalScrollBar->setProperty("interactive", false);
        }
        break;
    }

    case QEvent::TouchEnd: {
        m_wasTouched = false;
        break;
    }

    case QEvent::MouseButtonPress: {
        // NOTE: Flickable does not handle touch events, only synthesized mouse events
        m_wasTouched = static_cast<QMouseEvent *>(event)->source() != Qt::MouseEventNotSynthesized;
        if (!m_filterMouseEvents) {
            break;
        }
        if (!m_wasTouched) {
            if (m_verticalScrollBar) {
                m_verticalScrollBar->setProperty("interactive", true);
            }
            if (m_horizontalScrollBar) {
                m_horizontalScrollBar->setProperty("interactive", true);
            }
            break;
        }
        return !m_wasTouched && item == m_flickable;
    }

    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease: {
        setScrolling(false);
        if (!m_filterMouseEvents) {
            break;
        }
        if (static_cast<QMouseEvent *>(event)->source() == Qt::MouseEventNotSynthesized && item == m_flickable) {
            return true;
        }
        break;
    }

    case QEvent::HoverEnter:
    case QEvent::HoverMove: {
        if (!m_filterMouseEvents) {
            break;
        }
        if (m_wasTouched && (item == m_verticalScrollBar || item == m_horizontalScrollBar)) {
            if (m_verticalScrollBar) {
                m_verticalScrollBar->setProperty("interactive", true);
            }
            if (m_horizontalScrollBar) {
                m_horizontalScrollBar->setProperty("interactive", true);
            }
        }
        break;
    }

    case QEvent::KeyPress: {
        if (!m_keyNavigationEnabled) {
            break;
        }
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        bool horizontalScroll = keyEvent->modifiers() & m_defaultHorizontalScrollModifiers;
        switch (keyEvent->key()) {
        case Qt::Key_Up:
            return scrollUp();
        case Qt::Key_Down:
            return scrollDown();
        case Qt::Key_Left:
            return scrollLeft();
        case Qt::Key_Right:
            return scrollRight();
        case Qt::Key_PageUp:
            return horizontalScroll ? scrollLeft(pageWidth) : scrollUp(pageHeight);
        case Qt::Key_PageDown:
            return horizontalScroll ? scrollRight(pageWidth) : scrollDown(pageHeight);
        case Qt::Key_Home:
            return horizontalScroll ? scrollLeft(contentWidth) : scrollUp(contentHeight);
        case Qt::Key_End:
            return horizontalScroll ? scrollRight(contentWidth) : scrollDown(contentHeight);
        default:
            break;
        }
        break;
    }

    default:
        break;
    }

    return false;
}

#include "moc_wheelhandler.cpp"
