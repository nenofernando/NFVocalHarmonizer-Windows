#ifndef BuildRoot
  #error BuildRoot must point to build-vst3-windows-x64
#endif

#ifndef OutputDir
  #define OutputDir ".\dist"
#endif

#define PluginName "NF Vocal Harmonizer"
#define PluginVersion "1.0.0"
#define InstallerVersion "1.0.0"
#define Publisher "NF Audio Tools"
#define Copyright "NF Audio Tools - By Nenno Fernando. All rights reserved."
#define AuthorLine "NF Audio Tools - By Nenno Fernando"

[Setup]
AppId={{B7F1E2A0-9C4D-4B8E-A1F3-5D6C7E8F9012}
AppName={#PluginName}
AppVersion={#PluginVersion}
AppVerName={#PluginName} {#PluginVersion}
AppPublisher={#Publisher}
AppCopyright={#Copyright}
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=no
DefaultGroupName={#PluginName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=NF-Vocal-Harmonizer-Windows-x64-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#PluginName} {#PluginVersion}
VersionInfoVersion={#InstallerVersion}
VersionInfoCompany={#Publisher}
VersionInfoCopyright={#Copyright}
VersionInfoDescription={#PluginName} {#PluginVersion} Windows x64 VST3 installer ({#AuthorLine})
VersionInfoProductName={#PluginName}
VersionInfoProductVersion={#PluginVersion}
SetupLogging=yes
InfoBeforeFile=welcome_en.txt
InfoAfterFile=finish_en.txt

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "manualicon"; Description: "Create Start Menu shortcuts to the English and Portuguese manuals"; GroupDescription: "Shortcuts:"; Flags: checkedonce

[Files]
Source: "{#BuildRoot}\NFVocalHarmonizer_artefacts\Release\VST3\NF Vocal Harmonizer.vst3\*"; DestDir: "{commoncf64}\VST3\NF Vocal Harmonizer.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\Manuals\pdf\*"; DestDir: "{commonappdata}\NF Audio Tools\NF Vocal Harmonizer\Manuals"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\Manuals\pdf\*"; DestDir: "{commoncf64}\VST3\NF Vocal Harmonizer.vst3\Contents\Resources\Manuals"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#PluginName} Manual (English)"; Filename: "{commonappdata}\NF Audio Tools\NF Vocal Harmonizer\Manuals\NF_Vocal_Harmonizer_User_Manual_English.pdf"; Tasks: manualicon
Name: "{group}\{#PluginName} Manual (Portuguese)"; Filename: "{commonappdata}\NF Audio Tools\NF Vocal Harmonizer\Manuals\NF_Vocal_Harmonizer_Manual_Portugues.pdf"; Tasks: manualicon
Name: "{group}\Uninstall {#PluginName}"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\NF Vocal Harmonizer.vst3"
Type: filesandordirs; Name: "{commonappdata}\NF Audio Tools\NF Vocal Harmonizer"
