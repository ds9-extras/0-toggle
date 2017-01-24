import QtQuick 2.0
import QtQuick.Controls 1.0 as QtControls
import QtQuick.Layouts 1.0 as QtLayouts
import QtQuick.Dialogs 1.2 as QtDialogs

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents

QtLayouts.ColumnLayout {
    id: pageColumn;
    property alias cfg_systemZeronet: systemZeronetField.checked;
    property alias cfg_zeronetLocation: zeronetLocationField.text;
    QtControls.CheckBox {
        id: systemZeronetField;
        text: i18n("Use system ZeroNet instance (requires super user access, aka sudo)");
    }
    QtControls.Label {
        text: i18n("Location of the ZeroNet installation");
    }
    QtControls.TextField {
        id: zeronetLocationField;
        QtLayouts.Layout.fillWidth: true
        QtControls.ToolButton {
            anchors {
                right: parent.right;
                verticalCenter: parent.verticalCenter;
                margins: units.smallSpacing;
            }
            text: "...";
            onClicked: pathDialog.open();
        }
        QtDialogs.FileDialog {
            id: pathDialog;
            title: i18n("Select ZeroNet location");
            selectFolder: true;
            folder: cfg_zeronetLocation;
            onAccepted: {
                plasmoid.configuration.zeronetLocation = folder;
            }
        }
    }
    QtControls.Label {
        QtLayouts.Layout.fillWidth: true
        wrapMode: Text.WordWrap;
        text: i18n("Please point this at the location of your ZeroNet download. You can follow the instructions found on <a href=\"https://zeronet.readthedocs.io/en/latest/using_zeronet/installing/\">this website</a>.");
        onLinkActivated: Qt.openUrlExternally(link);
        MouseArea {
            anchors.fill: parent;
            cursorShape: parent.hoveredLink != "" ? Qt.PointingHandCursor : Qt.ArrowCursor;
            acceptedButtons: Qt.NoButton;
        }
    }
    Item {
        QtLayouts.Layout.fillHeight: true
    }
}
