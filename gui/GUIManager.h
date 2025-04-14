#ifndef GUIMANAGER_H
#define GUIMANAGER_H

//------------------------------------------------------------------------------
// Dependencies
//------------------------------------------------------------------------------

#include "../effects/NoiseGate.h"
#include "../effects/ThreeBandEQ.h"
#include "../effects/Limiter.h"

// Forward declaration to avoid including the full GLFW header
struct GLFWwindow;

namespace gui {

//------------------------------------------------------------------------------
// GUIManager Class
//------------------------------------------------------------------------------

/**
 * Manages the GUI system for controlling audio effects.
 * Handles window creation, input processing, and UI rendering.
 */
class GUIManager
{
public:
    //--------------------------------------------------------------------------
    // Constructor & Destructor
    //--------------------------------------------------------------------------

    /**
     * Creates a GUI manager that interfaces with audio processing effects.
     *
     * @param ng Reference to noise gate effect
     * @param threeBandEq Reference to equalizer effect
     * @param lim Reference to limiter effect
     * @param deEsserEnabled Reference to de-esser enable state
     * @param deEsserReduction Reference to de-esser reduction amount (in dB)
     * @param deEsserStartFreq Reference to de-esser frequency range lower bound (Hz)
     * @param deEsserEndFreq Reference to de-esser frequency range upper bound (Hz)
     */
    GUIManager(audio::NoiseGate& ng, audio::ThreeBandEQ& threeBandEq, audio::Limiter& lim,
              bool& deEsserEnabled, double& deEsserReduction,
              int& deEsserStartFreq, int& deEsserEndFreq);

    /**
     * Cleans up GUI resources including ImGui context and GLFW window.
     */
    ~GUIManager();

    //--------------------------------------------------------------------------
    // Public Interface
    //--------------------------------------------------------------------------

    /**
     * Sets up the GUI window, OpenGL context, and ImGui.
     *
     * @return true if initialization succeeded, false otherwise.
     */
    bool initialize();

    /**
     * Processes one frame of the GUI, including input handling and rendering.
     * Should be called repeatedly in the application's main loop.
     */
    void update();

    /**
     * Checks if the GUI should continue running.
     *
     * @return false when the window is closed or an error occurs.
     */
    bool isRunning() const;

private:
    //--------------------------------------------------------------------------
    // Member Variables
    //--------------------------------------------------------------------------

    GLFWwindow* window;   // OpenGL window handle (owned by ImGui + GLFW)
    bool running;         // Main loop control flag, true if app is active

    // Audio processing effect references (external ownership)
    audio::NoiseGate& noiseGate; // Noise gate effect instance
    audio::ThreeBandEQ& eq;      // 3-band equalizer instance
    audio::Limiter& limiter;     // Limiter effect instance

    // De-esser parameter references - modified directly through GUI
    bool& deesserEnabledRef;      // De-esser on/off toggle
    double& deesserReductionDBRef; // De-esser reduction in dB
    int& deesserStartFreqRef;     // De-esser frequency lower bound
    int& deesserEndFreqRef;       // De-esser frequency upper bound

    int selectedEffect;   // 0=Noise Gate, 1=EQ, 2=Limiter, 3=De-Esser (panel selector)

    //--------------------------------------------------------------------------
    // Private UI Rendering Methods
    //--------------------------------------------------------------------------

    /**
     * Renders the left panel containing the list of audio effects.
     */
    void renderEffectsPanel();

    /**
     * Renders the right panel with controls for the selected effect.
     * Uses selectedEffect index to switch between different panels.
     */
    void renderControlsPanel();

    /**
     * Renders controls specific to the Noise Gate effect.
     * Includes threshold, attack, and release sliders.
     */
    void renderNoiseGateControls();

    /**
     * Renders controls specific to the 3-Band EQ effect.
     * Includes low, mid, and high band gain controls.
     */
    void renderEQControls();

    /**
     * Renders controls specific to the Limiter effect.
     * Includes threshold, attack, and release parameters.
     */
    void renderLimiterControls();

    /**
     * Renders controls specific to the De-Esser effect.
     * Includes reduction amount and frequency range settings.
     */
    void renderDeEsserControls();
};

} // namespace gui

#endif // GUIMANAGER_H
