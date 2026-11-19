#pragma once
#include <JuceHeader.h>

// Paleta unica del plugin ("Lujo elegancia"). Cambia los colores solo aqui.
namespace Palette
{
    const juce::Colour background    { 0xff1F0C33 };  // fondo de la ventana
    const juce::Colour panel         { 0xff3B2155 };  // paneles / pistas de controles
    const juce::Colour grid          { 0xff634780 };  // rejilla de dB
    const juce::Colour barNormal     { 0xff937CAA };  // barras por debajo de la referencia
    const juce::Colour barOver       { 0xffDDBBFE };  // barras que pasan la referencia / acento
    const juce::Colour referenceLine { 0xffC9BED5 };  // linea de referencia
    const juce::Colour text          { 0xffC9BED5 };  // textos y etiquetas
    const juce::Colour textDim       { 0xff937CAA };  // textos secundarios

    // Canal Side (lo que esta a los lados en estereo). Cian "fluorescente",
    // como lo que brilla bajo luz negra: contrasta con los morados sin chocar.
    const juce::Colour side          { 0xff67E8F9 };

    // Colores de estado (luz de fase). Van fuera de la paleta a proposito:
    // un aviso tiene que distinguirse a simple vista del resto de la interfaz.
    // Siempre se acompanan de un texto, para no depender solo del color.
    const juce::Colour ok            { 0xff4ADE80 };  // verde
    const juce::Colour warn          { 0xffFACC15 };  // amarillo
    const juce::Colour danger        { 0xffF87171 };  // rojo
    const juce::Colour idle          { 0xff6B5B7E };  // sin senal
}
