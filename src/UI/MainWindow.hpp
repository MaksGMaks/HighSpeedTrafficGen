#pragma once

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
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSpacerItem>
#include <QTabWidget>
#include <QThread>
#include <QTimeEdit>
#include <QVBoxLayout>

#include "commonUI.hpp"
#include "HelpPage.hpp"
#include "../SettingsManager.hpp"
#include "UIUpdate/UIUpdater.hpp"

#include "UInt64Validator.hpp"

#include "Generator/common_generator.hpp"
#include "Generator/Generator.hpp"

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

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr); //, std::vector<interfaceModes> interfases
    ~MainWindow();

signals:
    void startGenerator(const genParams &params);
    void pauseGenerator();
    void resumeGenerator();
    void stopGenerator();

public slots:
    // Slot functions for handling button clicks and menu actions
    void onStartButtonClicked();
    void onPauseButtonClicked();
    void onFileSendButtonClicked();
    void onSelectFileButtonClicked();

    void onSavePpsGraphActionTriggered();
    void onSaveBpsGraphActionTriggered();
    void onSaveBPSGraphActionTriggered();
    void onSavePacketLossGraphActionTriggered();
    //void onAboutActionTriggered();
    void onHelpActionTriggered();

    void onRefreshInterfacesActionTriggered();
    void onInterfaceChanged(const QString &interfaceName);

    void onThemeChanged(const QString &theme);
    void onLanguageChanged(const QString &language);
    // Slot functions for handling outer events
    void onApplyStyleSheet(const QString &styleSheet);
    

private:
    void setupUi();
    void setupConnections();
    void setupSettings();
    void setupUtilitiesThread();
    
    void saveGraph(const QChartView *chartView);

    // Data members
    std::vector<interfaceModes> m_interfaces;
    std::unordered_map<std::string, interfaceModes> m_interfacesMap;
    SettingsManager m_settingsManager;
    HelpPage *m_helpPage;
    QThread *m_utilitiesThread;
    Generator *m_generator;
    UIUpdater *m_uiUpdater;

    // UI elements
    // Tabs
    QTabWidget *m_tabWidget;

    // Charts
    QChart *m_ppsChart;
    QChartView *m_ppsChartView;
    QChart *m_bpsChart;
    QChartView *m_bpsChartView;
    QChart *m_BPSChart;
    QChartView *m_BPSChartView;
    QChart *m_packetLossChart;
    QChartView *m_packetLossChartView;

    // Menu Bar
    QMenuBar *m_menuBar;
    QMenu *m_fileMenu;
    QMenu *m_savingsMenu;
    QMenu *m_settingsMenu;
    QMenu *m_themeMenu;
    QMenu *m_languageMenu;

    QAction *m_refreshInterfacesAction;
    QAction *m_savePpsGraphAction;
    QAction *m_saveBpsGraphAction;
    QAction *m_saveBPSGraphAction;
    QAction *m_savePacketLossGraphAction;
    QAction *m_lightThemeAction;
    QAction *m_darkThemeAction;
    QAction *m_enLanguageAction;
    QAction *m_uaLanguageAction;
    //QAction *m_aboutAction;
    QAction *m_helpAction;

    // Buttons
    QPushButton *m_startButton;
    QPushButton *m_pauseButton;
    QPushButton *m_fileSendButton;
    QPushButton *m_selectFileButton;

    // Labels
    QLabel *m_interfaceLabel;
    QLabel *m_modeLabel;
    QLabel *m_prefferedSpeedLabel;
    QLabel *m_packetSizeLabel;
    QLabel *m_burstSizeLabel;
    QLabel *m_copiesLabel;
    QLabel *m_sentLabel;
    QLabel *m_packetPatternLabel;

    QLabel *m_timeLabel;
    QLabel *m_speedLabel;
    QLabel *m_totalSendLabel;
    QLabel *m_totalCopiesLabel;

    QLabel *m_speedUnitLabel;
    QLabel *m_totalSendUnitLabel;

    // Line Edits
    QLineEdit *m_prefferedSpeedLineEdit;
    QLineEdit *m_packetSizeLineEdit;
    QLineEdit *m_burstSizeLineEdit;
    QLineEdit *m_copiesLineEdit;
    QLineEdit *m_sentLineEdit;
    QLineEdit *m_packetPatternLineEdit;
    QLineEdit *m_pathToFileLineEdit;

    QLineEdit *m_speedLineEdit;
    QLineEdit *m_totalSendLineEdit;
    QLineEdit *m_totalCopiesLineEdit;
    QTimeEdit *m_timeLineEdit;

    // Combo Boxes
    QComboBox *m_interfaceComboBox;
    QComboBox *m_modeComboBox;

    // Group Boxes
    QGroupBox *m_parametersGroupBox;
    QGroupBox *m_infoGroupBox;

    // Layouts
    QHBoxLayout *m_controlLayout;
    QHBoxLayout *m_interfaceLayout;
    QHBoxLayout *m_modeLayout;
    QHBoxLayout *m_prefferedSpeedLayout;
    QHBoxLayout *m_packetSizeLayout;
    QHBoxLayout *m_burstSizeLayout;
    QHBoxLayout *m_copiesLayout;
    QHBoxLayout *m_sentLayout;
    QHBoxLayout *m_packetPatternLayout;
    QHBoxLayout *m_filePathLayout;

    QHBoxLayout *m_speedLayout;
    QHBoxLayout *m_totalSendLayout;

    QHBoxLayout *m_centeredLayout;

    QVBoxLayout *m_graphLayout;
    QVBoxLayout *m_infoLayout;
    QVBoxLayout *m_parametersLayout;

    QVBoxLayout *m_timeLayout;

    QVBoxLayout *m_mainLayout;
    QWidget *m_centralWidget;
};