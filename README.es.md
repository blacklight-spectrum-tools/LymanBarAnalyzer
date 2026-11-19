# Bar Spectrum Analyzer (VST3)

*[Read in English](README.md)*

Analizador de espectro de referencia que muestra la energía por banda de
frecuencia en **barras** (como un EQ gráfico), pensado para insertarlo
justo **antes de un compresor** y ver qué bandas necesitan compresión y
dónde poner el umbral.

No modifica el audio: es solo un medidor, transparente en la señal.

## Compatibilidad

Plugin VST3 para **Windows**, compatible con cualquier DAW que cargue
plugins VST3: FL Studio, Ableton Live, Cubase, Studio One, Reaper, Bitwig,
Cakewalk, Reason 12+ y otros.

Probado hasta ahora en FL Studio. Están planeadas versiones para Pro Tools
(AAX) y macOS (AU / VST3).

## Funciones

**Pestaña Spectrum**
- 28 barras espaciadas logarítmicamente entre 20 Hz y 20 kHz.
- Línea de referencia de compresión: las barras que la pasan se iluminan y
  muestran cuántos dB la rebasan (por ejemplo `+3.2`), el dato que
  necesitas para el umbral.
- Peak hold en cada barra.
- Tilt (0 / 3 / 4.5 dB por octava) para juzgar el balance tonal. Usa 0
  para poner el umbral de un compresor.
- Medidor de nivel L/R con lecturas de Peak, RMS y LUFS de corto plazo
  (ITU-R BS.1770), y luz de CLIP que se queda encendida (clic para apagarla).
- Vista Sum o M/S: en M/S el canal Side se dibuja en cian sobre cada barra,
  para ver qué frecuencias están abiertas y cuáles en el centro.

**Pestaña Phase**
- Goniómetro con la imagen estéreo, con escala Auto (la figura llena el
  círculo) o Level (la distancia al centro es el nivel real; el borde es
  0 dBFS).
- Medidor de correlación de fase de -1 a +1.
- Luz de advertencia: verde (compatible con mono), amarillo (muy abierto,
  revisa en mono), rojo (partes de la mezcla se cancelan al sumar a mono).
- Mínimo y máximo de correlación alcanzados mientras suena, con reinicio.
- Revisión de graves: correlación de todo lo que está por debajo de 120 Hz,
  con su propia luz (Mono / Slightly wide / Stereo bass). Te dice si lo
  ancho está en el grave, donde estorba, o solo en los agudos.
- Medidor de balance L/R: muestra si la mezcla se carga hacia un lado.

La ventana se puede redimensionar y recuerda su tamaño y la última pestaña
abierta.

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
build/BarSpectrumAnalyzer_artefacts/Release/VST3/Bar Spectrum Analyzer Dev.vst3
```

La versión en desarrollo se llama **"Bar Spectrum Analyzer Dev"** para que
pueda convivir con la versión 1.0 sin reemplazarla. La versión 1.0 está
disponible en el tag de git `v1.0`.

## Instalar

El proyecto intenta copiar el plugin automáticamente a la carpeta estándar
de VST3 de Windows:

```
C:\Program Files\Common Files\VST3\
```

Esa carpeta requiere permisos de administrador. Si la compilación termina
con un error `Permission denied` en ese paso, el plugin **sí se compiló**;
solo copia a mano la carpeta `.vst3` a esa ruta (Windows pedirá confirmar
como administrador), o corre la compilación desde una terminal abierta como
administrador.

## Uso

1. Vuelve a buscar plugins en tu DAW. En FL Studio: **Options → Manage
   Plugins → Find plugins**.
2. Inserta **"Bar Spectrum Analyzer Dev"** en la pista o bus que quieras
   medir, **justo antes** del slot donde va tu compresor.
3. Mueve el slider de **referencia de compresión** hasta el nivel donde
   quieres que el compresor empiece a actuar. Las barras iluminadas están
   por encima, y el número arriba de cada una te dice por cuánto.
4. Abre la pestaña **Phase** para revisar el ancho estéreo y la
   compatibilidad con mono.
