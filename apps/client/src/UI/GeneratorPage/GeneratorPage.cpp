#include "GeneratorPage.hpp"

#include "../../../../../build/apps/client/HSET_GeneratorClient_autogen/include/ui_GeneratorPage.h"
#include "../../../../../libs/common/include/generator_values.hpp"

GeneratorPage::GeneratorPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::GeneratorPage)
{
    ui->setupUi(this);
    connect(ui->startBtn, &QPushButton::clicked, this, &GeneratorPage::onStartBtnClicked);
    connect(ui->stopBtn, &QPushButton::clicked, this, &GeneratorPage::onStopBtnClicked);
    connect(ui->pauseResBtn, &QPushButton::clicked, this, &GeneratorPage::onPauseBtnClicked);
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

void GeneratorPage::onStartBtnClicked() {
    genParams params;
    switch (ui->tabWidget->currentIndex()) {
    case 0:
        params.mode = GeneratorMode::RandomLaw;
        params.law = ui->randGen->law();
        break;

    case 1:
        params.mode = GeneratorMode::PcapPlayer;
        params.playerSettings = ui->pcapPlayer->settings();
        break;
    }
    params.time = (ui->daysEdit->text().toUInt() * 86400) + (ui->timeEdit->time().hour() * 3600) + (ui->timeEdit->time().minute() * 60) + ui->timeEdit->time().second();
    emit startGeneration(params);
}

void GeneratorPage::onStopBtnClicked() {
    emit stopGeneration();
}

void GeneratorPage::onPauseBtnClicked() {
    if(ui->pauseResBtn->isFlat()) {
        ui->pauseResBtn->setFlat(false);
        ui->pauseResBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPause));
        emit resumeGeneration();
    } else {
        ui->pauseResBtn->setFlat(true);
        ui->pauseResBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaSeekForward));
        emit pauseGeneration();
    }
}

void GeneratorPage::onStatsReceived(const ServerStats &stats) {
    ui->statWidget->onStatsReceived(stats);
}