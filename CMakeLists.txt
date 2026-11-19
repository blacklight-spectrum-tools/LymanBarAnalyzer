cmake_minimum_required(VERSION 3.15)
project(BarSpectrumAnalyzer VERSION 1.2.0)

# --------------------------------------------------------------------------
# JUCE viene como submodulo de git en ./JUCE
#   git clone --recurse-submodules ...   (o: git submodule update --init)
# --------------------------------------------------------------------------
add_subdirectory(JUCE)

# Identidad definitiva del plugin. NO cambiar PLUGIN_MANUFACTURER_CODE ni
# PLUGIN_CODE despues de distribuirlo: los DAWs guardan esos codigos en los
# proyectos y, si cambian, el plugin ya no se encuentra.
#   Blkt = Blacklight Spectrum Tools (el mismo en todos los plugins de la marca)
#   Lymn = Lyman Bar Analyzer
# Codigos anteriores (siguen instalables en paralelo): v1.0 = Tudo/Bspa,
# version de desarrollo "Bar Spectrum Analyzer Dev" = Tudo/Bsdv.
juce_add_plugin(BarSpectrumAnalyzer
    COMPANY_NAME "Blacklight Spectrum Tools"
    BUNDLE_ID com.blacklightspectrumtools.lymanbaranalyzer
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Blkt
    PLUGIN_CODE Lymn
    FORMATS VST3
    PRODUCT_NAME "Lyman Bar Analyzer")

juce_generate_juce_header(BarSpectrumAnalyzer)

target_sources(BarSpectrumAnalyzer
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/UI/SpectrumView.cpp
        Source/UI/PhaseView.cpp)

target_compile_definitions(BarSpectrumAnalyzer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BarSpectrumAnalyzer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
