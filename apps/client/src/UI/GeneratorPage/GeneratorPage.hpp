#pragma once
#include <QWidget>

#include "ui_GeneratorPage.h"
#include "StatisticWindow.hpp"

#include <QPushButton>

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

public slots:
  void onStatsReceived(const ServerStats &stats);

signals:
  void startGeneration(const genParams &params);
  void stopGeneration();
  void pauseGeneration();
  void resumeGeneration();

private slots:
  void onStartBtnClicked();
  void onStopBtnClicked();
  void onPauseBtnClicked();

private:
  Ui::GeneratorPage *ui;
};

