#include "qlinuxinputkeyboardwatcher.h"
#include <QStringList>
#include <QDebug>
#include <linux/input.h>
#include <linux/kd.h>
#include <QCoreApplication>
#include <unistd.h>

QLinuxInputKeyboardWatcher::QLinuxInputKeyboardWatcher(const QString &key, const QString &specification) : m_orig_kbmode(K_XLATE)
{
    Q_UNUSED( key );
    bool useUDev = false;
    ttymode = false;
    QStringList args = specification.split(QLatin1Char(':'));
    foreach (const QString &arg, args) {
        if( arg.startsWith( "udev" ) && !arg.contains( "no" ) )
            useUDev = true;
        else if( arg.startsWith( "/dev/" ) )
            args.removeAll( arg );
	else if( arg == "ttymode" )
	    ttymode = true;
	    args.removeAll( arg );
    }

    modifyTty();
    connect( QCoreApplication::instance(), SIGNAL( aboutToQuit() ), this, SLOT( cleanup() ) );

    if( !useUDev ) {
        m_spec = specification;
        addKeyboard();
        return;
    }
    m_spec = args.join( ":" );
    m_udev = udev_new();
    if( !m_udev ) {
        qWarning() << "Failed to start udev!  Will attempt to open a keyboard without it";
        addKeyboard();
    }

    //Look for already attached devices:
    parseConnectedDevices();
    //Watch for device add/remove
    startWatching();
}

QLinuxInputKeyboardWatcher::~QLinuxInputKeyboardWatcher()
{
    cleanup();
}

void QLinuxInputKeyboardWatcher::cleanup()
{
    QLinuxInputKeyboardHandler *keyboard;
    QStringList devnodes = keyboards.keys();
    foreach( const QString &devnode, devnodes ) {
        keyboard = keyboards.value( devnode );
        keyboards.remove( devnode );
        if( keyboard )
            delete keyboard;
    }
    resetTty();
}

void QLinuxInputKeyboardWatcher::addKeyboard(QString devnode)
{
    QString specification = m_spec;
    if( !devnode.isEmpty() ) {
        specification.append( ":" );
        specification.append( devnode );
    } else
        devnode = "default";

    qWarning() << "Adding keyboard at" << devnode;
    if( ttymode && keyboards.count() == 0 )
        specification.append( ":ttymode" );

    if( keyboards.find( devnode ) == keyboards.end() ) {
        QLinuxInputKeyboardHandler *keyboard = new QLinuxInputKeyboardHandler( "LinuxInputKeyboard", specification );
        keyboards.insert( devnode, keyboard );
    } else {
        qWarning() << "device already added " << devnode;
    }
}

void QLinuxInputKeyboardWatcher::removeKeyboard(QString &devnode)
{
    if( keyboards.contains( devnode ) ) {
        qWarning() << "Removing keyboard at" << devnode;
        QLinuxInputKeyboardHandler *keyboard = keyboards.value( devnode );
        keyboards.remove( devnode );
        delete keyboard;
    }
}

void QLinuxInputKeyboardWatcher::modifyTty()
{
    if( !ttymode )
        return;
    m_tty_fd = isatty(0) ? 0 : -1;

    if (m_tty_fd >= 0) {
        qWarning() << "Modifying tty now..";
        // save tty config for restore.
        tcgetattr(m_tty_fd, &m_tty_attr);

        struct ::termios termdata;
        tcgetattr(m_tty_fd, &termdata);

        // record the original mode so we can restore it again in the destructor.
        ::ioctl(m_tty_fd, KDGKBMODE, &m_orig_kbmode);

        // setting this translation mode is even needed in INPUT mode to prevent
        // the shell from also interpreting codes, if the process has a tty
        // attached: e.g. Ctrl+C wouldn't copy, but kill the application.
        ::ioctl(m_tty_fd, KDSKBMODE, K_MEDIUMRAW);

        // set the tty layer to pass-through
        termdata.c_iflag = (IGNPAR | IGNBRK) & (~PARMRK) & (~ISTRIP);
//        termdata.c_oflag = 0;
        termdata.c_cflag = CREAD | CS8;
        termdata.c_lflag = 0;
        termdata.c_cc[VTIME]=0;
        termdata.c_cc[VMIN]=1;
        cfsetispeed(&termdata, 9600);
        cfsetospeed(&termdata, 9600);
        tcsetattr(m_tty_fd, TCSANOW, &termdata);
    }
}

void QLinuxInputKeyboardWatcher::resetTty()
{
    if( !ttymode )
        return;
    ::ioctl(m_tty_fd, KDSKBMODE, m_orig_kbmode);
    tcsetattr(m_tty_fd, TCSANOW, &m_tty_attr);
    ttymode = false;
}

void QLinuxInputKeyboardWatcher::startWatching()
{
    m_monitor = udev_monitor_new_from_netlink( m_udev, "udev" );
    udev_monitor_filter_add_match_subsystem_devtype( m_monitor, "input", NULL );
    udev_monitor_enable_receiving( m_monitor );
    m_monitor_fd = udev_monitor_get_fd( m_monitor );

    socketNotifier = new QSocketNotifier( m_monitor_fd, QSocketNotifier::Read, this );
    connect( socketNotifier, SIGNAL( activated(int) ), this, SLOT( deviceDetected() ) );
}

void QLinuxInputKeyboardWatcher::stopWatching()
{
    if( socketNotifier )
        delete socketNotifier;
    if( m_monitor )
        udev_monitor_unref( m_monitor );

    socketNotifier = 0;
    m_monitor = 0;
    m_monitor_fd = 0;
}

void QLinuxInputKeyboardWatcher::deviceDetected()
{
    struct udev_device *dev;
    dev = udev_monitor_receive_device( m_monitor );
    if( !dev )
        return;

    QString action = udev_device_get_action( dev );
//    qWarning() << "Device detected! Action is" << action;
    if( action == "add" )
        pokeDevice( dev );
    else {
        // We can't determine what the device was, so we handle false positives outside of this class
        QString str = udev_device_get_devnode( dev );
        if( !str.isEmpty() )
            removeKeyboard(str);
    }
}

void QLinuxInputKeyboardWatcher::parseConnectedDevices()
{
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev;
    const char *str;

    enumerate = udev_enumerate_new( m_udev );
    udev_enumerate_add_match_subsystem( enumerate, "input" );
    udev_enumerate_scan_devices( enumerate );
    devices = udev_enumerate_get_list_entry( enumerate );

    udev_list_entry_foreach( dev_list_entry, devices ) {
        str = udev_list_entry_get_name( dev_list_entry );
        dev = udev_device_new_from_syspath( m_udev, str );
        pokeDevice( dev );
    }

    udev_enumerate_unref( enumerate );
}

void QLinuxInputKeyboardWatcher::pokeDevice(udev_device *dev)
{
    const char *str;
    QString devnode;

    str = udev_device_get_devnode( dev );
    if( !str ) return;

    devnode = str;

    const char *ignore = udev_device_get_property_value( dev, "QTWS_IGNORE" );
    QByteArray ba = devnode.toLocal8Bit();
    if(ignore && strcmp(ignore, "0")) {
        qWarning() << "ignored device " << devnode;
        return;
    }

    dev = udev_device_get_parent_with_subsystem_devtype( dev, "input", NULL );
    if( !dev ) return;

    // don't add lircd device, this has already been added manually
    str = udev_device_get_sysattr_value( dev, "name" );
    printf("%s : sysattr name %s\n", __func__, str);
    if (!strcmp(str, "lircd")) return;

    str = udev_device_get_sysattr_value( dev, "capabilities/key" );
    QStringList val = QString(str).split(' ', QString::SkipEmptyParts);

    bool ok;
    unsigned long long keys = val.last().toULongLong( &ok, 16 );
    if( !ok ) return;

    bool test = ( val.length() > 1 || ( val.length() == 1 && keys != 0 ) );
    if( test ) {
        addKeyboard(devnode);
        return;
    }
}
