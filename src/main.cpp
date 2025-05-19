#include <iostream>

#include <QApplication>
#include <filesystem>

#include "Generator/common_generator.hpp"
#include "Generator/Generator.hpp"
#include "UI/MainWindow.hpp"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    std::vector<interfaceModes> ifModes = findAllDevices();
    initialize_dpdk(ifModes);
    std::cout << "\nCheck for PF_RING\n"; 
    for(auto& dev : ifModes) {
        dev.pf_ring_zc_support = check_pfring_zc(dev.interfaceName);
        dev.pf_ring_standart_support = check_pfring_standard(dev.interfaceName);
    }
    for(auto dev : ifModes) {
        std::cout << "Device: " << dev.interfaceName << "\tSupport: " << ((dev.dpdk_support) ? "DPDK\t" : "NO DPDK\t") 
                                << ((dev.pf_ring_zc_support) ? "PF_ZC\t" : "NO PF_ZC\t") << ((dev.pf_ring_standart_support) ? "PF" : "NO PF") << std::endl;
    }

    MainWindow *mainWindow = new MainWindow();
    mainWindow->show();
    Generator* gen = new Generator;

    std::filesystem::path filePath = std::filesystem::current_path() / "test.txt"; 
    genParams params = {"ham0", 1, 0, 0, 1024, true, filePath.string(), 6, 0, 64};
    gen->doStart(params);

    return app.exec();
}