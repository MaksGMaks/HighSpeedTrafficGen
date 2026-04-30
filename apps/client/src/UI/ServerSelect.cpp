//
// Created by maksgmaks on 13.04.26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_ServerSelect.h" resolved

#include "ServerSelect.hpp"

ServerSelect::ServerSelect(QWidget *parent)
: QMainWindow(parent)
, ui(new Ui::ServerSelect) {
    ui->setupUi(this);
}

ServerSelect::~ServerSelect() {
    delete ui;
}