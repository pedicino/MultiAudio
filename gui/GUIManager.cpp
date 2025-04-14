#include "GUIManager.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>

namespace gui {

//------------------------------------------------------------------------------
// Constructor & Destructor
//------------------------------------------------------------------------------

GUIManager::GUIManager(audio::NoiseGate& ng, audio::ThreeBandEQ& threeBandEq, audio::Limiter& lim,
                       bool& deEsserEnabled, double& deEsserReduction,
                       int& deEsserStartFreq, int& deEsserEndFreq)
    : window(nullptr),
      running(false),
      noiseGate(ng),
      eq(threeBandEq),
      limiter(lim),
      deesserEnabledRef(deEsserEnabled),
      deesserReductionDBRef(deEsserReduction),
      deesserStartFreqRef(deEsserStartFreq),
      deesserEndFreqRef(deEsserEndFreq),
      selectedEffect(0) // Default to Noise Gate
{}

GUIManager::~GUIManager() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui::DestroyContext();
    }
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

//------------------------------------------------------------------------------
// Initialization
//------------------------------------------------------------------------------

bool GUIManager::initialize() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window = glfwCreateWindow(800, 400, "Multiaudio Processor", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    io.Fonts->Clear();
    ImFont* myFont = io.Fonts->AddFontFromFileTTF("gui/assets/Roboto-Regular.ttf", 20.0f);
    if (myFont == NULL) {
        io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
    }
    
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
    }

    running = true;
    return true;
}

//------------------------------------------------------------------------------
// Main Loop
//------------------------------------------------------------------------------

void GUIManager::update() {
    if (!window || glfwWindowShouldClose(window)) {
        running = false;
        return;
    }

    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Audio Processor", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

    ImGui::Columns(2, "MainColumns", true);
    ImGui::SetColumnWidth(0, 200);

    renderEffectsPanel();

    ImGui::NextColumn();
    renderControlsPanel();

    ImGui::Columns(1);
    ImGui::End();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
}

bool GUIManager::isRunning() const {
    return running;
}

//------------------------------------------------------------------------------
// UI Panels
//------------------------------------------------------------------------------

void GUIManager::renderEffectsPanel() {
    ImGui::BeginChild("EffectsPanel", ImVec2(0, 0), true);
    ImGui::Text("EFFECT STACK");
    ImGui::Separator();

    auto RenderEffectItem = [&](const char* name, int index) {
        bool is_selected = (selectedEffect == index);
        if (ImGui::Selectable(name, is_selected)) {
            selectedEffect = index;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("View/edit '%s' controls", name);
        }
    };

    RenderEffectItem("Noise Gate", 0);
    RenderEffectItem("De-Esser", 1);
    RenderEffectItem("Limiter", 2);
    RenderEffectItem("3-Band EQ", 3);

    ImGui::EndChild();
}

void GUIManager::renderControlsPanel() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 12));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
    
    ImGui::BeginChild("ControlsPanel", ImVec2(0, 0), true);

    switch (selectedEffect) {
        case 0: renderNoiseGateControls(); break;
        case 1: renderDeEsserControls(); break;
        case 2: renderLimiterControls(); break;
        case 3: renderEQControls(); break;
        default: ImGui::Text("Select an effect from the left panel."); break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

//------------------------------------------------------------------------------
// Effect-Specific Controls
//------------------------------------------------------------------------------

void GUIManager::renderNoiseGateControls() {
    ImGui::Text("NOISE GATE CONTROLS");
    ImGui::Separator();

    bool enabled = noiseGate.isEnabled();
    if (ImGui::Checkbox("Enabled##NoiseGate", &enabled)) {
        noiseGate.setEnabled(enabled);
    }

    float threshold = noiseGate.getThreshold();
    if (ImGui::SliderFloat("Threshold##NoiseGate", &threshold, 0.0f, 1.0f, "%.3f")) {
        noiseGate.setThreshold(threshold);
    }

    float attackTime = noiseGate.getAttackTime();
    if (ImGui::SliderFloat("Attack (ms)##NoiseGate", &attackTime, 0.1f, 50.0f, "%.1f ms")) {
        noiseGate.setAttackTime(attackTime);
    }

    float releaseTime = noiseGate.getReleaseTime();
    if (ImGui::SliderFloat("Release (ms)##NoiseGate", &releaseTime, 1.0f, 500.0f, "%.1f ms")) {
        noiseGate.setReleaseTime(releaseTime);
    }

    ImGui::Separator();
    ImGui::TextWrapped("Removes background noise by reducing gain when the signal is below the threshold.");
}

void GUIManager::renderEQControls() {
    ImGui::Text("3-BAND EQ CONTROLS");
    ImGui::Separator();

    bool enabled = eq.isEnabled();
    if (ImGui::Checkbox("Enabled##EQ", &enabled)) {
        eq.setEnabled(enabled);
    }

    float lowGain = eq.getBandGain(0);
    float midGain = eq.getBandGain(1);
    float highGain = eq.getBandGain(2);

    if (ImGui::SliderFloat("Low Gain##EQ", &lowGain, 0.0f, 6.0f, "%.1f")) {
        eq.setBandGain(0, lowGain);
    }
    ImGui::SameLine(); ImGui::Text(" (%.1f dB)", 20.0f * log10f(lowGain + 1e-6f));

    if (ImGui::SliderFloat("Mid Gain##EQ", &midGain, 0.0f, 6.0f, "%.1f")) {
        eq.setBandGain(1, midGain);
    }
    ImGui::SameLine(); ImGui::Text(" (%.1f dB)", 20.0f * log10f(midGain + 1e-6f));

    if (ImGui::SliderFloat("High Gain##EQ", &highGain, 0.0f, 6.0f, "%.1f")) {
        eq.setBandGain(2, highGain);
    }
    ImGui::SameLine(); ImGui::Text(" (%.1f dB)", 20.0f * log10f(highGain + 1e-6f));

    ImGui::Separator();
    ImGui::TextWrapped("Adjusts the volume (gain) of low, mid, and high frequency ranges.");
}

void GUIManager::renderLimiterControls() {
    ImGui::Text("LIMITER CONTROLS");
    ImGui::Separator();

    bool enabled = limiter.isEnabled();
    if (ImGui::Checkbox("Enabled##Limiter", &enabled)) {
        limiter.setEnabled(enabled);
    }

    float threshold = limiter.getThreshold();
    if (ImGui::SliderFloat("Threshold##Limiter", &threshold, 0.0f, 1.0f, "%.3f")) {
        limiter.setThreshold(threshold);
    }
    ImGui::SameLine(); ImGui::Text(" (%.1f dBFS)", 20.0f * log10f(threshold + 1e-6f));

    float attackTime = limiter.getAttackTime();
    if (ImGui::SliderFloat("Attack (ms)##Limiter", &attackTime, 0.1f, 50.0f, "%.1f ms")) {
        limiter.setAttackTime(attackTime);
    }

    float releaseTime = limiter.getReleaseTime();
    if (ImGui::SliderFloat("Release (ms)##Limiter", &releaseTime, 1.0f, 500.0f, "%.1f ms")) {
        limiter.setReleaseTime(releaseTime);
    }

    ImGui::Separator();
    ImGui::TextWrapped("Prevents audio peaks from exceeding the threshold, avoiding clipping.");
}

void GUIManager::renderDeEsserControls() {
    ImGui::Text("DE-ESSER CONTROLS");
    ImGui::Separator();

    if (ImGui::Checkbox("Enabled##DeEsser", &deesserEnabledRef)) {
        // Updated directly
    }

    float reduction = static_cast<float>(deesserReductionDBRef);
    if (ImGui::SliderFloat("Reduction (dB)##DeEsser", &reduction, 0.0f, 30.0f, "%.1f dB")) {
        deesserReductionDBRef = static_cast<double>(reduction);
    }

    int startFreq = deesserStartFreqRef;
    int endFreq = deesserEndFreqRef;

    if (ImGui::SliderInt("Start Freq##DeEsser", &startFreq, 2000, 10000, "%d Hz")) {
        if (startFreq >= deesserEndFreqRef) {
            endFreq = startFreq + 500;
            deesserEndFreqRef = endFreq;
        }
        deesserStartFreqRef = startFreq;
    }

    if (ImGui::SliderInt("End Freq##DeEsser", &endFreq, 3000, 12000, "%d Hz")) {
        if (endFreq <= deesserStartFreqRef) {
            startFreq = endFreq - 500;
            deesserStartFreqRef = startFreq;
        }
        deesserEndFreqRef = endFreq;
    }

    ImGui::Separator();
    ImGui::TextWrapped("Reduces sibilance ('s' sounds) by attenuating a specific high-frequency range.");
}

}
