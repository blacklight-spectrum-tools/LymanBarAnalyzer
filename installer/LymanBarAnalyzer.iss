; Instalador de Lyman Bar Analyzer (Windows, VST3)
; Lo compila GitHub Actions con Inno Setup 6:
;   iscc /DAppVersion=1.3.0 /DSourceVst3="ruta\Lyman Bar Analyzer.vst3" installer\LymanBarAnalyzer.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceVst3
  #define SourceVst3 "..\build\BarSpectrumAnalyzer_artefacts\Release\VST3\Lyman Bar Analyzer.vst3"
#endif

#define AppName "Lyman Bar Analyzer"
#define Publisher "Blacklight Spectrum Tools"

[Setup]
; Identificador fijo: no cambiarlo, asi las versiones nuevas se instalan encima
AppId={{6F2B8E4A-3C1D-4B7E-9A55-1D0C8E7F4A21}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL=https://github.com/blacklight-spectrum-tools
AppSupportURL=https://github.com/blacklight-spectrum-tools/LymanBarAnalyzer/issues
AppUpdatesURL=https://github.com/blacklight-spectrum-tools/LymanBarAnalyzer/releases
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
Uninstallable=yes
UninstallDisplayName={#AppName} (VST3)
CreateUninstallRegKey=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=Lyman-Bar-Analyzer-{#AppVersion}-Windows-Setup
OutputDir=Output
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Messages]
english.ReadyLabel1=Lyman will be installed in your VST3 folder (C:\Program Files\Common Files\VST3). Close your DAW before continuing, then rescan plug-ins.
spanish.ReadyLabel1=Lyman se instalará en tu carpeta VST3 (C:\Program Files\Common Files\VST3). Cierra tu DAW antes de continuar y después vuelve a buscar plugins.

[Files]
; El .vst3 es una carpeta (bundle): se copia completa
Source: "{#SourceVst3}\*"; DestDir: "{commoncf64}\VST3\{#AppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#AppName}.vst3"
