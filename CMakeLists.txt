cmake_minimum_required(VERSION 3.15)
project(BarSpectrumAnalyzer VERSION 1.2.0)

# --------------------------------------------------------------------------
# JUCE viene como submodulo de git en ./JUCE
#   git clone --recurse-submodules ...   (o: git submodule update --init)
# --------------------------------------------------------------------------
add_subdirectory(JUCE)

# Version de desarrollo: PRODUCT_NAME y PLUGIN_CODE distintos a la v1.0 para
# que FL Studio vea las dos como plugins separados y puedan convivir mientras
# se prueba. Al lanzar la version final se fijan el nombre del producto, la
# marca (COMPANY_NAME) y el PLUGIN_MANUFACTURER_CODE definitivos.
juce_add_plugin(BarSpectrumAnalyzer
    COMPANY_NAME "TuEstudio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Tudo
    PLUGIN_CODE Bsdv
    FORMATS VST3
    PRODUCT_NAME "Bar Spectrum Analyzer Dev")

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
