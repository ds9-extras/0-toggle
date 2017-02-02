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
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTimer>

#include <klocalizedstring.h>
#include <kformat.h>
#include <KTar>

// DO NOT LIKE! D:
#define KSharedConfigPtr KSharedConfigPtr_is_also_something_else
#include <kio/job.h>
#include <kio/copyjob.h>

#include <PackageKit/Daemon>

#include <sys/stat.h>

class zerocontrol::Private {
public:
    Private(zerocontrol* qq)
        : q(qq)
        , systemZeronet(false)
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
    QString workingOn;

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
        cmd.setWorkingDirectory(zeronetLocation);
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
                            zeronetPid = item.split(" ", QString::SkipEmptyParts).first().toInt();
                            break;
                        }
                    }
                }
            }
        }
        updateStatus();
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
    bool systemSanityCheck() {
        // check python install
        QProcess pythonTest;
        pythonTest.start("python", QStringList() << "--version");
        if(!(pythonTest.waitForStarted() && pythonTest.waitForFinished(1000))) {
//         if(true) {
            status = NoPython;
            // python does not function properly...
            return false;
        }
        // check zeronet availability
        if(!QFile::exists(QString("%1/zeronet.py").arg(zeronetLocation))) {
//         if(true) {
            status = NotZeronet;
            return false;
        }
        // If we get to here, then the system is sane
        return true;
    }
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
            if(systemSanityCheck()) {
                // We've just started up, find out whether we've actually got a running zeronet instance or not...
                findZeronetPid();
            }
        }
        else if(zeronetPid == -2) {
            if(systemSanityCheck()) {
                // We've already been stopped, or we checked, so we know our status explicitly
                status = NotRunning;
            }
        }
        emit q->statusChanged();
    }

    void downloadZeroNetResult(KJob* job) {
        if(job->error()) {
            endWorkingOn(i18n("Error downloading: %1").arg(job->errorString()), true);
        }
        else {
            workingOn = i18n("Decompressing ZeroNet...");
            emit q->workingOnChanged();
            KTar tarball(QString("%1.tar.gz").arg(zeronetLocation));
            if(tarball.open(QIODevice::ReadOnly)) {
                const KArchiveEntry* entry = tarball.directory()->entry("ZeroNet-master");
                if(entry && entry->isDirectory()) {
                    const KArchiveDirectory* dir = static_cast<const KArchiveDirectory*>(entry);
                    if(dir->copyTo(zeronetLocation)) {
                        endWorkingOn(i18n("Completed ZeroNet installation!"));
                        tarball.close();
                        QFile::remove(QString("%1.tar.gz").arg(zeronetLocation));
                        updateStatus();
                    }
                    else {
                        endWorkingOn(i18n("Failed to decompress the ZeroNet archive. The reported error was: %1").arg(tarball.errorString()), true);
                    }
                }
                else {
                    endWorkingOn(i18n("Archive is not the expected format. ZeroNet download would seem to be corrupted."), true);
                }
            }
            else {
                endWorkingOn(i18n("Failed to open downloaded ZeroNet archive for decompression."), true);
            }
        }
    }
    void endWorkingOn(QString message, bool isError = false) {
        if(isError) {
            qWarning() << "Work completed with an error!" << message;
        }
        else {
            qDebug() << "Work completed successfully:" << message;
        }
        workingOn = message;
        emit q->workingOnChanged();
        QTimer::singleShot(3000, q, [=](){ workingOn = ""; emit q->workingOnChanged(); });
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
            if(!QFile::exists(d->zeronetLocation)) {
                qDebug() << "The zeronet location setting is" << d->zeronetLocation << "but that directory does not exist";
            }
            if(d->useTor()) {
                QProcess::startDetached("python", QStringList() << "zeronet.py" << "--tor" << "always" << "--ui_ip" << "*", d->zeronetLocation, &d->zeronetPid);
            }
            else {
                QProcess::startDetached("python", QStringList() << "zeronet.py" << "--ui_ip" << "*", d->zeronetLocation, &d->zeronetPid);
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
    d->zeronetLocation = newLocation.replace("~", QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
    while(d->zeronetLocation.endsWith("/")) {
        d->zeronetLocation = d->zeronetLocation.left(d->zeronetLocation.length() - 1);
    }
    emit zeronetLocationChanged();
}

QString zerocontrol::workingOn() const
{
    return d->workingOn;
}

void zerocontrol::installZeroNet()
{
    QDir dir(d->zeronetLocation);
    if(dir.exists() && dir.entryList(QDir::AllEntries).count() > 2) {
        d->endWorkingOn(i18n("The zeronet location in settings (%1) already exists, and is not empty. Please either remove it, or ensure that it is empty.").arg(d->zeronetLocation), true);
    }
    else if(!dir.exists() && !dir.mkdir(d->zeronetLocation)) {
        d->endWorkingOn(i18n("The zeronet location in settings (%1) could not be created. Please ensure that it is set to a writeable location.").arg(d->zeronetLocation), true);
    }

    d->workingOn = i18n("Downloading ZeroNet...");
    emit workingOnChanged();
    QUrl source("https://github.com/HelloZeroNet/ZeroNet/archive/master.tar.gz");
    QUrl destination(QString("file://%1.tar.gz").arg(d->zeronetLocation));
    KIO::FileCopyJob* job = KIO::file_copy(source, destination, -1);
    connect(job, &KIO::Job::result, [job, this](){ d->downloadZeroNetResult(job); });
    // Would be lovely to do this with a lambda, but... processedAmount is ambiguously defined.
    connect(job, SIGNAL(processedAmount(KJob*, KJob::Unit, qulonglong)), this, SLOT(setProcessing(KJob*, KJob::Unit, qulonglong)));
    job->start();
}

void zerocontrol::setProcessing(KJob* /*job*/, KJob::Unit /*unit*/, qulonglong percent)
{
    KFormat format;
    d->workingOn = i18n("Downloading ZeroNet (%1)").arg(format.formatByteSize(percent));
    emit workingOnChanged();
}

void zerocontrol::installPython()
{
    qDebug() << "Attempting to install python";
    QStringList packages = QString(PYTHON_PACKAGE_NAMES).split(" ");
    auto resolveTransaction = PackageKit::Daemon::global()->resolve(packages, PackageKit::Transaction::FilterArch);
    Q_ASSERT(resolveTransaction);

    connect(resolveTransaction, &PackageKit::Transaction::percentageChanged, resolveTransaction, [this, resolveTransaction](){
        if(resolveTransaction->percentage() < 101) {
            d->workingOn = i18n("Installing python (%1%)").arg(QString::number(resolveTransaction->percentage()));
        }
        else {
            d->workingOn = i18n("Installing python...");
        }
        emit workingOnChanged();
    });
    QHash<QString, QString>* pkgs = new QHash<QString, QString>();
    QObject::connect(resolveTransaction, &PackageKit::Transaction::package, resolveTransaction, [pkgs](PackageKit::Transaction::Info info, const QString &packageID, const QString &/*summary*/) {
        if (info == PackageKit::Transaction::InfoAvailable) {
            pkgs->insert(PackageKit::Daemon::packageName(packageID), packageID);
        }
        qDebug() << "resolved package"  << info << packageID;
    });

    QObject::connect(resolveTransaction, &PackageKit::Transaction::finished, resolveTransaction, [this, pkgs, resolveTransaction](PackageKit::Transaction::Exit status) {
        if (status != PackageKit::Transaction::ExitSuccess) {
            d->endWorkingOn("Python is not available, or has an unexpected name. Please install it manually.", true);
            qWarning() << "resolve failed" << status;
            delete pkgs;
            resolveTransaction->deleteLater();
            return;
        }
        QStringList pkgids = pkgs->values();
        delete pkgs;

        if (pkgids.isEmpty()) {
            d->endWorkingOn("There was nothing new to install.");
            qDebug() << "Nothing to install";
        } else {
            qDebug() << "installing..." << pkgids;
            pkgids.removeDuplicates();
            auto installTransaction = PackageKit::Daemon::global()->installPackages(pkgids);
            QObject::connect(installTransaction, &PackageKit::Transaction::percentageChanged, qApp, [this, installTransaction]() {
                if(installTransaction->percentage() < 101) {
                    d->workingOn = i18n("Installing python (%1%)").arg(QString::number(installTransaction->percentage()));
                }
                else {
                    d->workingOn = i18n("Installing python...");
                }
                emit workingOnChanged();
            });
            QObject::connect(installTransaction, &PackageKit::Transaction::finished, qApp, [this, installTransaction](PackageKit::Transaction::Exit status) {
                if(status == PackageKit::Transaction::ExitSuccess) {
                    d->endWorkingOn("Installed Python");
                }
                else {
                    d->endWorkingOn("Python installation failed!", true);
                }
                installTransaction->deleteLater();
            });
        }
        resolveTransaction->deleteLater();
    });
}

K_EXPORT_PLASMA_APPLET_WITH_JSON(zerocontrol, zerocontrol, "metadata.json")

#include "zerocontrol.moc"
