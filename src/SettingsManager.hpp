#pragma once

#include <QString>
#include <QJsonObject>

class SettingsManager {
public:
    SettingsManager(const QString &filePath);

    bool load();
    bool save();

    QString theme() const;
    void setTheme(const QString &theme);

    QString language() const;
    void setLanguage(const QString &language);

private:
    QString m_filePath;
    QString m_theme = "light";      // default
    QString m_language = "en";      // default

    bool validate(const QJsonObject &obj);
};