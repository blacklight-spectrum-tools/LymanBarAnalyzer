#pragma once
#include <JuceHeader.h>

/**
    Zonas de guia por instrumento (boton "Guide" de la pestana Spectrum).

    Solo explican: que hay en cada zona y que pasa si sobra o si falta. Los
    rangos son aproximados (cada voz e instrumento es distinto); lo que si
    sale de la senal real es cuanto pasa la referencia cada zona.
*/
namespace GuideZones
{
    struct Zone
    {
        const char* name;
        const char* shortName;   // si el nombre no cabe en la tira
        float lo, hi;            // Hz
        const char* what;
        const char* tooMuch;
        const char* tooLittle;   // "" = no aplica
    };

    struct Instrument
    {
        const char* tag;         // etiqueta corta a la izquierda de la tira
        const char* menuName;
        const Zone* zones;
        int numZones;
    };

    inline constexpr Zone vocalZones[] = {
        { "Rumble",    "Rmbl",   20.0f,    90.0f, "Mic rumble, plosives, room noise", "Muddy, eats headroom (usually high-passed)", "" },
        { "Body",      "Body",   90.0f,   250.0f, "Warmth and weight of the voice",   "Boomy, muddy",                    "Thin" },
        { "Mud",       "Mud",   250.0f,   600.0f, "Boxy, 'cardboard' zone",           "Boxy, muffled, closed",           "Hollow" },
        { "Nasal",     "Nas",   800.0f,  1500.0f, "Nose and honk",                    "Nasal, telephone-like",           "Distant, scooped" },
        { "Presence",  "Pres",  2000.0f,  5000.0f, "Clarity of the words",            "Harsh, tiring",                   "Buried in the beat" },
        { "Sibilance", "Sib",   5000.0f,  9000.0f, "S and T sounds",                  "Sharp esses (de-esser)",          "Lisp, dull" },
        { "Air",       "Air",  10000.0f, 20000.0f, "Breath and sparkle",              "Hissy, brittle",                  "Dark, closed" },
    };

    inline constexpr Zone kickZones[] = {
        { "Sub",   "Sub",   20.0f,   50.0f, "Felt more than heard",            "Boomy on big systems, eats headroom", "No weight on clubs and subs" },
        { "Punch", "Pnch",  50.0f,  120.0f, "The thump of the kick",           "Clashes with the bass",               "Weak, no body" },
        { "Boxy",  "Box",  250.0f,  500.0f, "Cardboard zone",                  "Boxy, papery",                        "Hollow (often fine)" },
        { "Click", "Clk", 2000.0f, 5000.0f, "Beater attack, heard on phones",  "Clicky, sharp",                       "Vanishes on small speakers" },
    };

    inline constexpr Zone bassZones[] = {
        { "Sub",   "Sub",   20.0f,   60.0f, "Weight - keep it mono",                        "Muddy, eats headroom",  "Thin on big systems" },
        { "Body",  "Body",  60.0f,  250.0f, "The notes on most speakers",                   "Boomy, masks the kick", "Weak, distant" },
        { "Mud",   "Mud",  250.0f,  500.0f, "Low-mid build-up",                             "Muddy mix",             "Hollow" },
        { "Growl", "Grwl", 700.0f, 2500.0f, "Harmonics that make bass audible on phones",   "Honky, aggressive",     "Bass disappears on small speakers" },
    };

    inline constexpr Zone snareZones[] = {
        { "Body",   "Body",  150.0f,   250.0f, "Weight of the hit",   "Thick, boomy",    "Thin" },
        { "Boxy",   "Box",   400.0f,   800.0f, "Box and ring",        "Boxy, ringy",     "Hollow" },
        { "Crack",  "Crk",  1500.0f,  4000.0f, "Attack and snap",     "Harsh",           "Soft, no snap" },
        { "Sizzle", "Szl",  5000.0f, 10000.0f, "Wires and brightness","Harsh, splashy",  "Dull" },
    };

    inline constexpr Zone guitarZones[] = {
        { "Boom", "Boom",   80.0f,   200.0f, "Low end of the body",                    "Clashes with bass and kick", "Thin" },
        { "Mud",  "Mud",   200.0f,   500.0f, "Low-mid build-up",                       "Muddy",                      "Hollow" },
        { "Honk", "Honk",  500.0f,  1500.0f, "Character and tone",                     "Honky, nasal",               "Scooped" },
        { "Bite", "Bite", 2000.0f,  5000.0f, "Pick attack, presence",                  "Harsh, fizzy",               "Dull, buried" },
        { "Air",  "Air",  6000.0f, 12000.0f, "Sparkle (acoustic), fizz (distorted)",   "Fizzy, brittle",             "Dark" },
    };

    inline constexpr Zone keysZones[] = {
        { "Low",    "Low",    60.0f,   200.0f, "Left-hand weight",           "Clashes with the bass", "Thin" },
        { "Mud",    "Mud",   200.0f,   500.0f, "Low-mid build-up",           "Muddy, boxy",           "Hollow" },
        { "Body",   "Body",  500.0f,  2000.0f, "Main tone",                  "Honky",                 "Scooped" },
        { "Attack", "Atk",  2000.0f,  5000.0f, "Hammer attack, definition",  "Harsh, clanky",         "Soft, undefined" },
        { "Air",    "Air",  6000.0f, 16000.0f, "Shimmer",                    "Brittle",               "Dark" },
    };

    inline constexpr Zone mixZones[] = {
        { "Sub",        "Sub",    20.0f,    60.0f, "Felt, not heard - keep it mono",  "Boomy, eats headroom", "No weight" },
        { "Bass",       "Bass",   60.0f,   250.0f, "Kick and bass notes",             "Boomy",                "Thin" },
        { "Low mids",   "LoMid", 250.0f,   500.0f, "Where mud builds up",             "Muddy",                "Hollow" },
        { "Mids",       "Mids",  500.0f,  2000.0f, "Body of most instruments",        "Honky, boxy",          "Scooped, distant" },
        { "Upper mids", "UpMid", 2000.0f, 4000.0f, "The ear's most sensitive zone",   "Harsh, tiring",        "Dull, far away" },
        { "Presence",   "Pres",  4000.0f,  6000.0f, "Definition and edge",            "Sharp",                "Lacks definition" },
        { "Air",        "Air",   6000.0f, 20000.0f, "Brilliance and air",             "Hissy, brittle",       "Dark" },
    };

    // El orden es el del menu (el 0 del parametro es "Auto")
    inline const Instrument instruments[] = {
        { "MIX",  "Mix (general)", mixZones,    (int) std::size (mixZones) },
        { "VOX",  "Vocal",         vocalZones,  (int) std::size (vocalZones) },
        { "KICK", "Kick",          kickZones,   (int) std::size (kickZones) },
        { "BASS", "Bass / 808",    bassZones,   (int) std::size (bassZones) },
        { "SNR",  "Snare / clap",  snareZones,  (int) std::size (snareZones) },
        { "GTR",  "Guitar",        guitarZones, (int) std::size (guitarZones) },
        { "KEYS", "Keys / piano",  keysZones,   (int) std::size (keysZones) },
    };
    inline constexpr int numInstruments = 7;
    enum { mix = 0, vocal, kick, bass, snare, guitar, keys };

    /** Adivina el instrumento por el nombre de la pista ("Kik", "Lead Vox",
        "808", "Gtr 2"...). Sin pista reconocida: Mix (general). */
    inline int detect (const juce::String& trackName)
    {
        struct Key { const char* word; int instrument; };
        static const Key table[] = {
            { "vox", vocal }, { "vocal", vocal }, { "voc", vocal }, { "voz", vocal }, { "voice", vocal },
            { "adlib", vocal }, { "bgv", vocal }, { "coro", vocal }, { "rap", vocal }, { "dobles", vocal },
            { "bassdrum", kick }, { "kick", kick }, { "kik", kick }, { "bombo", kick }, { "bd", kick },
            { "808", bass }, { "bass", bass }, { "bajo", bass }, { "sub", bass }, { "bassline", bass },
            { "snare", snare }, { "snr", snare }, { "sd", snare }, { "caja", snare }, { "clap", snare }, { "rim", snare },
            { "guitar", guitar }, { "gtr", guitar }, { "guit", guitar }, { "guitarra", guitar }, { "requinto", guitar },
            { "piano", keys }, { "keyboard", keys }, { "keys", keys }, { "key", keys }, { "rhodes", keys }, { "ep", keys },
            { "organ", keys }, { "organo", keys }, { "teclado", keys }, { "teclados", keys },
        };

        // Palabras sueltas en minusculas: "Lead_Vox 2" -> lead, vox, 2
        juce::String clean;
        for (auto c : trackName.toLowerCase())
            clean << (juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c) : juce::String (" "));
        const auto words = juce::StringArray::fromTokens (clean, " ", "");

        for (const auto& w : words)
        {
            for (const auto& k : table)
            {
                const juce::String key (k.word);
                if (w == key)
                    return k.instrument;
                // "vocals", "kick2", "808s": la palabra empieza con la clave
                // (solo claves de 4+ letras o seguidas de numeros, para que
                // "sd" no atrape "sdfx" ni "ep" a "epic")
                if (w.startsWith (key))
                {
                    const auto rest = w.substring (key.length());
                    if (key.length() >= 4 || rest.containsOnly ("0123456789s"))
                        return k.instrument;
                }
            }
        }
        return mix;
    }
}
