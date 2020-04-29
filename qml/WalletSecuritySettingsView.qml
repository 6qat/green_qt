import Blockstream.Green 0.1
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5
import QtQuick.Controls.Material 2.3
import QtQuick.Layouts 1.12

ColumnLayout {
    property string title: qsTrId('id_security')

    spacing: 30

    SettingsController {
        id: controller
    }

    SettingsBox {
        title: qsTrId('id_access')
        description: qsTrId('id_enable_or_change_your_pin_to')

        Button {
            Component {
                id: change_pin_dialog
                ChangePinDialog {
                    modal: true
                    anchors.centerIn: parent
                }
            }
            flat: true
            text: qsTrId('id_change_pin')
            onClicked: change_pin_dialog.createObject(Window.window).open()
        }

    }

    SettingsBox {
        title: qsTrId('id_auto_logout_timeout')
        description: qsTrId('id_set_a_timeout_to_logout_after')
        enabled: !wallet.locked

        ComboBox {
            flat: true
            model: [1, 2, 5, 10, 60]
            width: 200
            delegate: ItemDelegate {
                width: parent.width
                text: qsTrId('id_1d_minutes').arg(modelData)
            }
            displayText: qsTrId('id_1d_minutes').arg(currentText)
            onCurrentTextChanged: controller.change({ altimeout: model[currentIndex] })
            currentIndex: model.indexOf(wallet.settings.altimeout)
        }
    }


    /*SettingsBox {
        title: qsTrId('id_watchonly_login')
        description: qsTrId('id_set_up_credentials_to_access_in')

        RowLayout {
            Switch {
                onCheckedChanged: credentials.visible = checked
            }

            ColumnLayout {

                id: credentials
                visible: false

                TextField {
                    placeholderText: qsTrId('id_username')
                    padding: 10
                }

                TextField {
                    placeholderText: qsTrId('id_password')
                    padding: 10
                }
            }
        }
    }*/

    SettingsBox {
        title: qsTrId('id_wallet_backup')
        description: qsTrId('id_your_wallet_backup_is_made_of') + "\n" + qsTrId('id_blockstream_does_not_have')

        RowLayout {
            Layout.fillWidth: true

            Component {
                id: mnemonic_dialog
                MnemonicDialog {
                    anchors.centerIn: parent
                }
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
        title: qsTrId('id_twofactor_authentication')
        description: qsTrId('id_enable_twofactor_authentication')
        enabled: !wallet.locked

        ColumnLayout {
            Component {
                id: enable_dialog
                TwoFactorEnableDialog {
                    description: switch(method) {
                                case 'sms':
                                    return qsTrId('id_enter_phone_number')
                                case 'gauth':
                                    return qsTrId('id_scan_the_qr_code_in_google')
                                case 'email':
                                    return qsTrId('id_enter_your_email_address')
                                case 'phone':
                                    return qsTrId('id_enter_phone_number')

                            }
                }
            }
            Component {
                id: disable_dialog
                TwoFactorDisableDialog { }
            }
            Repeater {
                model: wallet.config.all_methods || []

                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight
                    property string method: modelData

                    Image {
                        source: `svg/2fa_${method}.svg`
                        sourceSize.height: 32
                    }
                    Label {
                        text: method.toUpperCase()
                        Layout.minimumWidth: 64
                    }
                    Switch {
                        Binding on checked {
                            value: wallet.config[method].enabled
                        }

                        onClicked: {
                            checked = wallet.config[modelData].enabled;
                            if (!wallet.config[method].enabled) {
                                enable_dialog.createObject(stack_view, { method }).open();
                            } else {
                                disable_dialog.createObject(stack_view, { method }).open();
                            }
                        }
                    }
                }
            }
        }
    }

    SettingsBox {
        title: qsTrId('id_set_twofactor_threshold')
        description: qsTrId('id_set_a_limit_to_spend_without')
        enabled: !wallet.locked
        visible: !wallet.network.liquid

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Item {
                Layout.fillWidth: true
            }
            Button {
                property Component dialog: TwoFactorLimitDialog {}
                flat: true
                text: qsTrId('id_set_twofactor_threshold')
                onClicked: dialog.createObject(stack_view).open()
                Layout.alignment: Qt.AlignRight
            }
        }
    }


}