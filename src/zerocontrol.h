/*
 *   Copyright (C) 2017 by Dan Leinir Turthra Jensen <admin@leinir.dk>                  *
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

#ifndef ZEROCONTROL_H
#define ZEROCONTROL_H


#include <Plasma/Applet>

class zerocontrol : public Plasma::Applet
{
    Q_OBJECT
    Q_ENUMS(RunningStatus)
    Q_PROPERTY(QString buttonLabel READ buttonLabel NOTIFY statusChanged)
    Q_PROPERTY(RunningStatus status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY statusChanged)
    Q_PROPERTY(bool systemZeronet READ systemZeronet WRITE setSystemZeronet NOTIFY systemZeronetChanged)
    Q_PROPERTY(QString zeronetLocation READ zeronetLocation WRITE setZeronetLocation NOTIFY zeronetLocationChanged)

    Q_PROPERTY(QString workingOn READ workingOn NOTIFY workingOnChanged);
public:
    zerocontrol( QObject *parent, const QVariantList &args );
    ~zerocontrol();

    enum RunningStatus {
        Unknown = 0,
        Running = 1,
        NotRunning = 2,
        NotZeronet = 3,
        NoPython = 4
    };
    RunningStatus status() const;
    // NOTE This will attempt to set the status, but given the nature of
    // these things, there is no guarantee that the application will, in fact,
    // successfully start up (or shut down), or the timing of this operation.
    void setStatus(RunningStatus newStatus);
    Q_SIGNAL void statusChanged();

    QString iconName() const;
    QString buttonLabel() const;

    bool systemZeronet() const;
    void setSystemZeronet(bool newValue);
    Q_SIGNAL void systemZeronetChanged();

    QString zeronetLocation() const;
    void setZeronetLocation(QString newLocation);
    Q_SIGNAL void zeronetLocationChanged();

    QString workingOn() const;
    Q_SIGNAL void workingOnChanged();

    Q_INVOKABLE void installZeroNet();
    Q_INVOKABLE void installPython();

    Q_SLOT void setProcessing(KJob* job, KJob::Unit unit, qulonglong percent);
private:
    class Private;
    Private* d;
};

#endif
