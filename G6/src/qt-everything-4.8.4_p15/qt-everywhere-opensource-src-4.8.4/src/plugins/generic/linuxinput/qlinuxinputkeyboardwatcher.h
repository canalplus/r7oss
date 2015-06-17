#ifndef QLINUXINPUTKEYBOARDWATCHER_H
#define QLINUXINPUTKEYBOARDWATCHER_H

#include <QObject>
#include <QHash>
#include <QSocketNotifier>
#include "qlinuxinput.h"

#include <libudev.h>
#include <termios.h>


class QLinuxInputKeyboardWatcher : public QObject
{
    Q_OBJECT
public:
    explicit QLinuxInputKeyboardWatcher(const QString &key, const QString &specification);
    ~QLinuxInputKeyboardWatcher();

signals:

public slots:

private:
    QString m_spec;
    QHash<QString,QLinuxInputKeyboardHandler*> keyboards;

    struct udev *m_udev;
    struct udev_monitor *m_monitor;
    int m_monitor_fd;
    QSocketNotifier *socketNotifier;
    bool ttymode;

    void addKeyboard( QString devnode = QString() );
    void removeKeyboard( QString &devnode );

    void modifyTty();
    void resetTty();
    int m_tty_fd;
    struct termios m_tty_attr;
    int m_orig_kbmode;

    void startWatching();
    void stopWatching();
    void parseConnectedDevices();
    void pokeDevice(struct udev_device *dev);

private slots:
    void deviceDetected();
    void cleanup();

};

#endif // QLINUXINPUTKEYBOARDWATCHER_H
