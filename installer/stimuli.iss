; ---------------------------------------------------------------------------
; Stimuli - Windows installer (Inno Setup 6)
;
; Build the plugin first, then compile this script:
;
;   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
;   cmake --build build --config Release --parallel
;   iscc installer\stimuli.iss
;
; Output lands in dist\Stimuli-<version>-win64-setup.exe.
;
; Layout:
;   VST3               -> C:\Program Files\Common Files\VST3\Stimuli.vst3
;   Standalone         -> C:\Program Files\Stimuli\Stimuli.exe
;   Designer + Assets  -> C:\Program Files\Stimuli\
;   Licences           -> C:\Program Files\Stimuli\licenses\
;
; Overrides (pass to iscc, e.g. iscc /DAppVersion=0.2.0 installer\stimuli.iss):
;   AppVersion  must be numeric, e.g. 0.1.0
;   BuildRoot   path to the CMake build tree, no trailing backslash needed
; ---------------------------------------------------------------------------

#define AppName "Stimuli"
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#define AppPublisher "Stimuli"
#define AppExeName "Stimuli.exe"
#define DesignerExeName "Stimuli Designer.exe"
#define Vst3BundleName "Stimuli.vst3"

; Stable identifier: changing it makes Windows treat the build as a new product
; and leaves the old entry in Apps & features.
#define AppId "{{6F8E2B4A-1C7D-4E6B-9E4A-3B1D7C5A9F20}"

#ifndef BuildRoot
  #define BuildRoot AddBackslash(SourcePath) + "..\build"
#endif

#define BuildDir AddBackslash(BuildRoot)
#define RepoDir  AddBackslash(SourcePath) + ".."

; JUCE copies the built VST3 bundle to <build>\VST3 when JUCE_VST3_COPY_DIR is
; set (Plugin/CMakeLists.txt does). Newer CMake trees keep it under the plugin
; artefacts folder instead, so accept both.
#if FileExists(BuildDir + "VST3\\" + Vst3BundleName)
  #define Vst3Bundle BuildDir + "VST3\\" + Vst3BundleName
#elif FileExists(BuildDir + "Plugin\\MvtPlugin_artefacts\\Release\\VST3\\" + Vst3BundleName)
  #define Vst3Bundle BuildDir + "Plugin\\MvtPlugin_artefacts\\Release\\VST3\\" + Vst3BundleName
#else
  #error "No Release VST3 bundle found. Run: cmake --build build --config Release, or pass /DBuildRoot=<path>."
#endif

#define StandaloneExe BuildDir + "Plugin\\MvtPlugin_artefacts\\Release\\Standalone\\" + AppExeName
#define DesignerExe   BuildDir + "Designer\\MvtDesigner_artefacts\\Release\\" + DesignerExeName

#if !FileExists(StandaloneExe)
  #error "Standalone executable not found. Build the Release configuration first."
#endif

#if !FileExists(DesignerExe)
  #error "Designer executable not found. Build the Release configuration first."
#endif

[Setup]
AppId={#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
VersionInfoVersion={#AppVersion}
VersionInfoDescription={#AppName} installer
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=EULA.txt
InfoBeforeFile={#RepoDir}\Assets\fonts\OFL.txt
OutputDir={#RepoDir}\dist
OutputBaseFilename=Stimuli-{#AppVersion}-win64-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
; The binaries link the C runtime statically, so no redistributable is needed.
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; Windows 10 1607 and later, which covers every supported Windows 10 and 11.
MinVersion=10.0.17763
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName} {#AppVersion}
AllowNoIcons=yes
DisableWelcomePage=no

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; --- VST3 plugin -----------------------------------------------------------
Source: "{#Vst3Bundle}\*"; DestDir: "{commonpf64}\VST3\{#Vst3BundleName}"; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Applications ----------------------------------------------------------
Source: "{#StandaloneExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DesignerExe}"; DestDir: "{app}"; Flags: ignoreversion

; --- Skins, themes and embedded fonts --------------------------------------
Source: "{#RepoDir}\Assets\*"; DestDir: "{app}\Assets"; Flags: ignoreversion recursesubdirs createallsubdirs

; A VST3 runs inside the host process, so it cannot find Assets next to itself.
; This shared copy is what the plugin resolves skins from in a DAW.
Source: "{#RepoDir}\Assets\*"; DestDir: "{commonappdata}\Stimuli\Assets"; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Licence texts (AGPL for the program, OFL for the embedded Nunito faces)
Source: "{#RepoDir}\LICENSE"; DestDir: "{app}\licenses"; DestName: "LICENSE-AGPL-3.0.txt"; Flags: ignoreversion
Source: "{#RepoDir}\Assets\fonts\OFL.txt"; DestDir: "{app}\licenses"; DestName: "OFL-1.1-Nunito.txt"; Flags: ignoreversion
Source: "{#RepoDir}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}\licenses"; DestName: "THIRD_PARTY_NOTICES.md"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{#DesignerExeName}"; Filename: "{app}\{#DesignerExeName}"
Name: "{group}\Licences"; Filename: "{app}\licenses"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The plugin bundle is installed outside {app}; remove it explicitly so an
; uninstall leaves no orphaned VST3 behind. Skins saved by the user under
; {app}\Assets are removed with the folder too.
Type: filesandordirs; Name: "{commonpf64}\VST3\{#Vst3BundleName}"
Type: filesandordirs; Name: "{app}\Assets"
Type: filesandordirs; Name: "{commonappdata}\Stimuli"
Type: dirifempty; Name: "{app}\licenses"
Type: dirifempty; Name: "{app}"
