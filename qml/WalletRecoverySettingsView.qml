import Blockstream.Green 0.1
import Blockstream.Green.Core 0.1
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5
import QtQuick.Controls.Material 2.3
import QtQuick.Layouts 1.12

ColumnLayout {
    id: self
    required property Wallet wallet
    spacing: 16

    SettingsBox {
        title: qsTrId('id_wallet_backup')
        visible: !wallet.device
        contentItem: ColumnLayout {
            Label {
                Layout.fillWidth: true
                text: qsTrId('id_your_wallet_backup_is_made_of') + "\n" + qsTrId('id_blockstream_does_not_have')
                wrapMode: Label.WordWrap
            }
            Button {
                Layout.alignment: Qt.AlignRight
                flat: true
                text: qsTrId('id_show_my_wallet_backup')
                onClicked: mnemonic_dialog.createObject(stack_view).open()
            }
        }
    }

    SettingsBox {
        title: qsTrId('id_accounts_summary')
        contentItem: ColumnLayout {
            Label {
                Layout.fillWidth: true
                text: qsTrId('id_save_a_summary_of_your_accounts')
                wrapMode: Label.WordWrap
            }
            Button {
                Layout.alignment: Qt.AlignRight
                text: qsTrId('id_copy_to_clipboard')
                flat: true
                onClicked: {
                    const subaccounts = [];
                    for (let i = 0; i < wallet.accounts.length; i++) {
                        const data = wallet.accounts[i].json;
                        const subaccount = { type: data.type, pointer: data.pointer };
                        if (data.type === '2of3') subaccount.recovery_pub_key = data.recovery_pub_key;
                        subaccounts.push(subaccount)
                    }
                    Clipboard.copy(JSON.stringify({ subaccounts }, null, '  '))
                    ToolTip.show(qsTrId('id_copied_to_clipboard'), 1000);
                }
            }
        }
    }

    SettingsBox {
        title: qsTrId('id_set_timelock')
        visible: !wallet.network.liquid
        enabled: !!wallet.settings.notifications &&
                 !!wallet.settings.notifications.email_incoming &&
                 !!wallet.settings.notifications.email_outgoing
        contentItem: ColumnLayout {
            Label {
                Layout.fillWidth: true
                text: qsTrId('id_redeem_your_deposited_funds') + '\n\n' + qsTrId('id_enable_email_notifications_to')
                wrapMode: Label.WordWrap
            }
            Button {
                Layout.alignment: Qt.AlignRight
                flat: true
                text: qsTrId('id_set_timelock')
                onClicked: nlocktime_dialog.createObject(stack_view).open()
            }
        }
    }

    SettingsBox {
        title: qsTrId('id_set_an_email_for_recovery')
        visible: !wallet.network.liquid
        contentItem: ColumnLayout {
            Label {
                Layout.fillWidth: true
                text: qsTrId('id_set_up_an_email_to_get')
                wrapMode: Label.WordWrap
            }
            Button {
                Layout.alignment: Qt.AlignRight
                Component {
                    id: enable_dialog
                    SetRecoveryEmailDialog {
                        wallet: self.wallet
                    }
                }
                flat: true
                enabled: !wallet.config.email || !wallet.config.email.confirmed
                text: qsTrId('id_enable')
                onClicked: enable_dialog.createObject(stack_view).open()
            }
        }
    }

    Component {
        id: mnemonic_dialog
        MnemonicDialog {
            anchors.centerIn: parent
            wallet: self.wallet
        }
    }

    Component {
        id: nlocktime_dialog
        NLockTimeDialog {
            wallet: self.wallet
        }
    }
}
