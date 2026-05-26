#include <cstdio>
#include <cmath>
#include "top_256.h"

static const float TOL_UNIT  = 1e-3f;
static const float TOL_DC    = 1.0f;        // float FFT: DC bin = exactly N, tight tol
static const float TOL_ROUND = 1e-3f;

static float cmpx_mag(cmpxData v) {
    return sqrtf(v.real() * v.real() + v.imag() * v.imag());
}

static float cmpx_err(cmpxData a, cmpxData b) {
    float re = a.real() - b.real();
    float im = a.imag() - b.imag();
    return sqrtf(re * re + im * im);
}

static void push(hls::stream<dma_axis> &s, cmpxData v, bool last) {
    dma_axis w;
    w.data = cmpx_to_axi(v);
    w.keep = (ap_uint<8>)0xFF;   // 64-bit bus → 8 byte lanes
    w.last = last ? 1 : 0;
    s.write(w);
}

static cmpxData pop(hls::stream<dma_axis> &s) {
    return axi_to_cmpx(s.read().data);
}

static void fill_stream(hls::stream<dma_axis> &s,
                        cmpxData data[IMG_ROWS][IMG_COLS])
{
    int idx = 0;
    const int N = IMG_ROWS * IMG_COLS;
    for (int r = 0; r < IMG_ROWS; r++)
        for (int c = 0; c < IMG_COLS; c++)
            push(s, data[r][c], ++idx == N);
}

static void drain_stream(hls::stream<dma_axis> &s,
                         cmpxData data[IMG_ROWS][IMG_COLS])
{
    for (int r = 0; r < IMG_ROWS; r++)
        for (int c = 0; c < IMG_COLS; c++)
            data[r][c] = pop(s);
}

int main() {
    hls::stream<dma_axis> in_s ("in_s");
    hls::stream<dma_axis> out_s("out_s");
    bool ovflo     = false;
    const int N    = IMG_ROWS * IMG_COLS;   // 65536
    int pass_count = 0;
    int fail_count = 0;

    cmpxData input_buf [IMG_ROWS][IMG_COLS];
    cmpxData output_buf[IMG_ROWS][IMG_COLS];

    // ── Test 1: Impulse at (0,0) → all bins magnitude = 1.0 ──────────
    printf("=== Test 1: Impulse at (0,0), forward FFT ===\n");
    {
        for (int r = 0; r < IMG_ROWS; r++)
            for (int c = 0; c < IMG_COLS; c++)
                input_buf[r][c] = (r == 0 && c == 0)
                    ? cmpxData(1.0f, 0.0f)
                    : cmpxData(0.0f, 0.0f);

        fill_stream(in_s, input_buf);
        fft_2d_top_256(true, in_s, out_s, ovflo);
        drain_stream(out_s, output_buf);

        float max_err = 0.0f;
        for (int r = 0; r < IMG_ROWS; r++)
            for (int c = 0; c < IMG_COLS; c++) {
                float err = fabsf(cmpx_mag(output_buf[r][c]) - 1.0f);
                if (err > max_err) max_err = err;
            }
        bool pass = (max_err < TOL_UNIT);
        printf("  Max |magnitude - 1.0| = %.6f  (tol %.4f) --> %s\n",
               max_err, TOL_UNIT, pass ? "PASS" : "FAIL");
        if (pass) pass_count++; else fail_count++;
    }

    // ── Test 2: All-ones DC → bin(0,0) = N, rest = 0 ─────────────────
    printf("=== Test 2: All-ones DC signal ===\n");
    {
        for (int r = 0; r < IMG_ROWS; r++)
            for (int c = 0; c < IMG_COLS; c++)
                input_buf[r][c] = cmpxData(1.0f, 0.0f);

        fill_stream(in_s, input_buf);
        fft_2d_top_256(true, in_s, out_s, ovflo);
        drain_stream(out_s, output_buf);

        // float FFT: DC bin should be exactly N=65536, off-bins = 0
        float dc_mag  = cmpx_mag(output_buf[0][0]);
        float max_off = 0.0f;
        for (int r = 0; r < IMG_ROWS; r++)
            for (int c = 0; c < IMG_COLS; c++) {
                if (r == 0 && c == 0) continue;
                float m = cmpx_mag(output_buf[r][c]);
                if (m > max_off) max_off = m;
            }
        bool pass = (fabsf(dc_mag - (float)N) < TOL_DC) && (max_off < TOL_DC);
        printf("  DC bin = %.1f (expected %d) | max off-bin = %.6f"
               "  (tol %.1f) --> %s\n",
               dc_mag, N, max_off, TOL_DC, pass ? "PASS" : "FAIL");
        if (pass) pass_count++; else fail_count++;
    }

    // ── Test 3: Forward + Inverse round-trip ──────────────────────────
    printf("=== Test 3: Forward + Inverse Round-Trip ===\n");
    {
        for (int r = 0; r < IMG_ROWS; r++)
            for (int c = 0; c < IMG_COLS; c++)
                input_buf[r][c] = cmpxData(
                    cosf(2.0f * (float)M_PI * r / IMG_ROWS),
                    sinf(2.0f * (float)M_PI * c / IMG_COLS));

        // Forward pass
        fill_stream(in_s, input_buf);
        fft_2d_top_256(true, in_s, out_s, ovflo);
        drain_stream(out_s, output_buf);

        // Inverse pass
        fill_stream(in_s, output_buf);
        fft_2d_top_256(false, in_s, out_s, ovflo);
        drain_stream(out_s, output_buf);

        // Normalize by N and compare to original
        float max_err = 0.0f;
        for (int r = 0; r < IMG_ROWS; r++)
            for (int c = 0; c < IMG_COLS; c++) {
                cmpxData normalized(output_buf[r][c].real() / (float)N,
                                    output_buf[r][c].imag() / (float)N);
                float err = cmpx_err(normalized, input_buf[r][c]);
                if (err > max_err) max_err = err;
            }
        bool pass = (max_err < TOL_ROUND);
        printf("  Max normalized round-trip error = %.6f  (tol %.4f) --> %s\n",
               max_err, TOL_ROUND, pass ? "PASS" : "FAIL");
        if (pass) pass_count++; else fail_count++;
    }

    printf("\nOverflow flag : %s\n", ovflo ? "SET" : "clear");
    printf("Result        : %d/3 passed\n", pass_count);
    return (fail_count == 0) ? 0 : 1;
}
