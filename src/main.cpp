#include <iostream>

#include <QApplication>
#include <filesystem>

#include "Generator/common_generator.hpp"
#include "Generator/Generator.hpp"
#include "UI/MainWindow.hpp"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow* mainWindow = new MainWindow();
    mainWindow->show();

    return app.exec();
}