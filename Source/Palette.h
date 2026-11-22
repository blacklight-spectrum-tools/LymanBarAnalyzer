#pragma once
#include <JuceHeader.h>

// Paleta del plugin. Los colores se leen en vivo: al cambiar de tema en el
// engrane, todas las ventanas abiertas se redibujan con el tema nuevo.
//
// El tema es una preferencia del usuario (no del proyecto): se guarda en un
// archivo de ajustes y lo comparten todas las instancias de Lyman.
namespace Palette
{
    struct Theme
    {
        juce::Colour background, panel, grid, barNormal, barOver, referenceLine,
                     text, textDim, side, ok, warn, danger, idle;
    };

    // Avisos de siempre: van fuera de la paleta a proposito, para distinguirse
    // a simple vista del resto de la interfaz. Siempre con texto al lado.
    inline void setNormalWarnings (Theme& t)
    {
        t.ok     = juce::Colour (0xff4ADE80);   // verde
        t.warn   = juce::Colour (0xffFACC15);   // amarillo
        t.danger = juce::Colour (0xffF87171);   // rojo
    }

    // Avisos para daltonismo rojo-verde (~1 de cada 12 hombres). Colores de
    // la paleta Okabe-Ito: azul, amarillo y bermellon se distinguen por tono
    // y tambien por brillo (el bermellon es mucho mas oscuro que el amarillo).
    inline void setColourBlindWarnings (Theme& t)
    {
        t.ok     = juce::Colour (0xff4C9BE8);   // azul
        t.warn   = juce::Colour (0xffF0E442);   // amarillo
        t.danger = juce::Colour (0xffE2611A);   // bermellon
    }

    // "Blacklight" (por defecto): la paleta "Lujo elegancia" de siempre.
    // El Side es cian "fluorescente", como lo que brilla bajo luz negra.
    inline Theme blacklight()
    {
        Theme t;
        t.background    = juce::Colour (0xff1F0C33);
        t.panel         = juce::Colour (0xff3B2155);
        t.grid          = juce::Colour (0xff634780);
        t.barNormal     = juce::Colour (0xff937CAA);
        t.barOver       = juce::Colour (0xffDDBBFE);
        t.referenceLine = juce::Colour (0xffC9BED5);
        t.text          = juce::Colour (0xffC9BED5);
        t.textDim       = juce::Colour (0xff937CAA);
        t.side          = juce::Colour (0xff67E8F9);
        t.idle          = juce::Colour (0xff6B5B7E);
        setNormalWarnings (t);
        return t;
    }

    // "Graphite": gris neutro para sesiones largas. El lila se queda solo
    // donde importa (logo, pestana activa, lo que pasa la referencia): asi
    // sigue siendo Blacklight. Tonos aprobados por el usuario (nov 2026).
    inline Theme graphite()
    {
        Theme t = blacklight();
        t.background    = juce::Colour (0xff16171A);
        t.panel         = juce::Colour (0xff26282D);
        t.grid          = juce::Colour (0xff3A3D44);
        t.barNormal     = juce::Colour (0xff8A8F99);
        t.referenceLine = juce::Colour (0xffC8CCD4);
        t.text          = juce::Colour (0xffC8CCD4);
        t.textDim       = juce::Colour (0xff7E848E);
        t.idle          = juce::Colour (0xff5A5E66);
        return t;
    }

    // "Dim": la paleta de siempre con menos brillo (estudio a oscuras).
    // Los avisos se quedan con su brillo: tienen que seguir notandose.
    inline Theme dim()
    {
        Theme t = blacklight();
        t.background    = juce::Colour (0xff170926);
        t.panel         = juce::Colour (0xff2C1940);
        t.grid          = juce::Colour (0xff4A355F);
        t.barNormal     = juce::Colour (0xff6F5E84);
        t.barOver       = juce::Colour (0xffAE92CC);
        t.referenceLine = juce::Colour (0xff9C92A8);
        t.text          = juce::Colour (0xff9C92A8);
        t.textDim       = juce::Colour (0xff6F5E84);
        t.side          = juce::Colour (0xff4BB3C1);
        t.idle          = juce::Colour (0xff54476A);
        return t;
    }

    enum ThemeId { blacklightTheme = 0, graphiteTheme, dimTheme };

    inline Theme& current()
    {
        static Theme t = blacklight();
        return t;
    }

    // Sube cada vez que cambia el tema; las ventanas lo revisan en su timer
    inline int& version()
    {
        static int v = 0;
        return v;
    }

    // Colores que usa el codigo (siempre del tema actual)
    inline const juce::Colour& background    = current().background;
    inline const juce::Colour& panel         = current().panel;
    inline const juce::Colour& grid          = current().grid;
    inline const juce::Colour& barNormal     = current().barNormal;
    inline const juce::Colour& barOver       = current().barOver;
    inline const juce::Colour& referenceLine = current().referenceLine;
    inline const juce::Colour& text          = current().text;
    inline const juce::Colour& textDim       = current().textDim;
    inline const juce::Colour& side          = current().side;
    inline const juce::Colour& ok            = current().ok;
    inline const juce::Colour& warn          = current().warn;
    inline const juce::Colour& danger        = current().danger;
    inline const juce::Colour& idle          = current().idle;

    // --- Preferencia guardada (todas las instancias la comparten) ----------
    struct Settings
    {
        int  theme = blacklightTheme;
        bool colourBlind = false;
        bool largeReadings = false;   // modo para grabar video / ensenar
    };

    // Archivo de ajustes (en Windows: %APPDATA%\Blacklight Spectrum Tools).
    // Se abre solo para leer o guardar; no queda ningun objeto vivo.
    inline juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "Lyman Bar Analyzer";
        o.folderName          = "Blacklight Spectrum Tools";
        o.filenameSuffix      = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }

    inline Settings& settings()
    {
        static Settings s = []
        {
            juce::PropertiesFile file (settingsOptions());
            Settings loaded;
            loaded.theme       = juce::jlimit (0, 2, file.getIntValue ("theme", blacklightTheme));
            loaded.colourBlind = file.getBoolValue ("colourBlind", false);
            loaded.largeReadings = file.getBoolValue ("largeReadings", false);
            return loaded;
        }();
        return s;
    }

    inline void apply()
    {
        const auto& s = settings();
        current() = s.theme == graphiteTheme ? graphite()
                  : s.theme == dimTheme      ? dim()
                                             : blacklight();
        if (s.colourBlind)
            setColourBlindWarnings (current());
        ++version();
    }

    inline void save()
    {
        const auto& s = settings();
        juce::PropertiesFile file (settingsOptions());
        file.setValue ("theme", s.theme);
        file.setValue ("colourBlind", s.colourBlind);
        file.setValue ("largeReadings", s.largeReadings);
        file.saveIfNeeded();
    }

    // Llamar desde el hilo de la interfaz
    inline void setTheme (int theme, bool colourBlind)
    {
        auto& s = settings();
        s.theme       = juce::jlimit (0, 2, theme);
        s.colourBlind = colourBlind;
        save();
        apply();
    }

    // Lecturas grandes: solo cambia el dibujo (textos y lineas mas grandes
    // para que se lean en un video comprimido). El analisis es el mismo.
    inline bool large() { return settings().largeReadings; }
    inline float fs (float size) { return large() ? std::round (size * 1.35f * 2.0f) * 0.5f : size; }
    inline float big (float normal, float largeValue) { return large() ? largeValue : normal; }

    inline void setLargeReadings (bool on)
    {
        settings().largeReadings = on;
        save();
        ++version();      // las ventanas se reacomodan en su timer
    }

    // Se llama al abrir la primera ventana: carga el tema guardado
    inline void ensureLoaded()
    {
        static bool done = false;
        if (! done)
        {
            done = true;
            apply();
        }
    }
}
