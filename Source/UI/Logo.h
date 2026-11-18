#pragma once
#include <JuceHeader.h>

/**
    Logo de Blacklight Spectrum Tools ("lineas espectrales"): barras del mismo
    alto que se adelgazan y se juntan hacia la derecha, como un espectro que
    llega al ultravioleta. Geometria original en un lienzo de 240 x 80.
*/
namespace Logo
{
    struct Line { float x, w; };

    inline const Line lines[] = {
        { 0.00f, 22.82f }, { 39.12f, 19.40f }, { 72.37f, 16.49f }, { 100.63f, 14.01f },
        { 124.65f, 11.91f }, { 145.07f, 10.12f }, { 162.43f, 8.61f }, { 177.19f, 7.32f },
        { 189.73f, 6.22f }, { 200.38f, 5.29f }, { 209.45f, 4.49f }, { 217.15f, 3.82f },
        { 223.69f, 3.25f }, { 229.26f, 2.76f }, { 233.99f, 2.35f }, { 238.01f, 1.99f }
    };

    constexpr float aspect = 240.0f / 80.0f;   // ancho / alto

    /** Dibuja la marca dentro de `area`, manteniendo su proporcion. */
    inline void drawMark (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    {
        const float h = juce::jmin (area.getHeight(), area.getWidth() / aspect);
        const float w = h * aspect;
        auto box = area.withSizeKeepingCentre (w, h);
        const float scale = w / 240.0f;

        g.setColour (colour);
        for (const auto& l : lines)
            g.fillRect (box.getX() + l.x * scale, box.getY(),
                        juce::jmax (1.0f, l.w * scale), h);
    }
}
