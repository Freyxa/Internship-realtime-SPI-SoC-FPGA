#ifndef FFT_TOP1_H
#define FFT_TOP1_H

#include <hls_fft.h>
#include <hls_stream.h>
#include <complex>
#include <ap_fixed.h>
#include <ap_int.h>

#define FFT_LENGTH 64
#define NFFT_MAX   6

typedef ap_fixed<32, 1> data_t;
typedef std::complex<data_t> cmpxData;


typedef ap_uint<64> axi_word_t;


inline axi_word_t cmpx_to_axi(cmpxData x) {
    ap_uint<32> re = x.real().range(31, 0);
    ap_uint<32> im = x.imag().range(31, 0);
    axi_word_t w;
    w.range(31,  0) = re;
    w.range(63, 32) = im;
    return w;
}


inline cmpxData axi_to_cmpx(axi_word_t w) {
    data_t re, im;
    re.range(31, 0) = w.range(31,  0);
    im.range(31, 0) = w.range(63, 32);
    return cmpxData(re, im);
}

struct fft_param : hls::ip_fft::params_t {
    static const unsigned ordering_opt = hls::ip_fft::natural_order;
    static const unsigned max_nfft     = NFFT_MAX;
    static const unsigned input_width  = 32;
    static const unsigned output_width = 32;
    static const bool     has_nfft     = false;
    static const unsigned config_width = 8;
    static const unsigned scaling_opt  = hls::ip_fft::scaled;
    static const unsigned ovflo        = 1;
};

typedef hls::ip_fft::config_t<fft_param> config_t;
typedef hls::ip_fft::status_t<fft_param> status_t;


void fft_top1(bool direction,
             axi_word_t in[FFT_LENGTH],
             axi_word_t out[FFT_LENGTH],
             bool &ovflo);

#endif
