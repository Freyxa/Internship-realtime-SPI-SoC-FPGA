#include <iostream>
#include <iomanip>
#include <cmath>
#include "fft_top1.h"

int main() {
    axi_word_t test_input[FFT_LENGTH];   
    axi_word_t test_output[FFT_LENGTH];  
    bool direction = 1;
    bool overflow = false;

    std::cout << "--- Starting FFT Testbench ---" << std::endl;


    for (int i = 0; i < FFT_LENGTH; i++) {
        float real_val = 0.4f * sinf(2.0f * M_PI * i / FFT_LENGTH);
        float imag_val = 0.0f;

        
        data_t re_fixed = real_val;
        data_t im_fixed = imag_val;
        cmpxData sample(re_fixed, im_fixed);
        test_input[i] = cmpx_to_axi(sample);  
    }


    std::cout << "Input Signal (Time Domain):" << std::endl;
    for (int i = 0; i < FFT_LENGTH; i++) {
        cmpxData s = axi_to_cmpx(test_input[i]);
        std::cout << "  [" << i << "]: "
                  << s.real() << " + "
                  << s.imag() << "j" << std::endl;
    }

 
    fft_top1(direction, test_input, test_output, overflow);

    
    std::cout << "\nOutput Signal (Frequency Domain):" << std::endl;
    if (overflow) {
        std::cout << "WARNING: Overflow detected!" << std::endl;
    }

    std::cout << std::fixed << std::setprecision(6);

    float max_mag = 0.0f;
    int   max_bin = 0;

    for (int i = 0; i < FFT_LENGTH; i++) {
        cmpxData s = axi_to_cmpx(test_output[i]);  
        float re  = (float)s.real();
        float im  = (float)s.imag();
        float mag = sqrtf(re*re + im*im);

        std::cout << "  Bin [" << std::setw(2) << i << "]: "
                  << std::setw(10) << re << " + "
                  << std::setw(10) << im << "j"
                  << "  mag=" << mag << std::endl;

        if (mag > max_mag) {
            max_mag = mag;
            max_bin = i;
        }
    }


    std::cout << "\n--- Peak at bin [" << max_bin
              << "] magnitude = " << max_mag << " ---" << std::endl;

   
    if (max_bin == 1 || max_bin == 63) {
        std::cout << "PASS: Peak at expected bin." << std::endl;
    } else {
        std::cout << "FAIL: Peak at unexpected bin " << max_bin << std::endl;
    }

    std::cout << "--- Testbench Finished ---" << std::endl;
    return 0;
}
