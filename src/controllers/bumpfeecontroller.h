#ifndef GREEN_BUMPFEECONTROLLER_H
#define GREEN_BUMPFEECONTROLLER_H

#include "accountcontroller.h"

#include <QtQml>
#include <QJsonObject>

class Balance;
class Transaction;

class BumpFeeController : public AccountController
{
    Q_OBJECT
    Q_PROPERTY(int feeRate READ feeRate WRITE setFeeRate NOTIFY changed)
    Q_PROPERTY(QJsonObject tx READ tx NOTIFY txChanged)
    QML_ELEMENT
    QJsonObject m_tx;
    int m_fee_rate{0};
    int m_req{0};
    Handler* m_create_handler{nullptr};
public:
    BumpFeeController(QObject* parent = nullptr);
    int feeRate() const { return m_fee_rate; }
    void setFeeRate(int feeRate);
    Transaction* transaction();
    Q_INVOKABLE void bumpFee();
    QJsonObject tx() const { return m_tx; }

private slots:
    void create();
signals:
    void changed();
    void txChanged(const QJsonObject& tx);
};

#endif // GREEN_BUMPFEECONTROLLER_H
