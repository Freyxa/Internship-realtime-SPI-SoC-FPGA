#include "fft_top1.h"

void proc_fe(
    bool direction,
    hls::stream<config_t> &config_s,
    axi_word_t in[FFT_LENGTH],        // 64-bit AXI type
    hls::stream<cmpxData> &out_s)
{
    config_t config;
    config.setDir(direction);
    config.setSch(0x2A);
    config_s.write(config);

    // Single 64-bit read per iteration â†’ II=1
    for (int i = 0; i < FFT_LENGTH; i++) {
        #pragma HLS PIPELINE II=1
        out_s.write(axi_to_cmpx(in[i]));
    }
}

void proc_be(
    hls::stream<status_t> &status_in_s,
    bool &ovflo,
    hls::stream<cmpxData> &in_s,
    axi_word_t out[FFT_LENGTH])       // 64-bit AXI type
{
    // Single 64-bit write per iteration â†’ II=1
    for (int i = 0; i < FFT_LENGTH; i++) {
        #pragma HLS PIPELINE II=1
        out[i] = cmpx_to_axi(in_s.read());
    }

    status_t status_in = status_in_s.read();
    ovflo = status_in.getOvflo() & 0x1;
}

void fft_top1(
    bool direction,
    axi_word_t in[FFT_LENGTH],
    axi_word_t out[FFT_LENGTH],
    bool &ovflo)
{
	#pragma HLS INTERFACE m_axi     port=in    offset=slave bundle=gmem_in  max_read_burst_length=64  latency=64
    #pragma HLS INTERFACE m_axi     port=out   offset=slave bundle=gmem_out max_write_burst_length=64 latency=64
    #pragma HLS INTERFACE s_axilite port=in    bundle=control
    #pragma HLS INTERFACE s_axilite port=out   bundle=control
    #pragma HLS INTERFACE s_axilite port=direction bundle=control
    #pragma HLS INTERFACE s_axilite port=ovflo     bundle=control
    #pragma HLS INTERFACE s_axilite port=return    bundle=control

    hls::stream<cmpxData>  xn("xn");
    hls::stream<cmpxData>  xk("xk");
    hls::stream<config_t>  fft_config("fft_config");
    hls::stream<status_t>  fft_status("fft_status");

    #pragma HLS STREAM variable=xn         depth=64
    #pragma HLS STREAM variable=xk         depth=64
    #pragma HLS STREAM variable=fft_config depth=2
    #pragma HLS STREAM variable=fft_status depth=2

    #pragma HLS DATAFLOW

    proc_fe(direction, fft_config, in, xn);
    hls::fft<fft_param>(xn, xk, fft_status, fft_config);
    proc_be(fft_status, ovflo, xk, out);
}
