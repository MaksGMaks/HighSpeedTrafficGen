#pragma once

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_ethdev.h>

#include "common_generator.hpp"

void initialize_dpdk(std::vector<interfaceModes>& interfaces);
bool isHugepageMounted(const std::string& mountPoint);

void dpdkGenerate();