/*
 *   Copyright (C) 2017 by Dan Leinir Turthra Jensen <admin@leinir.dk>
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "zerocontrol.h"

#include <QDebug>
#include <QProcess>
#include <QTimer>

#include <klocalizedstring.h>

#include <sys/stat.h>
#include <signal.h>

class zerocontrol::Private {
public:
    Private(zerocontrol* qq)
        : q(qq)
        , systemZeronet(true)
        , zeronetPid(-1)
        , status(Unknown)
    {
        findZeronetPid();
        setWhatSuProgram();
        updateStatus();
    }
    zerocontrol* q;
    bool systemZeronet;
    QString zeronetLocation;

    void setWhatSuProgram() {
        whatSuProgram = "kdesu";
        if(QProcess::execute(whatSuProgram, QStringList() << "--version") < 0) {
            whatSuProgram = "kdesudo";
            if(QProcess::execute(whatSuProgram, QStringList() << "--version") < 0) {
                // this is a problem, what do?
                qDebug() << "No functioning kdesu or kdesudo was found. Please remidy this situation by installing one or the other";
            }
        }
    }
    QString whatSuProgram;
    QString runPrivilegedCommand(QStringList args)
    {
        QString output;
        QProcess cmd;
        cmd.start(whatSuProgram, args);
        if(cmd.waitForStarted()) {
            if(cmd.waitForFinished()) {
                output = cmd.readAll();
            }
        }
        return output;
    }

    Q_PID zeronetPid;
    void findZeronetPid() {
        // go through proc and see if anything there's zeronet
        QProcess findprocid;
        findprocid.start("ps", QStringList() << "-eo" << "pid,command");
        zeronetPid = -2;
        if(findprocid.waitForStarted()) {
            if(findprocid.waitForFinished()) {
                QStringList data = QString(findprocid.readAll()).split('\n');
                if(data.length() > 0) {
                    Q_FOREACH(const QString& item, data) {
                        if(item.contains("zeronet.py")) {
                            zeronetPid = item.split(" ").first().toInt();
                            break;
                        }
                    }
                }
            }
        }
    }

    bool useTor() {
        QProcess findprocid;
        findprocid.start("pidof", QStringList() << "tor");
        if(findprocid.waitForStarted()) {
            if(findprocid.waitForFinished()) {
                QStringList data = QString(findprocid.readAll()).split('\n');
                if(data.first().length() > 0) {
                    return true;
                }
            }
        }
        return false;
    }

    QTimer statusCheck;
    RunningStatus status;
    void updateStatus() {
        if(zeronetPid > 0) {
            status = Running;
            struct stat sts;
            QString procentry = QString("/proc/%1").arg(QString::number(zeronetPid));
            if (stat(procentry.toLatin1(), &sts) == -1 && errno == ENOENT) {
                // process doesn't exist
                status = NotRunning;
                zeronetPid = -2;
            }
        }
        else if(zeronetPid == -1) {
            // We've just started up, find out whether we've actually got a running zeronet instance or not...
            findZeronetPid();
        }
        else if(zeronetPid == -2) {
            // We've already been stopped, or we checked, so we know our status explicitly
            status = NotRunning;
        }
        emit q->statusChanged();
    }
};

zerocontrol::zerocontrol(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args)
    , d(new Private(this))
{
    connect(&d->statusCheck, &QTimer::timeout, [=](){ d->updateStatus(); });
    d->statusCheck.setInterval(5000);
    d->statusCheck.start();
}

zerocontrol::~zerocontrol()
{
    delete d;
}

zerocontrol::RunningStatus zerocontrol::status() const
{
    return d->status;
}

void zerocontrol::setStatus(zerocontrol::RunningStatus newStatus)
{
    d->updateStatus(); // Firstly, let's make sure out information is actually up to date
/// TODO Does system stuff make sense at all for zeronet? Should it always run as a user?
//     if(d->systemZeronet) {
//         switch(newStatus) {
//             case Running:
//                 // Start zeronet
//                 if(status() == NotRunning || status() == Unknown) {
//                     if(d->useTor()) {
//                         d->runPrivilegedCommand(QStringList() << "python" << "zeronet.py" << "--tor" << "always" << "--ui_ip" << "\"*\"");
//                     }
//                     else {
//                         d->runPrivilegedCommand(QStringList() << "python" << "zeronet.py" << "--ui_ip" << "\"*\"");
//                     }
//                     d->findZeronetPid();
//                 }
//                 break;
//             case NotRunning:
//                 // Stop zeronet
//                 if(status() == Running) {
//                     d->runPrivilegedCommand(QStringList() << "kill" << QString::number(d->zeronetPid));
//                     d->zeronetPid = -2;
//                 }
//                 break;
//             case Unknown:
//             default:
//                 // Do nothing, this doesn't make sense...
//                 qDebug() << "Request seting the status to Unknown? Not sure what that would mean at all...";
//                 break;
//         }
//     }
//     else {
        // a user-local zeronet instance, for non-admins
        // if running, send sigterm, otherwise start process
        if(status() == Running && newStatus == NotRunning) {
            kill(d->zeronetPid, SIGTERM);
            d->zeronetPid = -2;
        }
        else if((status() == NotRunning || status() == Unknown) && newStatus == Running) {
            if(d->useTor()) {
                QProcess::startDetached("python", QStringList() << "zeronet.py" << "--tor" << "always" << "--ui_ip" << "\"*\"", QString(), &d->zeronetPid);
            }
            else {
                QProcess::startDetached("python", QStringList() << "zeronet.py" << "--ui_ip" << "\"*\"", QString(), &d->zeronetPid);
            }
            qDebug() << "started zeronet with pid" << d->zeronetPid;
        }
//     }
    d->updateStatus();
}

QString zerocontrol::iconName() const
{
    QString icon;
    switch(status()) {
        case Running:
            icon = "process-stop";
            break;
        case NotRunning:
            icon = "system-run";
            break;
        case Unknown:
        default:
            icon = "unknown";
            break;
    }
    return icon;
}

QString zerocontrol::buttonLabel() const
{
    QString text;
    switch(status())
    {
        case Running:
            text = i18n("Stop ZeroNet");
            break;
        case NotRunning:
            text = i18n("Start ZeroNet");
            break;
        case Unknown:
        default:
            text = i18n("Please wait...");
            break;
    }
    return text;
}

bool zerocontrol::systemZeronet() const
{
    return d->systemZeronet;
}

void zerocontrol::setSystemZeronet(bool newValue)
{
    d->systemZeronet = newValue;
    emit systemZeronetChanged();
}

QString zerocontrol::zeronetLocation() const
{
    return d->zeronetLocation;
}

void zerocontrol::setZeronetLocation(QString newLocation)
{
    d->zeronetLocation = newLocation;
    emit zeronetLocationChanged();
}

K_EXPORT_PLASMA_APPLET_WITH_JSON(zerocontrol, zerocontrol, "metadata.json")

#include "zerocontrol.moc"
