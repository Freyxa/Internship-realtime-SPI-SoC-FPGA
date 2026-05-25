#include "2d_fft_top.h"


// Strip AXI framing, emit raw complex samples into an HLS stream
static void load_input(hls::stream<dma_axis>  &in,
                       hls::stream<cmpxData>  &out)
{
    for (int i = 0; i < IMG_COLS * IMG_ROWS; i++) {
        #pragma HLS PIPELINE II=1
        dma_axis word = in.read();
        out.write(axi_to_cmpx(word.data));
    }
}

// Add AXI framing back; assert TLAST on the final word
static void store_output(hls::stream<cmpxData>  &in,
                         hls::stream<dma_axis>  &out)
{
    for (int i = 0; i < IMG_COLS * IMG_ROWS; i++) {
        #pragma HLS PIPELINE II=1
        dma_axis word;
        word.data = cmpx_to_axi(in.read());
        word.keep = (ap_uint<8>)0xFF;
        word.last = (i == (IMG_COLS * IMG_ROWS - 1)) ? 1 : 0;
        out.write(word);
    }
}

// Push N configuration tokens into the FFT config stream
static void push_cfg(hls::stream<fft_config_t> &cfg_s, bool direction, int n)
{
    for (int i = 0; i < n; i++) {
        #pragma HLS PIPELINE II=1
        fft_config_t cfg;
        cfg.setDir(direction);
        // FIX (WARN 2): use 0x00 (no scaling) for float-mode IP to avoid
        // saturating internal accumulators.  Change to 0xAB only if you have
        // measured that no overflow occurs on your data range.
        cfg.setSch(0x00);
        cfg_s.write(cfg);
    }
}

// Drain N status tokens; OR overflow flags into *ovflo
static void drain_sts(hls::stream<fft_status_t> &sts_s, bool &ovflo, int n)
{
    bool local = false;
    for (int i = 0; i < n; i++) {
        #pragma HLS PIPELINE II=1
        fft_status_t sts = sts_s.read();
        if (sts.getOvflo()) local = true;
    }
    ovflo = local;
}

static void pass1_write_transpose(hls::stream<cmpxData>           &fft_out,
                                  cmpxData transpose_buf[IMG_COLS][IMG_ROWS])
{
    for (int c = 0; c < IMG_COLS; c++) {
        for (int r = 0; r < IMG_ROWS; r++) {
            #pragma HLS PIPELINE II=1
            transpose_buf[c][r] = fft_out.read();
        }
    }
}



static void pass2_read_transpose(cmpxData                        transpose_buf[IMG_COLS][IMG_ROWS],
                                 hls::stream<cmpxData>           &fft_in)
{
    for (int r = 0; r < IMG_ROWS; r++) {
        for (int c = 0; c < IMG_COLS; c++) {
            #pragma HLS PIPELINE II=1
            fft_in.write(transpose_buf[c][r]);
        }
    }
}


static void run_pass1(hls::stream<dma_axis>           &in_stream,
                      cmpxData                        transpose_buf[IMG_COLS][IMG_ROWS],
                      bool                             direction,
                      bool                            &ovflo)
{
    #pragma HLS DATAFLOW

    hls::stream<cmpxData>    fft1_in ("fft1_in");
    hls::stream<cmpxData>    fft1_out("fft1_out");
    hls::stream<fft_config_t> cfg1_s ("cfg1_s");
    hls::stream<fft_status_t> sts1_s ("sts1_s");

    // FIX (WARN 1): depths must be ≥ FFT_LENGTH (128) so the pipelined-
    // streaming FFT IP never back-pressures mid-frame.
    #pragma HLS STREAM variable=fft1_in   depth=128
    #pragma HLS STREAM variable=fft1_out  depth=128
    #pragma HLS STREAM variable=cfg1_s    depth=128
    #pragma HLS STREAM variable=sts1_s    depth=128

    push_cfg  (cfg1_s, direction, IMG_COLS);
    load_input(in_stream, fft1_in);
    hls::fft<fft_param>(fft1_in, fft1_out, sts1_s, cfg1_s);
    pass1_write_transpose(fft1_out, transpose_buf);
    drain_sts (sts1_s, ovflo, IMG_COLS);
}


static void run_pass2(cmpxData                        transpose_buf[IMG_COLS][IMG_ROWS],
                      hls::stream<dma_axis>           &out_stream,
                      bool                             direction,
                      bool                            &ovflo)
{
    #pragma HLS DATAFLOW

    hls::stream<cmpxData>    fft2_in ("fft2_in");
    hls::stream<cmpxData>    fft2_out("fft2_out");
    hls::stream<fft_config_t> cfg2_s ("cfg2_s");
    hls::stream<fft_status_t> sts2_s ("sts2_s");

    #pragma HLS STREAM variable=fft2_in   depth=128
    #pragma HLS STREAM variable=fft2_out  depth=128
    #pragma HLS STREAM variable=cfg2_s    depth=128
    #pragma HLS STREAM variable=sts2_s    depth=128

    push_cfg             (cfg2_s, direction, IMG_ROWS);
    pass2_read_transpose (transpose_buf, fft2_in);
    hls::fft<fft_param>  (fft2_in, fft2_out, sts2_s, cfg2_s);
    store_output         (fft2_out, out_stream);
    drain_sts            (sts2_s, ovflo, IMG_ROWS);
}


void fft_2d_top(
    bool                    direction,
    hls::stream<dma_axis>  &in_stream,
    hls::stream<dma_axis>  &out_stream,
    bool                   &ovflo)
{
    #pragma HLS INTERFACE axis      port=in_stream
    #pragma HLS INTERFACE axis      port=out_stream
    #pragma HLS INTERFACE s_axilite port=direction  bundle=control
    #pragma HLS INTERFACE s_axilite port=ovflo      bundle=control
    #pragma HLS INTERFACE s_axilite port=return     bundle=control

    // Single transpose buffer — Pass 1 writes, Pass 2 reads.
    // Cyclic partition on dim=2 gives the URAM 4 independent read/write ports
    // so the inner pipeline loop can sustain II=1.
    static cmpxData transpose_buf[IMG_COLS][IMG_ROWS];
    #pragma HLS ARRAY_PARTITION variable=transpose_buf cyclic factor=4 dim=2
    #pragma HLS BIND_STORAGE    variable=transpose_buf type=RAM_T2P impl=URAM

    bool ovflo1 = false;
    bool ovflo2 = false;

    // Pass 1: column FFTs — fills transpose_buf
    run_pass1(in_stream, transpose_buf, direction, ovflo1);

    // Pass 2: row FFTs — reads transpose_buf (sequential, not DATAFLOW here,
    // because pass2 has a true data dependency on pass1's URAM writes)
    run_pass2(transpose_buf, out_stream, direction, ovflo2);

    ovflo = ovflo1 || ovflo2;
}
