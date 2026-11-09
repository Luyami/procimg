#ifndef FILTERSETTINGS_H
#define FILTERSETTINGS_H

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

enum FilterOption : uint32_t {
    Filter_Negative             = 0,
    Filter_Brightness           = 1,
    Filter_Threshold            = 2,
    Filter_GammaCorrection      = 3,
    Filter_PiecewiseLinear      = 4,
    Filter_Convolution          = 5,
    Filter_Scale                = 6,
    Filter_Rotate               = 7,
    Filter_Fourier              = 8,
    Filter_HistogramEqualization= 9,
    Filter_Grayscale            = 10,
    Filter_HSI_Adjust           = 11,
    Filter_ColorBalance         = 12,
    Filter_Sepia                = 13,
    Filter_ChromaKey            = 14,
};

enum GrayscaleMethod : uint32_t {
    Gray_Average   = 0,
    Gray_Weighted  = 1
};

struct FilterSettings {
    uint32_t option = Filter_Negative;

    int brightness = 0;
    float threshold = 0.0f; 
    float gamma = 0.0f;

    float piecewiseR1 = 0.0f;
    float piecewiseS1 = 0.0f;
    float piecewiseR2 = 0.0f;
    float piecewiseS2 = 0.0f;

    float scale_factor_x = 1.0f;
    float scale_factor_y = 1.0f;

    uint32_t interpolation = 0;

    float rotation_angle = 0.0f;
    float pivot_x = 0.0f;
    float pivot_y = 0.0f;
    float canvas_offset_x = 0.0f;
    float canvas_offset_y = 0.0f;

    int kernelSize = 3;

    uint32_t grayMethod = Gray_Weighted;

    float hueShift = 0.0f;
    float saturationScale = 1.0f;
    float intensityScale = 1.0f;

    float cyanRed = 0.0f;
    float magentaGreen = 0.0f;
    float yellowBlue = 0.0f;

    alignas(16) glm::vec3 keyColor = glm::vec3(0.0f, 1.0f, 0.0f);
    float chromaDistance = 0.3f;
    float chromaSmoothness = 0.1f;

    bool normalizeKernel = false;
    bool isRegionSelected = false;
    glm::vec2 tlCorner = glm::vec2(0.0f);
    glm::vec2 brCorner = glm::vec2(0.0f);
    std::vector<float> kernel; 
};

#endif