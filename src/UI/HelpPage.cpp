#include "HelpPage.hpp"

HelpPage::HelpPage(QWidget *parent)
    : QMainWindow(parent), mainLayout(new QVBoxLayout(this)), helpTextBrowser(new QTextBrowser(this)) {
    setupUi();
    loadHelpContent();
}

void HelpPage::setupUi() {
    setWindowTitle("Help");
    setMinimumSize(800, 600);

    helpTextBrowser->setOpenExternalLinks(true);
    helpTextBrowser->setReadOnly(true);
    helpTextBrowser->setLineWrapMode(QTextEdit::NoWrap);

    mainLayout->addWidget(helpTextBrowser);
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
}

void HelpPage::loadHelpContent() {
    QString helpContent = QString(
        "<h1>%1</h1>"
        "<p>%2</p>"
        "<h2>%3</h2>"
        "<ul>"
        "<li><strong>%4</strong>: %5</li>"
        "<li><strong>%6</strong>: %7</li>"
        "<li><strong>%8</strong>: %9</li>"
        "</ul>"
        "<h2>%10</h2>"
        "<p>%11</p>"
        "<ul>"
        "<li><strong>%12</strong>: %13</li>"
        "<li><strong>%14</strong>: %15</li>"
        "<li><strong>%16</strong>: %17</li>"
        "<li><strong>%18</strong>: %19</li>"
        "<li><strong>%20</strong>: %21</li>"
        "<li><strong>%22</strong>: %23</li>"
        "<li><strong>%24</strong>: %25</li>"
        "</ul>"
        "<h2>%26</h2>"
        "<p>%27</p>"
    ).arg(
        tr("High Speed Traffic Generator Help"),
        tr("This application allows you to generate high-speed network traffic using various modes and configurations."),
        tr("Modes"),
        tr("PF_RING Standard"), tr("Uses PF_RING for packet generation."),
        tr("PF_RING ZC"), tr("Uses PF_RING Zero Copy for high-performance packet generation."),
        tr("DPDK"), tr("Uses DPDK for efficient packet processing."),
        tr("Parameters"),
        tr("You can configure the following parameters:"),
        tr("Interface"), tr("Select the network interface to use."),
        tr("Preffered Speed"), tr("Set the desired speed for packet transmission."),
        tr("Packet Size"), tr("Specify the size of packets to be sent."),
        tr("Burst Size"), tr("Define the number of packets sent in a burst."),
        tr("Copies"), tr("Set the number of copies of each packet to send."),
        tr("Packet Pattern"), tr("Choose a predefined packet pattern or specify a custom one."),
        tr("File Path"), tr("Select a file containing packet data to send."),
        tr("Controls"),
        tr("The main controls include Start, Pause, Resume, and Stop buttons to manage the traffic generation process.")
    );

    helpTextBrowser->setHtml(helpContent);
}