#include "MainWindow.hpp"

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent)
, m_settingsManager((std::filesystem::current_path() / "settings.json").string().c_str()) {
    std::cout << "[MainWindow::MainWindow] Initializing MainWindow" << std::endl;
    m_dpdkInitialized = initialize_dpdk();
    setupUtilitiesThread();
    setupUi();
    setupConnections();
    onRefreshInterfacesActionTriggered();
    setupSettings();
    m_utilitiesThread->start();
    m_totalTime = 0;
}

MainWindow::~MainWindow() {
    std::cout << "[MainWindow::~MainWindow] Destroying MainWindow" << std::endl;
    m_generator->doStop();
    m_utilitiesThread->quit();
    m_utilitiesThread->wait();
}

void MainWindow::onStartButtonClicked() {
    std::cout << "[MainWindow::onStartButtonClicked] Start/Stop button clicked" << std::endl;
    if(m_startButton->isFlat()) {
        m_startButton->setFlat(false);
        m_startButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
        m_pauseButton->setEnabled(false);
        if(m_pauseButton->isFlat()) {
            m_pauseButton->setFlat(false);
            m_pauseButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPause));
        }
        m_parametersGroupBox->setEnabled(true);
        m_timeLineEdit->setEnabled(true);
        emit stopGenerator();
    } else {
        if(m_prefferedSpeedLineEdit->text().isEmpty() || m_packetSizeLineEdit->text().isEmpty() ||
           m_burstSizeLineEdit->text().isEmpty() || m_copiesLineEdit->text().isEmpty() ||
           m_sentLineEdit->text().isEmpty() || m_packetPatternLineEdit->text().isEmpty()) {
            QMessageBox::warning(this, tr("Input Error"), tr("Please fill in all fields."));
            return;
        } 
        if(m_fileSendButton->isFlat() && m_pathToFileLineEdit->text().isEmpty()) {
            QMessageBox::warning(this, tr("Input Error"), tr("Please select a file to send."));
            return;
        }
        if(m_packetSizeLineEdit->text().toInt() < 10) {
            QMessageBox::warning(this, tr("Input Error"), tr("Packet size must be greater than 10 bytes."));
            return;
        }
        if(m_burstSizeLineEdit->text().toInt() < 1) {
            QMessageBox::warning(this, tr("Input Error"), tr("Burst size must be at least 1."));
            return;
        }
        if(m_modeComboBox->currentData() == 0) {
            QMessageBox::warning(this, tr("Input Error"), tr("Please select a valid interface mode."));
            return;
        }

        m_startButton->setFlat(true);
        m_startButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaStop));

        genParams params;
        params.interfaceName = m_interfaceComboBox->currentText().toStdString();
        params.mode = m_modeComboBox->currentData().toUInt();
        params.time = m_timeLineEdit->time().hour() * 3600 +
                      m_timeLineEdit->time().minute() * 60 +
                      m_timeLineEdit->time().second();
        params.speed = m_prefferedSpeedLineEdit->text().toULongLong();
        params.packSize = m_packetSizeLineEdit->text().toUInt();
        params.burstSize = m_burstSizeLineEdit->text().toUInt();
        params.copies = m_copiesLineEdit->text().toULongLong();
        params.fileSend = m_fileSendButton->isFlat();
        params.filePath = m_pathToFileLineEdit->text().toStdString();
        params.totalSend = m_sentLineEdit->text().toULongLong();
        params.packetPattern =  m_packetPatternLineEdit->text().toUInt(nullptr, 16);

        m_isFromZero = (m_timeLineEdit->time().msecsSinceStartOfDay() == 0);
        m_pauseButton->setEnabled(true);
        m_parametersGroupBox->setEnabled(false);

        m_bpsSeries->clear();
        m_BPSSeries->clear();
        m_totalCopiesLineEdit->setText("0");
        m_totalSendLineEdit->setText("0");
        m_speedLineEdit->setText("0");
        m_ppsLineEdit->setText("0");
        m_totalTime = 0;
        m_bpsAxisX->setRange(0, 10);
        m_BPSAxisX->setRange(0, 10);
        m_bpsAxisY->setRange(0, 100);
        m_BPSAxisY->setRange(0, 100);
        
        m_timeLineEdit->setEnabled(false);

        emit startGenerator(params);
    }
}

void MainWindow::onPauseButtonClicked() {
    std::cout << "[MainWindow::onPauseButtonClicked] Pause/Resume button clicked" << std::endl;
    if(m_pauseButton->isFlat()) {
        m_pauseButton->setFlat(false);
        m_pauseButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPause));
        emit resumeGenerator();
    } else {
        m_pauseButton->setFlat(true);
        m_pauseButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaSeekForward));
        emit pauseGenerator();
    }
}

void MainWindow::onFileSendButtonClicked() {
    std::cout << "[MainWindow::onFileSendButtonClicked] File send button clicked" << std::endl;
    if(m_fileSendButton->isFlat()) {
        m_fileSendButton->setFlat(false);
        m_fileSendButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogNoButton));
    } else {
        m_fileSendButton->setFlat(true);
        m_fileSendButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogYesButton));
    }
}

void MainWindow::onSelectFileButtonClicked() {
    std::cout << "[MainWindow::onSelectFileButtonClicked] Select file button clicked" << std::endl;
    QFileDialog fileDialog(this);
    fileDialog.setOption(QFileDialog::ShowDirsOnly, false);
    fileDialog.setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog.setNameFilter(tr("Text Files (*.txt);;Binary Files (*.bin);;Hex Files (*.hex)"));
    fileDialog.setViewMode(QFileDialog::Detail);
    fileDialog.setDirectory(QDir::homePath());
    fileDialog.setFilter(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    fileDialog.setFileMode(QFileDialog::ExistingFiles);

    if (fileDialog.exec() == QDialog::Accepted) {
        QStringList selectedFiles = fileDialog.selectedFiles();
        if (!selectedFiles.isEmpty()) {
            m_pathToFileLineEdit->setText(selectedFiles.first());
        }
    }
}

void MainWindow::onSaveBpsGraphActionTriggered() {
    std::cout << "[MainWindow::onSaveBpsGraphActionTriggered] Save BPS graph action triggered" << std::endl;
    saveGraph(m_bpsChartView);
}

void MainWindow::onSaveBPSGraphActionTriggered() {
    std::cout << "[MainWindow::onSaveBPSGraphActionTriggered] Save BPS graph action triggered" << std::endl;
    saveGraph(m_BPSChartView);
}

// void MainWindow::onAboutActionTriggered() {
//     // Show about dialog logic
// }

void MainWindow::onHelpActionTriggered() {
    std::cout << "[MainWindow::onHelpActionTriggered] Help action triggered" << std::endl;
    if (!m_helpPage->isVisible()) {
        m_helpPage->show();
    } else {
        m_helpPage->raise();
        m_helpPage->activateWindow();
    }
}

void MainWindow::onRefreshInterfacesActionTriggered() {
    std::cout << "[MainWindow::onRefreshInterfacesActionTriggered] Refresh interfaces action triggered" << std::endl;
    this->setCursor(Qt::WaitCursor);
    this->setEnabled(false);
    m_interfacesMap.clear();
    m_interfaces.clear();
    m_interfaceComboBox->clear();
    m_interfaces = findAllDevices();
    if(m_dpdkInitialized) {
        checkDPDKSupport(m_interfaces);
    }
    for(auto& dev : m_interfaces) {
        dev.pf_ring_zc_support = check_pfring_zc(dev.interfaceName);
        dev.pf_ring_standart_support = check_pfring_standard(dev.interfaceName);
        m_interfacesMap.insert({dev.interfaceName, dev});
    }
    for(auto& dev : m_interfaces) {
        m_interfaceComboBox->addItem(dev.interfaceName.c_str());
    }
    m_interfaceComboBox->setCurrentIndex(0);
    this->setEnabled(true);
    this->setCursor(Qt::ArrowCursor);
}

void MainWindow::onInterfaceChanged(const QString &interfaceName) {
    std::cout << "[MainWindow::onInterfaceChanged] Interface changed to: " << interfaceName.toStdString() << std::endl;
    m_modeComboBox->clear();
    if(m_interfacesMap.find(interfaceName.toStdString()) != m_interfacesMap.end()) {
        auto& dev = m_interfacesMap[interfaceName.toStdString()];
        if(dev.pf_ring_standart_support) {
            m_modeComboBox->addItem(tr("PF_RING Standard"), QVariant::fromValue(PF_RING_STANDARD));
        }
        if(dev.pf_ring_zc_support) {
            m_modeComboBox->addItem(tr("PF_RING ZC"), QVariant::fromValue(PF_RING_ZC));
        }
        if(dev.dpdk_support) {
            m_modeComboBox->addItem(tr("DPDK"), QVariant::fromValue(DPDK));
        }
        if(!dev.pf_ring_zc_support && !dev.pf_ring_standart_support && !dev.dpdk_support) {
            m_modeComboBox->addItem(tr("None"), QVariant::fromValue(0));
        }
    }
    m_modeComboBox->setCurrentIndex(0);
}

void MainWindow::saveGraph(const QChartView *chartView) {
    std::cout << "[MainWindow::saveGraph] Saving graph" << std::endl;
    QGraphicsScene* scene = chartView->scene();
    if (!scene) {
        QMessageBox::warning(this, "Error", "No graph to save.");
        return;
    }

    QString selectedFilter;
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Save Graph As Image",
        "", // Start in current dir
        "PNG Image (*.png);;JPEG Image (*.jpg);;BMP Image (*.bmp)",
        &selectedFilter
    );

    if (filePath.isEmpty())
        return; // User canceled

    QString extension = "png"; // default
    if (selectedFilter.contains("*.jpg"))
        extension = "jpg";
    else if (selectedFilter.contains("*.bmp"))
        extension = "bmp";
    if (!filePath.endsWith("." + extension, Qt::CaseInsensitive))
        filePath += "." + extension;

    QRectF sceneRect = scene->itemsBoundingRect();
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::white); // Optional

    QPainter painter(&image);
    scene->render(&painter, QRectF(), sceneRect);
    painter.end();

    if (!image.save(filePath)) {
        QMessageBox::warning(this, "Save Failed", "Could not save the image.");
    }
}

void MainWindow::onThemeChanged(const QString &theme) {
    std::cout << "[MainWindow::onThemeChanged] Theme changed to: " << theme.toStdString() << std::endl;
    if (theme == "light") {
        m_settingsManager.setTheme("light");
        m_bpsChart->setTheme(QChart::ChartThemeLight);
        m_BPSChart->setTheme(QChart::ChartThemeLight);
        onApplyStyleSheet(lightStyleSheet);
    } else {
        m_settingsManager.setTheme("dark");
        m_bpsChart->setTheme(QChart::ChartThemeDark);
        m_BPSChart->setTheme(QChart::ChartThemeDark);
        onApplyStyleSheet(darkStyleSheet);
    }
    m_settingsManager.save();
}

void MainWindow::onLanguageChanged(const QString &language) {

}

void MainWindow::onApplyStyleSheet(const QString &styleSheet) {
    std::cout << "[MainWindow::onApplyStyleSheet] Applying style sheet" << std::endl;
    qApp->setStyleSheet(styleSheet);
}

void MainWindow::onGeneratorFinished() {
    std::cout << "[MainWindow::onGeneratorFinished] Generator finished" << std::endl;
    m_startButton->setFlat(false);
    m_startButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
    m_pauseButton->setEnabled(false);
    m_pauseButton->setFlat(false);
    m_pauseButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPause));
    m_parametersGroupBox->setEnabled(true);
    m_timeLineEdit->setEnabled(true);
    if(!m_isFromZero) {
        m_timeLineEdit->setTime(QTime(0, 0, 0));
    }
}

void MainWindow::onUpdateGraph(const uint64_t &pps, const uint64_t &bps, const uint64_t &BPS, const int64_t &time) {
    QTime currentTime = m_timeLineEdit->time();
    if(m_isFromZero && !(time <= 0)) {
        m_timeLineEdit->setTime(currentTime.addSecs(1));
    } else if(!(time <= 0)) {
        m_timeLineEdit->setTime(currentTime.addSecs(-1));
    }
    m_ppsLineEdit->setText(QString::number(pps));
    m_bpsSeries->append(m_totalTime, bps);
    m_BPSSeries->append(m_totalTime, BPS);

// Adjust X-axis (time)
if (m_totalTime > 10) {
    m_bpsAxisX->setRange(m_totalTime - 10, m_totalTime);
    m_BPSAxisX->setRange(m_totalTime - 10, m_totalTime);
}
m_totalTime += 1;

// --- Detect and apply unit for BPS (bytes/sec) chart
    uint64_t accumulatedBPS = BPS;
    QString unitLabel;
    long double displayValue = accumulatedBPS;
    QString axisLabelFormat;
    if (accumulatedBPS >= (1024ULL * 1024 * 1024)) {
        axisLabelFormat = "%.2f GB/s";
        unitLabel = "GB/s";
        displayValue = accumulatedBPS / (long double)(1024ULL * 1024 * 1024);
    } else if (accumulatedBPS >= (1024ULL * 1024)) {
        axisLabelFormat = "%.2f MB/s";
        unitLabel = "MB/s";
        displayValue = accumulatedBPS / (long double)(1024 * 1024);
    } else if (accumulatedBPS >= 1024) {
        axisLabelFormat = "%.2f KB/s";
        unitLabel = "KB/s";
        displayValue = accumulatedBPS / 1024.0;
    } else {
        axisLabelFormat = "%d B/s";
        unitLabel = "B/s";
    }

    m_BPSAxisY->setLabelFormat(axisLabelFormat);
    m_speedUnitLabel->setText(unitLabel);
    m_speedLineEdit->setText(QString::number(displayValue, 'f', 2));

    // --- Detect and apply unit for bps chart
    BpsUnit newUnit = BpsUnit::BPS;
    long double bpsScale = 1.0;
    QString bpsFormat = "%d bps";
    if (bps >= (1000ULL * 1000 * 1000)) {
        newUnit = BpsUnit::GBPS;
        bpsScale = 1000.0 * 1000 * 1000;
        bpsFormat = "%.2f Gbps";
    } else if (bps >= (1000ULL * 1000)) {
        newUnit = BpsUnit::MBPS;
        bpsScale = 1000.0 * 1000;
        bpsFormat = "%.2f Mbps";
    } else if (bps >= 1000) {
        newUnit = BpsUnit::KBPS;
        bpsScale = 1000.0;
        bpsFormat = "%.2f Kbps";
    }

    m_bpsAxisY->setLabelFormat(bpsFormat);

    // --- Scale previous data if unit changed
    if (m_currentBpsUnit != newUnit) {
        double oldScale = 1.0;
        switch (m_currentBpsUnit) {
            case BpsUnit::BPS:  oldScale = 1.0; break;
            case BpsUnit::KBPS: oldScale = 1000.0; break;
            case BpsUnit::MBPS: oldScale = 1000000.0; break;
            case BpsUnit::GBPS: oldScale = 1000000000.0; break;
        }

        for (int i = 0; i < m_bpsSeries->count(); ++i) {
            QPointF pt = m_bpsSeries->at(i);
            double bpsVal = pt.y() * oldScale;       // convert to raw bps
            pt.setY(bpsVal / bpsScale);              // convert to new scale
            m_bpsSeries->replace(i, pt);
        }

        m_currentBpsUnit = newUnit;
    }

    // --- Adjust Y axis ranges
    // m_bpsAxisY->setRange(0, niceCeil(std::max(bps / bpsScale, (long double)m_bpsAxisY->max())));
    // m_BPSAxisY->setRange(0, niceBPSCeil);

    // --- Refresh UI
    m_bpsChartView->update();
    m_BPSChartView->update();
}

void MainWindow::onUpdateDynamicVariables(const uint64_t &totalSend, const uint64_t &totalCopies) {
    m_totalSendLineEdit->setText(QString::number(totalSend));
    m_totalCopiesLineEdit->setText(QString::number(totalCopies));
}

void MainWindow::setupUtilitiesThread() {
    std::cout << "[MainWindow::setupUtilitiesThread] Setting up utilities thread" << std::endl;
    m_utilitiesThread = new QThread(this);
    m_uiUpdater = new UIUpdater();
    m_uiUpdater->moveToThread(m_utilitiesThread);
    m_generator = new Generator();
    m_generator->moveToThread(m_utilitiesThread);
}

void MainWindow::setupSettings() {
    std::cout << "[MainWindow::setupSettings] Setting up settings manager" << std::endl;
    bool loaded = m_settingsManager.load();
    if (!loaded) {
        m_settingsManager.setTheme("light"); // Default theme
        m_settingsManager.setLanguage("en"); // Default language
        m_settingsManager.save();
    }

    // Apply theme from settings
    emit onThemeChanged(m_settingsManager.theme());
}

void MainWindow::setupConnections() {
    std::cout << "[MainWindow::setupConnections] Setting up connections" << std::endl;
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartButtonClicked);
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseButtonClicked);
    connect(m_fileSendButton, &QPushButton::clicked, this, &MainWindow::onFileSendButtonClicked);
    connect(m_selectFileButton, &QPushButton::clicked, this, &MainWindow::onSelectFileButtonClicked);

    connect(m_saveBpsGraphAction, &QAction::triggered, this, &MainWindow::onSaveBpsGraphActionTriggered);
    connect(m_saveBPSGraphAction, &QAction::triggered, this, &MainWindow::onSaveBPSGraphActionTriggered);
    connect(m_darkThemeAction, &QAction::triggered, this, [this]() { onThemeChanged("dark"); });
    connect(m_lightThemeAction, &QAction::triggered, this, [this]() { onThemeChanged("light"); });
    connect(m_uaLanguageAction, &QAction::triggered, this, [this]() { onLanguageChanged("ua"); });
    connect(m_enLanguageAction, &QAction::triggered, this, [this]() { onLanguageChanged("en"); });
    //connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAboutActionTriggered);
    connect(m_helpAction, &QAction::triggered, this, &MainWindow::onHelpActionTriggered);

    connect(m_refreshInterfacesAction, &QAction::triggered, this, &MainWindow::onRefreshInterfacesActionTriggered);
    connect(m_interfaceComboBox, &QComboBox::currentTextChanged, this, &MainWindow::onInterfaceChanged);

    connect(this, &MainWindow::startGenerator, m_generator, &Generator::doStart);
    connect(this, &MainWindow::pauseGenerator, m_generator, &Generator::doPause);
    connect(this, &MainWindow::resumeGenerator, m_generator, &Generator::doResume);
    connect(this, &MainWindow::stopGenerator, m_generator, &Generator::doStop);
    connect(this, &MainWindow::startGenerator, m_uiUpdater, &UIUpdater::onStartGenerator);
    connect(m_generator, &Generator::sendProgress, m_uiUpdater, &UIUpdater::onSendProgress);
    connect(m_generator, &Generator::sendHalfProgress, m_uiUpdater, &UIUpdater::onSendHalfProgress);
    connect(m_generator, &Generator::finished, this, &MainWindow::onGeneratorFinished);

    connect(m_uiUpdater, &UIUpdater::updateGraph, this, &MainWindow::onUpdateGraph);
    connect(m_uiUpdater, &UIUpdater::updateDynamicVariables, this, &MainWindow::onUpdateDynamicVariables);
}

void MainWindow::setupUi() {
    std::cout << "[MainWindow::setupUi] Setting up UI" << std::endl;
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

    m_ppsLabel = new QLabel(tr("PPS:"));
    m_ppsLabel->setMinimumSize(MINIMUM_INFO_LABEL_WIDTH, MINIMUM_INFO_LABEL_HEIGHT);
    m_ppsLabel->setAlignment(Qt::AlignCenter);
    m_ppsLabel->setFont(font);
    m_ppsLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
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
    m_fileSendButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogNoButton));
    m_fileSendButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_selectFileButton = new QPushButton();
    m_selectFileButton->setMinimumSize(50, 30);
    m_selectFileButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirOpenIcon));
    m_selectFileButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    
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
    m_prefferedSpeedLineEdit->setText("0");
    m_prefferedSpeedLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_packetSizeLineEdit = new QLineEdit();
    m_packetSizeLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_packetSizeLineEdit->setText("10");
    m_packetSizeLineEdit->setValidator(new QIntValidator(10, 65535, this));
    m_packetSizeLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_burstSizeLineEdit = new QLineEdit();
    m_burstSizeLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_burstSizeLineEdit->setText("1");
    m_burstSizeLineEdit->setValidator(new QIntValidator(1, 65535, this));
    m_burstSizeLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_copiesLineEdit = new QLineEdit();
    m_copiesLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_copiesLineEdit->setText("0");
    m_copiesLineEdit->setValidator(new UInt64Validator(0, 0xFFFFFFFFFFFFFFFF, this));
    m_copiesLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_sentLineEdit = new QLineEdit();
    m_sentLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_sentLineEdit->setText("0");
    m_sentLineEdit->setValidator(new UInt64Validator(0, 0xFFFFFFFFFFFFFFFF, this));
    m_sentLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
    m_packetPatternLineEdit = new QLineEdit();
    QRegularExpression hexRegex("^[0-9A-Fa-f]*$");
    QValidator *validator = new QRegularExpressionValidator(hexRegex, m_packetPatternLineEdit);
    m_packetPatternLineEdit->setValidator(validator);
    m_packetPatternLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_packetPatternLineEdit->setText("AA");
    m_packetPatternLineEdit->setMaxLength(2);
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

    m_ppsLineEdit = new QLineEdit();
    m_ppsLineEdit->setMinimumSize(MINIMUM_INFO_LINEEDIT_WIDTH, MINIMUM_INFO_LINEEDIT_HEIGHT);
    m_ppsLineEdit->setReadOnly(true);
    m_ppsLineEdit->setText("0");
    m_ppsLineEdit->setAlignment(Qt::AlignRight);
    m_ppsLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    
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
    m_bpsSeries = new QLineSeries();
    m_bpsChart = new QChart();
    m_bpsChart->setTitle(tr("Bits per second"));
    m_bpsChart->setAnimationOptions(QChart::NoAnimation);
    m_bpsChart->setTheme(QChart::ChartThemeDark);
    m_bpsChart->addSeries(m_bpsSeries);
    m_bpsChart->legend()->hide();
    m_bpsAxisX = new QValueAxis();
    m_bpsAxisX->setRange(0, 10);
    m_bpsAxisY = new QValueAxis();
    m_bpsAxisY->setRange(0, 100);
    m_bpsChart->addAxis(m_bpsAxisX, Qt::AlignBottom);
    m_bpsChart->addAxis(m_bpsAxisY, Qt::AlignLeft);
    m_bpsSeries->attachAxis(m_bpsAxisX);
    m_bpsSeries->attachAxis(m_bpsAxisY);
    m_bpsChartView = new QChartView(m_bpsChart);
    
    m_BPSSeries = new QLineSeries();
    m_BPSChart = new QChart();
    m_BPSChart->setTitle(tr("Bytes per second"));
    m_BPSChart->setAnimationOptions(QChart::NoAnimation);
    m_BPSChart->setTheme(QChart::ChartThemeDark);
    m_BPSChart->addSeries(m_BPSSeries);
    m_BPSChart->legend()->hide();
    m_BPSAxisX = new QValueAxis();
    m_BPSAxisX->setRange(0, 10);
    m_BPSAxisY = new QValueAxis();
    m_BPSAxisY->setRange(0, 100);
    m_BPSChart->addAxis(m_BPSAxisX, Qt::AlignBottom);
    m_BPSChart->addAxis(m_BPSAxisY, Qt::AlignLeft);
    m_BPSSeries->attachAxis(m_BPSAxisX);
    m_BPSSeries->attachAxis(m_BPSAxisY);
    m_BPSChartView = new QChartView(m_BPSChart);
    
    // Initialize tab widget
    m_tabWidget = new QTabWidget();
    m_tabWidget->setMinimumSize(300, 400);
    m_tabWidget->addTab(m_bpsChartView, tr("Bits per second"));
    m_tabWidget->addTab(m_BPSChartView, tr("Bytes per second"));
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
    m_infoLayout->addWidget(m_ppsLabel);
    m_infoLayout->addWidget(m_ppsLineEdit);
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
    m_refreshInterfacesAction = new QAction(tr("Refresh Interfaces"), this);
    m_fileMenu->addAction(m_refreshInterfacesAction);
    m_fileMenu->addSeparator();
    m_savingsMenu = new QMenu(tr("Save.."), this);
    m_saveBpsGraphAction = new QAction(tr("Save bps Graph"), this);
    m_saveBPSGraphAction = new QAction(tr("Save BPS Graph"), this);
    m_savingsMenu->addAction(m_saveBpsGraphAction);
    m_savingsMenu->addSeparator();
    m_savingsMenu->addAction(m_saveBPSGraphAction);
    m_fileMenu->addMenu(m_savingsMenu);
    m_menuBar->addMenu(m_fileMenu);

    m_settingsMenu = new QMenu(tr("Settings"), this);
    m_themeMenu = new QMenu(tr("Theme"), this);
    m_darkThemeAction = new QAction(tr("Dark"), this);
    m_lightThemeAction = new QAction(tr("Light"), this);
    m_themeMenu->addAction(m_darkThemeAction);
    m_themeMenu->addSeparator();
    m_themeMenu->addAction(m_lightThemeAction);
    m_settingsMenu->addMenu(m_themeMenu);
    m_languageMenu = new QMenu(tr("Language"), this);
    m_uaLanguageAction = new QAction("Українська", this);
    m_enLanguageAction = new QAction("English", this);
    m_languageMenu->addAction(m_uaLanguageAction);
    m_languageMenu->addSeparator();
    m_languageMenu->addAction(m_enLanguageAction);
    m_settingsMenu->addMenu(m_languageMenu);
    m_menuBar->addMenu(m_settingsMenu);
    
    //m_aboutAction = new QAction(tr("About"), this);
    m_helpAction = new QAction(tr("Help"), this);
    m_menuBar->addAction(m_helpAction);
    //m_menuBar->addSeparator();
    //m_menuBar->addAction(m_aboutAction);   
    this->setMenuBar(m_menuBar);

    // Set up help tooltips
    m_helpPage = new HelpPage(this);
    m_helpPage->setMinimumSize(600, 400);
    
}