import QtQuick 2.14
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.13

Dialog {
    id: self
    property alias toolbar: toolbar_loader.sourceComponent
    property bool showRejectButton: true
    clip: true
    modal: true
    padding: 32
    topPadding: 32
    bottomPadding: 32
    leftPadding: 32
    rightPadding: 32
    horizontalPadding: 0
    verticalPadding: 16
    anchors.centerIn: parent
    parent: Overlay.overlay
    spacing: 0
    header: Pane {
        leftPadding: self.leftPadding
        rightPadding: 16
        topPadding: showRejectButton ? 16 : 32
        bottomPadding: 0
        background: Item {
        }
        contentItem: RowLayout {
            spacing: 16
            Image {
                source: self.icon
                sourceSize.width: 32
                sourceSize.height: 32
            }
            Label {
                Layout.fillWidth: true
                text: title
                font.capitalization: Font.Capitalize
                font.pixelSize: 18
                font.styleName: 'Medium'
            }
            Loader {
                id: toolbar_loader
            }
            ToolButton {
                visible: self.showRejectButton
                flat: true
                icon.source: 'qrc:/svg/cancel.svg'
                icon.width: 16
                icon.height: 16
                onClicked: self.reject()
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: qsTrId('id_cancel')
                ToolTip.visible: hovered
            }
        }
    }
    Overlay.modal: Rectangle {
        color: '#c0080B0E'
    }
    background: Rectangle {
        radius: 16
        color: constants.c700
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, 0.2)
    }

    property string icon: ""
}
