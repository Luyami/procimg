#ifndef OPERATIONREGISTRY_H
#define OPERATIONREGISTRY_H

#include <string>
#include <filterSettings.h>
#include <chrono>
#include <ctime>
#include <iomanip>

using OperationDate = std::time_t;

struct OperationRegistry{
    std::string name;
    OperationDate date;
    FilterSettings settings;

    GLuint image;
    int width;
    int height;
    int workX;
    int workY;
    int workW;
    int workH;

    int drawX = 0;
    int drawY = 0;
    int drawW = 0;
    int drawH = 0;

    static OperationRegistry create(
        std::string name,
        GLuint image,
        int width,
        int height,
        int workX,
        int workY,
        int workW,
        int workH,
        FilterSettings settings
    );
};

#include <operationRegistry.inl>

#endif