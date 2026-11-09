#include <engine.h>
#include <filterSettings.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>

namespace fs = std::filesystem;

//GUI UTIL
void Engine::resetImageToBase(){
    int width, height;

    glBindTexture(GL_TEXTURE_2D, Engine::baseImage);

    glGetTexLevelParameteriv(
        GL_TEXTURE_2D,
        0,
        GL_TEXTURE_WIDTH,
        &width
    );

    glGetTexLevelParameteriv(
        GL_TEXTURE_2D,
        0,
        GL_TEXTURE_HEIGHT,
        &height
    );

    GLuint newCurr;
    GLuint newPrev;

    glGenTextures(1, &newCurr);
    glBindTexture(GL_TEXTURE_2D, newCurr);

    glTexStorage2D(
        GL_TEXTURE_2D,
        1,
        GL_RGBA8,
        width,
        height
    );

    glDeleteTextures(1, &Engine::currImage);
    Engine::currImage = newCurr;

    glGenTextures(1, &newPrev);
    glBindTexture(GL_TEXTURE_2D, newPrev);

    glTexStorage2D(
        GL_TEXTURE_2D,
        1,
        GL_RGBA8,
        width,
        height
    );

    glDeleteTextures(1, &Engine::prevImage);
    Engine::prevImage = newPrev;

    glCopyImageSubData(
        Engine::getBaseImage(), GL_TEXTURE_2D,
        0, 0, 0, 0,

        Engine::getPrevImage(), GL_TEXTURE_2D,
        0, 0, 0, 0,

        width,
        height,
        1
    );

    glCopyImageSubData(
        Engine::getBaseImage(), GL_TEXTURE_2D,
        0, 0, 0, 0,

        Engine::getCurrImage(), GL_TEXTURE_2D,
        0, 0, 0, 0,

        width,
        height,
        1
    );

    Engine::imageWidth  = width;
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
    Engine::invalidateFourierSpectrum();
}

namespace {

struct ImageRegionSelection {
    bool hasSelection = false;
    bool dragging = false;
    ImVec2 dragStartScreen = {};
    ImVec2 dragCurrentScreen = {};
    ImVec2 screenMin = {};
    ImVec2 screenMax = {};
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;

    void clear() {
        hasSelection = false;
        dragging = false;
    }
};

ImageRegionSelection gRegionSelection;
FilterSettings gFilterSettings;
bool gPivotDragging = false;

int gFourierBrushMode = 0;
float gFourierBrushRadius = 14.0f;
float gFourierGrayValue = 0.5f;
float gFourierHardness = 0.15f;
bool gFourierPainting = false;
ImVec2 gLastSpectrumPaint = {};
bool gLastSpectrumPaintValid = false;

// File Browser State
fs::path gCurrentBrowserPath = fs::current_path();
std::string gSelectedBrowserFile = "";
bool gTriggerLoadDialog = false;
bool gTriggerExportDialog = false;
bool gTriggerStegoDialog = false;
int gExportFormat = Engine::Export_PNG;
int gExportJpegQuality = 90;
char gExportFilename[256] = "export.png";
std::string gExportStatus;

char gStegoMessage[8192] = "";
std::string gStegoExtracted;
std::string gStegoStatus;
int gStegoTab = 0;

ImVec4 accentColor() { return ImVec4(0.28f, 0.55f, 0.86f, 1.0f); }

void applyProfessionalTheme() {
    static bool applied = false;
    if (applied) return;
    applied = true;

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg]         = ImVec4(0.11f, 0.11f, 0.13f, 1.0f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.09f, 0.09f, 0.11f, 1.0f);
    colors[ImGuiCol_PopupBg]            = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);
    colors[ImGuiCol_Border]             = ImVec4(0.22f, 0.22f, 0.26f, 1.0f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.16f, 0.16f, 0.19f, 1.0f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.24f, 0.24f, 0.28f, 1.0f);
    colors[ImGuiCol_TitleBg]            = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    colors[ImGuiCol_MenuBarBg]          = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    colors[ImGuiCol_Header]             = ImVec4(0.20f, 0.40f, 0.65f, 0.55f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.26f, 0.50f, 0.78f, 0.80f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.26f, 0.50f, 0.78f, 1.0f);
    colors[ImGuiCol_Button]             = ImVec4(0.22f, 0.45f, 0.70f, 0.85f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.28f, 0.52f, 0.80f, 1.0f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.18f, 0.38f, 0.62f, 1.0f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.35f, 0.58f, 0.85f, 1.0f);
    colors[ImGuiCol_SliderGrabActive]   = ImVec4(0.45f, 0.68f, 0.95f, 1.0f);
    colors[ImGuiCol_CheckMark]          = ImVec4(0.45f, 0.68f, 0.95f, 1.0f);
    colors[ImGuiCol_Separator]          = ImVec4(0.22f, 0.22f, 0.26f, 1.0f);
    colors[ImGuiCol_Text]               = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);
    colors[ImGuiCol_TextDisabled]       = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
    colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    colors[ImGuiCol_ScrollbarGrab]      = ImVec4(0.28f, 0.28f, 0.32f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.40f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.42f, 0.42f, 0.48f, 1.0f);

    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding       = 4.0f;
    style.WindowPadding     = ImVec2(12.0f, 12.0f);
    style.FramePadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
}

void sectionHeader(const char* label) {
    ImGui::Spacing();
    ImGui::TextUnformatted(label);
    ImGui::Separator();
    ImGui::Spacing();
}

float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

void drawCheckerboard(ImDrawList* draw, ImVec2 min, ImVec2 max, float cellSize) {
    const ImU32 light = IM_COL32(48, 48, 52, 255);
    const ImU32 dark  = IM_COL32(36, 36, 40, 255);
    for (float y = min.y; y < max.y; y += cellSize) {
        for (float x = min.x; x < max.x; x += cellSize) {
            const int ix = (int)((x - min.x) / cellSize);
            const int iy = (int)((y - min.y) / cellSize);
            const ImU32 color = ((ix + iy) & 1) ? light : dark;
            draw->AddRectFilled(
                ImVec2(x, y),
                ImVec2(std::min(x + cellSize, max.x), std::min(y + cellSize, max.y)),
                color
            );
        }
    }
}

void applyKernelPreset(std::vector<float>& kernel, int size, int presetIndex) {
    kernel.assign(size * size, 0.0f);
    const int center = size / 2;

    switch (presetIndex) {
        case 1: // Box blur
            std::fill(kernel.begin(), kernel.end(), 1.0f / (size * size));
            break;
        case 2: // Sharpen (3x3)
            if (size == 3) {
                float k[9] = { 0, -1, 0, -1, 5, -1, 0, -1, 0 };
                std::copy(k, k + 9, kernel.begin());
            } else {
                kernel[center * size + center] = 1.0f;
            }
            break;
        case 3: // Edge detect (3x3)
            if (size == 3) {
                float k[9] = { -1, -1, -1, -1, 8, -1, -1, -1, -1 };
                std::copy(k, k + 9, kernel.begin());
            } else {
                kernel[center * size + center] = 1.0f;
            }
            break;
        case 4: // Emboss (3x3)
            if (size == 3) {
                float k[9] = { -2, -1, 0, -1, 1, 1, 0, 1, 2 };
                std::copy(k, k + 9, kernel.begin());
            } else {
                kernel[center * size + center] = 1.0f;
            }
            break;
        default: // Identity
            kernel[center * size + center] = 1.0f;
            break;
    }
}

void resetKernelToIdentity(std::vector<float>& kernel, int size) {
    kernel.assign(size * size, 0.0f);
    kernel[(size / 2) * size + (size / 2)] = 1.0f;
}

bool drawPiecewiseCurveEditor(FilterSettings& settings) {
    sectionHeader("Transfer Curve");

    ImGui::TextDisabled("Drag the control points to shape the tone mapping curve.");
    ImGui::TextDisabled("Fixed endpoints: (0, 0) and (1, 1).");

    const ImVec2 graphSize(-1.0f, 260.0f);
    ImGui::InvisibleButton("##piecewise_canvas", graphSize, ImGuiButtonFlags_MouseButtonLeft);
    const ImVec2 canvasMin = ImGui::GetItemRectMin();
    const ImVec2 canvasMax = ImGui::GetItemRectMax();
    const ImVec2 canvasSize(canvasMax.x - canvasMin.x, canvasMax.y - canvasMin.y);
    const bool canvasHovered = ImGui::IsItemHovered();
    const bool canvasActive = ImGui::IsItemActive();

    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(canvasMin, canvasMax, IM_COL32(24, 24, 28, 255), 4.0f);
    draw->AddRect(canvasMin, canvasMax, IM_COL32(55, 55, 62, 255), 4.0f);

    auto toScreen = [&](float input, float output) -> ImVec2 {
        return ImVec2(
            canvasMin.x + input * canvasSize.x,
            canvasMax.y - output * canvasSize.y
        );
    };

    auto toNormalized = [&](ImVec2 screen) -> ImVec2 {
        return ImVec2(
            clamp01((screen.x - canvasMin.x) / canvasSize.x),
            clamp01((canvasMax.y - screen.y) / canvasSize.y)
        );
    };

    const int gridLines = 4;
    for (int i = 1; i < gridLines; ++i) {
        const float t = (float)i / (float)gridLines;
        const float x = canvasMin.x + t * canvasSize.x;
        const float y = canvasMax.y - t * canvasSize.y;
        draw->AddLine(ImVec2(x, canvasMin.y), ImVec2(x, canvasMax.y), IM_COL32(45, 45, 52, 255));
        draw->AddLine(ImVec2(canvasMin.x, y), ImVec2(canvasMax.x, y), IM_COL32(45, 45, 52, 255));
    }

    draw->AddLine(
        toScreen(0.0f, 0.0f),
        toScreen(1.0f, 1.0f),
        IM_COL32(70, 70, 78, 180),
        1.0f
    );

    const ImVec2 p0 = toScreen(0.0f, 0.0f);
    const ImVec2 p1 = toScreen(settings.piecewiseR1, settings.piecewiseS1);
    const ImVec2 p2 = toScreen(settings.piecewiseR2, settings.piecewiseS2);
    const ImVec2 p3 = toScreen(1.0f, 1.0f);

    const ImU32 curveColor = IM_COL32(72, 145, 220, 255);
    draw->AddLine(p0, p1, curveColor, 2.5f);
    draw->AddLine(p1, p2, curveColor, 2.5f);
    draw->AddLine(p2, p3, curveColor, 2.5f);

    const float handleRadius = 7.0f;
    const ImU32 anchorColor = IM_COL32(120, 120, 130, 255);
    draw->AddCircleFilled(p0, 4.0f, anchorColor);
    draw->AddCircleFilled(p3, 4.0f, anchorColor);

    enum DragTarget { DragNone = 0, DragP1 = 1, DragP2 = 2 };
    static int dragTarget = DragNone;

    const ImVec2 mouse = ImGui::GetIO().MousePos;

    auto distSq = [](ImVec2 a, ImVec2 b) -> float {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    };

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && canvasHovered) {
        const float hitRadius = handleRadius + 6.0f;
        const float hitRadiusSq = hitRadius * hitRadius;
        const float d1 = distSq(mouse, p1);
        const float d2 = distSq(mouse, p2);
        if (d1 <= hitRadiusSq || d2 <= hitRadiusSq)
            dragTarget = (d1 <= d2) ? DragP1 : DragP2;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        dragTarget = DragNone;

    if (dragTarget != DragNone && canvasActive) {
        const ImVec2 normalized = toNormalized(mouse);
        constexpr float kMinGap = 0.02f;

        if (dragTarget == DragP1) {
            settings.piecewiseR1 = std::min(normalized.x, settings.piecewiseR2 - kMinGap);
            settings.piecewiseS1 = normalized.y;
        } else {
            settings.piecewiseR2 = std::max(normalized.x, settings.piecewiseR1 + kMinGap);
            settings.piecewiseS2 = normalized.y;
        }
    }

    const bool p1Hovered = distSq(mouse, p1) <= (handleRadius + 6.0f) * (handleRadius + 6.0f);
    const bool p2Hovered = distSq(mouse, p2) <= (handleRadius + 6.0f) * (handleRadius + 6.0f);
    const ImU32 handleFill = IM_COL32(72, 145, 220, 255);
    const ImU32 handleOutline = IM_COL32(230, 230, 235, 255);

    draw->AddCircleFilled(p1, handleRadius + (p1Hovered || dragTarget == DragP1 ? 2.0f : 0.0f), handleFill);
    draw->AddCircle(p1, handleRadius + (p1Hovered || dragTarget == DragP1 ? 2.0f : 0.0f), handleOutline, 0, 1.5f);
    draw->AddCircleFilled(p2, handleRadius + (p2Hovered || dragTarget == DragP2 ? 2.0f : 0.0f), handleFill);
    draw->AddCircle(p2, handleRadius + (p2Hovered || dragTarget == DragP2 ? 2.0f : 0.0f), handleOutline, 0, 1.5f);

    draw->AddText(ImVec2(canvasMin.x + 4.0f, canvasMax.y - 16.0f), IM_COL32(130, 130, 138, 255), "Input");
    draw->AddText(ImVec2(canvasMin.x + 4.0f, canvasMin.y + 2.0f), IM_COL32(130, 130, 138, 255), "Output");

    if (canvasHovered && dragTarget == DragNone) {
        ImGui::BeginTooltip();
        ImGui::Text("Click and drag P1 or P2 to edit the curve.");
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    ImGui::TextUnformatted("Control Point 1");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.48f);
    ImGui::DragFloat("##R1", &settings.piecewiseR1, 0.005f, 0.0f, 1.0f, "r1 = %.3f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::DragFloat("##S1", &settings.piecewiseS1, 0.005f, 0.0f, 1.0f, "s1 = %.3f");

    ImGui::TextUnformatted("Control Point 2");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.48f);
    ImGui::DragFloat("##R2", &settings.piecewiseR2, 0.005f, 0.0f, 1.0f, "r2 = %.3f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::DragFloat("##S2", &settings.piecewiseS2, 0.005f, 0.0f, 1.0f, "s2 = %.3f");

    constexpr float kMinGap = 0.02f;
    settings.piecewiseR1 = clamp01(settings.piecewiseR1);
    settings.piecewiseS1 = clamp01(settings.piecewiseS1);
    settings.piecewiseR2 = clamp01(settings.piecewiseR2);
    settings.piecewiseS2 = clamp01(settings.piecewiseS2);
    settings.piecewiseR1 = std::min(settings.piecewiseR1, settings.piecewiseR2 - kMinGap);
    settings.piecewiseR2 = std::max(settings.piecewiseR2, settings.piecewiseR1 + kMinGap);

    const bool valid = settings.piecewiseR1 < settings.piecewiseR2;
    if (!valid)
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "r1 must be less than r2.");
    else
        ImGui::TextDisabled("Maps input intensity through three linear segments.");

    return valid;
}

void drawThresholdPreview(float threshold) {
    const ImVec2 barSize(-1.0f, 18.0f);
    ImGui::InvisibleButton("##threshold_preview", barSize);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImU32 black = IM_COL32(20, 20, 24, 255);
    const ImU32 white = IM_COL32(235, 235, 240, 255);
    const float split = min.x + threshold * (max.x - min.x);
    draw->AddRectFilled(min, ImVec2(split, max.y), black, 3.0f, ImDrawFlags_RoundCornersLeft);
    draw->AddRectFilled(ImVec2(split, min.y), max, white, 3.0f, ImDrawFlags_RoundCornersRight);
    draw->AddRect(min, max, IM_COL32(55, 55, 62, 255), 3.0f);
}

void drawGammaPreview(float gamma) {
    const ImVec2 graphSize(-1.0f, 64.0f);
    ImGui::InvisibleButton("##gamma_preview", graphSize);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    draw->AddRectFilled(min, max, IM_COL32(24, 24, 28, 255), 3.0f);
    draw->AddLine(min, max, IM_COL32(55, 55, 62, 255));

    const float safeGamma = std::max(gamma, 0.05f);
    ImVec2 prev = min;
    for (int i = 1; i <= 32; ++i) {
        const float t = (float)i / 32.0f;
        const float y = std::pow(t, 1.0f / safeGamma);
        const ImVec2 pt(min.x + t * (max.x - min.x), min.y - y * (min.y - max.y));
        draw->AddLine(prev, pt, IM_COL32(72, 145, 220, 255), 2.0f);
        prev = pt;
    }
}

static bool gTriggerChromaBgDialog = false;

void drawFilterParameters(FilterSettings& settings) {
    switch (settings.option) {
        case Filter_Negative:
            ImGui::TextWrapped(
                "Inverts each pixel channel. No parameters are required."
            );
            break;

        case Filter_Brightness:
            sectionHeader("Brightness Offset");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderInt("##Brightness", &settings.brightness, 0, 255, "%d");
            ImGui::TextDisabled("Adds this value to every pixel channel.");
            break;

        case Filter_Threshold:
            sectionHeader("Threshold");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##Threshold", &settings.threshold, 0.0f, 1.0f, "%.2f");
            drawThresholdPreview(settings.threshold);
            ImGui::TextDisabled("Pixels above the threshold become white; others become black.");
            break;

        case Filter_GammaCorrection:
            sectionHeader("Gamma");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##Gamma", &settings.gamma, 0.05f, 5.0f, "%.2f");
            drawGammaPreview(settings.gamma);
            ImGui::TextDisabled("Gamma < 1 brightens mid-tones; gamma > 1 darkens them.");
            break;

        case Filter_PiecewiseLinear:
            drawPiecewiseCurveEditor(settings);
            break;

        case Filter_Convolution: {
            sectionHeader("Kernel Size");
            int prevSize = settings.kernelSize;
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputInt("##KernelSize", &settings.kernelSize, 2, 2)) {
                if (settings.kernelSize < 1) settings.kernelSize = 1;
                if (settings.kernelSize % 2 == 0) settings.kernelSize += 1;
                if (settings.kernelSize > 9) settings.kernelSize = 9;
                if (settings.kernelSize != prevSize)
                    resetKernelToIdentity(settings.kernel, settings.kernelSize);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Odd sizes only (3, 5, 7, 9)");

            sectionHeader("Presets");
            static int presetIndex = 0;
            static const char* presetNames[] = {
                "Identity", "Box Blur", "Sharpen", "Edge Detect", "Emboss"
            };
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##Preset", &presetIndex, presetNames, IM_ARRAYSIZE(presetNames)))
                applyKernelPreset(settings.kernel, settings.kernelSize, presetIndex);

            sectionHeader("Kernel Values");
            const float cellWidth = 52.0f;
            for (int row = 0; row < settings.kernelSize; ++row) {
                for (int col = 0; col < settings.kernelSize; ++col) {
                    const int idx = row * settings.kernelSize + col;
                    ImGui::PushID(idx);
                    ImGui::SetNextItemWidth(cellWidth);
                    ImGui::InputFloat("##Cell", &settings.kernel[idx], 0.0f, 0.0f, "%.2f");
                    ImGui::PopID();
                    if (col + 1 < settings.kernelSize) ImGui::SameLine();
                }
            }

            ImGui::Spacing();
            ImGui::Checkbox("Normalize (divide by weight sum)", &settings.normalizeKernel);

            float sum = 0.0f;
            for (float v : settings.kernel) sum += v;
            ImGui::TextDisabled("Weight sum: %.2f", sum);
            break;
        }
        case Filter_Scale: {
            sectionHeader("Resize");

            int currentWidth  = Engine::getImageWidth();
            int currentHeight = Engine::getImageHeight();

            ImGui::TextDisabled("Resolution");

            ImGui::SetNextItemWidth(140.0f);
            static int newWidth = currentWidth;
            ImGui::InputInt("Width", &newWidth);

            ImGui::SetNextItemWidth(140.0f);
            static int newHeight = currentHeight;
            ImGui::InputInt("Height", &newHeight);

            settings.scale_factor_x = (float) (newWidth  / (float) currentWidth);
            settings.scale_factor_y = (float) (newHeight / (float) currentHeight);

            if (settings.scale_factor_x <= 0.0f)
                settings.scale_factor_x = 0.01f;

            if (settings.scale_factor_y <= 0.0f)
                settings.scale_factor_y = 0.01f;

            newWidth  = std::max(1, newWidth);
            newHeight = std::max(1, newHeight);

            ImGui::Spacing();

            ImGui::Text(
                "Current: %d x %d",
                currentWidth,
                currentHeight
            );

            ImGui::Text(
                "Result: %d x %d",
                newWidth,
                newHeight
            );

            sectionHeader("Interpolation");
            static const char* resizeInterpNames[] = { "Nearest", "Bilinear" };
            int resizeInterp = (int)settings.interpolation;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##ResizeInterp", &resizeInterp, resizeInterpNames, IM_ARRAYSIZE(resizeInterpNames)))
                settings.interpolation = (uint32_t)resizeInterp;
            ImGui::TextDisabled("Nearest keeps hard edges; bilinear blends neighboring pixels.");

            break;
        }
        case Filter_Rotate: {
            sectionHeader("Rotation");

            int currentWidth  = Engine::getImageWidth();
            int currentHeight = Engine::getImageHeight();

            static bool pivotInitialized = false;
            if (currentWidth > 0 && !pivotInitialized) {
                settings.pivot_x = Engine::getDrawX() + Engine::getDrawWidth()  * 0.5f;
                settings.pivot_y = Engine::getDrawY() + Engine::getDrawHeight() * 0.5f;
                pivotInitialized = true;
            }

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##Angle", &settings.rotation_angle, -180.0f, 180.0f, "%.1f deg");
            ImGui::TextDisabled("Positive angles rotate counter-clockwise.");

            sectionHeader("Pivot (pixels)");
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputFloat("X", &settings.pivot_x, 1.0f, 10.0f, "%.1f");
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputFloat("Y", &settings.pivot_y, 1.0f, 10.0f, "%.1f");

            if (ImGui::Button("Center")) {
                settings.pivot_x = Engine::getDrawX() + Engine::getDrawWidth()  * 0.5f;
                settings.pivot_y = Engine::getDrawY() + Engine::getDrawHeight() * 0.5f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Top-left")) {
                settings.pivot_x = (float)Engine::getDrawX();
                settings.pivot_y = (float)Engine::getDrawY();
            }

            ImGui::TextDisabled(
                "Drawing area: %d x %d",
                Engine::getDrawWidth(),
                Engine::getDrawHeight()
            );
            ImGui::TextDisabled(
                "Working area: %d x %d",
                Engine::getWorkWidth(),
                Engine::getWorkHeight()
            );
            ImGui::TextDisabled("Drag the pivot on the preview, or type coordinates.");

            sectionHeader("Interpolation");
            static const char* rotateInterpNames[] = { "Nearest", "Bilinear" };
            int rotateInterp = (int)settings.interpolation;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##RotateInterp", &rotateInterp, rotateInterpNames, IM_ARRAYSIZE(rotateInterpNames)))
                settings.interpolation = (uint32_t)rotateInterp;
            ImGui::TextDisabled("Nearest keeps hard edges; bilinear blends neighboring pixels.");

            break;
        }
        case Filter_Fourier: {
            sectionHeader("Fourier Spectrum");
            ImGui::TextWrapped(
                "Computes a 2D FFT of the working area, shows the shifted log-magnitude spectrum, "
                "and reconstructs the image with the inverse FFT after you paint a frequency mask."
            );

            if (Engine::getCurrImage() != 0 && !Engine::hasFourierSpectrum())
                Engine::computeFourierSpectrum();

            ImGui::Spacing();
            if (ImGui::Button("Recompute Spectrum", ImVec2(-1.0f, 0.0f)))
                Engine::computeFourierSpectrum();
            if (ImGui::Button("Reset Mask", ImVec2(-1.0f, 0.0f)))
                Engine::resetFourierMask();

            sectionHeader("Brush");
            static const char* brushModes[] = { "Black", "White", "Soft gray" };
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::Combo("##FftBrush", &gFourierBrushMode, brushModes, IM_ARRAYSIZE(brushModes));

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##FftRadius", &gFourierBrushRadius, 1.0f, 80.0f, "Radius: %.1f px");

            if (gFourierBrushMode == 2) {
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SliderFloat("##FftGray", &gFourierGrayValue, 0.0f, 1.0f, "Gray: %.2f");
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SliderFloat("##FftHard", &gFourierHardness, 0.0f, 1.0f, "Hardness: %.2f");
                ImGui::TextDisabled("Soft brush paints a Gaussian grayscale mask.");
            } else {
                ImGui::TextDisabled(
                    gFourierBrushMode == 0
                        ? "Black removes those frequencies (notch / low-pass style)."
                        : "White restores those frequencies (mask = 1)."
                );
            }

            if (Engine::hasFourierSpectrum()) {
                ImGui::TextDisabled(
                    "Spectrum: %d x %d (padded to powers of two)",
                    Engine::getSpectrumWidth(),
                    Engine::getSpectrumHeight()
                );
            }

            ImGui::TextDisabled("Paint on the spectrum in the preview. Then apply the inverse FFT.");
            break;
        }
        case Filter_HistogramEqualization: {
            sectionHeader("Histogram & Equalization");
            ImGui::TextWrapped("Displays the intensity distribution of pixels in the current image and allows histogram equalization.");
        
            if (Engine::getCurrImage() != 0) {
                int width = Engine::getImageWidth();
                int height = Engine::getImageHeight();
                std::vector<unsigned char> pixels((size_t)width * height * 4);
        
                glBindTexture(GL_TEXTURE_2D, Engine::getCurrImage());
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                glBindTexture(GL_TEXTURE_2D, 0);
        
                int histR[256] = {0}, histG[256] = {0}, histB[256] = {0}, histGray[256] = {0};
                Engine::computeHistogram(pixels, width, height, histR, histG, histB, histGray);
        
                float histR_f[256], histG_f[256], histB_f[256], histGray_f[256];
                for (int i = 0; i < 256; ++i) {
                    histR_f[i]    = static_cast<float>(histR[i]);
                    histG_f[i]    = static_cast<float>(histG[i]);
                    histB_f[i]    = static_cast<float>(histB[i]);
                    histGray_f[i] = static_cast<float>(histGray[i]);
                }
        
                static int selectedChannel = 0;
                ImGui::RadioButton("Gray", &selectedChannel, 0);  ImGui::SameLine();
                ImGui::RadioButton("R", &selectedChannel, 1);     ImGui::SameLine();
                ImGui::RadioButton("G", &selectedChannel, 2);     ImGui::SameLine();
                ImGui::RadioButton("B", &selectedChannel, 3);
        
                float* currentHist = histGray_f;
                if (selectedChannel == 1) currentHist = histR_f;
                else if (selectedChannel == 2) currentHist = histG_f;
                else if (selectedChannel == 3) currentHist = histB_f;
        
                ImGui::PlotHistogram("##HistogramPlot", currentHist, 256, 0, nullptr, 0.0f, FLT_MAX, ImVec2(-1.0f, 120.0f));
            }
            break;
        }
        case Filter_Grayscale: {
            sectionHeader("Grayscale Algorithm");
            static const char* grayMethods[] = { "Simple Average (R+G+B)/3", "Weighted Average (Luminance)" };
            int currentMethod = (int)settings.grayMethod;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##GrayMethod", &currentMethod, grayMethods, IM_ARRAYSIZE(grayMethods)))
                settings.grayMethod = (uint32_t)currentMethod;

            ImGui::TextDisabled(
                settings.grayMethod == 0
                    ? "Calculates the arithmetic mean of RGB channels."
                    : "Weights channels based on human visual perception (0.299R + 0.587G + 0.114B)."
            );
            break;
        }

        case Filter_HSI_Adjust: {
            sectionHeader("Hue / Saturation / Intensity (HSI)");
    
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##Hue", &settings.hueShift, -180.0f, 180.0f, "Hue Shift: %.1f deg");
        
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##Sat", &settings.saturationScale, 0.0f, 3.0f, "Saturation (S): %.2fx");
        
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##Intensity", &settings.intensityScale, 0.0f, 3.0f, "Intensity (I): %.2fx");
            
            ImGui::TextDisabled("Uses pure HSI color space where Intensity I = (R + G + B) / 3.");
            break;
        }

        case Filter_ColorBalance: {
            sectionHeader("Color Balance (Channel Adjustment)");
            ImGui::TextDisabled("Cyan / Red");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##CyanRed", &settings.cyanRed, -1.0f, 1.0f, "Cyan <-> Red: %.2f");

            ImGui::TextDisabled("Magenta / Green");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##MagGreen", &settings.magentaGreen, -1.0f, 1.0f, "Magenta <-> Green: %.2f");

            ImGui::TextDisabled("Yellow / Blue");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##YelBlue", &settings.yellowBlue, -1.0f, 1.0f, "Yellow <-> Blue: %.2f");
            break;
        }

        case Filter_Sepia: {
            sectionHeader("Sepia Tone");
            ImGui::TextWrapped("Applies a classic yellowish/warm grayscale transformation to give an aged photograph look.");
            break;
        }

        case Filter_ChromaKey: {
            sectionHeader("Chroma-Key Settings");
            ImGui::ColorEdit3("Key Color", glm::value_ptr(settings.keyColor));

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##Tolerance", &settings.chromaDistance, 0.01f, 1.0f, "Distance / Tolerance: %.2f");

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##Smoothness", &settings.chromaSmoothness, 0.0f, 0.5f, "Edge Smoothness: %.2f");

            ImGui::Spacing();
            if (ImGui::Button("Load Background Image...", ImVec2(-1.0f, 0.0f))) {
                gTriggerChromaBgDialog = true;
            }

            if (Engine::getChromaBgImage() != 0) {
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.4f, 1.0f), "Background image loaded.");
            } else {
                ImGui::TextDisabled("No background loaded (key region will become black/transparent).");
            }
            break;
        }
    }
}

void computeFittedImageRect(
    const ImVec2& boxMin,
    const ImVec2& boxSize,
    int texW,
    int texH,
    ImVec2& imgMin,
    ImVec2& imgSize
) {
    if (texW <= 0 || texH <= 0) {
        imgMin = boxMin;
        imgSize = boxSize;
        return;
    }

    const float boxAspect = boxSize.x / std::max(boxSize.y, 1.0f);
    const float texAspect = (float)texW / (float)texH;

    if (texAspect > boxAspect) {
        imgSize.x = boxSize.x;
        imgSize.y = boxSize.x / texAspect;
    } else {
        imgSize.y = boxSize.y;
        imgSize.x = boxSize.y * texAspect;
    }

    imgMin.x = boxMin.x + (boxSize.x - imgSize.x) * 0.5f;
    imgMin.y = boxMin.y + (boxSize.y - imgSize.y) * 0.5f;
}

ImVec2 canvasToScreen(const ImVec2& imageMin, const ImVec2& imageSize, float canvasX, float canvasY) {
    const float canvasW = (float)std::max(1, Engine::getImageWidth());
    const float canvasH = (float)std::max(1, Engine::getImageHeight());
    return ImVec2(
        imageMin.x + (canvasX / canvasW) * imageSize.x,
        imageMin.y + (canvasY / canvasH) * imageSize.y
    );
}

ImVec2 screenToCanvas(const ImVec2& imageMin, const ImVec2& imageSize, const ImVec2& screen) {
    const float canvasW = (float)std::max(1, Engine::getImageWidth());
    const float canvasH = (float)std::max(1, Engine::getImageHeight());
    const float nx = imageSize.x > 0.0f ? (screen.x - imageMin.x) / imageSize.x : 0.0f;
    const float ny = imageSize.y > 0.0f ? (screen.y - imageMin.y) / imageSize.y : 0.0f;
    return ImVec2(nx * canvasW, ny * canvasH);
}

void drawWorkingAreaOverlay(ImDrawList* draw, const ImVec2& imageMin, const ImVec2& imageSize) {
    if (Engine::getCurrImage() == 0)
        return;

    const ImVec2 workMin = canvasToScreen(
        imageMin,
        imageSize,
        (float)Engine::getWorkX(),
        (float)Engine::getWorkY()
    );
    const ImVec2 workMax = canvasToScreen(
        imageMin,
        imageSize,
        (float)(Engine::getWorkX() + Engine::getWorkWidth()),
        (float)(Engine::getWorkY() + Engine::getWorkHeight())
    );

    // Drawing area (the exported region)
    const ImVec2 drawMin = canvasToScreen(
        imageMin,
        imageSize,
        (float)Engine::getDrawX(),
        (float)Engine::getDrawY()
    );
    const ImVec2 drawMax = canvasToScreen(
        imageMin,
        imageSize,
        (float)(Engine::getDrawX() + Engine::getDrawWidth()),
        (float)(Engine::getDrawY() + Engine::getDrawHeight())
    );

    const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
    const ImU32 dim = IM_COL32(0, 0, 0, 90);

    // Dim everything outside the drawing area (out-of-bounds pixels)
    if (drawMin.x > imageMin.x)
        draw->AddRectFilled(imageMin, ImVec2(drawMin.x, imageMax.y), dim);
    if (drawMax.x < imageMax.x)
        draw->AddRectFilled(ImVec2(drawMax.x, imageMin.y), imageMax, dim);
    if (drawMin.y > imageMin.y)
        draw->AddRectFilled(ImVec2(drawMin.x, imageMin.y), ImVec2(drawMax.x, drawMin.y), dim);
    if (drawMax.y < imageMax.y)
        draw->AddRectFilled(ImVec2(drawMin.x, drawMax.y), ImVec2(drawMax.x, imageMax.y), dim);

    // Drawing area outline
    draw->AddRect(drawMin, drawMax, IM_COL32(72, 145, 220, 255), 0.0f, 0, 2.0f);

    // Working area outline
    draw->AddRect(workMin, workMax, IM_COL32(70, 220, 110, 255), 0.0f, 0, 2.0f);

    // Labels
    char buf[64];
    snprintf(buf, sizeof(buf), "Drawing %d x %d", Engine::getDrawWidth(), Engine::getDrawHeight());
    draw->AddText(ImVec2(drawMin.x + 4.0f, drawMin.y + 2.0f), IM_COL32(72, 145, 220, 255), buf);

    snprintf(buf, sizeof(buf), "Working %d x %d", Engine::getWorkWidth(), Engine::getWorkHeight());
    const ImVec2 labelSize = ImGui::CalcTextSize(buf);
    float labelY = workMax.y - labelSize.y - 2.0f;
    if (labelY < workMin.y) labelY = workMin.y + 2.0f;
    draw->AddText(ImVec2(workMin.x + 4.0f, labelY), IM_COL32(70, 220, 110, 255), buf);
}

void drawPivotHandle(ImDrawList* draw, const ImVec2& imageMin, const ImVec2& imageSize, const FilterSettings& settings) {
    const ImVec2 pivot = canvasToScreen(imageMin, imageSize, settings.pivot_x, settings.pivot_y);
    const ImU32 color = gPivotDragging
        ? IM_COL32(255, 220, 80, 255)
        : IM_COL32(255, 110, 70, 255);

    draw->AddLine(ImVec2(pivot.x - 14.0f, pivot.y), ImVec2(pivot.x + 14.0f, pivot.y), color, 2.0f);
    draw->AddLine(ImVec2(pivot.x, pivot.y - 14.0f), ImVec2(pivot.x, pivot.y + 14.0f), color, 2.0f);
    draw->AddCircleFilled(pivot, 4.5f, color);
    draw->AddCircle(pivot, 9.0f, color, 16, 2.0f);
}

void drawPreviewImageFrame(
    ImDrawList* draw,
    GLuint texture,
    const ImVec2& size,
    ImVec2* outMin = nullptr,
    ImVec2* outMax = nullptr
) {
    const ImVec2 boxMin = ImGui::GetCursorScreenPos();
    const ImVec2 boxMax(boxMin.x + size.x, boxMin.y + size.y);

    draw->AddRectFilled(boxMin, boxMax, IM_COL32(18, 18, 21, 255), 4.0f);
    draw->AddRect(boxMin, boxMax, IM_COL32(55, 55, 62, 255), 4.0f);

    int texW = 0;
    int texH = 0;
    if (texture != 0) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    ImVec2 imageMin = boxMin;
    ImVec2 imageSize = size;
    computeFittedImageRect(boxMin, size, texW, texH, imageMin, imageSize);
    const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);

    if (texture != 0) {
        // Checkerboard only behind the drawing area so its extent is visible
        drawCheckerboard(draw, imageMin, imageMax, 12.0f);
        draw->AddImage((ImTextureID)(intptr_t)texture, imageMin, imageMax);
    } else {
        const char* text = "No Image Loaded";
        ImVec2 textSize = ImGui::CalcTextSize(text);
        draw->AddText(
            ImVec2(boxMin.x + (size.x - textSize.x) * 0.5f, boxMin.y + (size.y - textSize.y) * 0.5f),
            IM_COL32(150, 150, 160, 255),
            text
        );
    }

    ImGui::Dummy(size);

    if (outMin) *outMin = imageMin;
    if (outMax) *outMax = imageMax;
}

void drawPreviewImage(
    ImDrawList* draw,
    GLuint texture,
    const char* label,
    const ImVec2& size
) {
    ImGui::TextDisabled("%s", label);
    drawPreviewImageFrame(draw, texture, size);
}

ImVec2 clampScreenPointToImage(const ImVec2& point, const ImVec2& imageMin, const ImVec2& imageSize) {
    const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
    return ImVec2(
        std::max(imageMin.x, std::min(point.x, imageMax.x)),
        std::max(imageMin.y, std::min(point.y, imageMax.y))
    );
}

void updateSelectionScreenRect(
    ImageRegionSelection& selection,
    const ImVec2& imageMin,
    const ImVec2& imageSize
) {
    const ImVec2 anchor = clampScreenPointToImage(selection.dragStartScreen, imageMin, imageSize);
    const ImVec2 corner = clampScreenPointToImage(selection.dragCurrentScreen, imageMin, imageSize);

    selection.screenMin = ImVec2(
        std::min(anchor.x, corner.x),
        std::min(anchor.y, corner.y)
    );
    selection.screenMax = ImVec2(
        std::max(anchor.x, corner.x),
        std::max(anchor.y, corner.y)
    );
}

int pixelIndexFromScreenCoord(
    float screenCoord,
    float imageOrigin,
    float imageSpan,
    int imageSize
) {
    if (imageSize <= 0) return 0;
    if (imageSpan <= 0.0f) return 0;

    const float normalized = clamp01((screenCoord - imageOrigin) / imageSpan);
    if (normalized >= 1.0f)
        return imageSize - 1;

    const int pixel = (int)std::floor(normalized * (float)imageSize);
    return std::max(0, std::min(imageSize - 1, pixel));
}

void updateSelectionPixelCorners(
    ImageRegionSelection& selection,
    const ImVec2& imageMin,
    const ImVec2& imageSize,
    int imageWidth,
    int imageHeight
) {
    selection.x0 = pixelIndexFromScreenCoord(
        selection.screenMin.x, imageMin.x, imageSize.x, imageWidth
    );
    selection.y0 = pixelIndexFromScreenCoord(
        selection.screenMin.y, imageMin.y, imageSize.y, imageHeight
    );
    selection.x1 = pixelIndexFromScreenCoord(
        selection.screenMax.x, imageMin.x, imageSize.x, imageWidth
    );
    selection.y1 = pixelIndexFromScreenCoord(
        selection.screenMax.y, imageMin.y, imageSize.y, imageHeight
    );

    if (selection.x0 > selection.x1) std::swap(selection.x0, selection.x1);
    if (selection.y0 > selection.y1) std::swap(selection.y0, selection.y1);
}

bool selectionMeetsMinimumSize(const ImageRegionSelection& selection) {
    const float width = selection.screenMax.x - selection.screenMin.x;
    const float height = selection.screenMax.y - selection.screenMin.y;
    return width >= 2.0f && height >= 2.0f;
}

void drawRegionSelectionOverlay(
    ImDrawList* draw,
    const ImageRegionSelection& selection
) {
    if (!selection.hasSelection && !selection.dragging)
        return;

    const ImU32 fillColor = selection.dragging
        ? IM_COL32(255, 210, 80, 55)
        : IM_COL32(72, 145, 220, 70);
    const ImU32 borderColor = selection.dragging
        ? IM_COL32(255, 210, 80, 255)
        : IM_COL32(72, 145, 220, 255);

    draw->AddRectFilled(selection.screenMin, selection.screenMax, fillColor);
    draw->AddRect(selection.screenMin, selection.screenMax, borderColor, 0.0f, 0, 2.0f);
}

void handlePivotDrag(const ImVec2& imageMin, const ImVec2& imageSize) {
    const ImVec2 pivotScreen = canvasToScreen(
        imageMin,
        imageSize,
        gFilterSettings.pivot_x,
        gFilterSettings.pivot_y
    );
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float dx = mouse.x - pivotScreen.x;
    const float dy = mouse.y - pivotScreen.y;
    const bool overPivot = (dx * dx + dy * dy) <= (12.0f * 12.0f);

    if (overPivot || gPivotDragging)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

    if (overPivot && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        gPivotDragging = true;

    if (gPivotDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 canvas = screenToCanvas(imageMin, imageSize, mouse);
        canvas.x = std::max(0.0f, std::min(canvas.x, (float)Engine::getImageWidth()));
        canvas.y = std::max(0.0f, std::min(canvas.y, (float)Engine::getImageHeight()));
        gFilterSettings.pivot_x = canvas.x;
        gFilterSettings.pivot_y = canvas.y;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        gPivotDragging = false;
}

void handleRegionSelectionInput(
    const ImVec2& imageMin,
    const ImVec2& imageSize,
    int imageWidth,
    int imageHeight
) {
    ImGui::SetCursorScreenPos(imageMin);
    ImGui::InvisibleButton("##edited_region_select", imageSize);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        gRegionSelection.clear();

    if (gFilterSettings.option == Filter_Rotate) {
        handlePivotDrag(imageMin, imageSize);
        return;
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        gRegionSelection.dragging = true;
        gRegionSelection.hasSelection = false;
        gRegionSelection.dragStartScreen = ImGui::GetMousePos();
        gRegionSelection.dragCurrentScreen = gRegionSelection.dragStartScreen;
        updateSelectionScreenRect(gRegionSelection, imageMin, imageSize);
    }

    if (gRegionSelection.dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        gRegionSelection.dragCurrentScreen = ImGui::GetMousePos();
        updateSelectionScreenRect(gRegionSelection, imageMin, imageSize);
    }

    if (gRegionSelection.dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        gRegionSelection.dragging = false;
        updateSelectionScreenRect(gRegionSelection, imageMin, imageSize);

        if (selectionMeetsMinimumSize(gRegionSelection)) {
            gRegionSelection.hasSelection = true;
            updateSelectionPixelCorners(gRegionSelection, imageMin, imageSize, imageWidth, imageHeight);
        }
    }
}

void paintSpectrumStroke(const ImVec2& fromCanvas, const ImVec2& toCanvas) {
    float value = 0.0f;
    float hardness = 1.0f;
    if (gFourierBrushMode == 1) {
        value = 1.0f;
        hardness = 1.0f;
    } else if (gFourierBrushMode == 2) {
        value = gFourierGrayValue;
        hardness = gFourierHardness;
    }

    const float dx = toCanvas.x - fromCanvas.x;
    const float dy = toCanvas.y - fromCanvas.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const int steps = std::max(1, (int)std::ceil(dist));

    for (int i = 0; i <= steps; ++i) {
        const float t = (float)i / (float)steps;
        Engine::paintFourierBrush(
            fromCanvas.x + dx * t,
            fromCanvas.y + dy * t,
            gFourierBrushRadius,
            value,
            hardness
        );
    }
}

ImVec2 spectrumScreenToCanvas(const ImVec2& imageMin, const ImVec2& imageSize, const ImVec2& screen) {
    const float w = (float)std::max(1, Engine::getSpectrumWidth());
    const float h = (float)std::max(1, Engine::getSpectrumHeight());
    const float nx = imageSize.x > 0.0f ? (screen.x - imageMin.x) / imageSize.x : 0.0f;
    const float ny = imageSize.y > 0.0f ? (screen.y - imageMin.y) / imageSize.y : 0.0f;
    return ImVec2(
        std::max(0.0f, std::min(nx * w, w)),
        std::max(0.0f, std::min(ny * h, h))
    );
}

void handleSpectrumPaint(const ImVec2& imageMin, const ImVec2& imageSize) {
    ImGui::SetCursorScreenPos(imageMin);
    ImGui::InvisibleButton("##spectrum_paint", imageSize);

    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mouse = ImGui::GetIO().MousePos;

    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        gFourierPainting = true;
        gLastSpectrumPaint = spectrumScreenToCanvas(imageMin, imageSize, mouse);
        gLastSpectrumPaintValid = true;
        paintSpectrumStroke(gLastSpectrumPaint, gLastSpectrumPaint);
    }

    if (gFourierPainting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 cur = spectrumScreenToCanvas(imageMin, imageSize, mouse);
        if (gLastSpectrumPaintValid)
            paintSpectrumStroke(gLastSpectrumPaint, cur);
        else
            paintSpectrumStroke(cur, cur);
        gLastSpectrumPaint = cur;
        gLastSpectrumPaintValid = true;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        gFourierPainting = false;
        gLastSpectrumPaintValid = false;
    }
}

void drawSpectrumEditor(
    ImDrawList* draw,
    const ImVec2& size
) {
    ImGui::TextDisabled("Shifted spectrum (DC at center)");

    if (Engine::getCurrImage() != 0 && !Engine::hasFourierSpectrum())
        Engine::computeFourierSpectrum();

    ImVec2 imageMin;
    ImVec2 imageMax;
    drawPreviewImageFrame(draw, Engine::getSpectrumTexture(), size, &imageMin, &imageMax);

    if (Engine::hasFourierSpectrum()) {
        const ImVec2 fittedSize(imageMax.x - imageMin.x, imageMax.y - imageMin.y);
        handleSpectrumPaint(imageMin, fittedSize);

        const ImVec2 mouse = ImGui::GetIO().MousePos;
        if (mouse.x >= imageMin.x && mouse.y >= imageMin.y &&
            mouse.x <= imageMax.x && mouse.y <= imageMax.y &&
            fittedSize.x > 0.0f && fittedSize.y > 0.0f) {
            const float scale = fittedSize.x / (float)std::max(1, Engine::getSpectrumWidth());
            const float r = gFourierBrushRadius * scale;
            const ImU32 col = gFourierBrushMode == 0
                ? IM_COL32(20, 20, 20, 200)
                : (gFourierBrushMode == 1 ? IM_COL32(240, 240, 240, 200) : IM_COL32(180, 180, 180, 200));
            draw->AddCircle(mouse, r, col, 32, 1.5f);
        }
    }
}

void drawEditedImageWithRegionSelection(
    ImDrawList* draw,
    const ImVec2& size
) {
    ImGui::TextDisabled("Edited");

    ImVec2 imageMin;
    ImVec2 imageMax;
    drawPreviewImageFrame(draw, Engine::getCurrImage(), size, &imageMin, &imageMax);

    const int imageWidth = Engine::getImageWidth();
    const int imageHeight = Engine::getImageHeight();

    if (Engine::getCurrImage() != 0) {
        const ImVec2 fittedSize(imageMax.x - imageMin.x, imageMax.y - imageMin.y);
        handleRegionSelectionInput(imageMin, fittedSize, imageWidth, imageHeight);
        drawWorkingAreaOverlay(draw, imageMin, fittedSize);
        if (gFilterSettings.option != Filter_Rotate)
            drawRegionSelectionOverlay(draw, gRegionSelection);
        if (gFilterSettings.option == Filter_Rotate)
            drawPivotHandle(draw, imageMin, fittedSize, gFilterSettings);
    }
}

void drawRegionSelectionInfo() {
    if (!gRegionSelection.hasSelection)
        return;
}

bool gSideBySideComparison = false;

void drawLoadImageModal() {
    if (gTriggerLoadDialog) {
        ImGui::OpenPopup("Load Image");
        gTriggerLoadDialog = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(550, 420), ImGuiCond_Appearing);

    bool isOpen = true;
    if (ImGui::BeginPopupModal("Load Image", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        
        ImGui::TextDisabled("Path:");
        ImGui::SameLine();
        std::string pathStr = gCurrentBrowserPath.string();
        char pathBuf[512];
        strncpy(pathBuf, pathStr.c_str(), sizeof(pathBuf) - 1);
        pathBuf[sizeof(pathBuf) - 1] = '\0';
        
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##path", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            fs::path newPath(pathBuf);
            std::error_code ec;
            if (fs::exists(newPath, ec) && fs::is_directory(newPath, ec)) {
                gCurrentBrowserPath = newPath;
                gSelectedBrowserFile = "";
            }
        }

        if (ImGui::Button("Go Up", ImVec2(60, 0))) {
            if (gCurrentBrowserPath.has_parent_path()) {
                gCurrentBrowserPath = gCurrentBrowserPath.parent_path();
                gSelectedBrowserFile = "";
            }
        }

        ImGui::Spacing();
        ImGui::BeginChild("##browser_list", ImVec2(0, -42.0f), true);

        struct FileEntry {
            std::string name;
            bool isDir;
        };
        std::vector<FileEntry> entries;
        std::error_code ec;
        
        for (const auto& entry : fs::directory_iterator(gCurrentBrowserPath, fs::directory_options::skip_permission_denied, ec)) {
            entries.push_back({ entry.path().filename().string(), entry.is_directory(ec) });
        }
        
        std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
            if (a.isDir != b.isDir) return a.isDir > b.isDir;
            return a.name < b.name;
        });

        for (const auto& entry : entries) {
            if (entry.isDir) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.68f, 0.95f, 1.0f));
                bool selected = ImGui::Selectable((std::string("[Dir]  ") + entry.name).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
                ImGui::PopStyleColor();

                if (selected && ImGui::IsMouseDoubleClicked(0)) {
                    gCurrentBrowserPath /= entry.name;
                    gSelectedBrowserFile = "";
                    break;
                }
            } else {
                bool isSelected = (gSelectedBrowserFile == entry.name);
                if (ImGui::Selectable((std::string("       ") + entry.name).c_str(), isSelected)) {
                    gSelectedBrowserFile = entry.name;
                }
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        bool hasSelection = !gSelectedBrowserFile.empty();

        ImGui::PushStyleColor(ImGuiCol_Button, accentColor());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.34f, 0.60f, 0.90f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.48f, 0.78f, 1.0f));
        ImGui::BeginDisabled(!hasSelection);
        
        if (ImGui::Button("Open", ImVec2(80, 0))) {
            fs::path fullPath = gCurrentBrowserPath / gSelectedBrowserFile;
            Engine::loadImage(fullPath.string().c_str(), 4);
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndDisabled();
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

const char* exportExtension(int format) {
    switch (format) {
        case Engine::Export_JPEG: return ".jpg";
        case Engine::Export_BMP:  return ".bmp";
        case Engine::Export_TGA:  return ".tga";
        default:                  return ".png";
    }
}

void applyExportExtension() {
    std::string name = gExportFilename;
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos)
        name = name.substr(0, dot);
    if (name.empty())
        name = "export";
    name += exportExtension(gExportFormat);
    std::snprintf(gExportFilename, sizeof(gExportFilename), "%s", name.c_str());
}

void drawDirectoryList(const char* childId, float bottomReserve, bool selectFiles) {
    ImGui::BeginChild(childId, ImVec2(0, bottomReserve), true);

    struct FileEntry {
        std::string name;
        bool isDir;
    };
    std::vector<FileEntry> entries;
    std::error_code ec;

    for (const auto& entry : fs::directory_iterator(gCurrentBrowserPath, fs::directory_options::skip_permission_denied, ec)) {
        entries.push_back({ entry.path().filename().string(), entry.is_directory(ec) });
    }

    std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name < b.name;
    });

    for (const auto& entry : entries) {
        if (entry.isDir) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.68f, 0.95f, 1.0f));
            bool selected = ImGui::Selectable((std::string("[Dir]  ") + entry.name).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
            ImGui::PopStyleColor();
            if (selected && ImGui::IsMouseDoubleClicked(0)) {
                gCurrentBrowserPath /= entry.name;
                gSelectedBrowserFile = "";
                break;
            }
        } else if (selectFiles) {
            bool isSelected = (gSelectedBrowserFile == entry.name);
            if (ImGui::Selectable((std::string("       ") + entry.name).c_str(), isSelected)) {
                gSelectedBrowserFile = entry.name;
            }
        } else {
            ImGui::TextDisabled("       %s", entry.name.c_str());
        }
    }
    ImGui::EndChild();
}

void drawExportImageModal() {
    if (gTriggerExportDialog) {
        ImGui::OpenPopup("Export Image");
        gTriggerExportDialog = false;
        gExportStatus.clear();
        applyExportExtension();
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560, 500), ImGuiCond_Appearing);

    bool isOpen = true;
    if (ImGui::BeginPopupModal("Export Image", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextDisabled("Folder:");
        ImGui::SameLine();
        std::string pathStr = gCurrentBrowserPath.string();
        char pathBuf[512];
        std::strncpy(pathBuf, pathStr.c_str(), sizeof(pathBuf) - 1);
        pathBuf[sizeof(pathBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##export_path", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            fs::path newPath(pathBuf);
            std::error_code ec;
            if (fs::exists(newPath, ec) && fs::is_directory(newPath, ec))
                gCurrentBrowserPath = newPath;
        }

        if (ImGui::Button("Go Up", ImVec2(60, 0))) {
            if (gCurrentBrowserPath.has_parent_path())
                gCurrentBrowserPath = gCurrentBrowserPath.parent_path();
        }

        ImGui::Spacing();
        drawDirectoryList("##export_browser", -160.0f, false);

        ImGui::Spacing();
        static const char* formatNames[] = { "PNG", "JPEG", "BMP", "TGA" };
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::Combo("Format", &gExportFormat, formatNames, IM_ARRAYSIZE(formatNames)))
            applyExportExtension();

        if (gExportFormat == Engine::Export_JPEG) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(180.0f);
            ImGui::SliderInt("Quality", &gExportJpegQuality, 1, 100);
        }

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("File", gExportFilename, sizeof(gExportFilename));
        ImGui::TextDisabled("Export Working Area (%d x %d).", Engine::getWorkWidth(), Engine::getWorkHeight());

        if (!gExportStatus.empty())
            ImGui::TextWrapped("%s", gExportStatus.c_str());

        ImGui::PushStyleColor(ImGuiCol_Button, accentColor());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.34f, 0.60f, 0.90f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.48f, 0.78f, 1.0f));
        ImGui::BeginDisabled(Engine::getCurrImage() == 0);
        if (ImGui::Button("Export", ImVec2(100, 0))) {
            applyExportExtension();
            fs::path fullPath = gCurrentBrowserPath / gExportFilename;
            if (Engine::exportImage(fullPath.string().c_str(), gExportFormat, gExportJpegQuality))
                gExportStatus = std::string("Saved: ") + fullPath.string();
            else
                gExportStatus = "Failed to export the image.";
        }
        ImGui::EndDisabled();
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void drawLoadChromaBgModal() {
    if (gTriggerChromaBgDialog) {
        ImGui::OpenPopup("Load Chroma Background");
        gTriggerChromaBgDialog = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(550, 420), ImGuiCond_Appearing);

    bool isOpen = true;
    if (ImGui::BeginPopupModal("Load Chroma Background", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        
        ImGui::TextDisabled("Path:");
        ImGui::SameLine();
        std::string pathStr = gCurrentBrowserPath.string();
        char pathBuf[512];
        strncpy(pathBuf, pathStr.c_str(), sizeof(pathBuf) - 1);
        pathBuf[sizeof(pathBuf) - 1] = '\0';
        
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##chroma_path", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            fs::path newPath(pathBuf);
            std::error_code ec;
            if (fs::exists(newPath, ec) && fs::is_directory(newPath, ec)) {
                gCurrentBrowserPath = newPath;
                gSelectedBrowserFile = "";
            }
        }

        if (ImGui::Button("Go Up", ImVec2(60, 0))) {
            if (gCurrentBrowserPath.has_parent_path()) {
                gCurrentBrowserPath = gCurrentBrowserPath.parent_path();
                gSelectedBrowserFile = "";
            }
        }

        ImGui::Spacing();

        drawDirectoryList("##chroma_browser_list", -42.0f, true); 

        ImGui::Spacing();
        bool hasSelection = !gSelectedBrowserFile.empty();

        ImGui::PushStyleColor(ImGuiCol_Button, accentColor());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.34f, 0.60f, 0.90f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.48f, 0.78f, 1.0f));
        
        ImGui::BeginDisabled(!hasSelection);
        if (ImGui::Button("Open", ImVec2(80, 0))) {
            fs::path fullPath = gCurrentBrowserPath / gSelectedBrowserFile;

            Engine::loadChromaBgImage(fullPath.string().c_str());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void drawSteganographyModal() {
    if (gTriggerStegoDialog) {
        ImGui::OpenPopup("Steganography");
        gTriggerStegoDialog = false;
        gStegoStatus.clear();
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(620, 540), ImGuiCond_Appearing);

    bool isOpen = true;
    if (ImGui::BeginPopupModal("Steganography", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        if (ImGui::BeginTabBar("##stego_tabs")) {
            if (ImGui::BeginTabItem("Hide Message")) {
                gStegoTab = 0;
                ImGui::TextWrapped(
                    "The message is stored in the least significant bits of the working area. "
                    "Export as PNG, BMP or TGA so the hidden data is preserved. JPEG will destroy it."
                );
                ImGui::TextDisabled(
                    "Capacity: %zu bytes",
                    Engine::getSteganographyCapacityBytes()
                );
                ImGui::InputTextMultiline("##stego_msg", gStegoMessage, sizeof(gStegoMessage), ImVec2(-1.0f, 180.0f));
                ImGui::TextDisabled("Characters: %zu", std::strlen(gStegoMessage));

                ImGui::PushStyleColor(ImGuiCol_Button, accentColor());
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.34f, 0.60f, 0.90f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.48f, 0.78f, 1.0f));
                ImGui::BeginDisabled(Engine::getCurrImage() == 0);
                if (ImGui::Button("Hide In Current Image", ImVec2(-1.0f, 32.0f))) {
                    if (Engine::embedSteganographyMessage(gStegoMessage, gStegoStatus))
                        gStegoStatus = "Message hidden in the current image. Export it (PNG/BMP/TGA) to keep the data.";
                }
                ImGui::EndDisabled();
                ImGui::PopStyleColor(3);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Read Message")) {
                gStegoTab = 1;
                ImGui::TextWrapped("Read a hidden message from the current image or from a chosen file.");

                if (ImGui::Button("Read From Current Image", ImVec2(-1.0f, 0.0f))) {
                    if (Engine::extractSteganographyMessageFromCurrent(gStegoExtracted, gStegoStatus))
                        gStegoStatus = "Message extracted from the current image.";
                    else
                        gStegoExtracted.clear();
                }

                ImGui::Separator();
                ImGui::TextDisabled("Folder:");
                ImGui::SameLine();
                std::string pathStr = gCurrentBrowserPath.string();
                char pathBuf[512];
                std::strncpy(pathBuf, pathStr.c_str(), sizeof(pathBuf) - 1);
                pathBuf[sizeof(pathBuf) - 1] = '\0';
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::InputText("##stego_path", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    fs::path newPath(pathBuf);
                    std::error_code ec;
                    if (fs::exists(newPath, ec) && fs::is_directory(newPath, ec)) {
                        gCurrentBrowserPath = newPath;
                        gSelectedBrowserFile = "";
                    }
                }

                if (ImGui::Button("Go Up##stego", ImVec2(60, 0))) {
                    if (gCurrentBrowserPath.has_parent_path()) {
                        gCurrentBrowserPath = gCurrentBrowserPath.parent_path();
                        gSelectedBrowserFile = "";
                    }
                }

                drawDirectoryList("##stego_browser", 140.0f, true);

                ImGui::BeginDisabled(gSelectedBrowserFile.empty());
                if (ImGui::Button("Read From Chosen Image", ImVec2(-1.0f, 0.0f))) {
                    fs::path fullPath = gCurrentBrowserPath / gSelectedBrowserFile;
                    if (Engine::extractSteganographyMessageFromFile(fullPath.string().c_str(), gStegoExtracted, gStegoStatus))
                        gStegoStatus = std::string("Message extracted from ") + gSelectedBrowserFile;
                    else
                        gStegoExtracted.clear();
                }
                ImGui::EndDisabled();

                ImGui::TextDisabled("Extracted Message:");
                ImGui::BeginChild("##stego_out", ImVec2(-1.0f, 90.0f), true);
                ImGui::TextWrapped("%s", gStegoExtracted.empty() ? "(none)" : gStegoExtracted.c_str());
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        if (!gStegoStatus.empty())
            ImGui::TextWrapped("%s", gStegoStatus.c_str());

        if (ImGui::Button("Close", ImVec2(80, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

} // namespace

void filterGUI() {
    FilterSettings& settings = gFilterSettings;
    static bool initialized = false;
    if (!initialized) {
        resetKernelToIdentity(settings.kernel, settings.kernelSize);
        settings.piecewiseR1 = 0.25f;
        settings.piecewiseS1 = 0.15f;
        settings.piecewiseR2 = 0.75f;
        settings.piecewiseS2 = 0.85f;
        settings.gamma = 1.0f;
        initialized = true;
    }

    static const char* filterNames[] = {
        "Negative",
        "Brightness",
        "Threshold",
        "Gamma Correction",
        "Piecewise Linear",
        "Convolution",
        "Resize",
        "Rotate",
        "Fourier",
        "Histogram Equalization",
        "Gray Scale",
        "HSI Adjust",
        "Color Balance",
        "Sepia",
        "Chroma Key"
    };
    static const char* filterDescriptions[] = {
        "Invert all color channels.",
        "Add a constant offset to pixel values.",
        "Binarize the image by luminance.",
        "Apply a power-law tone response.",
        "Contrast stretch with two control points.",
        "Apply a custom convolution kernel.",
        "Resize the image to a specific width and height.",
        "Rotate the image around an arbitrary pivot.",
        "Edit the shifted FFT spectrum and reconstruct with the inverse transform."
    };

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float panelWidth = 420.0f;
    ImGui::SetNextWindowPos(ImVec2(display.x - panelWidth, 28.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, display.y - 52.0f), ImGuiCond_Always);

    ImGui::Begin(
        "Filter Controls",
        nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse
    );

    ImGui::TextColored(accentColor(), "Lumage");
    ImGui::SameLine();
    ImGui::TextDisabled("| Image Processing");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    sectionHeader("Filter Type");

    const int filterCount = IM_ARRAYSIZE(filterNames);
    const int current = (int)settings.option;

    ImGui::BeginChild("FilterList", ImVec2(0.0f, 224.0f), true);
    for (int i = 0; i < filterCount; ++i) {
        const bool selected = current == i;
        if (ImGui::Selectable(filterNames[i], selected, 0, ImVec2(0.0f, 28.0f)))
            settings.option = (uint32_t)i;
        if (selected && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", filterDescriptions[i]);
    }
    ImGui::EndChild();

    if (current >= 0 && current < filterCount)
        ImGui::TextDisabled("%s", filterDescriptions[current]);

    ImGui::Spacing();
    drawFilterParameters(settings);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const bool disableApply =
        (settings.option == Filter_PiecewiseLinear &&
        settings.piecewiseR1 >= settings.piecewiseR2) || 
        Engine::getCurrImage() == 0;

    ImGui::PushStyleColor(ImGuiCol_Button, accentColor());
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.34f, 0.60f, 0.90f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.48f, 0.78f, 1.0f));
    ImGui::BeginDisabled(disableApply);
    const char* applyLabel =
        settings.option == Filter_Fourier ? "Apply Inverse FFT" : "Apply Filter";
    if (ImGui::Button(applyLabel, ImVec2(-1.0f, 36.0f))){
        settings.isRegionSelected = gRegionSelection.hasSelection;
        settings.tlCorner = glm::vec2(gRegionSelection.x0, gRegionSelection.y0);
        settings.brCorner = glm::vec2(gRegionSelection.x1, gRegionSelection.y1);

        Engine::processImage(settings);
        
        if (settings.option == Filter_Rotate){
            settings.pivot_x += Engine::getLastCanvasOffset().x;
            settings.pivot_y += Engine::getLastCanvasOffset().y;
        }
    }
    ImGui::EndDisabled();
    ImGui::PopStyleColor(3);

    ImGui::BeginDisabled(Engine::getCurrImage() == 0);
    if (ImGui::Button("Reset to Original", ImVec2(-1.0f, 32.0f)))
        Engine::resetImageToBase();
    ImGui::EndDisabled();

    ImGui::End();
}

void imagePreviewGUI() {
    const float sideMargin = 12.0f;
    const float topOffset = 28.0f;
    const float bottomOffset = 24.0f;
    const float panelWidth = 420.0f;
    const float imageGap = 12.0f;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float availWidth = display.x - panelWidth - sideMargin * 3.0f;
    const float availHeight = display.y - topOffset - bottomOffset - sideMargin - 28.0f;

    const bool fourierMode = gFilterSettings.option == Filter_Fourier;
    const bool dual = gSideBySideComparison || fourierMode;
    const float previewSize = dual
        ? std::min((availWidth - imageGap) * 0.5f, availHeight)
        : std::min(availWidth, availHeight);

    const float contentWidth = dual
        ? previewSize * 2.0f + imageGap
        : previewSize;

    const float selectionInfoHeight = gRegionSelection.hasSelection ? 132.0f : 0.0f;

    ImGui::SetNextWindowPos(ImVec2(sideMargin, topOffset), ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(contentWidth + 24.0f, previewSize + 172.0f + selectionInfoHeight),
        ImGuiCond_Always
    );

    ImGui::Begin(
        "Preview",
        nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar
    );

    sectionHeader("Image Preview");

    ImGui::Spacing();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 imageDrawSize(previewSize, previewSize);

    if (fourierMode) {
        ImGui::BeginGroup();
        drawPreviewImage(draw, Engine::getCurrImage(), "Working image", imageDrawSize);
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, imageGap);

        ImGui::BeginGroup();
        drawSpectrumEditor(draw, imageDrawSize);
        ImGui::EndGroup();
    } else if (gSideBySideComparison) {
        ImGui::BeginGroup();
        drawPreviewImage(draw, Engine::getBaseImage(), "Original", imageDrawSize);
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, imageGap);

        ImGui::BeginGroup();
        drawEditedImageWithRegionSelection(draw, imageDrawSize);
        ImGui::EndGroup();
    } else {
        drawEditedImageWithRegionSelection(draw, imageDrawSize);
    }

    drawRegionSelectionInfo();

    ImGui::Spacing();
    
    if (Engine::getCurrImage() != 0) {
        ImGui::TextDisabled(
            "Drawing area: %d x %d",
            Engine::getImageWidth(),
            Engine::getImageHeight()
        );
        ImGui::TextDisabled(
            "Working area: %d x %d  (export)",
            Engine::getWorkWidth(),
            Engine::getWorkHeight()
        );
        ImGui::TextColored(ImVec4(0.28f, 0.57f, 0.86f, 1.0f), "Blue outline: drawing area (full canvas).");
        ImGui::TextColored(ImVec4(0.27f, 0.86f, 0.43f, 1.0f), "Green outline: working area (image extents).");
    } else {
        ImGui::TextDisabled("Drawing area: 0 x 0");
    }

    ImGui::End();
}

void statusBarGUI() {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, display.y - 24.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display.x, 24.0f), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin(
        "StatusBar",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings
    );

    ImGui::TextDisabled("Ready");
    ImGui::SameLine(display.x - 210.0f);

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void mainMenuBarGUI() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load Image..."))
                gTriggerLoadDialog = true;
            if (ImGui::MenuItem("Export Image...", nullptr, false, Engine::getCurrImage() != 0))
                gTriggerExportDialog = true;
            if (ImGui::MenuItem("Steganography..."))
                gTriggerStegoDialog = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Image", nullptr, false, Engine::getCurrImage() != 0))
                Engine::resetImageToBase();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                Engine::stop();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Side-by-Side Comparison", nullptr, &gSideBySideComparison);
            ImGui::Separator();
            ImGui::MenuItem("Filter Controls", nullptr, nullptr, false);
            ImGui::MenuItem("Image Preview", nullptr, nullptr, false);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::TextDisabled("Lumage — GPU image filters");
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void Engine::__guiStep() {
    applyProfessionalTheme();

    glClearColor(0.07f, 0.07f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    mainMenuBarGUI();
    drawLoadImageModal();
    drawExportImageModal();
    drawSteganographyModal();
    drawLoadChromaBgModal();
    imagePreviewGUI();
    filterGUI();
    statusBarGUI();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(Engine::window);
}