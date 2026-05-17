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
#include <QChart>
#include <QChartView>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLineSeries>
#include <QLocale>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSpacerItem>
#include <QTabWidget>
#include <QThread>
#include <QTime>
#include <QTimeEdit>
#include <QTranslator>
#include <QVBoxLayout>
#include <QValueAxis>

#include "commonUI.hpp"
#include "HelpPage.hpp"
// #include "../SettingsManager.hpp"
// #include "UIUpdate/UIUpdater.hpp"

#include "ConstructorPage/ConstructorPage.hpp"
#include "GeneratorPage/GeneratorPage.hpp"
#include "UInt64Validator.hpp"

#include "ServerSelect.hpp"

// #include "Generator/common_generator.hpp"
// #include "Generator/Generator.hpp"

#define MINIMUM_PARAM_LABEL_WIDTH 180
#define MINIMUM_PARAM_LABEL_HEIGHT 30
#define MINIMUM_INFO_LABEL_WIDTH 150
#define MINIMUM_INFO_LABEL_HEIGHT 30
#define MINIMUM_UNIT_LABEL_WIDTH 80
#define MINIMUM_UNIT_LABEL_HEIGHT 30
#define MINIMUM_INFO_LINEEDIT_WIDTH 100
#define MINIMUM_INFO_LINEEDIT_HEIGHT 30
#define MINIMUM_PARAM_LINEEDIT_WIDTH 100
#define MINIMUM_PARAM_LINEEDIT_HEIGHT 30
#define MINIMUM_PARAM_BUTTON_WIDTH 100
#define MINIMUM_PARAM_BUTTON_HEIGHT 100
#define MINIMUM_PARAM_COMBOBOX_WIDTH 100
#define MINIMUM_PARAM_COMBOBOX_HEIGHT 30

#define PF_RING_STANDARD 1
#define PF_RING_ZC 2
#define DPDK 4

namespace Ui {
    class MainWindow;
}


class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr); //, std::vector<interfaceModes> interfases
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