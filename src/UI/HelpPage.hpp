#pragma once

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>

class HelpPage : public QMainWindow {
    Q_OBJECT
public:
    explicit HelpPage(QWidget *parent = nullptr);
    ~HelpPage() = default;

private:
    QVBoxLayout *mainLayout;          // Main layout for the HelpPage
    QTextBrowser *helpTextBrowser;    // Text browser to display help content

    void setupUi();                   // Function to set up the UI components
    void loadHelpContent();           // Function to load help content into the text browser
};