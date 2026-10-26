; Inno Setup script -> LunaFX-Setup.exe
#define AppName "Luna FX"
#define AppVer  "1.1.1"
#define Art     "..\build\LunaFX_artefacts\Release"

[Setup]
AppId={{8D2A6C1E-9B43-4D0C-AE52-BCDE28A11112}
AppName={#AppName}
AppVersion={#AppVer}
AppPublisher=LunaLabs
DefaultDirName={autopf64}\{#AppName}
DefaultGroupName={#AppName}
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
OutputDir=dist
OutputBaseFilename=LunaFX-Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
UninstallDisplayName={#AppName}
WizardStyle=modern

[Types]
Name: "full";   Description: "VST3 plugin + Standalone"
Name: "vst";    Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plugin";           Types: full vst custom; Flags: fixed
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
Source: "{#Art}\VST3\Luna FX.vst3\*";    DestDir: "{commoncf64}\VST3\Luna FX.vst3"; Flags: recursesubdirs ignoreversion; Components: vst3
Source: "{#Art}\Standalone\Luna FX.exe"; DestDir: "{app}";                            Flags: ignoreversion;               Components: standalone

[Icons]
Name: "{group}\Luna FX";           Filename: "{app}\Luna FX.exe"; Components: standalone
Name: "{group}\Uninstall Luna FX"; Filename: "{uninstallexe}"
