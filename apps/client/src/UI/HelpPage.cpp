//
// Created by maksgmaks on 16.04.26.
//

// You may need to build the project (run Qt uic code generator) to get "ui_HelpPage.h" resolved

#include "HelpPage.h"
#include "ui_HelpPage.h"


HelpPage::HelpPage(QWidget *parent) : QMainWindow(parent), ui(new Ui::HelpPage) {
    ui->setupUi(this);
}

HelpPage::~HelpPage() {
    delete ui;
}