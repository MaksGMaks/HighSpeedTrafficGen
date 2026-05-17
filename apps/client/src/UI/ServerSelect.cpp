//
// Created by maksgmaks on 13.04.26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_ServerSelect.h" resolved

#include "ServerSelect.hpp"

#include "../../../../build/apps/client/HSET_GeneratorClient_autogen/include/ui_ServerSelect.h"

ServerSelect::ServerSelect(NetworkManager* manager, QWidget *parent)
: QMainWindow(parent)
, ui(new Ui::ServerSelect) {
    ui->setupUi(this);
    m_manager = manager;
    connect(ui->connectBtn, &QPushButton::clicked, this, &ServerSelect::onConnectBtnClicked);
}

ServerSelect::~ServerSelect() {
    delete ui;
}

void ServerSelect::onConnectBtnClicked() {
    if (ui->portEdit->text().isEmpty() || ui->ipEdit->text().isEmpty()) return;
    m_manager->connect(ui->ipEdit->text().toStdString(), ui->portEdit->text().toUShort());
}