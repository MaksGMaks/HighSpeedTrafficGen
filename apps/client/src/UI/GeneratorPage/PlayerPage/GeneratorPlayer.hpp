#pragma once
#include <QWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QNetworkInterface>

#include "ui_GeneratorPlayer.h"

namespace Ui { class GeneratorPlayer; }

struct PlayerSettings {
    QString filePath;
    int     startPacket  = 1;
    int     endPacket    = 0;    // 0 = last
    bool    loop         = false;
    int     loopCount    = 1;    // 1 = play once
    int     speedMode    = 0;    // 0=original, 1=multiplier, 2=fixed, 3=max
    double  speedMult    = 1.0;
    int     fixedRate    = 1000; // pkt/s
};

class GeneratorPlayer : public QWidget {
    Q_OBJECT
public:
    explicit GeneratorPlayer(QWidget *parent = nullptr);
    ~GeneratorPlayer();

    // Read current UI state
    PlayerSettings settings() const;

    // Update file info fields from outside (e.g. after reading pcap header)
    void setFileInfo(int totalPackets, qint64 totalBytes,
                   double durationSecs, const QString &linkType);

    void setEnabled(bool enabled); // lock/unlock during playback
    void changeEvent(QEvent *event) override;

signals:
    void settingsChanged(PlayerSettings settings);

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