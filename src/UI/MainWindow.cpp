#include "MainWindow.hpp"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    //, m_interfaces(interfaces) 
    {
    setupUi();
    setupConnections();
}

MainWindow::~MainWindow() {
    // Cleanup if needed
}

void MainWindow::setupUi() {
    // Set up the main window
    setWindowTitle(tr("High Speed Traffic Generator"));
    resize(1200, 800);

    // initialize labels
    QFont font;
    font.setPointSize(18);
    font.setBold(true);

    m_interfaceLabel = new QLabel(tr("Interface:"));
    m_interfaceLabel->setMinimumSize(MINIMUM_PARAM_LABEL_WIDTH, MINIMUM_PARAM_LABEL_HEIGHT);
    m_interfaceLabel->setAlignment(Qt::AlignCenter);
    m_interfaceLabel->setFont(font);
    m_interfaceLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_modeLabel = new QLabel(tr("Mode:"));
    m_modeLabel->setMinimumSize(MINIMUM_PARAM_LABEL_WIDTH, MINIMUM_PARAM_LABEL_HEIGHT);
    m_modeLabel->setAlignment(Qt::AlignCenter);
    m_modeLabel->setFont(font);
    m_modeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_prefferedSpeedLabel = new QLabel(tr("Preffered Speed:"));
    m_prefferedSpeedLabel->setMinimumSize(MINIMUM_PARAM_LABEL_WIDTH, MINIMUM_PARAM_LABEL_HEIGHT);
    m_prefferedSpeedLabel->setAlignment(Qt::AlignCenter);
    m_prefferedSpeedLabel->setFont(font);
    m_prefferedSpeedLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_packetSizeLabel = new QLabel(tr("Packet Size:"));
    m_packetSizeLabel->setMinimumSize(MINIMUM_PARAM_LABEL_WIDTH, MINIMUM_PARAM_LABEL_HEIGHT);
    m_packetSizeLabel->setAlignment(Qt::AlignCenter);
    m_packetSizeLabel->setFont(font);
    m_packetSizeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_burstSizeLabel = new QLabel(tr("Burst Size:"));
    m_burstSizeLabel->setMinimumSize(MINIMUM_PARAM_LABEL_WIDTH, MINIMUM_PARAM_LABEL_HEIGHT);
    m_burstSizeLabel->setAlignment(Qt::AlignCenter);
    m_burstSizeLabel->setFont(font);
    m_burstSizeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_copiesLabel = new QLabel(tr("Copies:"));
    m_copiesLabel->setMinimumSize(MINIMUM_PARAM_LABEL_WIDTH, MINIMUM_PARAM_LABEL_HEIGHT);
    m_copiesLabel->setAlignment(Qt::AlignCenter);
    m_copiesLabel->setFont(font);
    m_copiesLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_sentLabel = new QLabel(tr("Sent:"));
    m_sentLabel->setMinimumSize(MINIMUM_PARAM_LABEL_WIDTH, MINIMUM_PARAM_LABEL_HEIGHT);
    m_sentLabel->setAlignment(Qt::AlignCenter);
    m_sentLabel->setFont(font);
    m_sentLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_packetPatternLabel = new QLabel(tr("Packet Pattern:"));
    m_packetPatternLabel->setMinimumSize(MINIMUM_PARAM_LABEL_WIDTH, MINIMUM_PARAM_LABEL_HEIGHT);
    m_packetPatternLabel->setAlignment(Qt::AlignCenter);
    m_packetPatternLabel->setFont(font);
    m_packetPatternLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_timeLabel = new QLabel(tr("Time:"));
    m_timeLabel->setMinimumSize(MINIMUM_INFO_LABEL_WIDTH, MINIMUM_INFO_LABEL_HEIGHT);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setFont(font);
    m_timeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_speedLabel = new QLabel(tr("Speed:"));
    m_speedLabel->setMinimumSize(MINIMUM_INFO_LABEL_WIDTH, MINIMUM_INFO_LABEL_HEIGHT);
    m_speedLabel->setAlignment(Qt::AlignCenter);
    m_speedLabel->setFont(font);
    m_speedLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_totalSendLabel = new QLabel(tr("Total Sent:"));
    m_totalSendLabel->setMinimumSize(MINIMUM_INFO_LABEL_WIDTH, MINIMUM_INFO_LABEL_HEIGHT);
    m_totalSendLabel->setAlignment(Qt::AlignCenter);
    m_totalSendLabel->setFont(font);
    m_totalSendLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_totalCopiesLabel = new QLabel(tr("Total Copies:"));
    m_totalCopiesLabel->setMinimumSize(MINIMUM_INFO_LABEL_WIDTH, MINIMUM_INFO_LABEL_HEIGHT);
    m_totalCopiesLabel->setAlignment(Qt::AlignCenter);
    m_totalCopiesLabel->setFont(font);
    m_totalCopiesLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_speedUnitLabel = new QLabel(tr("MBps"));
    m_speedUnitLabel->setMinimumSize(MINIMUM_UNIT_LABEL_WIDTH, MINIMUM_UNIT_LABEL_HEIGHT);
    m_speedUnitLabel->setAlignment(Qt::AlignCenter);
    m_speedUnitLabel->setFont(font);
    m_speedUnitLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_totalSendUnitLabel = new QLabel(tr("B"));
    m_totalSendUnitLabel->setMinimumSize(MINIMUM_UNIT_LABEL_WIDTH, MINIMUM_UNIT_LABEL_HEIGHT);
    m_totalSendUnitLabel->setAlignment(Qt::AlignCenter);
    m_totalSendUnitLabel->setFont(font);
    m_totalSendUnitLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);


    // Initialize buttons
    m_fileSendButton = new QPushButton(tr("Send File"));
    m_fileSendButton->setMinimumSize(80, 30);
    m_fileSendButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_selectFileButton = new QPushButton();
    m_selectFileButton->setMinimumSize(100, 30);
    m_selectFileButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirOpenIcon));
    m_selectFileButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_startButton = new QPushButton();
    m_startButton->setMinimumSize(MINIMUM_PARAM_BUTTON_WIDTH, MINIMUM_PARAM_BUTTON_HEIGHT);
    m_startButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
    m_startButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_pauseButton = new QPushButton();
    m_pauseButton->setMinimumSize(MINIMUM_PARAM_BUTTON_WIDTH, MINIMUM_PARAM_BUTTON_HEIGHT);
    m_pauseButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPause));
    m_pauseButton->setEnabled(false);
    m_pauseButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // Initialize combo boxes
    m_interfaceComboBox = new QComboBox();
    m_interfaceComboBox->setMinimumSize(MINIMUM_PARAM_COMBOBOX_WIDTH, MINIMUM_PARAM_COMBOBOX_HEIGHT);
    m_interfaceComboBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_modeComboBox = new QComboBox();
    m_modeComboBox->setMinimumSize(MINIMUM_PARAM_COMBOBOX_WIDTH, MINIMUM_PARAM_COMBOBOX_HEIGHT);
    m_modeComboBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // Initialize line edits
    m_prefferedSpeedLineEdit = new QLineEdit();
    m_prefferedSpeedLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_prefferedSpeedLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_packetSizeLineEdit = new QLineEdit();
    m_packetSizeLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_packetSizeLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_burstSizeLineEdit = new QLineEdit();
    m_burstSizeLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_burstSizeLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_copiesLineEdit = new QLineEdit();
    m_copiesLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_copiesLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_sentLineEdit = new QLineEdit();
    m_sentLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_sentLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_packetPatternLineEdit = new QLineEdit();
    m_packetPatternLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_packetPatternLineEdit->setPlaceholderText("AA");
    m_packetPatternLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_pathToFileLineEdit = new QLineEdit();
    m_pathToFileLineEdit->setMinimumSize(2 * MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_pathToFileLineEdit->setPlaceholderText("Path to file");
    m_pathToFileLineEdit->setReadOnly(true);
    m_pathToFileLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_timeLineEdit = new QTimeEdit();
    m_timeLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_timeLineEdit->setDisplayFormat("hh:mm:ss");
    m_timeLineEdit->setTime(QTime(0, 0, 0));
    m_timeLineEdit->setAlignment(Qt::AlignCenter);
    m_timeLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_speedLineEdit = new QLineEdit();
    m_speedLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_speedLineEdit->setReadOnly(true);
    m_speedLineEdit->setText("0");
    m_speedLineEdit->setAlignment(Qt::AlignRight);
    m_speedLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_totalSendLineEdit = new QLineEdit();
    m_totalSendLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_totalSendLineEdit->setReadOnly(true);
    m_totalSendLineEdit->setText("0");
    m_totalSendLineEdit->setAlignment(Qt::AlignRight);
    m_totalSendLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_totalCopiesLineEdit = new QLineEdit();
    m_totalCopiesLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_totalCopiesLineEdit->setReadOnly(true);
    m_totalCopiesLineEdit->setText("0");
    m_totalCopiesLineEdit->setAlignment(Qt::AlignCenter);
    m_totalCopiesLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // Initialize charts
    m_ppsChart = new QChart();
    m_ppsChart->setTitle(tr("Packets per second"));
    m_ppsChart->setAnimationOptions(QChart::AllAnimations);
    m_ppsChart->setTheme(QChart::ChartThemeDark);
    m_ppsChartView = new QChartView(m_ppsChart);
    
    m_bpsChart = new QChart();
    m_bpsChart->setTitle(tr("Bits per second"));
    m_bpsChart->setAnimationOptions(QChart::AllAnimations);
    m_bpsChart->setTheme(QChart::ChartThemeDark);
    m_bpsChartView = new QChartView(m_bpsChart);
    
    m_BPSChart = new QChart();
    m_BPSChart->setTitle(tr("Bytes per second"));
    m_BPSChart->setAnimationOptions(QChart::AllAnimations);
    m_BPSChart->setTheme(QChart::ChartThemeDark);
    m_BPSChartView = new QChartView(m_BPSChart);
    
    m_packetLossChart = new QChart();
    m_packetLossChart->setTitle(tr("Packet Loss"));
    m_packetLossChart->setAnimationOptions(QChart::AllAnimations);
    m_packetLossChart->setTheme(QChart::ChartThemeDark);
    m_packetLossChartView = new QChartView(m_packetLossChart);

    // Initialize tab widget
    m_tabWidget = new QTabWidget();
    m_tabWidget->setMinimumSize(300, 400);
    m_tabWidget->addTab(m_ppsChartView, tr("Packets per second"));
    m_tabWidget->addTab(m_bpsChartView, tr("Bits per second"));
    m_tabWidget->addTab(m_BPSChartView, tr("Bytes per second"));
    m_tabWidget->addTab(m_packetLossChartView, tr("Packet Loss"));
    m_tabWidget->setCurrentIndex(0);
    m_tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Set up group boxes
    m_parametersGroupBox = new QGroupBox(tr("Parameters"));
    m_parametersGroupBox->setMinimumSize(600, 400);
    m_parametersGroupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_infoGroupBox = new QGroupBox(tr("Info"));
    m_infoGroupBox->setMinimumSize(200, 400);
    m_infoGroupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_infoGroupBox->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_infoGroupBox->setFocusPolicy(Qt::NoFocus);

    // Set up the layout
    QSpacerItem *edgeSpacer = new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
    QSpacerItem *infoSpacer = new QSpacerItem(20, 400, QSizePolicy::Fixed, QSizePolicy::Maximum);

    m_timeLayout = new QVBoxLayout();
    m_timeLayout->addWidget(m_timeLabel);
    m_timeLayout->addWidget(m_timeLineEdit);

    m_controlLayout = new QHBoxLayout();
    m_controlLayout->addWidget(m_startButton);
    m_controlLayout->addWidget(m_pauseButton);
    m_controlLayout->addLayout(m_timeLayout);

    m_speedLayout = new QHBoxLayout();
    m_speedLayout->addWidget(m_speedLineEdit);
    m_speedLayout->addWidget(m_speedUnitLabel);

    m_totalSendLayout = new QHBoxLayout();
    m_totalSendLayout->addWidget(m_totalSendLineEdit);
    m_totalSendLayout->addWidget(m_totalSendUnitLabel);

    m_infoLayout = new QVBoxLayout();
    m_infoLayout->addWidget(m_speedLabel);
    m_infoLayout->addLayout(m_speedLayout);
    m_infoLayout->addWidget(m_totalSendLabel);
    m_infoLayout->addLayout(m_totalSendLayout);
    m_infoLayout->addWidget(m_totalCopiesLabel);
    m_infoLayout->addWidget(m_totalCopiesLineEdit);
    m_infoLayout->addItem(infoSpacer);

    m_interfaceLayout = new QHBoxLayout();
    m_interfaceLayout->addWidget(m_interfaceLabel);
    m_interfaceLayout->addWidget(m_interfaceComboBox);

    m_modeLayout = new QHBoxLayout();
    m_modeLayout->addWidget(m_modeLabel);
    m_modeLayout->addWidget(m_modeComboBox);

    m_prefferedSpeedLayout = new QHBoxLayout();
    m_prefferedSpeedLayout->addWidget(m_prefferedSpeedLabel);
    m_prefferedSpeedLayout->addWidget(m_prefferedSpeedLineEdit);

    m_packetSizeLayout = new QHBoxLayout();
    m_packetSizeLayout->addWidget(m_packetSizeLabel);
    m_packetSizeLayout->addWidget(m_packetSizeLineEdit);

    m_burstSizeLayout = new QHBoxLayout();
    m_burstSizeLayout->addWidget(m_burstSizeLabel);
    m_burstSizeLayout->addWidget(m_burstSizeLineEdit);

    m_copiesLayout = new QHBoxLayout();
    m_copiesLayout->addWidget(m_copiesLabel);
    m_copiesLayout->addWidget(m_copiesLineEdit);

    m_sentLayout = new QHBoxLayout();
    m_sentLayout->addWidget(m_sentLabel);
    m_sentLayout->addWidget(m_sentLineEdit);

    m_packetPatternLayout = new QHBoxLayout();
    m_packetPatternLayout->addWidget(m_fileSendButton);
    m_packetPatternLayout->addWidget(m_packetPatternLabel);
    m_packetPatternLayout->addWidget(m_packetPatternLineEdit);

    m_filePathLayout = new QHBoxLayout();
    m_filePathLayout->addWidget(m_pathToFileLineEdit);
    m_filePathLayout->addWidget(m_selectFileButton);
    
    m_parametersLayout = new QVBoxLayout();
    m_parametersLayout->addLayout(m_interfaceLayout);
    m_parametersLayout->addLayout(m_modeLayout);
    m_parametersLayout->addLayout(m_prefferedSpeedLayout);
    m_parametersLayout->addLayout(m_packetSizeLayout);
    m_parametersLayout->addLayout(m_burstSizeLayout);
    m_parametersLayout->addLayout(m_copiesLayout);
    m_parametersLayout->addLayout(m_sentLayout);
    m_parametersLayout->addLayout(m_packetPatternLayout);
    m_parametersLayout->addLayout(m_filePathLayout);

    m_parametersGroupBox->setLayout(m_parametersLayout);
    m_infoGroupBox->setLayout(m_infoLayout);

    m_graphLayout = new QVBoxLayout();
    m_graphLayout->addWidget(m_tabWidget);
    m_graphLayout->addLayout(m_controlLayout);

    m_centeredLayout = new QHBoxLayout();
    m_centeredLayout->addLayout(m_graphLayout);
    m_centeredLayout->addWidget(m_infoGroupBox);
    m_centeredLayout->addWidget(m_parametersGroupBox);

    m_mainLayout = new QVBoxLayout();
    m_mainLayout->addItem(edgeSpacer);
    m_mainLayout->addLayout(m_centeredLayout);
    m_mainLayout->addItem(edgeSpacer);

    m_centralWidget = new QWidget();
    m_centralWidget->setLayout(m_mainLayout);

    this->setCentralWidget(m_centralWidget);

    // Set up the menu bar
    m_menuBar = new QMenuBar(this);
    m_fileMenu = new QMenu(tr("File"), this);
    m_savingsMenu = new QMenu(tr("Save.."), this);
    m_savePpsGraphAction = new QAction(tr("Save pps Graph"), this);
    m_saveBpsGraphAction = new QAction(tr("Save bps Graph"), this);
    m_saveBPSGraphAction = new QAction(tr("Save BPS Graph"), this);
    m_savePacketLossGraphAction = new QAction(tr("Save Packet Loss Graph"), this);
    m_savingsMenu->addAction(m_savePpsGraphAction);
    m_savingsMenu->addAction(m_saveBpsGraphAction);
    m_savingsMenu->addAction(m_saveBPSGraphAction);
    m_savingsMenu->addAction(m_savePacketLossGraphAction);
    m_fileMenu->addMenu(m_savingsMenu);
    m_menuBar->addMenu(m_fileMenu);


    m_settingsMenu = new QMenu(tr("Settings"), this);
    m_themeAction = new QAction(tr("Theme"), this);
    m_interfaceAction = new QAction(tr("Language"), this);
    m_settingsMenu->addAction(m_themeAction);
    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_interfaceAction);
    m_menuBar->addMenu(m_settingsMenu);
    
    m_aboutAction = new QAction(tr("About"), this);
    m_howToUseAction = new QAction(tr("How to use"), this);
    m_menuBar->addAction(m_howToUseAction);
    m_menuBar->addSeparator();
    m_menuBar->addAction(m_aboutAction);   
    this->setMenuBar(m_menuBar);
}

void MainWindow::setupConnections() {

}