#pragma once
#include <JuceHeader.h>

// Paleta unica del plugin ("Lujo elegancia"). Cambia los colores solo aqui.
namespace Palette
{
    const juce::Colour background    { 0xff1F0C33 };  // fondo de la ventana
    const juce::Colour panel         { 0xff3B2155 };  // paneles / pistas de controles
    const juce::Colour grid          { 0xff634780 };  // rejilla de dB
    const juce::Colour barNormal     { 0xff937CAA };  // barras por debajo de la referencia
    const juce::Colour barOver       { 0xffDDBBFE };  // barras que pasan la referencia
    const juce::Colour referenceLine { 0xffC9BED5 };  // linea de referencia
    const juce::Colour text          { 0xffC9BED5 };  // textos y etiquetas
}
