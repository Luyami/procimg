#ifndef ENGINE_H
#define ENGINE_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <random>
#include <fstream>
#include <sstream>
#include <string>
#include <filterSettings.h>
#include <operationRegistry.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <stb_image.h>

#define PI 3.14159265358979323846

//static
class Engine{
    private:
        static bool isRunning;
        static float camMoveSpeed;

        //image vars
        static GLuint baseImage;
        static GLuint prevImage;
        static GLuint currImage;
        static GLuint chromaBgImage;
        static int imageWidth;
        static int imageHeight;
        static int workX;
        static int workY;
        static int workW;
        static int workH;
        static int drawX;
        static int drawY;
        static int drawW;
        static int drawH;
        static glm::vec2 lastCanvasOffset;

        //glfw vars
        static GLFWwindow* window;
        static int window_width;
        static int window_height;
        static float aspectRatio;

        //shaders vars
        static GLuint shaderProgram;
        static GLuint modelLoc;
        static GLuint viewLoc;
        static GLuint projLoc;
        static GLuint colorLoc;
        static GLuint pointSizeLoc;
        static GLuint kernelSSBO;
        static GLuint settingsUBO;

        //Operation registry
        static std::vector<OperationRegistry> operationsLog;

        static GLuint __createComputeShader(const char* path);
        static GLuint __createComputeProgram(const char* path);

        static void __disableAllOnClick();
        
        static void __spawnCirclesButton();
        static void __convexPolyCreationButton();
        static void __polygonCreationButton();

        static void __objectExplorer();
        static void __objectProperties();

        static void __convexPolyCreationTools();

        //initGLFW -> initGLEW -> initImGUI -> initShaders (engine setup pipeline)
        static void __initGLFW();
        static void __initGLEW();
        static void __initImGUI();
        static void __initShaders();
        static void __setup();

        //input -> build gui -> physics -> render scene and gui -> (engine update loop pipeline)
        static bool __inputStep();
        static void __guiStep();
        static void __physicsStep(); static void __computePhysics();
        static void __renderStep();

        static void __updateLoop();
    public:
        Engine() = delete;

        //GUI UTIL
        static void resetImageToBase();
        //**

        static GLuint getBaseImage();
        static GLuint getPrevImage();
        static GLuint getCurrImage();

        static void addOperationToLog(FilterSettings op);
        static OperationRegistry peekOperationsLog();
        static OperationRegistry popOperationsLog();
        static void resetOperationsLog();
        static void revertOperation();

        static int getImageWidth();
        static int getImageHeight();
        static int getWorkX();
        static int getWorkY();
        static int getWorkWidth();
        static int getWorkHeight();
        static int getDrawX();
        static int getDrawY();
        static int getDrawWidth();
        static int getDrawHeight();
        static glm::vec2 getLastCanvasOffset();
        static void loadImage(const char* path, uint components);
        static void processImage(FilterSettings settings, bool shouldLog = true);

        static void invalidateFourierSpectrum();
        static bool hasFourierSpectrum();
        static void computeFourierSpectrum();
        static void resetFourierMask();
        static void paintFourierBrush(float x, float y, float radius, float value, float hardness);
        static void applyInverseFourier(bool shouldLog = true);
        static GLuint getSpectrumTexture();
        static int getSpectrumWidth();
        static int getSpectrumHeight();

        static void computeHistogram(const std::vector<unsigned char>& pixels, int width, int height, 
                int histR[256], int histG[256], int histB[256], int histGray[256]);
        static void applyHistogramEqualization(bool shouldLog = true);

        static glm::vec3 rgbToHsi(const glm::vec3& c);
        static glm::vec3 hsiToRgb(const glm::vec3& hsi);
        static glm::vec4 applyHSIAdjust(glm::vec4 color, const FilterSettings& settings);
        static void loadChromaBgImage(const char* path);
        static GLuint getChromaBgImage();

        enum ExportFormat {
            Export_PNG  = 0,
            Export_JPEG = 1,
            Export_BMP  = 2,
            Export_TGA  = 3
        };

        static bool exportImage(const char* path, int format, int jpegQuality = 90);
        static size_t getSteganographyCapacityBytes();
        static bool embedSteganographyMessage(const char* message, std::string& error);
        static bool extractSteganographyMessageFromFile(const char* path, std::string& message, std::string& error);
        static bool extractSteganographyMessageFromCurrent(std::string& message, std::string& error);

        static void setup(int width, int height);
        static void setup();
        static void run();

        static void stop();
};

#endif