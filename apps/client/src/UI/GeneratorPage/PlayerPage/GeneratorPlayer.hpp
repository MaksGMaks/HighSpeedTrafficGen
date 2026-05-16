#pragma once
#include <QWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QNetworkInterface>

#include  "generator_values.hpp"

#include "ui_GeneratorPlayer.h"

namespace Ui { class GeneratorPlayer; }

class GeneratorPlayer : public QWidget {
    Q_OBJECT
public:
    explicit GeneratorPlayer(QWidget *parent = nullptr);
    ~GeneratorPlayer();

    // Read current UI state
    PcapParams::PlayerSettings settings() const;

    // Update file info fields from outside (e.g. after reading pcap header)
    void setFileInfo(int totalPackets, qint64 totalBytes,
                   double durationSecs, const QString &linkType);

    void setEnabled(bool enabled); // lock/unlock during playback
    void changeEvent(QEvent *event) override;

signals:
    void settingsChanged(PcapParams::PlayerSettings settings);

private slots:
    void onBrowseClicked();
    void onClearFileClicked();
    void onSpeedModeChanged(int idx);
    void onLoopCheckToggled(bool checked);

private:
    static QString formatBytes(qint64 bytes);
    static QString formatDuration(double seconds);

    Ui::GeneratorPlayer *ui;
};