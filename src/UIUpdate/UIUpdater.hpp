#pragma once

#include <QObject>

class UIUpdater : public QObject {
    Q_OBJECT
public:
    explicit UIUpdater(QObject *parent = nullptr);
    ~UIUpdater() = default;
signals:

public slots:
    
private:
};