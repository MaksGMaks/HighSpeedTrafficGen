#pragma once

#include "../../../../build/apps/client/HSET_GeneratorClient_autogen/include/ui_MainWindow.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QActionGroup>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <vector>

#include <QApplication>
#include <QTranslator>

#include "commonUI.hpp"
#include "HelpPage.hpp"

#include "ConstructorPage/ConstructorPage.hpp"
#include "GeneratorPage/GeneratorPage.hpp"

#include "ServerSelect.hpp"

// #include "Generator/common_generator.hpp"
// #include "Generator/Generator.hpp"

namespace Ui {
    class MainWindow;
}


class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void showServerConnect();
    void showMainWindow();

private slots:
    void onActionDark();
    void onActionLight();
    void onActionEnglish();
    void onActionUkrainian();
    void onTrafficPlayerTab();
    void onPacketConstructorTab();
    void onHelpPageTab();

private:
    void changeEvent(QEvent *event) override;
    void applyTheme(AppTheme theme);
    void setupMenuBar();
    void saveSettings();
    void loadSettings();
    void loadLanguage(const QString &locale);

    Ui::MainWindow *ui;
    HelpPage *helpPage;
    QTranslator m_translator;
    AppTheme m_currentTheme = AppTheme::Dark;

    ServerSelect* m_serverSelect;
    NetworkManager* m_manager;

};