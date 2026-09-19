# JUCE wrapper — VST3 / Standalone build.
#
# STATUS: NOT BUILT IN THE DEVELOPMENT ENVIRONMENT.
# The development machine has no MSVC C++ toolchain (Visual Studio 2022 without
# the C++ workload) and no JUCE checkout, so this target has never been compiled
# here.  Everything below it — the DSP voice and the host adapter — is compiled
# and covered by the test suite.  See ../../../docs/VST3.md before trusting this
# target.
#
# This file is added by the top-level CMakeLists when SH101_BUILD_VST3=ON, which
# also requires -DJUCE_DIR=<path to a JUCE checkout>.

juce_add_plugin(SH101Plugin
    COMPANY_NAME              "SH101 Emulation Project"
    PLUGIN_MANUFACTURER_CODE  ShEm
    PLUGIN_CODE               Sh01
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "SH-101"
    # Note: no Roland trademarks, logos or panel artwork are used anywhere in
    # this product — the brief requires behaviour/sound only, not branding.
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
)

target_sources(SH101Plugin PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    SH101LookAndFeel.cpp
)

target_include_directories(SH101Plugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../..")

target_link_libraries(SH101Plugin
    PRIVATE
        sh101_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# The plugin renders mono and copies to every output: no stereo widening.
target_compile_definitions(SH101Plugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# ---- Editor / preset smoke test ---------------------------------------------
# Builds the plugin's own editor directly (VST3 hosting hides a plugin's view in
# a native child window, so it cannot be inspected through the host path),
# snapshots it to a PNG and checks that the panel actually draws and lays its
# controls out.  This is the test that catches a blank editor window.
juce_add_console_app(sh101_editor_test PRODUCT_NAME "sh101_editor_test")

target_sources(sh101_editor_test PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    SH101LookAndFeel.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/editor_test.cpp"
)

# tools/editor_test.cpp includes the plugin's own headers, so both the wrapper
# directory and the shared source root must be on the include path.
target_include_directories(sh101_editor_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

# The macros juce_add_plugin would normally generate, for the sources above.
target_compile_definitions(sh101_editor_test PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JucePlugin_Name="SH-101"
    JucePlugin_IsSynth=1
    JucePlugin_WantsMidiInput=1
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JucePlugin_EditorRequiresKeyboardFocus=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(sh101_editor_test
    PRIVATE
        sh101_core
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
