#include "ga.h"
#include "json.h"
#include "network.h"
#include "networkmanager.h"
#include "session.h"
#include "util.h"
#include "wallet.h"
#include "walletmanager.h"

#include <QDir>
#include <QJsonDocument>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QDirIterator>
#include <QUuid>

#include <gdk.h>

WalletManager::WalletManager()
{
    auto config = Json::fromObject({{ "datadir", GetDataDir("gdk") }});
    GA_init(config.get());

    QDirIterator it(GetDataDir("wallets"));
    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QFile::ReadOnly)) continue;
        QJsonParseError parser_error;
        auto doc = QJsonDocument::fromJson(file.readAll(), &parser_error);
        if (parser_error.error != QJsonParseError::NoError) continue;
        if (!doc.isObject()) continue;
        auto data = doc.object();
        Wallet* wallet = new Wallet(this);
        wallet->m_id = QFileInfo(file).baseName();
        wallet->m_proxy = data.value("proxy").toString("");
        wallet->m_use_tor = data.value("use_tor").toBool(false);
        wallet->m_pin_data = QByteArray::fromBase64(data.value("pin_data").toString().toLocal8Bit());
        wallet->m_name = data.value("name").toString();
        wallet->m_network = NetworkManager::instance()->network(data.value("network").toString());
        wallet->m_login_attempts_remaining = data.value("login_attempts_remaining").toInt();
        addWallet(wallet);
    }
}

WalletManager *WalletManager::instance()
{
    static WalletManager wallet_manager;
    return &wallet_manager;
}

void WalletManager::addWallet(Wallet* wallet)
{
    m_wallets.append(wallet);
    emit changed();
    emit walletAdded(wallet);
}

Wallet* WalletManager::createWallet()
{
    return new Wallet(this);
}

void WalletManager::insertWallet(Wallet* wallet)
{
    Q_ASSERT(wallet->m_id.isEmpty() && !wallet->m_pin_data.isEmpty());
    wallet->m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QFile file(GetDataFile("wallets", wallet->m_id));
    Q_ASSERT(!file.exists());
    addWallet(wallet);

    QMetaObject::invokeMethod(wallet->m_session->m_context, [wallet] {
        QMetaObject::invokeMethod(wallet, [wallet] {
            wallet->save();
        });
    });
}

void WalletManager::removeWallet(Wallet* wallet)
{
    emit aboutToRemove(wallet);
    // Q_ASSERT(wallet->connection() == Wallet::Disconnected);
    m_wallets.removeOne(wallet);
    emit changed();
    if (!wallet->m_id.isEmpty()) {
        QMetaObject::invokeMethod(wallet->m_session->m_context, [wallet] {
            QMetaObject::invokeMethod(wallet, [wallet] {
                bool result = QFile::remove(GetDataFile("wallets", wallet->m_id));
                Q_ASSERT(result);
            });
        });
    }
}

QQmlListProperty<Wallet> WalletManager::wallets()
{
    return QQmlListProperty<Wallet>(this, &m_wallets,
        [](QQmlListProperty<Wallet>* property) { return static_cast<QVector<Wallet*>*>(property->data)->size(); },
    [](QQmlListProperty<Wallet>* property, int index) { return static_cast<QVector<Wallet*>*>(property->data)->at(index); });
}

QString WalletManager::newWalletName(Network* network) const
{
    if (!network) return {};
    QSet<QString> names;
    for (Wallet* wallet : m_wallets) {
        if (wallet->network() == network) names.insert(wallet->name());
    }
    int n = 0;
    QString name = QString("My %1 Wallet").arg(network->name());
    while (names.contains(name)) {
        name = QString("My %1 Wallet %2").arg(network->name()).arg(++n);
    }
    return name;
}

Wallet* WalletManager::signup(const QString& proxy, bool use_tor, Network* network, const QString& name, const QStringList& mnemonic, const QByteArray& pin)
{
    Q_ASSERT(mnemonic.size() == 24 || mnemonic.size() == 27);
    Wallet* wallet = new Wallet(this);
    wallet->m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    wallet->m_proxy = proxy;
    wallet->m_use_tor = use_tor;
    wallet->m_network = network;
    wallet->m_name = name;
    wallet->connect(proxy, use_tor);
    wallet->signup(mnemonic, pin);
    addWallet(wallet);
    return wallet;
}

QJsonObject WalletManager::parseUrl(const QString &url)
{
    QJsonObject r;
    QUrl res(url);
    QUrlQuery q(res);

    r.insert("address", res.path());
    r.insert("amount", q.queryItemValue("amount"));

    return r;
}
