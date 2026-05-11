#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QDoubleValidator>
#include "generator_values.hpp"

// One row: [Label] [Mode combo] [Static/Range/Random stacked input]
class ParamRow : public QWidget {
    Q_OBJECT
public:
    enum class Type { IP, Port, Size, TimeDiff, TTL };

    explicit ParamRow(QWidget *parent = nullptr);

    void  setLabel(const QString &text);
    void  setType(Type type);

    // Read current state into GenLaw::Mode + values
    GenLaw::Mode mode()       const;
    QString      staticValue() const;
    QString      rangeMin()    const;
    QString      rangeMax()    const;
    QString      rangeStep()   const;
    GenLaw::Distribution distribution() const;

    // Restore state
    void setMode(GenLaw::Mode m);
    void setStaticValue(const QString &v);
    void setRange(const QString &mn, const QString &mx, const QString &step = {});
    void setDistribution(GenLaw::Distribution d);

signals:
    void changed();

private slots:
    void onModeChanged(int idx);

private:
    void buildStaticPage();
    void buildRangePage();
    void buildRandomPage();
    QString placeholderForType() const;

    Type            m_type = Type::IP;

    QLabel         *m_label       = nullptr;
    QComboBox      *m_modeCombo   = nullptr;
    QStackedWidget *m_stack       = nullptr;

    // Static page
    QLineEdit      *m_staticEdit  = nullptr;

    // Range page
    QLineEdit      *m_rangeMin    = nullptr;
    QLineEdit      *m_rangeMax    = nullptr;
    QLineEdit      *m_rangeStep   = nullptr;
    QComboBox      *m_distCombo   = nullptr;

    // Random page — just distribution selector
    QComboBox      *m_randDistCombo = nullptr;
};