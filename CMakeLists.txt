cmake_minimum_required(VERSION 3.15)
project(BarSpectrumAnalyzer VERSION 1.0.0)

# --------------------------------------------------------------------------
# Ajusta esta ruta: debe apuntar a tu copia local de JUCE
#   git clone https://github.com/juce-framework/JUCE.git
# Puedes clonarlo dentro de esta misma carpeta (queda como ./JUCE) o
# cambiar la ruta de add_subdirectory() más abajo.
# --------------------------------------------------------------------------
add_subdirectory(JUCE)

juce_add_plugin(BarSpectrumAnalyzer
    COMPANY_NAME "TuEstudio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Tudo
    PLUGIN_CODE Bspa
    FORMATS VST3
    PRODUCT_NAME "Bar Spectrum Analyzer")

juce_generate_juce_header(BarSpectrumAnalyzer)

target_sources(BarSpectrumAnalyzer
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

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
