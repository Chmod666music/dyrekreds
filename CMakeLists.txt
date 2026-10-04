cmake_minimum_required(VERSION 3.22)

project(Dyrekreds VERSION 0.9.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release builds must work in both current Apple-Silicon hosts and older Intel
# hosts.  Keep this in the project, rather than only in CI, so a local macOS
# release build cannot accidentally ship a single-architecture plug-in.
# A caller may still deliberately override either cache variable.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
        "Architectures to build for macOS")
    # Apple-Silicon plug-ins require macOS 11 or newer; using that deployment
    # target also keeps the universal binary's two slices compatible.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
        "Minimum macOS version for the universal build")
endif()

add_subdirectory(JUCE)

# GUI fonts, embedded into the binary. Both are licensed under the SIL OFL
# (see assets/fonts/OFL-*.txt, which must ship alongside any distribution).
juce_add_binary_data(DyrekredsAssets
    SOURCES
        assets/fonts/UnifrakturMaguntia.ttf
        assets/fonts/IMFellEnglish.ttf)

# PLUGIN_MANUFACTURER_CODE and PLUGIN_CODE identify Dyrekreds in plugin hosts.
# These codes were established before the first public Dyrekreds release.
# They must remain unchanged after release, or saved DAW sessions may lose
# their connection to the plugin.
juce_add_plugin(Dyrekreds
    COMPANY_NAME "Fuimadane"
    BUNDLE_ID com.fuimadane.dyrekreds
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Fuim
    PLUGIN_CODE Dyrk
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Dyrekreds")

target_sources(Dyrekreds PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_compile_definitions(Dyrekreds PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    # Disabling the splash screen is permitted when JUCE is used under the (A)GPL.
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Dyrekreds
    PRIVATE
        DyrekredsAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Validate generated VST3 manifests with the parser supplied by Steinberg's
# VST3 SDK.  moduleinfo.json is JSON5 by specification, so a strict JSON
# parser would reject valid manifests that contain trailing commas.
add_executable(DyrekredsVST3ManifestValidator
    tools/ValidateVST3ModuleInfo.cpp
    JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/public.sdk/source/vst/moduleinfo/moduleinfoparser.cpp)
target_compile_features(DyrekredsVST3ManifestValidator PRIVATE cxx_std_17)
target_include_directories(DyrekredsVST3ManifestValidator PRIVATE
    JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK)

# Headless DSP tests, run by CI on every platform.
juce_add_console_app(DyrekredsTests PRODUCT_NAME "DyrekredsTests")
target_sources(DyrekredsTests PRIVATE
    tests/Tests.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)
target_compile_definitions(DyrekredsTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JucePlugin_Name="Dyrekreds")
target_link_libraries(DyrekredsTests PRIVATE
    DyrekredsAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags)

# Offline demo renderer: plays the factory presets to a WAV for the demo video.
# Built on demand, not part of the plugin targets.
juce_add_console_app(DyrekredsDemoRender PRODUCT_NAME "DyrekredsDemoRender")
set_target_properties(DyrekredsDemoRender PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(DyrekredsDemoRender PRIVATE
    tools/RenderDemo.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)
target_compile_definitions(DyrekredsDemoRender PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JucePlugin_Name="Dyrekreds")
target_link_libraries(DyrekredsDemoRender PRIVATE
    DyrekredsAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags)

# CLAP export (MIT-licensed wrapper around the JUCE plugin target)
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)
clap_juce_extensions_plugin(TARGET Dyrekreds
    CLAP_ID "com.fuimadane.dyrekreds"
    CLAP_FEATURES instrument synthesizer)
