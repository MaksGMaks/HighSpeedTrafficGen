#pragma once

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QPushButton>

QT_BEGIN_NAMESPACE
namespace Ui { class HelpPage; }
QT_END_NAMESPACE

/**
 * @class HelpPage
 * @brief Modal help dialog for the Traffic Generator application.
 *
 * Displays user documentation organised into sections:
 *   - Getting Started (server connection, window layout)
 *   - Random Generator (how to generate traffic with a statistical law)
 *   - PCAP Player     (how to replay an existing capture file)
 *   - Packet Constructor (overview + per-tab field reference)
 *   - Statistics Window  (metric definitions)
 *
 * Navigation is driven by a QListWidget on the left; the right side
 * shows the corresponding QTextBrowser page inside a QStackedWidget.
 */
class HelpPage : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the HelpPage dialog.
     * @param parent  Parent widget (may be nullptr for a top-level dialog).
     */
    explicit HelpPage(QWidget *parent = nullptr);

    /** @brief Destroys the dialog and its UI. */
    ~HelpPage() override;

    void changeEvent(QEvent *event) override;

    /**
     * @brief Open the dialog and jump directly to a named section.
     *
     * Convenience method so other parts of the application can open
     * context-sensitive help on a specific topic.
     *
     * @param section  One of the Section enum values.
     */
    enum class Section {
        GettingStarted  = 0,
        RandomGenerator = 1,
        PcapPlayer      = 2,
        Constructor     = 3,
        EthernetTab     = 4,
        NetworkProto    = 5,
        TransferProto   = 6,
        DataTab         = 7,
        Statistics      = 8
    };

    void showSection(Section section);

private slots:
    /** @brief Called when the user clicks a navigation item.
     *  Switches the content stack to the matching page. */
    void onNavItemClicked(QListWidgetItem *item);

    /** @brief Closes the dialog (connected to the Close button). */
    void onCloseClicked();

private:
    Ui::HelpPage *ui;

    /** @brief Wires signals/slots after the UI is loaded. */
    void setupConnections();
};