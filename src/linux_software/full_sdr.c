#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

#define _BSD_SOURCE

#define RADIO_FIFO_BASEADDR 0x43c10000
#define ISR 0
#define RLR 9
#define RDR 12
#define RDFO 7
#define RDFD 8
#define SRR 10
#define DELAY 1000000

#define RADIO_PERIPH_ADDRESS 0x43c00000
#define RADIO_TUNER_FAKE_ADC_PINC_OFFSET 0
#define RADIO_TUNER_TUNER_PINC_OFFSET 1
#define RADIO_TUNER_CONTROL_REG_OFFSET 2
#define RADIO_TUNER_TIMER_REG_OFFSET 3

// mutual exclusion semaphore for thread synchronization
volatile int thread_flag = 0;
int ethernet_flag = 1;

// this struct will hold the arguments passed to the play_tune() function,
// which are the radio memory address and the base frequency
struct song_arg_struct {
    volatile unsigned int *ptrToRadio;
    float base_frequency;
};

// this struct will hold the arguments passed to the play_tune() function,
// which are the radio memory address and the base frequency
struct udp_arg_struct {
    volatile unsigned int *ptrToFifo;
    char *ipStr;
    char *portStr;
};

// the below code uses a device called /dev/mem to get a pointer to a physical
// address.  We will use this pointer to read/write the custom peripheral
volatile unsigned int * get_a_pointer(unsigned int phys_addr)
{

    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    void *map_base = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, phys_addr);
    volatile unsigned int *radio_base = (volatile unsigned int *)map_base;
    return (radio_base);
}


void radioTuner_tuneRadio(volatile unsigned int *ptrToRadio, float tune_frequency)
{
    float pinc = (-1.0*tune_frequency)*(float)(1<<27)/125.0e6;
    *(ptrToRadio+RADIO_TUNER_TUNER_PINC_OFFSET)=(int)pinc;
}

void radioTuner_setAdcFreq(volatile unsigned int* ptrToRadio, float freq)
{
    float pinc = freq*(float)(1<<27)/125.0e6;
    *(ptrToRadio+RADIO_TUNER_FAKE_ADC_PINC_OFFSET) = (int)pinc;
}

void* play_tune(void* arg)
{
    int i;
    struct song_arg_struct *arg_struct = (struct song_arg_struct *)arg;

    thread_flag = 1; // signal the thread is active

    float freqs[16] = {1760.0,1567.98,1396.91, 1318.51, 1174.66, 1318.51, 1396.91, 1567.98, 1760.0, 0, 1760.0, 0, 1760.0, 1975.53, 2093.0,0};
    float durations[16] = {1,1,1,1,1,1,1,1,.5,0.0001,.5,0.0001,1,1,2,0.0001};
    for (i=0;i<16;i++)
    {
        radioTuner_setAdcFreq(arg_struct->ptrToRadio,freqs[i]+arg_struct->base_frequency);
        usleep((int)(durations[i]*500000));
    }

    thread_flag = 0; // signal the thread has completed

    return NULL;
}


void* send_udp_data(void* arg)
{
    int len;
    int sock;
    int i;
    unsigned int buf[257];
    unsigned int count = 0;
    struct sockaddr_in dest;
    struct udp_arg_struct *arg_struct = (struct udp_arg_struct *)arg;
    volatile unsigned int *radio_fifo = arg_struct->ptrToFifo;

    // open socket
    // AF_INET = IPv4
    // SOCK_DGRAM is one way, as opposed to SOCK_STREAM
    // 0 is the index into the AF_INET family
    sock = socket(AF_INET,SOCK_DGRAM,0);
    if(sock < 0) {
        printf("\n\rError opening socket\n\r");
        exit(1);
    }

    // configure destination structure
    dest.sin_family = AF_INET;
    inet_pton(dest.sin_family,arg_struct->ipStr,&(dest.sin_addr)); // convert ip addr to binary
    dest.sin_port = htons(atoi(arg_struct->portStr)); // convert int to network byte order

    while (1) {
        while(radio_fifo[RDFO] < 256) {}
        buf[0] = count;
        for(i=0;i<256;i++) {
            len = radio_fifo[RLR]; // must be read
            buf[i+1] = radio_fifo[RDFD];
        }

        // send buffer
        if (ethernet_flag) {
            sendto(sock,buf,sizeof(buf),0,(struct sockaddr *) &dest,sizeof(dest));
            count++;
        }
    }

    return NULL;
}

void print_benchmark(volatile unsigned int *periph_base)
{
    // the below code does a little benchmark, reading from the peripheral a bunch
    // of times, and seeing how many clocks it takes.  You can use this information
    // to get an idea of how fast you can generally read from an axi-lite slave device
    unsigned int start_time;
    unsigned int stop_time;
    start_time = *(periph_base+RADIO_TUNER_TIMER_REG_OFFSET);
    for (int i=0;i<2048;i++)
        stop_time = *(periph_base+RADIO_TUNER_TIMER_REG_OFFSET);
    printf("Elapsed time in clocks = %u\n",stop_time-start_time);
    float throughput=0;

    // please insert your code here for calculate the actual throughput in Mbytes/second
    // how much data was transferred? How long did it take?
    unsigned int bytes_transferred = (4*2049);
    float time_spent = ((float) (stop_time-start_time))/(125000000);
    throughput = ((float) bytes_transferred)/time_spent;

    printf("You transferred %u bytes of data in %f seconds\n",bytes_transferred,time_spent);
    printf("Measured Transfer throughput = %f Mbytes/sec\n",(throughput/1000000));
}

void print_help_message() {
    printf("\n\r\n\rJustin Poiani - Lab 9\n\r\n\r");
    printf("Controls:\n\r");
    printf("- To toggle ethernet, press \'e\', then enter.\n\r");
    printf("- To set source frequency, press \'f\'. At the prompt, type up to 9 decimal numbers, then enter.\n\r");
    printf("- To set tuner frequency, press \'t\'. At the prompt, type up to 9 decimal numbers, then enter.\n\r");
    printf("- To play a song, press \'s\', then enter.\n\r");
    printf("- To run the FIFO throughput benchmark, press \'b\', then enter.\n\r");
    printf("- To increase the source_frequency by 0100 Hz, press \'u\', then enter.\n\r");
    printf("- To increase the source_frequency by 1000 Hz, press \'U\', then enter.\n\r");
    printf("- To decrease the source_frequency by 0100 Hz, press \'d\', then enter.\n\r");
    printf("- To decrease the source_frequency by 1000 Hz, press \'D\', then enter.\n\r");
    printf("Any other key will reprint this message.\n\r");
}

// NOTES: the rlr must be read prior to reading the first word in a packet.
// since the packet size is one, the RLR register must be read before each data
// word to prevent a read overrun.
//
// according to page 38 of PG080, the ip holds a maximum of (depth/4)+2 one
// word packets. for a depth of 1024, that means 258 one word packets.
int main(int argc, char *argv[])
{
    int i = 0;
    pthread_t thread_id;
    struct song_arg_struct song_args;
    struct udp_arg_struct udp_args;
    char inChar, inBuf[16];
    long long unsigned int source_freq = 30e6+1000, old_source_freq = 30e6+1000;
    long long unsigned int tuner_freq = 30e6, old_tuner_freq = 30e6;

    printf("Usage:\n\rSupply exactly 2 command line arguments: IP address of " \
           "destination, followed by the destination port.\n\r");
    if (argc != 3) {
        printf("Please supply exactly two arguments: IP then port number.\n\r");
        return 1;
    }
    print_help_message();

    // first, get a pointer to the peripheral base address using /dev/mem and the function mmap
    volatile unsigned int *my_periph = get_a_pointer(RADIO_PERIPH_ADDRESS);
    volatile unsigned int *radio_fifo = get_a_pointer(RADIO_FIFO_BASEADDR);

    *(my_periph+RADIO_TUNER_CONTROL_REG_OFFSET) = 0; // make sure radio isn't in reset

    // tune the radio to default center freq of 30 MHz
    printf("Tuning Radio to 30MHz\n\r");
    radioTuner_tuneRadio(my_periph,tuner_freq);
    radioTuner_setAdcFreq(my_periph,source_freq);
    printf("Playing Tone at near 30MHz\r\n");

    // reset radio fifo axi interface
    radio_fifo[SRR] = 0xA5;

    // clear radio fifo interrupt register
    /* printf("Radio fifo interrupt register reads %x\n\r",radio_fifo[0]); */
    radio_fifo[ISR] = 0xFFFFFFFF; // write to clear reset bits
    /* printf("Radio fifo interrupt register reads %x\n\r",radio_fifo[0]); */

    // start sending data over udp
    udp_args.ptrToFifo = radio_fifo;
    udp_args.ipStr = argv[1];
    udp_args.portStr = argv[2];
    pthread_create(&thread_id,NULL,send_udp_data,&udp_args);

    // set parameter for song function
    song_args.ptrToRadio = my_periph;

    while(1) {

        // prompt for input
        printf("Enter a single character command: ");

        // get character from stdin (blocking)
        inChar = getchar();

        // flush the stdin buffer to disregard extra input chars
        // https://www.geeksforgeeks.org/clearing-the-input-buffer-in-cc/
        while ((getchar()) != '\n'); // got this from geeksforgeeks

        // check the received byte
        switch (inChar) {

            case '\0': // no input- do not change behavior
            case '\n': // no input- do not change behavior
                break;

            case 'e':
                ethernet_flag = 1 - ethernet_flag; // toggle ethernet
                break;

            case 'f': // set frequency of audio source
                printf("(SOURCE) Type up to 9 numbers, then press enter: ");
                for(i=0;i<15;i++)
                    inBuf[i] = '\n';
                for(i=0;i<9;i++) { // input up to 9 numbers
                    inBuf[i] = getchar();
                    if (inBuf[i] == '\n') {
                        break;
                    }
                }
                printf("\n\r");
                source_freq = atoi(inBuf); // ascii to integer
                break;

            case 't': // set frequency of tuner dds for ddc
                printf("(TUNER) Type up to 9 numbers, then press enter: ");
                for(i=0;i<15;i++)
                    inBuf[i] = '\n';
                for(i=0;i<9;i++) { // input up to 9 numbers
                    inBuf[i] = getchar();
                    if (inBuf[i] == '\n') {
                        break;
                    }
                }
                printf("\n\r");
                tuner_freq = atoi(inBuf); // ascii to integer
                break;

            case 's': // play song
                song_args.base_frequency = tuner_freq;
                play_tune(&song_args);
                radioTuner_setAdcFreq(my_periph,source_freq); // reset tone
                break;

            case 'b': // run benchmark
                print_benchmark(my_periph);
                break;

            case 'u': // inc 100 Hz
                source_freq += 100;
                break;

            case 'U': // inc 1 kHz
                source_freq += 1000;
                break;

            case 'd': // dec 100 Hz
                source_freq -= 100;
                break;

            case 'D': // dec 1 kHz
                source_freq -= 1000;
                break;

            default: // unrecognized input- print help message
                print_help_message();
                break;
        }

        // apply any changes to the tuner freq
        if (tuner_freq != old_tuner_freq) {
            radioTuner_tuneRadio(my_periph,tuner_freq);
            old_tuner_freq = tuner_freq;
        }

        // apply any changes to the source freq
        if (source_freq != old_source_freq) {

            radioTuner_setAdcFreq(my_periph,source_freq);
            old_source_freq = source_freq;
        }

    }
    return 0;
}
