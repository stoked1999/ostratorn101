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
