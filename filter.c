#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _FILE_OFFSET_BITS 64
#define _DEFAULT_BUFFER_SIZE 1000 // in samples
#define ITEM_SIZE sizeof(int16_t)

void filter_file(FILE* input, size_t num_chan, size_t num_coeffs, float* a_coeffs, float* b_coeffs, size_t samples_per_buffer) {
    // allocate buffers
    size_t hist_size = num_chan * num_coeffs;

    size_t buffer_size = num_chan * samples_per_buffer;
    int16_t* buffer = calloc(buffer_size + hist_size, ITEM_SIZE);
    if (buffer == NULL) {
        printf("Error: calloc failed to allocate buffer.\n");
        exit(-1);
    }

    int16_t* input_ringbuf = calloc(hist_size, ITEM_SIZE);
    if (input_ringbuf == NULL) {
        printf("Error: calloc failed to allocate input_ringbuf.\n");
        exit(-1);
    }
    size_t irb_idx = 0;

    // determine the size of the file and set appropriate constants
    fseeko(input, 0, SEEK_END);
    const size_t byte_count = ftello(input);
    if (byte_count < 0) {
        printf("Error: cannot determine length of file.\n");
        exit(-1);
    }
    if (fseeko(input, 0, SEEK_SET) < 0) {
        printf("Error: failed to seek in input file.\n");
        exit(-1);
    }
    const size_t num_buffers = byte_count / (buffer_size * ITEM_SIZE);

    int buff_loc;
    double acc; // accumulator
    for (int buf_idx = 0; buf_idx < num_buffers + 1; buf_idx++) { // note the end condition
        // copy the end of the last buffer into the leftpad section of the
        // current buffer, to serve as the autoregressive inputs for the first
        // few samples of the following buffer
        memcpy(buffer, buffer + buffer_size, hist_size * ITEM_SIZE);

        // read another chunk into the buffer (sparing the leftpad section)
        size_t items_read = fread(buffer + hist_size, ITEM_SIZE, buffer_size, input);
        if ((items_read == 0) && (buf_idx < num_buffers)) {
            printf("Error: failed to read from input file.\n");
            exit(-1);
        }
        // the final buffer read might be partial
        size_t bytes_read = items_read * ITEM_SIZE;
        size_t samples_read = items_read / num_chan;

        for (int samp_idx = 0; samp_idx < samples_read; samp_idx++) {
            // copy the current sample vector into the front of the ring buffer
            memcpy(input_ringbuf + (irb_idx * num_chan), buffer + hist_size + (samp_idx * num_chan), num_chan * ITEM_SIZE);

            // filter computations
            for (int chan_idx = 0; chan_idx < num_chan; chan_idx++) {
                buff_loc = hist_size + samp_idx * num_chan + chan_idx;
                acc = b_coeffs[0] * input_ringbuf[irb_idx * num_chan + chan_idx];
                for (int coeff_idx = 1; coeff_idx < num_coeffs; coeff_idx++) {
                    // it's ((num_coeffs + irb_idx - coeff_idx) % num_coeffs) because in C, -1 % 5 == -1, not 4
                    acc += b_coeffs[coeff_idx] * input_ringbuf[((num_coeffs + irb_idx - coeff_idx) % num_coeffs) * num_chan + chan_idx];
                    acc -= a_coeffs[coeff_idx] * buffer[buff_loc - (coeff_idx * num_chan)];
                }
                acc = acc / a_coeffs[0];
                acc = (acc > INT16_MAX) ? INT16_MAX : acc;
                acc = (acc < INT16_MIN) ? INT16_MIN : acc;
                buffer[buff_loc] = acc;
            }
            // (circularly) update the index into the ring buffer
            irb_idx = (irb_idx + 1) % num_coeffs;
        }
        // rewind to the beginning of the chunk just operated on in the buffer
        if (fseeko(input, -bytes_read, SEEK_CUR) < 0) {
            printf("Error: failed to seek in input file.\n");
            exit(-1);
        }
        // write the buffer (not including its leftpad) back to the file
        if (fwrite(buffer + hist_size, ITEM_SIZE, items_read, input) != items_read) {
            printf("Error: failed to write to file.\n");
            exit(-1);
        }
    }
    return;
}

void print_usage(void) {
    printf("usage: quickfilt [-h] file channels -a a0 [a1 ...] -b b0 [b1 ...]\n\n");
    printf("Filter a file in-place using given coefficients.\n\n");
    printf("positional arguments:\n");
    printf("  file              name of file to filter\n");
    printf("  channels          number of channels in file (positive integer)\n");
    printf("  -a a0 [a1 ...]    AR filter coefficients (1 or more)\n");
    printf("  -b b0 [b1 ...]    FIR filter coefficients (1 or more)\n\n");
    printf("optional arguments:\n");
    printf("  -h, --help        show this help message and exit\n\n");
    printf("The number of AR and FIR filter coefficients must be the same.\n");
    return;
}

int main(int argc, char* argv[]) {
    if ((argc > 1) && (strncmp(argv[1], "-h", 2) == 0 || strncmp(argv[1], "--h", 3) == 0)) {
        print_usage();
        return 0;
    }

    if (argc < 6) {
        print_usage();
        exit(-1);
    }

    char* file = argv[1];

    unsigned int num_channels = strtoumax(argv[2], NULL, 10);
    if (num_channels == 0) {
        print_usage();
        printf("num_channels must be an integer > 0.\n");
        exit(-1);
    }

    int num_a = 0;
    int in_arg;
    if (strncmp(argv[3], "-a", 2) == 0) {
        in_arg = 4;
        while (in_arg < argc) {
            if (strncmp(argv[in_arg], "-b", 2) == 0) {
                break;
            }
            num_a++;
            in_arg++;
        }
    }
    else {
        print_usage();
        exit(-1);
    }
    float* a_coeffs = malloc(num_a * sizeof(float));
    if (a_coeffs == NULL) {
        printf("Error: malloc failed to create a_coeffs.\n");
        exit(-1);
    }
    for (int i = 0; i < num_a; i++) {
        a_coeffs[i] = strtof(argv[4 + i], NULL);
    }

    int num_b = 0;
    if (strncmp(argv[4 + num_a], "-b", 2) == 0) {
        in_arg = 5 + num_a;
        while (in_arg < argc) {
            num_b++;
            in_arg++;
        }
    }
    else {
        print_usage();
        exit(-1);
    }
    float* b_coeffs = malloc(num_b * sizeof(float));
    if (b_coeffs == NULL) {
        printf("Error: malloc failed to create b_coeffs.\n");
        exit(-1);
    }
    for (int i = 0; i < num_b; i++) {
        b_coeffs[i] = strtof(argv[5 + num_b + i], NULL);
    }

    if (num_a < num_b) {
        print_usage();
        printf("Error: number of 'a' and 'b' coefficients must be the same.\n");
        exit(-1);
    }

    printf("-a (AR) coefficients:\n");
    for (int i = 0; i < num_a; i++) {
        printf("  a_%d: %f\n", i, a_coeffs[i]);
    }
    printf("-b (FIR) coefficients:\n");
    for (int i = 0; i < num_b; i++) {
        printf("  b_%d: %f\n", i, b_coeffs[i]);
    }

    FILE* input = fopen(file, "r+");
    if (input == 0) {
        printf("Error: could not open file %s\n", file);
        exit(-1);
    }

    off_t buffer_size = _DEFAULT_BUFFER_SIZE;

    filter_file(input, num_channels, num_a, a_coeffs, b_coeffs, buffer_size);
    fflush(input);
    fclose(input);

    return 0;
}

