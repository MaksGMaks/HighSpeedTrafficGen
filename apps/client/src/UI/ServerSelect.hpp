#pragma once

#include <QMainWindow>
#include "ui_ServerSelect.h"

namespace Ui {
    class ServerSelect;
}

class ServerSelect : public QMainWindow {
    Q_OBJECT

public:
    explicit ServerSelect(QWidget *parent = nullptr);

    ~ServerSelect() override;

private:
    Ui::ServerSelect *ui;
};