# FullSDR for 742

Run make_project.bat (windows) or make_project.sh (linux) to build the project all the way through SD card creation.

To use the linux application, compile with 'gcc full_sdr.c -o sdr'; then call using './sdr XXX.XXX.XXX.XXX YYYY', where X is the IP address of the target and Y is the port number.


Controls:
- To toggle ethernet, press 'e', then enter.
- To set source frequency, press 'f'. At the prompt, type up to 9 decimal numbers, then enter.
- To set tuner frequency, press 't'. At the prompt, type up to 9 decimal numbers, then enter.
- To play a song, press 's', then enter.
- To run the FIFO throughput benchmark, press 'b', then enter.
- To increase the source_frequency by 0100 Hz, press 'u', then enter.
- To increase the source_frequency by 1000 Hz, press 'U', then enter.
- To decrease the source_frequency by 0100 Hz, press 'd', then enter.
- To decrease the source_frequency by 1000 Hz, press 'D', then enter.

