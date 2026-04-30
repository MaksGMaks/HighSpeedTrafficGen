#include "UInt64Validator.hpp"
#include <QRegularExpression>

UInt64Validator::UInt64Validator(QObject *parent)
: QValidator(parent)
, m_bottom(0)
, m_top(std::numeric_limits<quint64>::max()) {}

UInt64Validator::UInt64Validator(quint64 bottom, quint64 top, QObject *parent)
: QValidator(parent)
, m_bottom(bottom)
, m_top(top) {}

void UInt64Validator::setBottom(quint64 bottom) {
    m_bottom = bottom;
}

void UInt64Validator::setTop(quint64 top) {
    m_top = top;
}

quint64 UInt64Validator::bottom() const {
    return m_bottom;
}

quint64 UInt64Validator::top() const {
    return m_top;
}

QValidator::State UInt64Validator::validate(QString &input, int &pos) const {
    Q_UNUSED(pos);

    if (input.isEmpty())
        return Intermediate;

    if (!input.contains(QRegularExpression("^\\d+$")))
        return Invalid;

    bool ok = false;
    quint64 value = input.toULongLong(&ok);
    if (!ok)
        return Invalid;

    if (value >= m_bottom && value <= m_top)
        return Acceptable;

    return Intermediate;
}
