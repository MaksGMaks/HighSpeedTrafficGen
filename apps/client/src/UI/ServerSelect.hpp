#pragma once

#include <QMainWindow>
#include "ui_ServerSelect.h"
#include "../Network/NetworkManager.hpp"

namespace Ui {
    class ServerSelect;
}

class ServerSelect : public QMainWindow {
    Q_OBJECT

public:
    explicit ServerSelect(NetworkManager* manager, QWidget *parent = nullptr);

    ~ServerSelect() override;

private slots:
    void onConnectBtnClicked();

private:
    Ui::ServerSelect *ui;
    NetworkManager* m_manager;
};