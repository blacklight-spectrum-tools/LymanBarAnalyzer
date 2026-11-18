# Bar Spectrum Analyzer (VST3)

*[Read in English](README.md)*

Analizador de espectro de referencia, estilo SPAN pero mostrando la energía
por banda de frecuencia en **barras** (como un EQ gráfico), pensado para
insertarlo justo **antes de un compresor** y decidir con más precisión en
qué bandas y a qué nivel (dB) fijar el umbral.

- 28 barras espaciadas logarítmicamente entre 20 Hz y 20 kHz.
- Medidor con ataque rápido / liberación lenta (comportamiento tipo VU real).
- Slider "Referencia (dB)" automatizable: dibuja una línea amarilla y pinta
  de naranja cualquier banda que la supere — así ves de un vistazo qué
  frecuencias necesitan compresión y cuáles no.
- No modifica el audio: es solo un medidor, transparente en la señal.

## Requisitos

- **Git**
- **CMake** 3.15 o más reciente: https://cmake.org/download/
- **Visual Studio 2022** (o sus Build Tools) con el componente
  "Desktop development with C++".

JUCE viene incluido como submódulo de git, no hace falta descargarlo aparte.

## Descargar el código

Clona el repositorio **con sus submódulos** para que baje JUCE:

```
git clone --recurse-submodules https://github.com/velivaneski/BarSpectrumAnalyzer.git
```

Si ya lo clonaste sin esa opción y la carpeta `JUCE/` está vacía, corre
dentro del proyecto:

```
git submodule update --init
```

## Compilar

Desde la carpeta del proyecto, en PowerShell o símbolo de sistema:

```
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

La primera compilación tarda varios minutos porque compila JUCE completo.
Al terminar, el plugin queda en:

```
build/BarSpectrumAnalyzer_artefacts/Release/VST3/Bar Spectrum Analyzer.vst3
```

## Instalar

El proyecto intenta copiar el plugin automáticamente a la carpeta estándar
de VST3 de Windows:

```
C:\Program Files\Common Files\VST3\
```

Esa carpeta requiere permisos de administrador. Si la compilación termina
con un error `Permission denied` en ese paso, el plugin **sí se compiló**;
solo copia a mano la carpeta `Bar Spectrum Analyzer.vst3` a esa ruta
(Windows pedirá confirmar como administrador), o corre la compilación desde
una terminal abierta como administrador.

## Usar en FL Studio

1. Abre FL Studio → **Options → Manage Plugins**.
2. Pulsa **Find plugins** para que lo detecte.
3. Aparece como **"Bar Spectrum Analyzer"** en la lista de efectos.
4. Insértalo en el canal del mixer que quieras medir, **justo antes** del
   slot donde vas a poner tu compresor.
5. Mueve el slider **"Referencia (dB)"** hasta el nivel al que quieres que
   el compresor empiece a actuar. Las barras en **naranja** están por encima
   de ese umbral: son las bandas que necesitan compresión en ese momento.
   Las que quedan en **verde** están por debajo.
