#pragma once
#include <QWidget>

#include "ui_GeneratorParams.h"
#include "ParamRow.hpp"
#include "generator_values.hpp"
#include <QHBoxLayout>

namespace Ui {
    class GeneratorParams;
}

class GeneratorParams : public QWidget {
    Q_OBJECT
public:
    explicit GeneratorParams(QWidget *parent = nullptr);
    ~GeneratorParams();

    GenLaw::Law law() const;             // read current UI state
    void setLaw(const GenLaw::Law &law); // restore state
    void resetToDefaults();
    void changeEvent(QEvent *event) override;
signals:
    void lawApplied(GenLaw::Law law);

private slots:
    void onProtoModeChanged(int idx);
    // void onApply();
    // void onReset();

private:
    void setupRows();
    void setupSectionLabels();

    Ui::GeneratorParams *ui;
};