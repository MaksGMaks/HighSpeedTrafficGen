sudo insmod ./pf_ring.ko [min_num_slots=N] [enable_tx_capture=1|0] [ enable_ip_defrag=1|0]

min_num_slots
Minimum number of packets the kernel module should be able to enqueue (default – 4096).
enable_tx_capture
Set to 1 to capture outgoing packets, set to 0 to disable capture outgoing packets (default – RX+TX).
enable_ip_defrag
Set to 1 to enable IP defragmentation, only RX traffic is defragmented (default – disabled)