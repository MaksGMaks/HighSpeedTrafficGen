#pragma once

#include <QValidator>
#include <limits>

class UInt64Validator : public QValidator {
    Q_OBJECT
public:
    explicit UInt64Validator(QObject *parent = nullptr);
    UInt64Validator(quint64 bottom, quint64 top, QObject *parent = nullptr);

    void setBottom(quint64 bottom);
    void setTop(quint64 top);
    quint64 bottom() const;
    quint64 top() const;

    State validate(QString &input, int &pos) const override;

private:
    quint64 m_bottom;
    quint64 m_top;
};
