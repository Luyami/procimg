#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <engine.h>
#include <fft.h>
#include <stb_image_write.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>

using namespace glm;

//private
bool Engine::isRunning = false;
GLFWwindow* Engine::window = nullptr;
GLuint Engine::baseImage = 0;
GLuint Engine::prevImage = 0;
GLuint Engine::currImage = 0;
GLuint Engine::chromaBgImage = 0;
int Engine::imageWidth = 0;
int Engine::imageHeight = 0;
int Engine::workX = 0;
int Engine::workY = 0;
int Engine::workW = 0;
int Engine::workH = 0;
int Engine::drawX = 0;
int Engine::drawY = 0;
int Engine::drawW = 0;
int Engine::drawH = 0;
glm::vec2 Engine::lastCanvasOffset = glm::vec2(0.0f);
int Engine::window_width = 0;
int Engine::window_height = 0;
float Engine::aspectRatio = 0.0f;
std::vector<OperationRegistry> Engine::operationsLog = std::vector<OperationRegistry>();
GLuint Engine::shaderProgram = 0;
GLuint Engine::modelLoc = 0;
GLuint Engine::viewLoc = 0;
GLuint Engine::projLoc = 0;
GLuint Engine::colorLoc = 0;
GLuint Engine::pointSizeLoc = 0;
GLuint Engine::kernelSSBO = 0;
GLuint Engine::settingsUBO = 0;

void Engine::__initGLFW(){
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(Engine::window_width,Engine::window_height,"Lumage",NULL,NULL);
    glfwMakeContextCurrent(window);

    Engine::window = window;
}

void Engine::__initGLEW(){
    glewExperimental = GL_TRUE;
    glewInit();
}

void Engine::__initImGUI(){
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}

std::string readFile(const char* path){
    std::ifstream file(path);

    if (!file){
        printf("Failed to open shader: %s\n", path);
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

GLuint Engine::__createComputeShader(const char* path){
    std::string source = readFile(path);

    if (source.empty())
        return 0;

    const char* sourcePtr = source.c_str();

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);

    glShaderSource(
        shader,
        1,
        &sourcePtr,
        nullptr
    );

    glCompileShader(shader);

    GLint success;

    glGetShaderiv(
        shader,
        GL_COMPILE_STATUS,
        &success
    );

    if (!success){
        char log[1024];

        glGetShaderInfoLog(
            shader,
            1024,
            nullptr,
            log
        );

        printf("Compute shader compilation failed:\n%s\n", log);

        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

GLuint Engine::__createComputeProgram(const char* path){
    GLuint shader = __createComputeShader(path);

    if (shader == 0)
        return 0;

    GLuint program = glCreateProgram();

    glAttachShader(program, shader);
    glLinkProgram(program);

    GLint success;

    glGetProgramiv(
        program,
        GL_LINK_STATUS,
        &success
    );

    if (!success){
        char log[1024];

        glGetProgramInfoLog(
            program,
            1024,
            nullptr,
            log
        );

        printf("Compute program linking failed:\n%s\n", log);

        glDeleteProgram(program);
        glDeleteShader(shader);

        return 0;
    }

    glDeleteShader(shader);

    return program;
}

void Engine::addOperationToLog(FilterSettings op){
    if (Engine::baseImage == 0) return;

    //save image state
    GLuint newImage;

    glGenTextures(1, &newImage);

    glBindTexture(GL_TEXTURE_2D, newImage);
    glTexStorage2D(
        GL_TEXTURE_2D,
        1,
        GL_RGBA8,
        Engine::getImageWidth(),
        Engine::getImageHeight()
    );

    glCopyImageSubData(
        Engine::getCurrImage(), GL_TEXTURE_2D,
        0, 0, 0, 0,

        newImage, GL_TEXTURE_2D,
        0, 0, 0, 0,

        Engine::getImageWidth(),
        Engine::getImageHeight(),
        1
    );
    
    std::string opName;
    switch (op.option){
        case FilterOption::Filter_Negative:         opName = "Negative";        break;
        case FilterOption::Filter_Brightness:       opName = "Brightness";      break;
        case FilterOption::Filter_Threshold:        opName = "Threshold";       break;
        case FilterOption::Filter_GammaCorrection:  opName = "GammaCorrection"; break;
        case FilterOption::Filter_PiecewiseLinear:  opName = "PiecewiseLinear"; break;
        case FilterOption::Filter_Convolution:      opName = "Convolution"; break;
        case FilterOption::Filter_Scale:            opName = "Resize";      break;
        case FilterOption::Filter_Rotate:           opName = "Rotate";      break;
        case FilterOption::Filter_Fourier:          opName = "Fourier";     break;
        case 15:                                    opName = "Steganography"; break;
        default: opName = "??"; break;
    }

    OperationRegistry reg = OperationRegistry::create(
        opName,
        newImage,
        Engine::getImageWidth(),
        Engine::getImageHeight(),
        Engine::workX,
        Engine::workY,
        Engine::workW,
        Engine::workH,
        op
    );
    reg.drawX = Engine::drawX;
    reg.drawY = Engine::drawY;
    reg.drawW = Engine::drawW;
    reg.drawH = Engine::drawH;

    Engine::operationsLog.push_back(reg);
}
OperationRegistry Engine::peekOperationsLog(){
    if (Engine::operationsLog.size() == 0) return OperationRegistry();

    return Engine::operationsLog[Engine::operationsLog.size() - 1];
}
OperationRegistry Engine::popOperationsLog(){
    if (Engine::operationsLog.size() == 0) return OperationRegistry();

    OperationRegistry reg = Engine::operationsLog[Engine::operationsLog.size() - 1];
    Engine::operationsLog.pop_back();

    return reg;
}
void Engine::resetOperationsLog(){
    for (int i = 0; i < Engine::operationsLog.size(); ++i){
        glDeleteTextures(1, &Engine::operationsLog[i].image);
    }

    Engine::operationsLog.clear();
}
void Engine::revertOperation(){
    if (Engine::operationsLog.size() == 0) return;
    Engine::invalidateFourierSpectrum();

    OperationRegistry op = Engine::popOperationsLog();

    //new images to update dimensions... etc
    GLuint newCurr;
    GLuint newPrev;

    glGenTextures(1, &newCurr);
    glBindTexture(GL_TEXTURE_2D, newCurr);

    glTexStorage2D(
        GL_TEXTURE_2D,
        1,
        GL_RGBA8,
        op.width,
        op.height
    );

    glDeleteTextures(1, &Engine::currImage);
    Engine::currImage = newCurr;

    glGenTextures(1, &newPrev);
    glBindTexture(GL_TEXTURE_2D, newPrev);

    glTexStorage2D(
        GL_TEXTURE_2D,
        1,
        GL_RGBA8,
        op.width,
        op.height
    );

    glDeleteTextures(1, &Engine::prevImage);
    Engine::prevImage = newPrev;

    //copying from previous image
    glCopyImageSubData(
        op.image, GL_TEXTURE_2D,
        0, 0, 0, 0,

        Engine::currImage, GL_TEXTURE_2D,
        0, 0, 0, 0,

        op.width,
        op.height,
        1
    );

    glCopyImageSubData(
        op.image, GL_TEXTURE_2D,
        0, 0, 0, 0,

        Engine::prevImage, GL_TEXTURE_2D,
        0, 0, 0, 0,

        op.width,
        op.height,
        1
    );

    Engine::imageWidth  = op.width;
    Engine::imageHeight = op.height;
    Engine::workX = op.workX;
    Engine::workY = op.workY;
    Engine::workW = op.workW;
    Engine::workH = op.workH;
    Engine::drawX = op.drawX;
    Engine::drawY = op.drawY;
    Engine::drawW = op.drawW;
    Engine::drawH = op.drawH;

    glDeleteTextures(1, &op.image);
}

GLuint Engine::getBaseImage() {return Engine::baseImage;}
GLuint Engine::getPrevImage() {return Engine::prevImage;}
GLuint Engine::getCurrImage() {return Engine::currImage;}

int Engine::getImageWidth()  {return Engine::imageWidth;}
int Engine::getImageHeight() {return Engine::imageHeight;}
int Engine::getWorkX() {return Engine::workX;}
int Engine::getWorkY() {return Engine::workY;}
int Engine::getWorkWidth() {return Engine::workW;}
int Engine::getWorkHeight() {return Engine::workH;}
int Engine::getDrawX() {return Engine::drawX;}
int Engine::getDrawY() {return Engine::drawY;}
int Engine::getDrawWidth() {return Engine::drawW;}
int Engine::getDrawHeight() {return Engine::drawH;}
glm::vec2 Engine::getLastCanvasOffset() {return Engine::lastCanvasOffset;}

namespace {

void configureEmptyRGBA8Texture(GLuint texture, int width, int height) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
}

GLuint createEmptyRGBA8Texture(int width, int height) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    configureEmptyRGBA8Texture(texture, width, height);
    return texture;
}

void rotatePoint(
    float x,
    float y,
    float pivotX,
    float pivotY,
    float c,
    float s,
    float& outX,
    float& outY
) {
    const float dx = x - pivotX;
    const float dy = y - pivotY;
    outX = c * dx - s * dy + pivotX;
    outY = s * dx + c * dy + pivotY;
}

void computeRotatedCanvas(
    int srcW,
    int srcH,
    float pivotX,
    float pivotY,
    float angleDeg,
    int workX,
    int workY,
    int workW,
    int workH,
    int drawX,
    int drawY,
    int drawW,
    int drawH,
    int& outW,
    int& outH,
    float& offsetX,
    float& offsetY,
    int& outWorkX,
    int& outWorkY,
    int& outWorkW,
    int& outWorkH,
    int& outDrawX,
    int& outDrawY
) {
    outW = srcW;
    outH = srcH;
    
    offsetX = 0.0f;
    offsetY = 0.0f;

    outWorkX = workX;
    outWorkY = workY;
    outWorkW = workW;
    outWorkH = workH;
    
    outDrawX = drawX;
    outDrawY = drawY;
}

struct FourierState {
    bool ready = false;
    int specW = 0;
    int specH = 0;
    int srcX = 0;
    int srcY = 0;
    int srcW = 0;
    int srcH = 0;
    std::vector<FftComplex> specR;
    std::vector<FftComplex> specG;
    std::vector<FftComplex> specB;
    std::vector<float> mask;
    std::vector<float> logNorm;
    std::vector<unsigned char> preview;
    GLuint texture = 0;
};

FourierState gFourier;

void destroyFourierTexture() {
    if (gFourier.texture != 0) {
        glDeleteTextures(1, &gFourier.texture);
        gFourier.texture = 0;
    }
}

void uploadSpectrumPreview() {
    if (!gFourier.ready || gFourier.specW <= 0 || gFourier.specH <= 0)
        return;

    const int n = gFourier.specW * gFourier.specH;
    gFourier.preview.resize((size_t)n * 4);

    for (int i = 0; i < n; ++i) {
        const float v = std::max(0.0f, std::min(1.0f, gFourier.logNorm[i] * gFourier.mask[i]));
        const unsigned char g = (unsigned char)(v * 255.0f + 0.5f);
        gFourier.preview[(size_t)i * 4 + 0] = g;
        gFourier.preview[(size_t)i * 4 + 1] = g;
        gFourier.preview[(size_t)i * 4 + 2] = g;
        gFourier.preview[(size_t)i * 4 + 3] = 255;
    }

    if (gFourier.texture == 0)
        glGenTextures(1, &gFourier.texture);

    glBindTexture(GL_TEXTURE_2D, gFourier.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        gFourier.specW,
        gFourier.specH,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        gFourier.preview.data()
    );
    glBindTexture(GL_TEXTURE_2D, 0);
}

void rebuildLogNorm() {
    const int n = gFourier.specW * gFourier.specH;
    gFourier.logNorm.assign((size_t)n, 0.0f);

    float maxLog = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float mag =
            (fftMagnitude(gFourier.specR[i]) +
             fftMagnitude(gFourier.specG[i]) +
             fftMagnitude(gFourier.specB[i])) / 3.0f;
        const float v = std::log(1.0f + mag);
        gFourier.logNorm[i] = v;
        if (v > maxLog) maxLog = v;
    }

    if (maxLog > 1e-8f) {
        const float inv = 1.0f / maxLog;
        for (int i = 0; i < n; ++i)
            gFourier.logNorm[i] *= inv;
    }
}

} // namespace

void Engine::invalidateFourierSpectrum() {
    gFourier.ready = false;
    gFourier.specW = 0;
    gFourier.specH = 0;
    gFourier.specR.clear();
    gFourier.specG.clear();
    gFourier.specB.clear();
    gFourier.mask.clear();
    gFourier.logNorm.clear();
    gFourier.preview.clear();
    destroyFourierTexture();
}

bool Engine::hasFourierSpectrum() {
    return gFourier.ready;
}

GLuint Engine::getSpectrumTexture() {
    return gFourier.texture;
}

int Engine::getSpectrumWidth() {
    return gFourier.specW;
}

int Engine::getSpectrumHeight() {
    return gFourier.specH;
}

void Engine::resetFourierMask() {
    if (!gFourier.ready) return;
    std::fill(gFourier.mask.begin(), gFourier.mask.end(), 1.0f);
    uploadSpectrumPreview();
}

void Engine::computeFourierSpectrum() {
    if (Engine::currImage == 0) return;

    const int canvasW = Engine::imageWidth;
    const int canvasH = Engine::imageHeight;
    if (canvasW <= 0 || canvasH <= 0) return;

    int srcX = Engine::workX;
    int srcY = Engine::workY;
    int srcW = Engine::workW;
    int srcH = Engine::workH;
    if (srcW <= 0 || srcH <= 0) {
        srcX = 0;
        srcY = 0;
        srcW = canvasW;
        srcH = canvasH;
    }

    std::vector<unsigned char> pixels((size_t)canvasW * (size_t)canvasH * 4);
    glBindTexture(GL_TEXTURE_2D, Engine::currImage);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    const int specW = fftNextPow2(srcW);
    const int specH = fftNextPow2(srcH);
    const int n = specW * specH;

    gFourier.specR.assign((size_t)n, {});
    gFourier.specG.assign((size_t)n, {});
    gFourier.specB.assign((size_t)n, {});
    gFourier.mask.assign((size_t)n, 1.0f);

    for (int y = 0; y < srcH; ++y) {
        for (int x = 0; x < srcW; ++x) {
            const int cx = srcX + x;
            const int cy = srcY + y;
            if (cx < 0 || cy < 0 || cx >= canvasW || cy >= canvasH)
                continue;
            const size_t pi = ((size_t)cy * (size_t)canvasW + (size_t)cx) * 4;
            const size_t fi = (size_t)y * (size_t)specW + (size_t)x;
            gFourier.specR[fi].re = pixels[pi + 0] / 255.0f;
            gFourier.specG[fi].re = pixels[pi + 1] / 255.0f;
            gFourier.specB[fi].re = pixels[pi + 2] / 255.0f;
        }
    }

    fft2d(gFourier.specR, specW, specH, false);
    fft2d(gFourier.specG, specW, specH, false);
    fft2d(gFourier.specB, specW, specH, false);
    fftShift2d(gFourier.specR, specW, specH);
    fftShift2d(gFourier.specG, specW, specH);
    fftShift2d(gFourier.specB, specW, specH);

    gFourier.specW = specW;
    gFourier.specH = specH;
    gFourier.srcX = srcX;
    gFourier.srcY = srcY;
    gFourier.srcW = srcW;
    gFourier.srcH = srcH;
    gFourier.ready = true;

    rebuildLogNorm();
    uploadSpectrumPreview();
}

void Engine::paintFourierBrush(float x, float y, float radius, float value, float hardness) {
    if (!gFourier.ready) return;
    if (radius <= 0.0f) return;

    value = std::max(0.0f, std::min(1.0f, value));
    hardness = std::max(0.0f, std::min(1.0f, hardness));

    const int x0 = std::max(0, (int)std::floor(x - radius));
    const int y0 = std::max(0, (int)std::floor(y - radius));
    const int x1 = std::min(gFourier.specW - 1, (int)std::ceil(x + radius));
    const int y1 = std::min(gFourier.specH - 1, (int)std::ceil(y + radius));

    float sigma = radius * (0.52f - 0.38f * hardness);
    if (sigma < 0.35f) sigma = 0.35f;
    const float invTwoSigma2 = 1.0f / (2.0f * sigma * sigma);
    const bool hardStamp = hardness >= 0.92f;

    for (int py = y0; py <= y1; ++py) {
        for (int px = x0; px <= x1; ++px) {
            const float dx = ((float)px + 0.5f) - x;
            const float dy = ((float)py + 0.5f) - y;
            const float d2 = dx * dx + dy * dy;
            const float d = std::sqrt(d2);
            if (d > radius) continue;

            float w = 1.0f;
            if (!hardStamp)
                w = std::exp(-d2 * invTwoSigma2);
            else if (d > radius)
                continue;

            const size_t i = (size_t)py * (size_t)gFourier.specW + (size_t)px;
            gFourier.mask[i] = gFourier.mask[i] * (1.0f - w) + value * w;
            if (gFourier.mask[i] < 0.0f) gFourier.mask[i] = 0.0f;
            if (gFourier.mask[i] > 1.0f) gFourier.mask[i] = 1.0f;
        }
    }

    uploadSpectrumPreview();
}

void Engine::applyInverseFourier(bool shouldLog) {
    if (Engine::currImage == 0) return;
    if (!gFourier.ready)
        Engine::computeFourierSpectrum();
    if (!gFourier.ready) return;

    if (shouldLog) {
        FilterSettings op;
        op.option = Filter_Fourier;
        Engine::addOperationToLog(op);
    }

    const int specW = gFourier.specW;
    const int specH = gFourier.specH;
    const int n = specW * specH;

    std::vector<FftComplex> r = gFourier.specR;
    std::vector<FftComplex> g = gFourier.specG;
    std::vector<FftComplex> b = gFourier.specB;
    for (int i = 0; i < n; ++i) {
        r[i] = fftScale(r[i], gFourier.mask[i]);
        g[i] = fftScale(g[i], gFourier.mask[i]);
        b[i] = fftScale(b[i], gFourier.mask[i]);
    }

    fftShift2d(r, specW, specH);
    fftShift2d(g, specW, specH);
    fftShift2d(b, specW, specH);
    fft2d(r, specW, specH, true);
    fft2d(g, specW, specH, true);
    fft2d(b, specW, specH, true);

    const int canvasW = Engine::imageWidth;
    const int canvasH = Engine::imageHeight;
    std::vector<unsigned char> pixels((size_t)canvasW * (size_t)canvasH * 4);

    glBindTexture(GL_TEXTURE_2D, Engine::currImage);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    auto clamp01 = [](float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    };

    for (int y = 0; y < gFourier.srcH; ++y) {
        for (int x = 0; x < gFourier.srcW; ++x) {
            const int cx = gFourier.srcX + x;
            const int cy = gFourier.srcY + y;
            if (cx < 0 || cy < 0 || cx >= canvasW || cy >= canvasH)
                continue;
            const size_t fi = (size_t)y * (size_t)specW + (size_t)x;
            const size_t pi = ((size_t)cy * (size_t)canvasW + (size_t)cx) * 4;
            pixels[pi + 0] = (unsigned char)(clamp01(r[fi].re) * 255.0f + 0.5f);
            pixels[pi + 1] = (unsigned char)(clamp01(g[fi].re) * 255.0f + 0.5f);
            pixels[pi + 2] = (unsigned char)(clamp01(b[fi].re) * 255.0f + 0.5f);
        }
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        canvasW,
        canvasH,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data()
    );

    if (Engine::prevImage != 0) {
        glCopyImageSubData(
            Engine::currImage, GL_TEXTURE_2D,
            0, 0, 0, 0,
            Engine::prevImage, GL_TEXTURE_2D,
            0, 0, 0, 0,
            canvasW,
            canvasH,
            1
        );
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Engine::computeHistogram(const std::vector<unsigned char>& pixels, int width, int height, 
                               int histR[256], int histG[256], int histB[256], int histGray[256]) {
    std::fill(histR, histR + 256, 0);
    std::fill(histG, histG + 256, 0);
    std::fill(histB, histB + 256, 0);
    std::fill(histGray, histGray + 256, 0);

    size_t totalPixels = (size_t)width * height;
    for (size_t i = 0; i < totalPixels; ++i) {
        unsigned char r = pixels[i * 4 + 0];
        unsigned char g = pixels[i * 4 + 1];
        unsigned char b = pixels[i * 4 + 2];
        unsigned char gray = static_cast<unsigned char>(0.299f * r + 0.587f * g + 0.114f * b);

        histR[r]++;
        histG[g]++;
        histB[b]++;
        histGray[gray]++;
    }
}

void Engine::applyHistogramEqualization(bool shouldLog) {
    if (Engine::currImage == 0) return;

    if (shouldLog) {
        FilterSettings op;
        op.option = FilterOption::Filter_HistogramEqualization;
        Engine::addOperationToLog(op);
    }

    int width = Engine::imageWidth;
    int height = Engine::imageHeight;
    int totalPixels = width * height;
    std::vector<unsigned char> pixels((size_t)totalPixels * 4);

    glBindTexture(GL_TEXTURE_2D, Engine::currImage);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    int histY[256] = {0};
    for (int i = 0; i < totalPixels; ++i) {
        unsigned char r = pixels[i * 4 + 0];
        unsigned char g = pixels[i * 4 + 1];
        unsigned char b = pixels[i * 4 + 2];
        unsigned char y = static_cast<unsigned char>(0.299f * r + 0.587f * g + 0.114f * b);
        histY[y]++;
    }

    int cdf[256] = {0};
    int sum = 0;
    int cdfMin = -1;

    for (int i = 0; i < 256; ++i) {
        sum += histY[i];
        cdf[i] = sum;
        if (cdfMin == -1 && cdf[i] > 0) {
            cdfMin = cdf[i];
        }
    }

    if (totalPixels == cdfMin) {
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }

    unsigned char lutY[256];
    for (int i = 0; i < 256; ++i) {
        float normalized = (float)(cdf[i] - cdfMin) / (float)(totalPixels - cdfMin);
        lutY[i] = static_cast<unsigned char>(std::clamp(std::round(normalized * 255.0f), 0.0f, 255.0f));
    }

    for (int i = 0; i < totalPixels; ++i) {
        float r = pixels[i * 4 + 0];
        float g = pixels[i * 4 + 1];
        float b = pixels[i * 4 + 2];

        float y = 0.299f * r + 0.587f * g + 0.114f * b;

        if (y > 0.0f) {
            float newY = static_cast<float>(lutY[static_cast<unsigned char>(y)]);
            float scale = newY / y;

            pixels[i * 4 + 0] = static_cast<unsigned char>(std::clamp(r * scale, 0.0f, 255.0f));
            pixels[i * 4 + 1] = static_cast<unsigned char>(std::clamp(g * scale, 0.0f, 255.0f));
            pixels[i * 4 + 2] = static_cast<unsigned char>(std::clamp(b * scale, 0.0f, 255.0f));
        }
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    if (Engine::prevImage != 0) {
        glCopyImageSubData(Engine::currImage, GL_TEXTURE_2D, 0, 0, 0, 0,
                           Engine::prevImage, GL_TEXTURE_2D, 0, 0, 0, 0,
                           width, height, 1);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Engine::loadChromaBgImage(const char* path) {
    if (chromaBgImage != 0) {
        glDeleteTextures(1, &chromaBgImage);
        chromaBgImage = 0;
    }

    int width, height, channels;
    unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
    if (!data) {
        printf("Chroma-Key background image failed to load: %s\n", path);
        return;
    }

    glGenTextures(1, &chromaBgImage);
    glBindTexture(GL_TEXTURE_2D, chromaBgImage);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
}

GLuint Engine::getChromaBgImage() {
    return Engine::chromaBgImage;
}

namespace {

bool readCurrentRgba(std::vector<unsigned char>& pixels, int& width, int& height) {
    if (Engine::getCurrImage() == 0) return false;
    width = Engine::getImageWidth();
    height = Engine::getImageHeight();
    if (width <= 0 || height <= 0) return false;
    pixels.resize((size_t)width * (size_t)height * 4);
    glBindTexture(GL_TEXTURE_2D, Engine::getCurrImage());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void cropWorkingArea(
    const std::vector<unsigned char>& src,
    int srcW,
    int srcH,
    std::vector<unsigned char>& dst,
    int& dstW,
    int& dstH,
    int& originX,
    int& originY
) {
    originX = Engine::getWorkX();
    originY = Engine::getWorkY();
    dstW = Engine::getWorkWidth();
    dstH = Engine::getWorkHeight();

    if (dstW <= 0 || dstH <= 0) {
        originX = 0;
        originY = 0;
        dstW = srcW;
        dstH = srcH;
    }

    originX = std::max(0, std::min(originX, srcW - 1));
    originY = std::max(0, std::min(originY, srcH - 1));
    dstW = std::max(1, std::min(dstW, srcW - originX));
    dstH = std::max(1, std::min(dstH, srcH - originY));

    dst.resize((size_t)dstW * (size_t)dstH * 4);
    for (int y = 0; y < dstH; ++y) {
        const unsigned char* row = src.data() + (((size_t)(originY + y) * (size_t)srcW + (size_t)originX) * 4);
        std::memcpy(dst.data() + ((size_t)y * (size_t)dstW * 4), row, (size_t)dstW * 4);
    }
}

bool writeBitIntoRgb(std::vector<unsigned char>& rgba, size_t pixelCount, size_t& bitIndex, int bit) {
    const size_t pix = bitIndex / 3;
    const size_t ch = bitIndex % 3;
    if (pix >= pixelCount) return false;
    rgba[pix * 4 + ch] = (unsigned char)((rgba[pix * 4 + ch] & 0xFE) | (bit & 1));
    bitIndex++;
    return true;
}

bool writeByteIntoRgb(std::vector<unsigned char>& rgba, size_t pixelCount, size_t& bitIndex, uint8_t value) {
    for (int i = 0; i < 8; ++i) {
        if (!writeBitIntoRgb(rgba, pixelCount, bitIndex, (value >> i) & 1))
            return false;
    }
    return true;
}

bool readBitFromRgb(const unsigned char* rgba, size_t pixelCount, size_t& bitIndex, int& bit) {
    const size_t pix = bitIndex / 3;
    const size_t ch = bitIndex % 3;
    if (pix >= pixelCount) return false;
    bit = rgba[pix * 4 + ch] & 1;
    bitIndex++;
    return true;
}

bool readByteFromRgb(const unsigned char* rgba, size_t pixelCount, size_t& bitIndex, uint8_t& value) {
    value = 0;
    for (int i = 0; i < 8; ++i) {
        int bit = 0;
        if (!readBitFromRgb(rgba, pixelCount, bitIndex, bit))
            return false;
        value |= (uint8_t)(bit << i);
    }
    return true;
}

bool extractMessageFromRgba(
    const unsigned char* rgba,
    int width,
    int height,
    std::string& message,
    std::string& error
) {
    message.clear();
    const size_t pixelCount = (size_t)width * (size_t)height;
    const size_t capacityBits = pixelCount * 3;
    if (capacityBits < 64) {
        error = "Image is too small to contain a hidden message.";
        return false;
    }

    size_t bitIndex = 0;
    uint8_t magic[4] = {};
    for (int i = 0; i < 4; ++i) {
        if (!readByteFromRgb(rgba, pixelCount, bitIndex, magic[i])) {
            error = "Failed to read message header.";
            return false;
        }
    }

    if (magic[0] != 'L' || magic[1] != 'U' || magic[2] != 'M' || magic[3] != '1') {
        error = "No hidden Lumage message found in this image.";
        return false;
    }

    uint32_t length = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t b = 0;
        if (!readByteFromRgb(rgba, pixelCount, bitIndex, b)) {
            error = "Failed to read message length.";
            return false;
        }
        length |= ((uint32_t)b) << (8 * i);
    }

    const size_t remainingBytes = (capacityBits - bitIndex) / 8;
    if (length > remainingBytes) {
        error = "Hidden message header is invalid or truncated.";
        return false;
    }

    message.resize(length);
    for (uint32_t i = 0; i < length; ++i) {
        uint8_t b = 0;
        if (!readByteFromRgb(rgba, pixelCount, bitIndex, b)) {
            error = "Failed to read the hidden message.";
            return false;
        }
        message[i] = (char)b;
    }

    error.clear();
    return true;
}

} // namespace

size_t Engine::getSteganographyCapacityBytes() {
    int w = Engine::getWorkWidth();
    int h = Engine::getWorkHeight();
    if (w <= 0 || h <= 0) {
        w = Engine::getImageWidth();
        h = Engine::getImageHeight();
    }
    const size_t bits = (size_t)std::max(0, w) * (size_t)std::max(0, h) * 3;
    if (bits < 64) return 0;
    return (bits / 8) - 8;
}

bool Engine::exportImage(const char* path, int format, int jpegQuality) {
    if (!path || path[0] == '\0') return false;

    std::vector<unsigned char> canvas;
    int canvasW = 0;
    int canvasH = 0;
    if (!readCurrentRgba(canvas, canvasW, canvasH))
        return false;

    std::vector<unsigned char> work;
    int workW = 0, workH = 0, originX = 0, originY = 0;
    cropWorkingArea(canvas, canvasW, canvasH, work, workW, workH, originX, originY);

    jpegQuality = std::max(1, std::min(100, jpegQuality));
    int ok = 0;
    switch (format) {
        case Export_JPEG: {
            std::vector<unsigned char> rgb((size_t)workW * (size_t)workH * 3);
            for (int i = 0; i < workW * workH; ++i) {
                rgb[(size_t)i * 3 + 0] = work[(size_t)i * 4 + 0];
                rgb[(size_t)i * 3 + 1] = work[(size_t)i * 4 + 1];
                rgb[(size_t)i * 3 + 2] = work[(size_t)i * 4 + 2];
            }
            ok = stbi_write_jpg(path, workW, workH, 3, rgb.data(), jpegQuality);
            break;
        }
        case Export_BMP:
            ok = stbi_write_bmp(path, workW, workH, 4, work.data());
            break;
        case Export_TGA:
            ok = stbi_write_tga(path, workW, workH, 4, work.data());
            break;
        case Export_PNG:
        default:
            ok = stbi_write_png(path, workW, workH, 4, work.data(), workW * 4);
            break;
    }
    return ok != 0;
}

bool Engine::embedSteganographyMessage(const char* message, std::string& error) {
    if (Engine::currImage == 0) {
        error = "Load an image first.";
        return false;
    }

    const char* text = message ? message : "";
    const size_t msgLen = std::strlen(text);
    const size_t capacity = Engine::getSteganographyCapacityBytes();
    if (msgLen > capacity) {
        error = "Message is too long for this image.";
        return false;
    }

    std::vector<unsigned char> canvas;
    int canvasW = 0;
    int canvasH = 0;
    if (!readCurrentRgba(canvas, canvasW, canvasH)) {
        error = "Could not read the current image.";
        return false;
    }

    std::vector<unsigned char> work;
    int workW = 0, workH = 0, originX = 0, originY = 0;
    cropWorkingArea(canvas, canvasW, canvasH, work, workW, workH, originX, originY);

    const size_t pixelCount = (size_t)workW * (size_t)workH;
    size_t bitIndex = 0;
    const uint8_t magic[4] = { 'L', 'U', 'M', '1' };
    for (int i = 0; i < 4; ++i) {
        if (!writeByteIntoRgb(work, pixelCount, bitIndex, magic[i])) {
            error = "Failed to write message header.";
            return false;
        }
    }

    const uint32_t length = (uint32_t)msgLen;
    for (int i = 0; i < 4; ++i) {
        if (!writeByteIntoRgb(work, pixelCount, bitIndex, (uint8_t)((length >> (8 * i)) & 0xFF))) {
            error = "Failed to write message length.";
            return false;
        }
    }

    for (size_t i = 0; i < msgLen; ++i) {
        if (!writeByteIntoRgb(work, pixelCount, bitIndex, (uint8_t)text[i])) {
            error = "Failed to write the message.";
            return false;
        }
    }

    for (int y = 0; y < workH; ++y) {
        unsigned char* dst = canvas.data() + (((size_t)(originY + y) * (size_t)canvasW + (size_t)originX) * 4);
        const unsigned char* src = work.data() + ((size_t)y * (size_t)workW * 4);
        std::memcpy(dst, src, (size_t)workW * 4);
    }

    FilterSettings op;
    op.option = 15;
    Engine::addOperationToLog(op);

    glBindTexture(GL_TEXTURE_2D, Engine::currImage);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        canvasW,
        canvasH,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        canvas.data()
    );

    if (Engine::prevImage != 0) {
        glCopyImageSubData(
            Engine::currImage, GL_TEXTURE_2D, 0, 0, 0, 0,
            Engine::prevImage, GL_TEXTURE_2D, 0, 0, 0, 0,
            canvasW, canvasH, 1
        );
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    Engine::invalidateFourierSpectrum();

    error.clear();
    return true;
}

bool Engine::extractSteganographyMessageFromCurrent(std::string& message, std::string& error) {
    std::vector<unsigned char> canvas;
    int canvasW = 0;
    int canvasH = 0;
    if (!readCurrentRgba(canvas, canvasW, canvasH)) {
        error = "Load an image first.";
        return false;
    }

    std::vector<unsigned char> work;
    int workW = 0, workH = 0, originX = 0, originY = 0;
    cropWorkingArea(canvas, canvasW, canvasH, work, workW, workH, originX, originY);
    return extractMessageFromRgba(work.data(), workW, workH, message, error);
}

bool Engine::extractSteganographyMessageFromFile(const char* path, std::string& message, std::string& error) {
    if (!path || path[0] == '\0') {
        error = "Choose an image file.";
        return false;
    }

    int width = 0, height = 0, channels = 0;
    unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
    if (!data) {
        error = "Failed to load the selected image.";
        return false;
    }

    const bool ok = extractMessageFromRgba(data, width, height, message, error);
    stbi_image_free(data);
    return ok;
}

glm::vec3 Engine::rgbToHsi(const glm::vec3& rgb) {
    float r = std::clamp(rgb.r, 0.0f, 1.0f);
    float g = std::clamp(rgb.g, 0.0f, 1.0f);
    float b = std::clamp(rgb.b, 0.0f, 1.0f);

    float sumRGB = r + g + b;
    float i = sumRGB / 3.0f;

    if (i <= 1e-6f || sumRGB <= 1e-6f) {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }

    float minRGB = std::min({r, g, b});
    float s = 1.0f - (3.0f / sumRGB) * minRGB;
    s = std::clamp(s, 0.0f, 1.0f);

    if (s <= 1e-6f) {
        return glm::vec3(0.0f, 0.0f, i);
    }

    float num = 0.5f * ((r - g) + (r - b));
    float den = std::sqrt((r - g) * (r - g) + (r - b) * (g - b));

    if (den <= 1e-6f) {
        return glm::vec3(0.0f, s, i);
    }

    float theta = std::acos(std::clamp(num / den, -1.0f, 1.0f));
    float h = (b > g) ? (2.0f * PI - theta) : theta;

    return glm::vec3(glm::degrees(h), s, i);
}

glm::vec3 Engine::hsiToRgb(const glm::vec3& hsi) {
    float h = std::fmod(hsi.x, 360.0f);
    if (h < 0.0f) h += 360.0f;

    float s = std::clamp(hsi.y, 0.0f, 1.0f);
    float i = std::clamp(hsi.z, 0.0f, 1.0f);

    if (s <= 1e-6f) {
        return glm::vec3(i);
    }

    float r = 0.0f, g = 0.0f, b = 0.0f;
    float hRad = glm::radians(h);

    if (h < 120.0f) {
        b = i * (1.0f - s);
        float denom = std::cos(glm::radians(60.0f) - hRad);
        r = (denom == 0.0f) ? i : i * (1.0f + (s * std::cos(hRad)) / denom);
        g = 3.0f * i - (r + b);
    } else if (h < 240.0f) {
        hRad -= glm::radians(120.0f);
        r = i * (1.0f - s);
        float denom = std::cos(glm::radians(60.0f) - hRad);
        g = (denom == 0.0f) ? i : i * (1.0f + (s * std::cos(hRad)) / denom);
        b = 3.0f * i - (r + g);
    } else {
        hRad -= glm::radians(240.0f);
        g = i * (1.0f - s);
        float denom = std::cos(glm::radians(60.0f) - hRad);
        b = (denom == 0.0f) ? i : i * (1.0f + (s * std::cos(hRad)) / denom);
        r = 3.0f * i - (g + b);
    }

    return glm::clamp(glm::vec3(r, g, b), 0.0f, 1.0f);
}

glm::vec4 Engine::applyHSIAdjust(glm::vec4 color, const FilterSettings& settings) {
    glm::vec3 hsi = rgbToHsi(glm::vec3(color));

    hsi.x += settings.hueShift;

    hsi.y = std::clamp(hsi.y * settings.saturationScale, 0.0f, 1.0f);
    hsi.z = std::clamp(hsi.z * settings.intensityScale, 0.0f, 1.0f);

    glm::vec3 rgb = hsiToRgb(hsi);
    return glm::vec4(rgb, color.a);
}

void Engine::loadImage(const char* path, uint components){
    Engine::invalidateFourierSpectrum();

    if (baseImage != 0){
        glDeleteTextures(1, &baseImage);
        baseImage = 0;
    }

    if (prevImage != 0){
        glDeleteTextures(1, &prevImage);
        prevImage = 0;
    }

    if (currImage != 0){
        glDeleteTextures(1, &currImage);
        currImage = 0;
    }

    int width, height, channels;

    unsigned char* data = stbi_load(
        path,
        &width,
        &height,
        &channels,
        components
    );

    if (!data){
        printf("Failed to load image: %s\n", path);
        return;
    }

    GLenum format;
    GLenum internalFormat;

    switch (components){
        case 1:
            format = GL_RED;
            internalFormat = GL_R8;
            break;

        case 2:
            format = GL_RG;
            internalFormat = GL_RG8;
            break;

        case 3:
            format = GL_RGB;
            internalFormat = GL_RGB8;
            break;

        case 4:
            format = GL_RGBA;
            internalFormat = GL_RGBA8;
            break;

        default:
            printf("Unsupported components: %u\n", components);
            stbi_image_free(data);
            return;
    }

    GLuint textures[3];
    glGenTextures(3, textures);

    for (int i = 0; i < 3; i++){
        glBindTexture(GL_TEXTURE_2D, textures[i]);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        if (components == 1){
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
        }

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            internalFormat,
            width,
            height,
            0,
            format,
            GL_UNSIGNED_BYTE,
            data
        );
    }

    Engine::imageWidth = width;
    Engine::imageHeight = height;
    Engine::workX = 0;
    Engine::workY = 0;
    Engine::workW = width;
    Engine::workH = height;
    Engine::drawX = 0;
    Engine::drawY = 0;
    Engine::drawW = width;
    Engine::drawH = height;
    Engine::lastCanvasOffset = glm::vec2(0.0f);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

    Engine::baseImage = textures[0];
    Engine::prevImage = textures[1];
    Engine::currImage = textures[2];
}

void Engine::processImage(FilterSettings settings, bool shouldLog){
    if (Engine::baseImage == 0) return;

    if (settings.option == FilterOption::Filter_Fourier) {
        Engine::applyInverseFourier(shouldLog);
        return;
    }

    Engine::invalidateFourierSpectrum();

    GLuint base = Engine::baseImage;
    GLuint prev = Engine::prevImage;
    GLuint curr = Engine::currImage;
    int width = Engine::getImageWidth();
    int height = Engine::getImageHeight();

    if (shouldLog) Engine::addOperationToLog(settings);

    Engine::lastCanvasOffset = glm::vec2(0.0f);
    bool rebuiltCanvas = false;

    if (settings.option == FilterOption::Filter_HistogramEqualization) {
        Engine::applyHistogramEqualization(shouldLog);
        return;
    }

    if (settings.option == FilterOption::Filter_Scale){
        width  = (int) (width * settings.scale_factor_x);
        height = (int) (height * settings.scale_factor_y);
        width  = std::max(1, width);
        height = std::max(1, height);

        Engine::workX = (int)(Engine::workX * settings.scale_factor_x);
        Engine::workY = (int)(Engine::workY * settings.scale_factor_y);
        Engine::workW = std::max(1, (int)(Engine::workW * settings.scale_factor_x));
        Engine::workH = std::max(1, (int)(Engine::workH * settings.scale_factor_y));

        Engine::drawX = (int)(Engine::drawX * settings.scale_factor_x);
        Engine::drawY = (int)(Engine::drawY * settings.scale_factor_y);
        Engine::drawW = std::max(1, (int)(Engine::drawW * settings.scale_factor_x));
        Engine::drawH = std::max(1, (int)(Engine::drawH * settings.scale_factor_y));

        GLuint resized = createEmptyRGBA8Texture(width, height);
        glDeleteTextures(1, &curr);
        curr = resized;
        rebuiltCanvas = true;
    } else if (settings.option == FilterOption::Filter_Rotate) {
        settings.isRegionSelected = false;

        int newW = width;
        int newH = height;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        int newWorkX = Engine::workX;
        int newWorkY = Engine::workY;
        int newWorkW = Engine::workW;
        int newWorkH = Engine::workH;
        int newDrawX = Engine::drawX;
        int newDrawY = Engine::drawY;

        computeRotatedCanvas(
            width,
            height,
            settings.pivot_x,
            settings.pivot_y,
            settings.rotation_angle,
            Engine::workX,
            Engine::workY,
            Engine::workW,
            Engine::workH,
            Engine::drawX,
            Engine::drawY,
            Engine::drawW,
            Engine::drawH,
            newW,
            newH,
            offsetX,
            offsetY,
            newWorkX,
            newWorkY,
            newWorkW,
            newWorkH,
            newDrawX,
            newDrawY
        );

        settings.canvas_offset_x = offsetX;
        settings.canvas_offset_y = offsetY;
        Engine::lastCanvasOffset = glm::vec2(offsetX, offsetY);

        Engine::workX = newWorkX;
        Engine::workY = newWorkY;
        Engine::workW = newWorkW;
        Engine::workH = newWorkH;

        Engine::drawX = newDrawX;
        Engine::drawY = newDrawY;
        Engine::drawW = newW;
        Engine::drawH = newH;

        if (newW != width || newH != height || offsetX != 0.0f || offsetY != 0.0f){
            GLuint expanded = createEmptyRGBA8Texture(newW, newH);
            glDeleteTextures(1, &curr);
            curr = expanded;
            width = newW;
            height = newH;
            rebuiltCanvas = true;
        }
    }

    glUseProgram(Engine::shaderProgram);

    GLint hasSelectionLoc = glGetUniformLocation(Engine::shaderProgram, "hasSelection");
    GLint tlCornerLoc     = glGetUniformLocation(Engine::shaderProgram, "tlCorner");
    GLint brCornerLoc     = glGetUniformLocation(Engine::shaderProgram, "brCorner");

    glUniform1i(hasSelectionLoc, settings.isRegionSelected ? 1 : 0);
    glUniform2f(tlCornerLoc, settings.tlCorner.x, settings.tlCorner.y);
    glUniform2f(brCornerLoc, settings.brCorner.x, settings.brCorner.y);

    GLuint bgTex = Engine::getChromaBgImage();
    if (bgTex == 0) {
        bgTex = prev;
    }

    //Setting images
    glBindImageTexture(
        0,
        base,
        0,
        GL_FALSE,
        0,
        GL_READ_WRITE,
        GL_RGBA8
    );

    glBindImageTexture(
        1,
        prev,
        0,
        GL_FALSE,
        0,
        GL_READ_WRITE,
        GL_RGBA8
    );

    glBindImageTexture(
        2,
        curr,
        0,
        GL_FALSE,
        0,
        GL_READ_WRITE,
        GL_RGBA8
    );

    glBindImageTexture(
        3,
        Engine::getChromaBgImage(),
        0,
        GL_FALSE,
        0,
        GL_READ_ONLY,
        GL_RGBA8
    );

    //Organizing threads
    GLuint groupsX =
        (width + 15) / 16;

    GLuint groupsY =
        (height + 15) / 16;

    //Buffering filter settings
    glBindBuffer(GL_UNIFORM_BUFFER, settingsUBO);

    glBufferSubData(
        GL_UNIFORM_BUFFER,
        0,
        sizeof(FilterSettings),
        &settings
    );

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Buffering convolution kernel info
    if (settings.option == 5){
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, Engine::kernelSSBO);

        glBufferData(
            GL_SHADER_STORAGE_BUFFER,
            settings.kernel.size() * sizeof(float),
            settings.kernel.data(),
            GL_DYNAMIC_DRAW
        );

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        GLuint kernelSizeLocation = glGetUniformLocation(shaderProgram, "kernelSize");
        glUniform1i(kernelSizeLocation, settings.kernelSize);
    }

    glDispatchCompute(
        groupsX,
        groupsY,
        1
    );

    glMemoryBarrier(
        GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
    );

    //Updating prev to curr
    if (rebuiltCanvas){
        GLuint resized = createEmptyRGBA8Texture(width, height);
        glDeleteTextures(1, &prev);
        prev = resized;
    }

    glCopyImageSubData(
        curr, GL_TEXTURE_2D,
        0, 0, 0, 0,

        prev, GL_TEXTURE_2D,
        0, 0, 0, 0,

        width,
        height,
        1
    );

    Engine::baseImage = base;
    Engine::prevImage = prev;
    Engine::currImage = curr;
    Engine::imageWidth = width;
    Engine::imageHeight = height;

    glUseProgram(0);
}

void Engine::__setup(){
    Engine::__initGLFW();
    Engine::__initGLEW();
    Engine::__initImGUI();

    Engine::shaderProgram = Engine::__createComputeProgram("shaders/filters.comp");

    //FilterSettings UBO
    glGenBuffers(1, &Engine::settingsUBO);

    glBindBuffer(GL_UNIFORM_BUFFER, Engine::settingsUBO);

    glBufferData(
        GL_UNIFORM_BUFFER,
        sizeof(FilterSettings),
        nullptr,
        GL_DYNAMIC_DRAW
    );

    glBindBufferBase(
        GL_UNIFORM_BUFFER,
        0,
        Engine::settingsUBO
    );

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Convolution Kernel SSBO
    glGenBuffers(1, &Engine::kernelSSBO);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, Engine::kernelSSBO);

    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        0,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, Engine::kernelSSBO);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Engine::__updateLoop(){
    while(Engine::isRunning){
        if (!Engine::__inputStep()) break;

        Engine::__guiStep();
        //Engine::__physicsStep();
        //Engine::__renderStep();
    }
}

void Engine::setup(int width, int height){
    Engine::window_width = width;
    Engine::window_height = height;
    Engine::aspectRatio = (float) width / (float) height;
    
    Engine::__setup();
}

void Engine::setup(){
    Engine::setup(1280, 720);
}

void Engine::run(){
    Engine::isRunning = true;

    Engine::__updateLoop();
}

void Engine::stop(){
    Engine::isRunning = false;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
}