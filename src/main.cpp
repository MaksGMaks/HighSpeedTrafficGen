#include <iostream>

#include <QApplication>
#include <filesystem>

#include "Generator/common_generator.hpp"
#include "Generator/Generator.hpp"
#include "UI/MainWindow.hpp"

int main(int argc, char *argv[]) {
    std::vector<std::string> locales = {
        "TrafficGenerator_en.qm",
        "TrafficGenerator_uk.qm",
    };

    std::filesystem::path translationsDir = std::filesystem::current_path() / "Translations";
    for(auto &locale : locales) {
        if(!(std::filesystem::exists(translationsDir / locale))) {
            std::cerr << "[main] Can't load : " << locale << ". File doesn't exist. Terminating" << std::endl;
            return -1;
        }
    }

    QApplication app(argc, argv);
    MainWindow* mainWindow = new MainWindow();
    mainWindow->show();

    return app.exec();
}