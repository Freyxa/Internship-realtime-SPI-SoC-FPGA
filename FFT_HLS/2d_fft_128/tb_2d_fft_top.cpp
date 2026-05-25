#include <cstdio>
#include <cmath>
#include "2d_fft_top.h"

static void push(hls::stream<dma_axis> &s, cmpxData v, bool last) {
    dma_axis w;
    w.data = cmpx_to_axi(v);
    w.keep = (ap_uint<8>)0xFF;
    w.last = last ? 1 : 0;
    s.write(w);
}

static cmpxData pop(hls::stream<dma_axis> &s) {
    return axi_to_cmpx(s.read().data);
}

int main() {
    hls::stream<dma_axis> in_s("in_s");
    hls::stream<dma_axis> out_s("out_s");
    bool ovflo = false;
    const int N = IMG_ROWS * IMG_COLS;
    int pass_count = 0;
    int fail_count = 0;

    printf("=== Test 1: Impulse at (0,0), forward FFT ===\n");
    int idx = 0;
    for (int c = 0; c < IMG_COLS; c++) {
        for (int r = 0; r < IMG_ROWS; r++) {
            idx++;
            cmpxData val = (r == 0 && c == 0) ? cmpxData(1.0f, 0.0f) : cmpxData(0.0f, 0.0f);
            push(in_s, val, idx == N);
        }
    }

    fft_2d_top(true, in_s, out_s, ovflo);

    float max_err_t1 = 0.0f;
    for (int i = 0; i < N; i++) {
        float mag = std::abs(pop(out_s));
        float err = std::fabs(mag - 1.0f);
        if (err > max_err_t1) max_err_t1 = err;
    }
    bool t1 = (max_err_t1 < 1e-3f);
    printf("  Max |magnitude - 1.0| = %.6f  --> %s\n", max_err_t1, t1 ? "PASS" : "FAIL");
    if (t1) pass_count++; else fail_count++;

    printf("=== Test 2: All-ones DC signal ===\n");
    idx = 0;
    for (int c = 0; c < IMG_COLS; c++) {
        for (int r = 0; r < IMG_ROWS; r++) {
            idx++;
            push(in_s, cmpxData(1.0f, 0.0f), idx == N);
        }
    }

    fft_2d_top(true, in_s, out_s, ovflo);

    float max_err_t2 = 0.0f;
    for (int i = 0; i < N; i++) {
        cmpxData v = pop(out_s);
        float expected_mag = (i == 0) ? (float)N : 0.0f;
        float err = std::fabs(std::abs(v) - expected_mag);
        if (err > max_err_t2) max_err_t2 = err;
    }
    bool t2 = (max_err_t2 < 1.0f);
    printf("  Max magnitude error = %.3f  --> %s\n", max_err_t2, t2 ? "PASS" : "FAIL");
    if (t2) pass_count++; else fail_count++;

    printf("=== Test 3: Forward + Inverse Round-Trip ===\n");
    cmpxData ref[IMG_ROWS][IMG_COLS];
    for (int r = 0; r < IMG_ROWS; r++) {
        for (int c = 0; c < IMG_COLS; c++) {
            ref[r][c] = cmpxData(
                cosf(2.0f * (float)M_PI * r / IMG_ROWS),
                sinf(2.0f * (float)M_PI * c / IMG_COLS));
        }
    }

    // Forward pass
    idx = 0;
    for (int c = 0; c < IMG_COLS; c++) {
        for (int r = 0; r < IMG_ROWS; r++) {
            idx++;
            push(in_s, ref[r][c], idx == N);
        }
    }
    fft_2d_top(true, in_s, out_s, ovflo);

    // Collect forward output
    // Note: Since our pass 2 loops r then c, the output streams out in row-major order
    cmpxData fwd_out[IMG_ROWS][IMG_COLS];
    for (int r = 0; r < IMG_ROWS; r++) {
        for (int c = 0; c < IMG_COLS; c++) {
            fwd_out[r][c] = pop(out_s);
        }
    }

    // Inverse pass - To run it through the same core properly, we must send it back
    // column-major relative to its current orientation to perform the 2D inverse
    idx = 0;
    for (int c = 0; c < IMG_COLS; c++) {
        for (int r = 0; r < IMG_ROWS; r++) {
            idx++;
            // We feed fwd_out transposed back into the system
            push(in_s, fwd_out[r][c], idx == N);
        }
    }
    fft_2d_top(false, in_s, out_s, ovflo);

    // Compare output against original
    float max_err_t3 = 0.0f;
    for (int r = 0; r < IMG_ROWS; r++) {
        for (int c = 0; c < IMG_COLS; c++) {
            cmpxData got = pop(out_s);
            float err = std::abs(got - ref[r][c] * (float)N) / (float)N;
            if (err > max_err_t3) max_err_t3 = err;
        }
    }
    bool t3 = (max_err_t3 < 1e-3f);
    printf("  Max normalized round-trip error = %.6f  --> %s\n", max_err_t3, t3 ? "PASS" : "FAIL");
    if (t3) pass_count++; else fail_count++;

    printf("\nOverflow flag: %s\n", ovflo ? "SET (check scaling)" : "clear");
    printf("Result: %d/3 passed\n", pass_count);

    return (fail_count == 0) ? 0 : 1;
}
