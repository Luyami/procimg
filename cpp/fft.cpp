#include <fft.h>
#include <algorithm>
#include <cmath>

#ifndef PI
#define PI 3.14159265358979323846
#endif

int fftNextPow2(int n) {
    if (n <= 1) return 1;
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

void fft1d(FftComplex* data, int n, bool inverse) {
    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(data[i], data[j]);
    }

    for (int len = 2; len <= n; len <<= 1) {
        const float ang = (float)(2.0 * PI / (double)len) * (inverse ? 1.0f : -1.0f);
        const FftComplex wlen = { std::cos(ang), std::sin(ang) };
        for (int i = 0; i < n; i += len) {
            FftComplex w = { 1.0f, 0.0f };
            const int half = len / 2;
            for (int k = 0; k < half; ++k) {
                const FftComplex u = data[i + k];
                const FftComplex v = fftMul(data[i + k + half], w);
                data[i + k] = fftAdd(u, v);
                data[i + k + half] = fftSub(u, v);
                w = fftMul(w, wlen);
            }
        }
    }

    if (inverse) {
        const float invN = 1.0f / (float)n;
        for (int i = 0; i < n; ++i)
            data[i] = fftScale(data[i], invN);
    }
}

void fft2d(std::vector<FftComplex>& data, int width, int height, bool inverse) {
    for (int y = 0; y < height; ++y)
        fft1d(&data[(size_t)y * (size_t)width], width, inverse);

    std::vector<FftComplex> column((size_t)height);
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y)
            column[y] = data[(size_t)y * (size_t)width + (size_t)x];
        fft1d(column.data(), height, inverse);
        for (int y = 0; y < height; ++y)
            data[(size_t)y * (size_t)width + (size_t)x] = column[y];
    }
}

void fftShift2d(std::vector<FftComplex>& data, int width, int height) {
    const int halfW = width / 2;
    const int halfH = height / 2;
    std::vector<FftComplex> shifted(data.size());

    for (int y = 0; y < height; ++y) {
        const int sy = (y + halfH) % height;
        for (int x = 0; x < width; ++x) {
            const int sx = (x + halfW) % width;
            shifted[(size_t)sy * (size_t)width + (size_t)sx] =
                data[(size_t)y * (size_t)width + (size_t)x];
        }
    }

    data.swap(shifted);
}
