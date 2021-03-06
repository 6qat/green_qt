#include "account.h"
#include "asset.h"
#include "ga.h"
#include "json.h"
#include "network.h"
#include "util.h"
#include "wallet.h"
#include "resolver.h"
#include "handler.h"
#include "handlers/connecthandler.h"
#include "handlers/loginhandler.h"
#include "registeruserhandler.h"
#include "session.h"

#include <type_traits>

#include <QDateTime>
#include <QDebug>
#include <QJsonObject>
#include <QLocale>
#include <QSettings>
#include <QTimer>
#include <QUuid>

#include <gdk.h>

class GetSubAccountsHandler : public Handler
{
    void call(GA_session* session, GA_auth_handler** auth_handler) override
    {
        int res = GA_get_subaccounts(session, auth_handler);
        Q_ASSERT(res == GA_OK);
    }
public:
    GetSubAccountsHandler(Wallet* wallet)
        : Handler(wallet)
    {
    }
};

class LoginWithPinHandler : public Handler
{
    const QByteArray m_pin_data;
    const QByteArray m_pin;
    void call(GA_session* session, GA_auth_handler** auth_handler) override
    {
        auto pin_data = Json::stringToJson(m_pin_data);
        int err = GA_login_with_pin(session, m_pin.constData(), pin_data.get(), auth_handler);
        Q_ASSERT(err == GA_OK);
    }
public:
    LoginWithPinHandler(Wallet* wallet, const QByteArray& pin_data, const QByteArray& pin)
        : Handler(wallet)
        , m_pin_data(pin_data)
        , m_pin(pin)
    {
    }
};

namespace {
QByteArray getMnemonicPassphrase(GA_session* session)
{
    char* data;
    int err = GA_get_mnemonic_passphrase(session, "", &data);
    Q_ASSERT(err == GA_OK);
    QByteArray mnemonic(data);
    GA_destroy_string(data);
    return mnemonic;
}
QByteArray pinDataForNewPin(GA_session* session, const QByteArray& pin)
{
    const auto mnemonic = getMnemonicPassphrase(session);
    GA_json* data;
    int err = GA_set_pin(session, mnemonic.constData(), pin.constData(), "greenqt", &data);
    Q_ASSERT(err == GA_OK);
    auto pin_data = Json::jsonToString(data);
    err = GA_destroy_json(data);
    Q_ASSERT(err == GA_OK);
    return pin_data;
}
} // namespace

class SetPinHandler : public Handler
{
    const QByteArray m_pin;
    QByteArray m_pin_data;
    void call(GA_session* session, GA_auth_handler** auth_handler) override
    {
        Q_UNUSED(auth_handler);
        m_pin_data = pinDataForNewPin(session, m_pin);
    }
public:
    SetPinHandler(Wallet* wallet, const QByteArray& pin)
        : Handler(wallet)
        , m_pin(pin)
    {
    }
    QByteArray pinData() const
    {
        Q_ASSERT(!m_pin_data.isEmpty());
        return m_pin_data;
    }
};

Wallet::Wallet(QObject *parent)
    : QObject(parent)
{
}

void Wallet::connect(const QString& proxy, bool use_tor)
{
    if (m_connection == Connected) {
        Q_ASSERT(m_proxy == proxy && m_use_tor == use_tor);
    } else {
        setConnection(Connecting);
    }

    bool needs_save = false;
    if (m_proxy != proxy) {
        m_proxy = proxy;
        emit proxyChanged(m_proxy);
        needs_save = true;
    }
    if (m_use_tor != use_tor) {
        m_use_tor = use_tor;
        emit useTorChanged(m_use_tor);
        needs_save = true;
    }
    if (needs_save) {
        save();
    }
    connectNow();
}

// TODO: extract connection/login/... to WalletController
// TODO: controllers should deal with failures so that UI can see them
void Wallet::connectNow()
{
    Q_ASSERT(m_network);

    if (m_connection == Disconnected) return;

    if (!m_session) {
        createSession();
        // TODO: handle reconnect
        auto handler = new ConnectHandler(m_session, m_network, m_proxy, m_use_tor);
        QObject::connect(handler, &ConnectHandler::error, this, [this] {
            connectNow();
        });
        QObject::connect(handler, &ConnectHandler::done, this, [handler] {
            handler->deleteLater();
        });
        handler->exec();
    }
}

void Wallet::disconnect()
{
    Q_ASSERT(m_connection != Disconnected);
    Q_ASSERT(m_authentication == Authenticated);

    if (m_logout_timer != -1 ) {
        killTimer(m_logout_timer);
        m_logout_timer = -1;
        qApp->removeEventFilter(this);
    }

    auto accounts = m_accounts;
    m_accounts.clear();
    m_accounts_by_pointer.clear();
    emit accountsChanged();

    m_settings = {};
    m_config = {};
    m_currencies = {};
    m_events = {};

    setConnection(Disconnected);
    setAuthentication(Unauthenticated);

    delete m_session;
    m_session = nullptr;

    qDeleteAll(accounts);
    qDeleteAll(m_assets.values());
    m_assets.clear();
}

Wallet::~Wallet()
{
    if (m_session) {
        delete m_session;
    }
}

QString Wallet::id() const
{
    Q_ASSERT(!m_id.isEmpty() || m_device);
    return m_id;
}

void Wallet::setNetwork(Network* network)
{
    Q_ASSERT(!m_network);
    m_network = network;
    emit networkChanged(m_network);
}

void Wallet::setName(const QString& name)
{
    Q_ASSERT(m_name.isEmpty());
    m_name = name;
    emit nameChanged(m_name);
}

QJsonObject Wallet::settings() const
{
    return m_settings;
}

QJsonObject Wallet::currencies() const
{
    return m_currencies;
}

QQmlListProperty<Account> Wallet::accounts()
{
    return { this, &m_accounts };
}

void Wallet::handleNotification(const QJsonObject &notification)
{
    QString event = notification.value("event").toString();
    Q_ASSERT(!event.isEmpty());

    QJsonValue data = notification.value(event);

    if (data.isObject()) emit this->notification(event, data.toObject());

    m_events.insert(event, data);
    emit eventsChanged(m_events);

    if (event == "transaction") {
        QJsonObject transaction = data.toObject();
        for (auto pointer : transaction.value("subaccounts").toArray()) {
            auto account = m_accounts_by_pointer.value(pointer.toInt());
            account->handleNotification(notification);
        }
        return;
    }

    if (event == "settings") {
        setAuthentication(Authenticated);
        setSettings(data.toObject());
        auto timer = new QTimer(this);
        timer->start(100);
        QObject::connect(timer, &QTimer::timeout, [this] {
            qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
            setBusy(timestamp - m_last_timestamp > 300);
        });
        QMetaObject::invokeMethod(m_session->m_context, [this] {
            auto timer = new QTimer(m_session->m_context);
            timer->start(100);
            QObject::connect(timer, &QTimer::timeout, [this] {
                m_last_timestamp = QDateTime::currentMSecsSinceEpoch();
            });
        });
        return;
    }

    if (event == "twofactor_reset") {
        setLocked(data.toObject().value("is_active").toBool());
        return;
    }

    if (event == "fees") {
        // TODO: fees are being used in QML as `event.fees`.
        return;
    }

    if (event == "block") {
        for (auto account : m_accounts) {
            account->handleNotification(notification);
        }
        return;
    }

    qDebug() << "UNHANDLED NOTIFICATION" << notification;
}

QJsonObject Wallet::events() const
{
    return m_events;
}

QStringList Wallet::mnemonic() const
{
    QStringList result;
    QMetaObject::invokeMethod(m_session->m_context, [this, &result] {
        char* mnemonic = nullptr;
        int err = GA_get_mnemonic_passphrase(m_session->m_session, "", &mnemonic);
        Q_ASSERT(err == GA_OK);
        result = QString(mnemonic).split(' ');
        GA_destroy_string(mnemonic);
    }, Qt::BlockingQueuedConnection);
    return result;
}

void Wallet::changePin(const QByteArray& pin)
{
    auto handler = new SetPinHandler(this, pin);
    QObject::connect(handler, &Handler::done, this, [this, handler] {
        handler->deleteLater();
        m_pin_data = handler->pinData();
        save();
    });
    handler->exec();
}

void Wallet::loginWithPin(const QByteArray& pin)
{
    Q_ASSERT(m_login_attempts_remaining > 0);

    if (m_pin_data.isEmpty()) return;

    setAuthentication(Authenticating);

    auto handler = new LoginWithPinHandler(this, m_pin_data, pin);
    handler->connect(handler, &Handler::done, this, [this, handler] {
        handler->deleteLater();
        if (m_login_attempts_remaining < 3) {
            m_login_attempts_remaining = 3;
            save();
            emit loginAttemptsRemainingChanged(m_login_attempts_remaining);
        }
        setAuthentication(Authenticated);
        updateCurrencies();
        updateSettings();
        reload();
        updateConfig();
    });
    handler->connect(handler, &Handler::error, this, [this, handler] {
        handler->deleteLater();
        const auto error = handler->result().value("error").toString();
        if (error.contains("exception:login failed")) {
            Q_ASSERT(m_login_attempts_remaining > 0);
            setAuthentication(Unauthenticated);
            --m_login_attempts_remaining;
            save();
            emit loginAttemptsRemainingChanged(m_login_attempts_remaining);
            if (m_login_attempts_remaining == 0) setConnection(Disconnected);
        }
        if (error.contains("exception:reconnect required")) {
            setAuthentication(Unauthenticated);
            return;
        }
        qWarning() << "unhandled login_with_pin error";
    });
    handler->exec();
}

void Wallet::signup(const QStringList& mnemonic, const QByteArray& pin)
{
    Q_ASSERT(mnemonic.size() == 24);

    setAuthentication(Authenticating);

    auto register_user_handler = new RegisterUserHandler(this, mnemonic);
    auto login_handler = new LoginHandler(this, mnemonic);
    auto set_pin_handler = new SetPinHandler(this, pin);

    QObject::connect(register_user_handler, &Handler::done, this, [register_user_handler, login_handler] {
        register_user_handler->deleteLater();
        login_handler->exec();
    });
    QObject::connect(login_handler, &Handler::done, this, [login_handler, set_pin_handler] {
        login_handler->deleteLater();
        set_pin_handler->exec();
    });
    QObject::connect(set_pin_handler, &Handler::done, this, [this, set_pin_handler] {
        set_pin_handler->deleteLater();
        m_pin_data = set_pin_handler->pinData();
        save();
        updateCurrencies();
        reload();
        updateConfig();
        setAuthentication(Authenticated);
    });
    register_user_handler->exec();
}

void Wallet::login(const QStringList& mnemonic, const QString& password)
{
    setAuthentication(Authenticating);

    auto handler = new LoginHandler(this, mnemonic, password);
    QObject::connect(handler, &Handler::done, this, [this, handler] {
       handler->deleteLater();
       updateCurrencies();
       reload();
       updateConfig();
       setAuthentication(Authenticated);
    });
    QObject::connect(handler, &Handler::error, this, [this, handler] {
        handler->deleteLater();
        const auto error = handler->result().value("error").toString();
        // TODO: these are examples of errors
        // these sould be handled in Handler class, see TODO above
        // {"action":"get_xpubs","device":{},"error":"get_xpubs exception:login failed:id_login_failed","status":"error"}
        // {"action":"get_xpubs","device":{},"error":"get_xpubs exception:reconnect required","status":"error"}
        emit loginError(error);
        return setAuthentication(Unauthenticated);
    });
    handler->exec();
}

void Wallet::setPin(const QByteArray& pin)
{
    Q_ASSERT(m_authentication == Authenticated);
    Q_ASSERT(m_name.isEmpty());
    Q_ASSERT(m_pin_data.isEmpty());

    auto handler = new SetPinHandler(this, pin);
    QObject::connect(handler, &Handler::done, this, [this, handler] {
        handler->deleteLater();
        m_pin_data = handler->pinData();
        emit pinSet();
    });
    handler->exec();
}

void Wallet::reload()
{
    if (m_network->isLiquid()) {
        // Load cached assets
        refreshAssets(false);
    }

    auto handler = new GetSubAccountsHandler(this);
    QObject::connect(handler, &Handler::done, this, [this, handler] {
        QJsonArray accounts = handler->result().value("result").toObject().value("subaccounts").toArray();

        for (QJsonValue data : accounts) {
            QJsonObject json = data.toObject();
            int pointer = json.value("pointer").toInt();
            Account* account = getOrCreateAccount(pointer);
            account->update(data.toObject());
            account->reload();
        }

        emit accountsChanged();

        updateConfig();

        if (m_network->isLiquid()) {
            // Update cached assets
            refreshAssets(true);
        }
    });
    QObject::connect(handler, &Handler::resolver, this, [](Resolver* resolver) {
        resolver->resolve();
    });
    handler->exec();
}

void Wallet::refreshAssets(bool refresh)
{
    Q_ASSERT(m_network->isLiquid());

    QMetaObject::invokeMethod(m_session->m_context, [this, refresh] {
        auto params = Json::fromObject({
            { "assets", true },
            { "icons", true },
            { "refresh", refresh }
        });
        GA_json* output;
        int err = GA_refresh_assets(m_session->m_session, params.get(), &output);
        if (err != GA_OK) {
            return;
        }

        auto assets = Json::toObject(output);
        err = GA_destroy_json(output);
        Q_ASSERT(err == GA_OK);

        QMetaObject::invokeMethod(this, [this, assets] {
            auto icons = assets.value("icons").toObject();

            for (auto&& ref : assets.value("assets").toObject()) {
                QString id = ref.toObject().value("asset_id").toString();
                if (id.isEmpty()) continue;
                Asset* asset = getOrCreateAsset(id);
                asset->setData(ref.toObject());
                if (icons.contains(id)) {
                    asset->setIcon("data:image/png;base64," + icons.value(id).toString());
                }
            }
            for (auto account : m_accounts) {
                account->updateBalance();
            }
        });
    });
}

void Wallet::updateConfig()
{
    GA_json* config;
    int err = GA_get_twofactor_config(m_session->m_session, &config);
    Q_ASSERT(err == GA_OK);
    m_config = Json::toObject(config);
    GA_destroy_json(config);
    emit configChanged();

    setLocked(m_config.value("twofactor_reset").toObject().value("is_active").toBool());
}

void Wallet::updateSettings()
{
    GA_json* settings;
    int err = GA_get_settings(m_session->m_session, &settings);
    Q_ASSERT(err == GA_OK);
    auto data = Json::toObject(settings);
    GA_destroy_json(settings);
    setSettings(data);
}

void Wallet::updateCurrencies()
{
    GA_json* currencies;
    int err = GA_get_available_currencies(m_session->m_session, &currencies);
    Q_ASSERT(err == GA_OK);
    m_currencies = Json::toObject(currencies);
    GA_destroy_json(currencies);
}

void Wallet::save()
{
    if (m_id.isEmpty()) return;
    QJsonDocument doc({
        { "version", 1 },
        { "name", m_name },
        { "network", m_network->id() },
        { "login_attempts_remaining", m_login_attempts_remaining },
        { "pin_data", QString::fromLocal8Bit(m_pin_data.toBase64()) },
        { "proxy", m_proxy },
        { "use_tor", m_use_tor }
    });
    QFile file(GetDataFile("wallets", m_id));
    bool result = file.open(QFile::ReadWrite);
    Q_ASSERT(result);
    file.write(doc.toJson());
    result = file.flush();
    Q_ASSERT(result);
}

void Wallet::setConnection(ConnectionStatus connection)
{
    if (m_connection == connection) return;
    qDebug() << "connection change" << m_connection << " -> " << connection;
    m_connection = connection;
    emit connectionChanged();
}

void Wallet::setAuthentication(AuthenticationStatus authentication)
{
    if (m_authentication == authentication) return;
    qDebug() << "authentication change" << m_authentication << " -> " << authentication;
    m_authentication = authentication;
    emit authenticationChanged();
}

QJsonObject Wallet::convert(const QJsonObject& value) const
{
    auto details = Json::fromObject(value);
    GA_json* balance;
    int err = GA_convert_amount(m_session->m_session, details.get(), &balance);
    if (err != GA_OK) return {};
    QJsonObject result = Json::toObject(balance);
    GA_destroy_json(balance);
    return result;
}

QString Wallet::formatAmount(qint64 amount, bool include_ticker) const
{
    return formatAmount(amount, include_ticker, m_settings.value("unit").toString());
}

QString Wallet::formatAmount(qint64 amount, bool include_ticker, const QString& unit) const
{
    Q_ASSERT(m_network);
    const auto effective_unit = unit.isEmpty() ? m_settings.value("unit").toString() : unit;
    if (effective_unit.isEmpty()) {
        return {};
    }
    auto str = convert({{ "satoshi", amount }}).value(effective_unit == "\u00B5BTC" ? "ubtc" : effective_unit.toLower()).toString();
    auto val = str.toDouble();
    if (val == ((int64_t) val)) {
        str = QLocale::system().toString(val, 'f', 0);
    } else {
        str = QLocale::system().toString(val, 'f', 8);
        str.remove(QRegExp("\\.?0+$"));
    }
    if (include_ticker) {
        str += (m_network->isLiquid() ? " L-" : " ") + effective_unit;
    }
    return str;
}

qint64 Wallet::amountToSats(const QString& amount) const
{
    return parseAmount(amount, m_settings.value("unit").toString());
}

qint64 Wallet::parseAmount(const QString& amount, const QString& unit) const
{
    if (amount.isEmpty()) return 0;
    QString sanitized_amount = amount;
    sanitized_amount.replace(',', '.');
    auto details = Json::fromObject({{ unit == "\u00B5BTC" ? "ubtc" : unit.toLower(), sanitized_amount }});
    GA_json* balance;
    int err = GA_convert_amount(m_session->m_session, details.get(), &balance);
    if (err != GA_OK) return 0;
    QJsonObject result = Json::toObject(balance);
    GA_destroy_json(balance);
    return result.value("sats").toString().toLongLong();
}

Asset* Wallet::getOrCreateAsset(const QString& id)
{
    Q_ASSERT(m_network && m_network->isLiquid());
    QString key = id == "btc" ? "6f0279e9ed041c3d710a9f57d0c02928416460c4b722ae3457a11eec381c526d" : id;

    Asset* asset = m_assets.value(key);
    if (!asset) {
        asset = new Asset(key, this);
        m_assets.insert(key, asset);
    }
    return asset;
}

void Wallet::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged(m_busy);
}

Account* Wallet::getOrCreateAccount(int pointer)
{
    Account* account = m_accounts_by_pointer.value(pointer);
    if (!account) {
        account = new Account(this);
        m_accounts.append(account);
        m_accounts_by_pointer.insert(pointer, account);
    }
    return account;
}

void Wallet::createSession()
{
    Q_ASSERT(!m_session);
    m_session = new Session(this);
    QObject::connect(m_session, &Session::notificationHandled, this, &Wallet::handleNotification);

    QObject::connect(m_session, &Session::sessionEvent, [this](bool connected) {
        setConnection(connected ? Connected : m_connection);
    });
    QObject::connect(m_session, &Session::networkEvent, [this](bool connected, bool heartbeat_timeout, bool login_required) {
        Q_UNUSED(heartbeat_timeout);
        if (!connected) {
            setConnection(Connecting);
        } else {
            setConnection(Connected);
            if (login_required) {
                setAuthentication(Unauthenticated);
            }
        }
    });
    emit sessionChanged(m_session);
}

void Wallet::setSession()
{
    setConnection(Connected);
    setAuthentication(Authenticated);
    updateSettings();
    updateCurrencies();
    updateConfig();
    reload();
}

void Wallet::setSettings(const QJsonObject& settings)
{
    if (m_settings == settings) return;
    m_settings = settings;
    emit settingsChanged();

    if (m_logout_timer != -1 ) {
        killTimer(m_logout_timer);
        m_logout_timer = -1;
    }
    if (!m_device) {
        int altimeout = m_settings.value("altimeout").toInt();
        if (altimeout > 0) {
            m_logout_timer = startTimer(altimeout * 60 * 1000);
            qApp->installEventFilter(this);
        } else {
            qApp->removeEventFilter(this);
        }
    }
}

bool Wallet::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::MouseMove) {
        Q_ASSERT(m_logout_timer != -1);
        killTimer(m_logout_timer);
        int altimeout = m_settings.value("altimeout").toInt();
        m_logout_timer = startTimer(altimeout * 60 * 1000);
    }
    return QObject::eventFilter(object, event);
}

void Wallet::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_logout_timer) {
        if (m_device) return;
        disconnect();
    }
}

void Wallet::setLocked(bool locked)
{
    if (m_locked == locked) return;
    m_locked = locked;
    emit lockedChanged(m_locked);
}

