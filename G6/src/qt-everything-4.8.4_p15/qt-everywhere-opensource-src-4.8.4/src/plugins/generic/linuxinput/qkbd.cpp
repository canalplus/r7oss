/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial Usage
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qkbd.h"
#include "qkbd_p.h"

#ifndef QT_NO_KEYBOARD

#include <QFile>
#include <QDataStream>
#include <QStringList>

#include <qwindowsysteminterface_qpa.h>
#include "qtimer.h"
#include <stdlib.h>

#include <qdebug.h>

//#define QT_DEBUG_KEYMAP


QT_BEGIN_NAMESPACE

class QLinuxInputKeycodeHandlerPrivate : public QObject
{
    Q_OBJECT
public:
    QLinuxInputKeycodeHandlerPrivate(QLinuxInputKeycodeHandler *h, const QString &device)
        : m_handler(h), m_modifiers(0), m_composing(0), m_dead_unicode(0xffff),
          m_no_zap(false), m_do_compose(false),
          m_keymap(0), m_keymap_size(0), m_keycompose(0), m_keycompose_size(0)
    {
        m_ar_timer = new QTimer(this);
        m_ar_timer->setSingleShot(true);
        connect(m_ar_timer, SIGNAL(timeout()), SLOT(autoRepeat()));
        m_ar_delay = 400;
        m_ar_period = 80;

        memset(m_locks, 0, sizeof(m_locks));

        QString keymap;
        QStringList args = device.split(QLatin1Char(':'));
        foreach (const QString &arg, args) {
            if (arg.startsWith(QLatin1String("keymap=")))
                keymap = arg.mid(7);
            else if (arg == QLatin1String("disable-zap"))
                m_no_zap = true;
            else if (arg == QLatin1String("enable-compose"))
                m_do_compose = true;
            else if (arg.startsWith(QLatin1String("repeat-delay=")))
                m_ar_delay = arg.mid(13).toInt();
            else if (arg.startsWith(QLatin1String("repeat-rate=")))
                m_ar_period = arg.mid(12).toInt();
        }

        if (keymap.isEmpty() || !loadKeymap(keymap))
            unloadKeymap();
    }

    ~QLinuxInputKeycodeHandlerPrivate()
    {
        unloadKeymap();
    }

    void beginAutoRepeat(int uni, int code, Qt::KeyboardModifiers mod)
    {
        m_ar_unicode = uni;
        m_ar_keycode = code;
        m_ar_modifier = mod;
        m_ar_timer->start(m_ar_delay);
    }

    void endAutoRepeat()
    {
        m_ar_timer->stop();
    }

    static Qt::KeyboardModifiers toQtModifiers(quint8 mod)
    {
        Qt::KeyboardModifiers qtmod = Qt::NoModifier;

        if (mod & (QLinuxInputKeyboardMap::ModShift | QLinuxInputKeyboardMap::ModShiftL | QLinuxInputKeyboardMap::ModShiftR))
            qtmod |= Qt::ShiftModifier;
        if (mod & (QLinuxInputKeyboardMap::ModControl | QLinuxInputKeyboardMap::ModCtrlL | QLinuxInputKeyboardMap::ModCtrlR))
            qtmod |= Qt::ControlModifier;
        if (mod & QLinuxInputKeyboardMap::ModAlt)
            qtmod |= Qt::AltModifier;

        return qtmod;
    }

    void unloadKeymap();
    bool loadKeymap(const QString &file);

private slots:
    void autoRepeat()
    {
        m_handler->processKeyEvent(m_ar_unicode, m_ar_keycode, m_ar_modifier, false, true);
        m_handler->processKeyEvent(m_ar_unicode, m_ar_keycode, m_ar_modifier, true, true);
        m_ar_timer->start(m_ar_period);
    }

private:
    QLinuxInputKeycodeHandler *m_handler;

    // auto repeat simulation
    int m_ar_unicode;
    int m_ar_keycode;
    Qt::KeyboardModifiers m_ar_modifier;
    int m_ar_delay;
    int m_ar_period;
    QTimer *m_ar_timer;

    // keymap handling
    quint8 m_modifiers;
    quint8 m_locks[3];
    int m_composing;
    quint16 m_dead_unicode;

    bool m_no_zap;
    bool m_do_compose;

    const QLinuxInputKeyboardMap::Mapping *m_keymap;
    int m_keymap_size;
    const QLinuxInputKeyboardMap::Composing *m_keycompose;
    int m_keycompose_size;

    static const QLinuxInputKeyboardMap::Mapping s_keymap_default[];
    static const QLinuxInputKeyboardMap::Composing s_keycompose_default[];

    friend class QLinuxInputKeycodeHandler;
};

// simple builtin US keymap
#include "qkbd_defaultmap_p.h"

// the unloadKeymap() function needs to be AFTER the defaultmap include,
// since the sizeof(s_keymap_default) wouldn't work otherwise.

void QLinuxInputKeycodeHandlerPrivate::unloadKeymap()
{
    if (m_keymap && m_keymap != s_keymap_default)
        delete [] m_keymap;
    if (m_keycompose && m_keycompose != s_keycompose_default)
        delete [] m_keycompose;

    m_keymap = s_keymap_default;
    m_keymap_size = sizeof(s_keymap_default) / sizeof(s_keymap_default[0]);
    m_keycompose = s_keycompose_default;
    m_keycompose_size = sizeof(s_keycompose_default) / sizeof(s_keycompose_default[0]);

    // reset state, so we could switch keymaps at runtime
    m_modifiers = 0;
    memset(m_locks, 0, sizeof(m_locks));
    m_composing = 0;
    m_dead_unicode = 0xffff;
}

bool QLinuxInputKeycodeHandlerPrivate::loadKeymap(const QString &file)
{
    QFile f(file);

    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("Could not open keymap file '%s'", qPrintable(file));
        return false;
    }

    // .qmap files have a very simple structure:
    // quint32 magic           (QKeyboard::FileMagic)
    // quint32 version         (1)
    // quint32 keymap_size     (# of struct QKeyboard::Mappings)
    // quint32 keycompose_size (# of struct QKeyboard::Composings)
    // all QKeyboard::Mappings via QDataStream::operator(<<|>>)
    // all QKeyboard::Composings via QDataStream::operator(<<|>>)

    quint32 qmap_magic, qmap_version, qmap_keymap_size, qmap_keycompose_size;

    QDataStream ds(&f);

    ds >> qmap_magic >> qmap_version >> qmap_keymap_size >> qmap_keycompose_size;

    if (ds.status() != QDataStream::Ok || qmap_magic != QLinuxInputKeyboardMap::FileMagic || qmap_version != 1 || qmap_keymap_size == 0) {
        qWarning("'%s' is ot a valid.qmap keymap file.", qPrintable(file));
        return false;
    }

    QLinuxInputKeyboardMap::Mapping *qmap_keymap = new QLinuxInputKeyboardMap::Mapping[qmap_keymap_size];
    QLinuxInputKeyboardMap::Composing *qmap_keycompose = qmap_keycompose_size ? new QLinuxInputKeyboardMap::Composing[qmap_keycompose_size] : 0;

    for (quint32 i = 0; i < qmap_keymap_size; ++i)
        ds >> qmap_keymap[i];
    for (quint32 i = 0; i < qmap_keycompose_size; ++i)
        ds >> qmap_keycompose[i];

    if (ds.status() != QDataStream::Ok) {
        delete [] qmap_keymap;
        delete [] qmap_keycompose;

        qWarning("Keymap file '%s' can not be loaded.", qPrintable(file));
        return false;
    }

    // unload currently active and clear state
    unloadKeymap();

    m_keymap = qmap_keymap;
    m_keymap_size = qmap_keymap_size;
    m_keycompose = qmap_keycompose;
    m_keycompose_size = qmap_keycompose_size;

    m_do_compose = true;

    return true;
}


QLinuxInputKeycodeHandler::QLinuxInputKeycodeHandler(const QString &device)
{
    d = new QLinuxInputKeycodeHandlerPrivate(this, device);
}

/*!
    \overload
*/
QLinuxInputKeycodeHandler::QLinuxInputKeycodeHandler()
{
    d = new QLinuxInputKeycodeHandlerPrivate(this, QString());
}



QLinuxInputKeycodeHandler::~QLinuxInputKeycodeHandler()
{
    delete d;
}

void QLinuxInputKeycodeHandler::processKeyEvent(int unicode, int keycode, Qt::KeyboardModifiers modifiers,
                        bool isPress, bool autoRepeat)
{
    QWindowSystemInterface::handleKeyEvent( 0, ( isPress ? QEvent::KeyPress : QEvent::KeyRelease ), keycode, modifiers, QString( unicode ), autoRepeat );
}

void QLinuxInputKeycodeHandler::beginAutoRepeat(int uni, int code, Qt::KeyboardModifiers mod)
{
    d->beginAutoRepeat(uni, code, mod);
}

void QLinuxInputKeycodeHandler::endAutoRepeat()
{
    d->endAutoRepeat();
}

QLinuxInputKeycodeHandler::KeycodeAction QLinuxInputKeycodeHandler::processKeycode(quint16 keycode, bool pressed, bool autorepeat)
{
    KeycodeAction result = None;
    bool first_press = pressed && !autorepeat;

    const QLinuxInputKeyboardMap::Mapping *map_plain = 0;
    const QLinuxInputKeyboardMap::Mapping *map_withmod = 0;

    // get a specific and plain mapping for the keycode and the current modifiers
    for (int i = 0; i < d->m_keymap_size && !(map_plain && map_withmod); ++i) {
        const QLinuxInputKeyboardMap::Mapping *m = d->m_keymap + i;
        if (m->keycode == keycode) {
            if (m->modifiers == 0)
                map_plain = m;

            quint8 testmods = d->m_modifiers;
            if (d->m_locks[0] /*CapsLock*/ && (m->flags & QLinuxInputKeyboardMap::IsLetter))
                testmods ^= QLinuxInputKeyboardMap::ModShift;
            if (m->modifiers == testmods)
                map_withmod = m;
        }
    }

#ifdef QT_DEBUG_KEYMAP
    qWarning("Processing key event: keycode=%3d, modifiers=%02x pressed=%d, autorepeat=%d  |  plain=%d, withmod=%d, size=%d", \
             keycode, d->m_modifiers, pressed ? 1 : 0, autorepeat ? 1 : 0, \
             map_plain ? map_plain - d->m_keymap : -1, \
             map_withmod ? map_withmod - d->m_keymap : -1, \
             d->m_keymap_size);
#endif

    const QLinuxInputKeyboardMap::Mapping *it = map_withmod ? map_withmod : map_plain;

    if (!it) {
#ifdef QT_DEBUG_KEYMAP
        // we couldn't even find a plain mapping
        qWarning("Could not find a suitable mapping for keycode: %3d, modifiers: %02x", keycode, d->m_modifiers);
#endif
        return result;
    }

    bool skip = false;
    quint16 unicode = it->unicode;
    quint32 qtcode = it->qtcode;

    if ((it->flags & QLinuxInputKeyboardMap::IsModifier) && it->special) {
        // this is a modifier, i.e. Shift, Alt, ...
        if (pressed)
            d->m_modifiers |= quint8(it->special);
        else
            d->m_modifiers &= ~quint8(it->special);
    } else if (qtcode >= Qt::Key_CapsLock && qtcode <= Qt::Key_ScrollLock) {
        // (Caps|Num|Scroll)Lock
        if (first_press) {
            quint8 &lock = d->m_locks[qtcode - Qt::Key_CapsLock];
            lock ^= 1;

            switch (qtcode) {
            case Qt::Key_CapsLock  : result = lock ? CapsLockOn : CapsLockOff; d->m_modifiers ^= QLinuxInputKeyboardMap::ModShift; break;
            case Qt::Key_NumLock   : result = lock ? NumLockOn : NumLockOff; break;
            case Qt::Key_ScrollLock: result = lock ? ScrollLockOn : ScrollLockOff; break;
            default                : break;
            }
        }
    } else if ((it->flags & QLinuxInputKeyboardMap::IsSystem) && it->special && first_press) {
        switch (it->special) {
        case QLinuxInputKeyboardMap::SystemReboot:
            result = Reboot;
            break;

        case QLinuxInputKeyboardMap::SystemZap:
            if (!d->m_no_zap)
                qApp->quit();
            break;

        case QLinuxInputKeyboardMap::SystemConsolePrevious:
            result = PreviousConsole;
            break;

        case QLinuxInputKeyboardMap::SystemConsoleNext:
            result = NextConsole;
            break;

        default:
            if (it->special >= QLinuxInputKeyboardMap::SystemConsoleFirst &&
                it->special <= QLinuxInputKeyboardMap::SystemConsoleLast) {
                result = KeycodeAction(SwitchConsoleFirst + ((it->special & QLinuxInputKeyboardMap::SystemConsoleMask) & SwitchConsoleMask));
            }
            break;
        }

        skip = true; // no need to tell Qt about it
    } else if ((qtcode == Qt::Key_Multi_key) && d->m_do_compose) {
        // the Compose key was pressed
        if (first_press)
            d->m_composing = 2;
        skip = true;
    } else if ((it->flags & QLinuxInputKeyboardMap::IsDead) && d->m_do_compose) {
        // a Dead key was pressed
        if (first_press && d->m_composing == 1 && d->m_dead_unicode == unicode) { // twice
            d->m_composing = 0;
            qtcode = Qt::Key_unknown; // otherwise it would be Qt::Key_Dead...
        } else if (first_press && unicode != 0xffff) {
            d->m_dead_unicode = unicode;
            d->m_composing = 1;
            skip = true;
        } else {
            skip = true;
        }
    }

    if (!skip) {
        // a normal key was pressed
        const int modmask = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::KeypadModifier;

        // we couldn't find a specific mapping for the current modifiers,
        // or that mapping didn't have special modifiers:
        // so just report the plain mapping with additional modifiers.
        if ((it == map_plain && it != map_withmod) ||
            (map_withmod && !(map_withmod->qtcode & modmask))) {
            qtcode |= QLinuxInputKeycodeHandlerPrivate::toQtModifiers(d->m_modifiers);
        }

        if (d->m_composing == 2 && first_press && !(it->flags & QLinuxInputKeyboardMap::IsModifier)) {
            // the last key press was the Compose key
            if (unicode != 0xffff) {
                int idx = 0;
                // check if this code is in the compose table at all
                for ( ; idx < d->m_keycompose_size; ++idx) {
                    if (d->m_keycompose[idx].first == unicode)
                        break;
                }
                if (idx < d->m_keycompose_size) {
                    // found it -> simulate a Dead key press
                    d->m_dead_unicode = unicode;
                    unicode = 0xffff;
                    d->m_composing = 1;
                    skip = true;
                } else {
                    d->m_composing = 0;
                }
            } else {
                d->m_composing = 0;
            }
        } else if (d->m_composing == 1 && first_press && !(it->flags & QLinuxInputKeyboardMap::IsModifier)) {
            // the last key press was a Dead key
            bool valid = false;
            if (unicode != 0xffff) {
                int idx = 0;
                // check if this code is in the compose table at all
                for ( ; idx < d->m_keycompose_size; ++idx) {
                    if (d->m_keycompose[idx].first == d->m_dead_unicode && d->m_keycompose[idx].second == unicode)
                        break;
                }
                if (idx < d->m_keycompose_size) {
                    quint16 composed = d->m_keycompose[idx].result;
                    if (composed != 0xffff) {
                        unicode = composed;
                        qtcode = Qt::Key_unknown;
                        valid = true;
                    }
                }
            }
            if (!valid) {
                unicode = d->m_dead_unicode;
                qtcode = Qt::Key_unknown;
            }
            d->m_composing = 0;
        }

        if (!skip) {
#ifdef QT_DEBUG_KEYMAP
            qWarning("Processing: uni=%04x, qt=%08x, qtmod=%08x", unicode, qtcode & ~modmask, (qtcode & modmask));
#endif

            // send the result to the server
            processKeyEvent(unicode, qtcode & ~modmask, Qt::KeyboardModifiers(qtcode & modmask), pressed, autorepeat);
        }
    }
    return result;
}

QT_END_NAMESPACE

#include "qkbd.moc"

#endif // QT_NO_KEYBOARD
