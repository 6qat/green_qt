import Blockstream.Green 0.1
import Blockstream.Green.Core 0.1
import QtQuick 2.13
import QtQuick.Controls 2.13
import QtQuick.Controls.Material 2.3
import QtQuick.Layouts 1.13
import QtQuick.Window 2.12

AbstractDialog {
    id: self
    required property Wallet wallet

    title: wallet.name

    focus: true

    Connections {
        target: self.wallet
        function onLoginAttemptsRemainingChanged(loginAttemptsRemaining) {
            pin_view.clear()
        }
    }

    header: Pane {
        leftPadding: 32
        rightPadding: 32
        topPadding: 16
        bottomPadding: 16
        background: Item {}
        RowLayout {
            spacing: 16
            Image {
                sourceSize.height: 32
                sourceSize.width: 32
                source: icons[self.wallet.network.id]
            }
            Label {
                Layout.fillWidth: true
                text: self.wallet.name
                font.capitalization: Font.Capitalize
                font.pixelSize: 18
                font.styleName: 'Medium'
            }
        }
    }
    contentItem: ColumnLayout {
        spacing: 8
        PinView {
            id: pin_view
            Layout.alignment: Qt.AlignHCenter
            enabled: self.wallet.authentication === Wallet.Unauthenticated && self.wallet.loginAttemptsRemaining > 0
            onPinChanged: {
                if (pin.valid) {
                    const proxy = Settings.useProxy ? Settings.proxyHost + ':' + Settings.proxyPort : ''
                    const use_tor = Settings.useTor
                    self.wallet.connect(proxy, use_tor);
                    self.wallet.loginWithPin(pin.value);
                }
            }
        }
        Label {
            Layout.topMargin: 8
            Layout.maximumWidth: 250
            Layout.minimumHeight: 25
            Layout.alignment: Qt.AlignHCenter
            opacity: self.wallet.authentication === Wallet.Unauthenticated ? 1 : 0
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            text: switch (self.wallet.loginAttemptsRemaining) {
                case 0: return qsTrId('id_no_attempts_remaining')
                case 1: return qsTrId('id_last_attempt_if_failed_you_will')
                case 2: return qsTrId('id_attempts_remaining_d').arg(self.wallet.loginAttemptsRemaining)
                default: return qsTrId('id_enter_pin')
            }
            Behavior on opacity { NumberAnimation {} }
        }
        Button {
            highlighted: true
            Layout.minimumWidth: 250
            Layout.minimumHeight: 25
            visible: self.wallet.loginAttemptsRemaining === 0
            text: 'Restore Wallet'
            onClicked: pushLocation(restore_dialog.location)
        }
    }
}
