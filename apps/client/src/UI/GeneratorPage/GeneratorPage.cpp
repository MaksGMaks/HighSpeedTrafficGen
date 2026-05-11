#include "GeneratorPage.hpp"

#include "../../../../../build/apps/client/HSET_GeneratorClient_autogen/include/ui_GeneratorPage.h"

GeneratorPage::GeneratorPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::GeneratorPage) {
  ui->setupUi(this);
}

GeneratorPage::~GeneratorPage() { delete ui; }

void GeneratorPage::setDarkTheme(bool dark) {
  ui->statWidget->setDarkTheme(dark);
}

void GeneratorPage::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    ui->retranslateUi(this);
    ui->statWidget->changeEvent(event);
    ui->randGen->changeEvent(event);
    ui->pcapPlayer->changeEvent(event);
  }
  QWidget::changeEvent(event);
}