#include "ParamRow.hpp"

ParamRow::ParamRow(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(6);

    m_label = new QLabel(this);
    m_label->setMinimumWidth(120);
    m_label->setMaximumWidth(120);
    layout->addWidget(m_label);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("Static");
    m_modeCombo->addItem("Range");
    m_modeCombo->addItem("Random");
    m_modeCombo->setMinimumWidth(80);
    m_modeCombo->setMaximumWidth(80);
    layout->addWidget(m_modeCombo);

    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack);

    buildStaticPage();
    buildRangePage();
    buildRandomPage();

    connect(m_modeCombo, &QComboBox::currentIndexChanged,
            this, &ParamRow::onModeChanged);
}

void ParamRow::buildStaticPage()
{
    auto *w = new QWidget();
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);

    m_staticEdit = new QLineEdit(w);
    m_staticEdit->setPlaceholderText("value");
    l->addWidget(m_staticEdit);

    connect(m_staticEdit, &QLineEdit::textChanged, this, &ParamRow::changed);
    m_stack->addWidget(w); // index 0
}

void ParamRow::buildRangePage()
{
    auto *w = new QWidget();
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(4);

    auto *minLabel = new QLabel("Min:", w);
    minLabel->setMaximumWidth(28);
    m_rangeMin = new QLineEdit(w);
    m_rangeMin->setPlaceholderText("min");

    auto *maxLabel = new QLabel("Max:", w);
    maxLabel->setMaximumWidth(28);
    m_rangeMax = new QLineEdit(w);
    m_rangeMax->setPlaceholderText("max");

    auto *stepLabel = new QLabel("Step:", w);
    stepLabel->setMaximumWidth(32);
    m_rangeStep = new QLineEdit(w);
    m_rangeStep->setPlaceholderText("0=cont.");
    m_rangeStep->setMaximumWidth(60);

    m_distCombo = new QComboBox(w);
    m_distCombo->addItem("Uniform");
    m_distCombo->addItem("Normal");
    m_distCombo->addItem("Exponential");
    m_distCombo->setMaximumWidth(100);

    l->addWidget(minLabel);
    l->addWidget(m_rangeMin);
    l->addWidget(maxLabel);
    l->addWidget(m_rangeMax);
    l->addWidget(stepLabel);
    l->addWidget(m_rangeStep);
    l->addWidget(m_distCombo);

    for (auto *e : {m_rangeMin, m_rangeMax, m_rangeStep})
        connect(e, &QLineEdit::textChanged, this, &ParamRow::changed);
    connect(m_distCombo, &QComboBox::currentIndexChanged, this, &ParamRow::changed);

    m_stack->addWidget(w); // index 1
}

void ParamRow::buildRandomPage()
{
    auto *w = new QWidget();
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);

    auto *label = new QLabel("Distribution:", w);
    m_randDistCombo = new QComboBox(w);
    m_randDistCombo->addItem("Uniform");
    m_randDistCombo->addItem("Normal");
    m_randDistCombo->addItem("Exponential");
    m_randDistCombo->setMaximumWidth(120);

    auto *infoLabel = new QLabel("(full type range)", w);
    infoLabel->setStyleSheet("color: gray; font-style: italic;");

    l->addWidget(label);
    l->addWidget(m_randDistCombo);
    l->addWidget(infoLabel);
    l->addStretch();

    connect(m_randDistCombo, &QComboBox::currentIndexChanged,
            this, &ParamRow::changed);

    m_stack->addWidget(w); // index 2
}

void ParamRow::setLabel(const QString &text) { m_label->setText(text); }

void ParamRow::setType(Type type)
{
    m_type = type;

    // Set appropriate validators
    QValidator *v = nullptr;
    switch (type) {
    case Type::Port:
        v = new QIntValidator(0, 65535, this);
        m_staticEdit->setPlaceholderText("0–65535");
        m_rangeMin->setPlaceholderText("0");
        m_rangeMax->setPlaceholderText("65535");
        break;
    case Type::Size:
        v = new QIntValidator(14, 65535, this);
        m_staticEdit->setPlaceholderText("64");
        m_rangeMin->setPlaceholderText("14");
        m_rangeMax->setPlaceholderText("65535");
        break;
    case Type::TTL:
        v = new QIntValidator(1, 255, this);
        m_staticEdit->setPlaceholderText("64");
        m_rangeMin->setPlaceholderText("1");
        m_rangeMax->setPlaceholderText("255");
        break;
    case Type::TimeDiff:
        v = new QDoubleValidator(0.0, 3600.0, 6, this);
        m_staticEdit->setPlaceholderText("0.001 s");
        m_rangeMin->setPlaceholderText("0.0");
        m_rangeMax->setPlaceholderText("1.0");
        break;
    case Type::IP:
        // Regex validator for IPv4
        m_staticEdit->setInputMask("000.000.000.000;_");
        m_rangeMin->setInputMask("000.000.000.000;_");
        m_rangeMax->setInputMask("000.000.000.000;_");
        m_rangeStep->setVisible(false); // IP range steps by last octet
        break;
    }

    if (v && type != Type::IP) {
        m_staticEdit->setValidator(v);
        m_rangeMin->setValidator(v);
        m_rangeMax->setValidator(v);
        m_rangeStep->setValidator(new QDoubleValidator(0, 1e9, 6, this));
    }
}

void ParamRow::onModeChanged(int idx)
{
    m_stack->setCurrentIndex(idx);
    emit changed();
}

// ── Getters ───────────────────────────────────────────────────────────────────

GenLaw::Mode ParamRow::mode() const
{
    switch (m_modeCombo->currentIndex()) {
    case 0:  return GenLaw::Mode::Static;
    case 1:  return GenLaw::Mode::Range;
    default: return GenLaw::Mode::Random;
    }
}

QString ParamRow::staticValue()  const { return m_staticEdit->text(); }
QString ParamRow::rangeMin()     const { return m_rangeMin->text();   }
QString ParamRow::rangeMax()     const { return m_rangeMax->text();   }
QString ParamRow::rangeStep()    const { return m_rangeStep->text();  }

GenLaw::Distribution ParamRow::distribution() const
{
    const int idx = (mode() == GenLaw::Mode::Range)
                        ? m_distCombo->currentIndex()
                        : m_randDistCombo->currentIndex();
    switch (idx) {
    case 1:  return GenLaw::Distribution::Normal;
    case 2:  return GenLaw::Distribution::Exponential;
    default: return GenLaw::Distribution::Uniform;
    }
}

// ── Setters ───────────────────────────────────────────────────────────────────

void ParamRow::setMode(GenLaw::Mode m)
{
    switch (m) {
    case GenLaw::Mode::Static: m_modeCombo->setCurrentIndex(0); break;
    case GenLaw::Mode::Range:  m_modeCombo->setCurrentIndex(1); break;
    case GenLaw::Mode::Random: m_modeCombo->setCurrentIndex(2); break;
    }
}

void ParamRow::setStaticValue(const QString &v) { m_staticEdit->setText(v); }

void ParamRow::setRange(const QString &mn, const QString &mx, const QString &step)
{
    m_rangeMin->setText(mn);
    m_rangeMax->setText(mx);
    if (!step.isEmpty()) m_rangeStep->setText(step);
}

void ParamRow::setDistribution(GenLaw::Distribution d)
{
    const int idx = (d == GenLaw::Distribution::Normal)      ? 1 :
                    (d == GenLaw::Distribution::Exponential) ? 2 : 0;
    m_distCombo->setCurrentIndex(idx);
    m_randDistCombo->setCurrentIndex(idx);
}