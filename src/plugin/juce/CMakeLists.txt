# ÖstraTorn101 — JUCE wrapper (VST3 / Standalone).
#
# Build with tools/build_vst3_msvc.bat (vcvars + Ninja).  See
# ../../../docs/VST3.md for the whole story, including why the CMake
# "Visual Studio 17 2022" generator cannot be used on this machine.
#
# This file is added by the top-level CMakeLists when SH101_BUILD_VST3=ON, which
# also requires -DJUCE_DIR=<path to a JUCE checkout>.

juce_add_plugin(SH101Plugin
    COMPANY_NAME              "OstraTorn Audio"
    PLUGIN_MANUFACTURER_CODE  ShEm
    PLUGIN_CODE               Sh01
    FORMATS                   VST3 Standalone
    # The build-facing name is ASCII on purpose.  A non-ASCII value here becomes a
    # compiler command-line define, and the build tool's Windows argv handling
    # mangles it — which produced mojibake in the VST3 manifest and made hosts
    # reject the bundle outright.  The name a DAW displays is set below, as a C
    # escape sequence that survives the command line intact.
    PRODUCT_NAME              "OstraTorn101"
    # Note: no manufacturer trademarks, logos or panel artwork are reproduced
    # anywhere in this product.  The panel credits the instrument it is inspired
    # by, which is what the brief requires: behaviour and sound, not branding.
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
    OstraTornLookAndFeel.cpp
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

# The product name and the panel lettering are non-ASCII, so the compiler must be
# told the sources are UTF-8 (otherwise MSVC reads them in the local code page and
# the name comes out as mojibake in the host's plugin list).
target_compile_options(SH101Plugin PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# The plugin renders mono and copies to every output: no stereo widening.
target_compile_definitions(SH101Plugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# The name the host displays, as UTF-8 bytes written the way a C string literal
# takes them: "\303\226" is the two-byte UTF-8 sequence for "Ö".  Written this way
# the value on the compiler command line is pure ASCII, so no tool in the chain can
# mis-convert it.  This feeds the VST3 class name (see the LOCAL PATCH in JUCE's
# juce_VST3ModuleInfo.h) and the plugin's own getName().
target_compile_definitions(SH101Plugin PRIVATE
    "JucePlugin_DisplayName=\"\\303\\226straTorn101\""
)

# IMPORTANT: JucePlugin_Vst3ClassName is deliberately NOT set here.  A non-ASCII
# VST3 class name makes JUCE's own plugin-hosting path fail to instantiate the
# plugin (see docs/VST3.md), so the class stays ASCII and the umlaut lives in the
# display name above, which is what the panel prints.

# ---- Editor / preset smoke test ---------------------------------------------
# Builds the plugin's own editor directly (VST3 hosting hides a plugin's view in
# a native child window, so it cannot be inspected through the host path),
# snapshots it to a PNG and checks that the panel draws, lays its controls out and
# round-trips a user preset through the library.  This is the test that catches a
# blank or half-drawn panel.
juce_add_console_app(sh101_editor_test PRODUCT_NAME "sh101_editor_test")

target_sources(sh101_editor_test PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    OstraTornLookAndFeel.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/editor_test.cpp"
)

# tools/editor_test.cpp includes the wrapper's own headers, so both this directory
# and the shared source root must be on the include path.
target_include_directories(sh101_editor_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

# The macros juce_add_plugin would normally generate, for the sources above.
target_compile_definitions(sh101_editor_test PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JucePlugin_Name="OstraTorn101"
    "JucePlugin_DisplayName=\"\\303\\226straTorn101\""
    JucePlugin_IsSynth=1
    JucePlugin_WantsMidiInput=1
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JucePlugin_EditorRequiresKeyboardFocus=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_compile_options(sh101_editor_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(sh101_editor_test
    PRIVATE
        sh101_core
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
