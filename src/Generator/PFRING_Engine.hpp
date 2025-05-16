#pragma once

#include <iostream>

#include <pfring.h>
#include <pfring_zc.h>

bool check_pfring_standard(const std::string& ifname);
bool check_pfring_zc(const std::string& ifname);

void pfringGenerate(pfring* ring, const uint8_t* packetData, uint32_t packetLen);
void pfringZCGenerate();