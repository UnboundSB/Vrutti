[Setup]
AppName=Vrutti Alpha SleepyRabbit
AppVersion=0.0.1
AppPublisher=UnboundSB
AppPublisherURL=https://github.com/UnboundSB/Vrutti
DefaultDirName={autopf}\Vrutti
DefaultGroupName=Vrutti
OutputBaseFilename=Vrutti_Alpha_0.0.1_SleepyRabbit_Setup
OutputDir=..\
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; SetupIconFile=..\logos\vrutti-logo.ico
UninstallDisplayIcon={app}\vrutti.exe

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; The main executable
Source: "..\Vrutti_Release\vrutti.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Vrutti_Release\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; The extension host dependencies
Source: "..\Vrutti_Release\src\ext\*"; DestDir: "{app}\src\ext"; Flags: ignoreversion recursesubdirs createallsubdirs

; The frontend web bundle
Source: "..\Vrutti_Release\src\ui\frontend\dist\*"; DestDir: "{app}\src\ui\frontend\dist"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Vrutti IDE"; Filename: "{app}\vrutti.exe"
Name: "{commondesktop}\Vrutti IDE"; Filename: "{app}\vrutti.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\vrutti.exe"; Description: "Launch Vrutti IDE"; Flags: nowait postinstall skipifsilent
