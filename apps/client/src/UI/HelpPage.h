//
// Created by maksgmaks on 16.04.26.
//

#ifndef TRAFFICGENERATOR_HELPPAGE_H
#define TRAFFICGENERATOR_HELPPAGE_H

#include <QMainWindow>


QT_BEGIN_NAMESPACE

namespace Ui {
    class HelpPage;
}

QT_END_NAMESPACE

class HelpPage : public QMainWindow {
    Q_OBJECT

public:
    explicit HelpPage(QWidget *parent = nullptr);

    ~HelpPage() override;

private:
    Ui::HelpPage *ui;
};


#endif //TRAFFICGENERATOR_HELPPAGE_H