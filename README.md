# CalibrateClk-with-random-gaps-from-Snd
This code is part of a project to improve the accuracy of timestamping packets in networks with an external clock server
Report accuracy and gap trend of clock timestamps vs actual timestamps
To compile: gcc -o <name> <filename.c>
To run:
./rcv <snd1_IP>
./snd1 <rcv_IP> <num_packets> <rate_in_Mbps>
./snd2 <rcvIP> <num_packets> <rate_in_Mbps>
