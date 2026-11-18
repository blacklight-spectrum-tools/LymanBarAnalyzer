# Bar Spectrum Analyzer (VST3)

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

No puedo compilar el binario .vst3 desde este entorno (se necesita
Visual Studio + JUCE instalados en Windows), así que te dejo el proyecto
completo listo para compilar en tu máquina.

## Requisitos

1. **JUCE**: clona el framework dentro de esta misma carpeta del proyecto:
   ```
   git clone https://github.com/juce-framework/JUCE.git
   ```
   Debe quedar como `BarSpectrumAnalyzer/JUCE/` (junto a este CMakeLists.txt).
   Si prefieres tenerlo en otro lugar, edita la línea `add_subdirectory(JUCE)`
   en `CMakeLists.txt` con la ruta correcta.

2. **CMake** (3.15 o más reciente): https://cmake.org/download/

3. **Visual Studio 2022** (Windows) con el componente
   "Desktop development with C++" instalado.

## Compilar

Desde la carpeta del proyecto (`BarSpectrumAnalyzer/`), en símbolo de
sistema o PowerShell:

```
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

Al terminar, el VST3 queda en:
```
build/BarSpectrumAnalyzer_artefacts/Release/VST3/Bar Spectrum Analyzer.vst3
```

Como el CMakeLists tiene `COPY_PLUGIN_AFTER_BUILD TRUE`, debería copiarse
automáticamente a la carpeta estándar de VST3 de Windows:
```
C:\Program Files\Common Files\VST3\
```
Si no aparece ahí, cópialo manualmente a esa ruta.

## Usar en FL Studio

1. Abre FL Studio → **Options → Manage Plugins**.
2. Pulsa **Find plugins** (o **Find more plugins**) para que lo detecte.
3. Debería aparecer como **"Bar Spectrum Analyzer"** en la lista de efectos.
4. Insértalo en el canal del mixer que quieras medir, **justo antes** del
   slot donde vas a poner tu compresor.
5. Mueve el slider **"Referencia (dB)"** hasta el nivel al que quieres que
   el compresor empiece a actuar. Las barras que queden en **naranja**
   están por encima de ese umbral: son las bandas que realmente necesitan
   compresión en ese momento. Las que quedan en **verde** están por debajo.

## Ajustes que puedes pedirme después

- Cambiar el rango de frecuencias o el número de barras.
- Añadir modo "peak hold" (que la punta de cada barra se quede marcada
  unos segundos).
- Añadir un modo estéreo (mostrar izquierda/derecha por separado).
- Convertir esto en una herramienta web (HTML) además del VST3, para usarla
  sin abrir FL Studio.
