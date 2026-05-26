#ifndef TOP_256_H_
#define TOP_256_H_

#include <complex>
#include "hls_fft.h"
#include "hls_stream.h"
#include "ap_int.h"
#include "ap_axi_sdata.h"

// Dimensions
#define IMG_ROWS   256
#define IMG_COLS   256
#define FFT_LENGTH 256

// Data types — float32 real + float32 imag = 64 bits, same as 128 version
typedef std::complex<float>  cmpxData;
typedef ap_axiu<64, 0, 0, 0> dma_axis;   // 64-bit: 32 real + 32 imag

// FFT IP configuration
struct fft_param : hls::ip_fft::params_t {
    static const unsigned input_width        = 32;
    static const unsigned output_width       = 32;
    static const unsigned status_width       = 8;
    static const unsigned config_width       = 16;
    static const unsigned max_nfft           = 8;   // 2^8 = 256 points
    static const unsigned architecture       = hls::ip_fft::pipelined_streaming_io;
    static const unsigned phase_factor_width = 24;
    static const bool     has_nfft           = false;
};

typedef hls::ip_fft::config_t<fft_param> fft_config_t;
typedef hls::ip_fft::status_t<fft_param> fft_status_t;

// Pack / unpack helpers — identical to 128 version
inline cmpxData axi_to_cmpx(ap_uint<64> val) {
    union { unsigned u; float f; } re, im;
    re.u = val.range(31,  0).to_uint();
    im.u = val.range(63, 32).to_uint();
    return cmpxData(re.f, im.f);
}

inline ap_uint<64> cmpx_to_axi(cmpxData val) {
    union { unsigned u; float f; } re, im;
    re.f = val.real();
    im.f = val.imag();
    ap_uint<64> out;
    out.range(31,  0) = re.u;
    out.range(63, 32) = im.u;
    return out;
}

void fft_2d_top_256(
    bool                    direction,
    hls::stream<dma_axis>  &in_stream,
    hls::stream<dma_axis>  &out_stream,
    bool                   &ovflo
);

#endif
