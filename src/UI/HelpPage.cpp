#include "HelpPage.hpp"

HelpPage::HelpPage(QWidget *parent)
: QMainWindow(parent), mainLayout(new QVBoxLayout(this)), helpTextBrowser(new QTextBrowser(this)) {
    setupUi();
    loadHelpContent();
}

void HelpPage::setupUi() {
    setWindowTitle("Help");
    setMinimumSize(1000, 800);
    setWindowIcon(QIcon(":/icons/help_icon.png"));

    helpTextBrowser->setOpenExternalLinks(true);
    helpTextBrowser->setReadOnly(true);
    helpTextBrowser->setLineWrapMode(QTextEdit::NoWrap);

    mainLayout->addWidget(helpTextBrowser);
    QWidget *centralWidget = new QWidget();
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
}

void HelpPage::loadHelpContent() {
    QString helpContent = tr(R"(
    <h1>High Speed Traffic Generator manual page</h1>

    <h2>General Look</h2>
    <p>Generator Window has 4 main blocks:</p>
    <ul>
    <li>Parameters Block on the right;</li>
    <li>Info Block in the centre;</li>
    <li>Graph Info Block on the left;</li>
    <li>Control Block under the Graph Block.</li>
    </ul>

    <h2>Control Block under Graph Block.</h2>
    <p>Control Block consists of two buttons and a time selector.<br>
    First button is the Start/Stop button (has a media-player-like Play icon when the generator is not active and Stop icon when active). Used to start and stop generating.<br>
    Second is the Pause/Resume button. Used to pause generating and resume it.<br><br>
    <b>Note!</b> When the Generator is active, all options in the Parameters Block and time selector are frozen.<br>
    Right of the Pause/Resume button is the Time Selector. When time is set to 0, the Generator will generate continuous traffic until the time hits 23:59:59 (or 1 day of generating). When stopped, the Time Selector will show total time of generating.<br>
    When time is set before Start, time begins to count down to 0, and for this time, Generator will create traffic.
    </p>

    <h2>Parameters Block gives you the opportunity to set generating parameters, such as:</h2>
    <ul>
    <li><b>Interface</b> – Name of the device which will be used for traffic to flow.</li>
    <li><b>Mode</b> – There are 3 modes:<br>
     1) PF_RING_STANDARD – uses default PF_RING to generate traffic. Make sure <code>pfring.ko</code> is loaded in your system.<br>
     2) PF_RING_ZC – a more advanced way to generate faster traffic. Not all devices support it. If on install you used <code>-DLOAD_DRIVER=TRUE</code>, you’ll have a directory with ZC driver and need to load it using <code>./load_driver.sh</code> script.<br>
     3) DPDK – advanced way to generate traffic directly alongside the kernel. Requires DPDK preset before running the program.
    </li>
    <li><b>Preferred Speed</b> – Speed of output traffic.</li>
    <li><b>Packet Size</b> – Size of output packets. Can be from 16B to 64KB.</li>
    <li><b>Burst Size</b> – For ZC and DPDK, size of a burst of packets that will be sent together at the same time. Can be from 1 to 512.</li>
    <li><b>Copies</b> – Total copies that will be sent until generator stops. If set to 0, it will generate an infinite amount of copies. Max value is 18,446,744,073,709,551,615.</li>
    <li><b>Sent</b> – Total amount of bytes that will be sent until generator stops. If set to 0, it will generate an infinite amount. Max value is 18,446,744,073,709,551,615.</li>
    <li><b>Send File button</b> – If active, will send contents of file as packets (e.g. if file has 1024 bytes and packet size is 512, it will send 2 packets).</li>
    <li><b>Packet Pattern</b> – Fill packet data in Hex format.</li>
    <li><b>Path to file</b> – Path to a <code>.txt</code>, <code>.bin</code> or <code>.hex</code> file. Use the folder icon button to select.</li>
    </ul>

    <p><b>IMPORTANT NOTE!</b> Preferred speed does NOT mean that output speed will exactly match the value. Possible reasons for lower speed:</p>
    <ul>
    <li>Too low packet size – small packets require more loops, which reduces speed.</li>
    <li>Too low burst size – same logic as packet size, applicable to ZC and DPDK.</li>
    <li>Hardware limit – network device can't send such volume or HugePages can't handle it.</li>
    <li>Control handling – generator performs time checks and state comparisons which take time.</li>
    </ul>

    <h2>Graph and Info Blocks</h2>
    <p>The Graph Block shows traffic load in bits and bytes (switchable).<br>
    The Info Block shows dynamic values (e.g., speed, packets per second) and static values (total copies sent, total data size sent).</p>

    <h2>Menu Options</h2>
    <ul>
    <li><b>File</b> – lets you:
     <ul>
     <li>Refresh interfaces – useful when network devices have changed.</li>
     <li>Save graphs (bps or BPS) as an image.</li>
     </ul>
    </li>
    <li><b>Settings</b> – lets you:
     <ul>
     <li>Change the application theme (light or dark).</li>
     <li>Select application language (currently supports Ukrainian and English).</li>
     </ul>
    </li>
    <li><b>Help</b> – Opens this page.</li>
    </ul>
    )");

    helpTextBrowser->setHtml(helpContent);
}