import Blockstream.Green.Core 0.1
import QtQuick 2.13
import QtQuick.Controls 2.13
import QtQuick.Controls.Material 2.3
import QtQuick.Layouts 1.13
import QtQuick.Window 2.12

Button {
    id: self
    required property string location
    property int count: 0
    property bool isCurrent: Window.window.location === location
    property bool busy: false
    topPadding: 8
    bottomPadding: 8
    leftPadding: 16
    rightPadding: 16
    topInset: 0
    leftInset: 0
    rightInset: 0
    bottomInset: 0

    Layout.fillWidth: true
    onClicked: pushLocation(location)
    icon.width: 24
    icon.height: 24
    Behavior on implicitWidth { SmoothedAnimation { velocity: 600 } }
    background: Item {
        Rectangle {
            anchors.fill: parent
            visible: self.isCurrent
            color: constants.c500
            radius: 4
        }
        Rectangle {
            anchors.fill: parent
            visible: self.hovered
            color: constants.c600
            radius: 4
        }
    }
    contentItem: RowLayout {
        spacing: 12
        Image {
            source: self.icon.source
            sourceSize.width: self.icon.width
            sourceSize.height: self.icon.height
        }
        Label {
            visible: !Settings.collapseSideBar
            text: self.text
            Layout.fillWidth: true
            rightPadding: 16
            font.styleName: 'Regular'
        }
        ProgressBar {
            visible: !Settings.collapseSideBar
            indeterminate: self.busy
            opacity: self.busy ? 1 : 0
            Layout.minimumWidth: 16
            Layout.maximumWidth: 16
        }
        Label {
            visible: !Settings.collapseSideBar
            opacity: count > 0
            text: count
            color: '#444444'
            font.pixelSize: 12
            font.styleName: 'Medium'
            horizontalAlignment: Label.AlignHCenter
            leftPadding: 6
            rightPadding: 6
            background: Rectangle {
                color: 'white'
                radius: 4
            }
        }
    }
}
