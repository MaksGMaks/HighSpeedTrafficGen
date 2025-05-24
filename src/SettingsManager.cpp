#include "SettingsManager.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

SettingsManager::SettingsManager(const QString &filePath)
    : m_filePath(filePath) {}

bool SettingsManager::load() {
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Settings file not found, using defaults.";
        return false;  // file missing is not an error, fallback to defaults
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Settings file corrupted: JSON parse error:" << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "Settings file corrupted: root is not an object";
        return false;
    }

    QJsonObject obj = doc.object();

    if (!validate(obj)) {
        qWarning() << "Settings file corrupted: validation failed";
        return false;
    }

    m_theme = obj["theme"].toString("light");
    m_language = obj["language"].toString("en");

    return true;
}

bool SettingsManager::save() {
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open settings file for writing.";
        return false;
    }

    QJsonObject obj;
    obj["theme"] = m_theme;
    obj["language"] = m_language;

    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QString SettingsManager::theme() const {
    return m_theme;
}

void SettingsManager::setTheme(const QString &theme) {
    m_theme = theme;
}

QString SettingsManager::language() const {
    return m_language;
}

void SettingsManager::setLanguage(const QString &language) {
    m_language = language;
}

bool SettingsManager::validate(const QJsonObject &obj) {
    // Validate that keys exist and values are acceptable strings
    if (!obj.contains("theme") || !obj.contains("language")) {
        return false;
    }

    QString theme = obj["theme"].toString();
    QString language = obj["language"].toString();

    // Simple validation - theme must be "light" or "dark"
    if (theme != "light" && theme != "dark") {
        return false;
    }

    // Language validation example - check if length == 2 (e.g. "en", "fr", "ua")
    if (language.length() != 2) {
        return false;
    }

    return true;
}
