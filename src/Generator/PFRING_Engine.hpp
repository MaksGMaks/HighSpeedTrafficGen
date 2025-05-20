#pragma once

#include <iostream>
#include <vector>
#include <cstdint>

#include <pfring.h>
#include <pfring_zc.h>

bool check_pfring_standard(const std::string& ifname);
bool check_pfring_zc(const std::string& ifname);