
#define GETVER=Copy(ISOTOXINVER,1,Len(ISOTOXINVER)-1)

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{DC27C193-E910-4FE3-8BA7-F7E9B22F8339}
AppName=Isotoxin
AppVersion={#GETVER}
;AppVerName=Isotoxin 0.4.469
AppPublisher=Isotoxin dev
AppPublisherURL=http://isotoxin.im
AppSupportURL=http://isotoxin.im
AppUpdatesURL=http://isotoxin.im
DefaultDirName={pf}\Isotoxin
DefaultGroupName=Isotoxin
AllowNoIcons=yes
OutputDir=C:\_dev\isotoxin\~inno\result
OutputBaseFilename=isetup
SetupIconFile=C:\_dev\isotoxin\source\isotoxin\res\isotoxin.ico
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64

[Code]

procedure PrepareConfig();
var
  P, C : String;
begin
  C := ExpandConstant('{%USERNAME}');
  if C = '' then begin
    C := 'default';
  end;
  P := ExpandConstant('{userappdata}') + '\Isotoxin\' + C + '.profile';
  C := ExpandConstant('{userappdata}') + '\Isotoxin\config.db';
  if not FileExists(P) then begin
    SaveStringToFile(P, '', False);
  end;
  if not FileExists(C) then begin
    SaveStringToFile(C, '', False);
  end;
end;

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startup"; Description: "{cm:AutoStartProgram, Isotoxin} on login"; GroupDescription: "{cm:AutoStartProgramGroupDescription}"

[Files]
Source: "..\~inno\isotoxin.exe"; DestDir: "{app}"; Flags: ignoreversion 32bit           ; Check: not Is64BitInstallMode
Source: "..\~inno\plghost.exe"; DestDir: "{app}"; Flags: ignoreversion 32bit            ; Check: not Is64BitInstallMode
Source: "..\~inno\proto.lan.dll"; DestDir: "{app}"; Flags: ignoreversion 32bit          ; Check: not Is64BitInstallMode
Source: "..\~inno\proto.tox.dll"; DestDir: "{app}"; Flags: ignoreversion 32bit          ; Check: not Is64BitInstallMode
Source: "..\~inno\proto.xmp.dll"; DestDir: "{app}"; Flags: ignoreversion 32bit          ; Check: not Is64BitInstallMode
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
Source: "..\~inno\64\isotoxin.exe"; DestDir: "{app}"; Flags: ignoreversion 64bit        ; Check: Is64BitInstallMode
Source: "..\~inno\64\plghost.exe"; DestDir: "{app}"; Flags: ignoreversion 64bit         ; Check: Is64BitInstallMode
Source: "..\~inno\64\proto.lan.dll"; DestDir: "{app}"; Flags: ignoreversion 64bit       ; Check: Is64BitInstallMode
Source: "..\~inno\64\proto.tox.dll"; DestDir: "{app}"; Flags: ignoreversion 64bit       ; Check: Is64BitInstallMode
Source: "..\~inno\64\proto.xmp.dll"; DestDir: "{app}"; Flags: ignoreversion 64bit       ; Check: Is64BitInstallMode

Source: "C:\_dev\isotoxin\~inno\isotoxin.data"; DestDir: "{app}"; Flags: ignoreversion; AfterInstall: PrepareConfig()

[Icons]
Name: "{group}\Isotoxin"; Filename: "{app}\isotoxin.exe"
Name: "{commondesktop}\Isotoxin"; Filename: "{app}\isotoxin.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\isotoxin.exe"; Description: "{cm:LaunchProgram,Isotoxin}"; Flags: nowait postinstall skipifsilent

[Dirs]
Name: "{userappdata}\Isotoxin"; Flags: uninsneveruninstall

[Registry]
Root: "HKCU"; Subkey: "Software\\Microsoft\\Windows\\CurrentVersion\\Run"; ValueType: string; ValueName: "isotoxin.run"; ValueData: "{app}\isotoxin.exe minimize"; Flags: noerror uninsdeletevalue; Tasks: startup
