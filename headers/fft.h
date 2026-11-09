#ifndef FFT_H
#define FFT_H

#include <cmath>
#include <vector>

struct FftComplex {
    float re = 0.0f;
    float im = 0.0f;
};

inline FftComplex fftAdd(FftComplex a, FftComplex b) {
    return { a.re + b.re, a.im + b.im };
}

inline FftComplex fftSub(FftComplex a, FftComplex b) {
    return { a.re - b.re, a.im - b.im };
}

inline FftComplex fftMul(FftComplex a, FftComplex b) {
    return { a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re };
}

inline FftComplex fftScale(FftComplex a, float s) {
    return { a.re * s, a.im * s };
}

inline float fftMagnitude(FftComplex a) {
    return std::sqrt(a.re * a.re + a.im * a.im);
}

int fftNextPow2(int n);

void fft1d(FftComplex* data, int n, bool inverse);

void fft2d(std::vector<FftComplex>& data, int width, int height, bool inverse);

void fftShift2d(std::vector<FftComplex>& data, int width, int height);

#endif
