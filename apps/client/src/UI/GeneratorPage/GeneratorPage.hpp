#pragma once
#include <QWidget>

#include "ui_GeneratorPage.h"
#include "StatisticWindow.hpp"

namespace Ui {
class GeneratorPage;
}

class GeneratorPage : public QWidget {
  Q_OBJECT

public:
  explicit GeneratorPage(QWidget *parent = nullptr);
  ~GeneratorPage() override;

  void setDarkTheme(bool dark);
  void changeEvent(QEvent *event) override;

private:
  Ui::GeneratorPage *ui;
};

