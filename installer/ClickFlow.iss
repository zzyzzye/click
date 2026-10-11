#ifndef AppVersion
  #error AppVersion must be provided by the release script.
#endif
#ifndef SourceDir
  #error SourceDir must be provided by the release script.
#endif
#ifndef OutputDir
  #error OutputDir must be provided by the release script.
#endif

#define AppName "ClickFlow"
#define AppPublisher "ClickFlow"
#define AppExeName "ClickFlow.exe"
#define AppIdValue "C2A5B39A-329F-4EF3-8E1F-9A2C2C93281C"
#define ProjectUrl "https://github.com/zzyzzye/click"
#ifndef AppArchitecture
  #define AppArchitecture "x64"
#endif
#if AppArchitecture == "arm64"
  #define AllowedArchitecture "arm64"
  #define PlatformName "win-arm64"
#else
  #define AllowedArchitecture "x64compatible"
  #define PlatformName "win64"
#endif

[Setup]
AppId={{{#AppIdValue}}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#ProjectUrl}
AppSupportURL={#ProjectUrl}/issues
AppUpdatesURL={#ProjectUrl}/releases
DefaultDirName={autopf}\{#AppName}
UsePreviousAppDir=yes
UsePreviousTasks=yes
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed={#AllowedArchitecture}
ArchitecturesInstallIn64BitMode={#AllowedArchitecture}
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename={#AppName}-{#AppVersion}-{#PlatformName}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\src\app\resources\ClickFlow.ico
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}
CloseApplications=force
CloseApplicationsFilter={#AppExeName}
RestartApplications=no
ChangesAssociations=no
DisableWelcomePage=no
SetupLogging=yes
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} Windows Installer
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}

[Languages]
Name: "chinesesimp"; MessagesFile: "languages\ChineseSimplified.isl"

[Tasks]
Name: "startmenuicon"; Description: "创建开始菜单快捷方式"; GroupDescription: "快捷方式："; Flags: checkedonce
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式："; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: startmenuicon
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "运行 {#AppName}"; Flags: nowait postinstall skipifsilent; Check: not IsAppUpdate

[Registry]
Root: HKCU; Subkey: "Software\ClickFlow\ClickFlow"; Flags: uninsdeletekey

[UninstallDelete]
Type: filesandordirs; Name: "{userappdata}\ClickFlow\ClickFlow"

[Code]
function IsAppUpdate: Boolean;
begin
  Result := ExpandConstant('{param:ClickFlowUpdate|0}') = '1';
end;

procedure InitializeWizard;
begin
  if IsAppUpdate then
    WizardForm.Caption := '正在更新 ClickFlow';
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  PreviousDir: String;
begin
  Result := '';
  if not IsAppUpdate then exit;
  { 更新必须识别原安装；不允许更新助手意外创建第二套安装。 }
  if not RegQueryStringValue(HKLM64,
      'Software\Microsoft\Windows\CurrentVersion\Uninstall\{C2A5B39A-329F-4EF3-8E1F-9A2C2C93281C}_is1',
      'InstallLocation', PreviousDir) then
    Result := '找不到原安装，已取消更新。请手动运行安装包。'
  else if CompareText(RemoveBackslashUnlessRoot(PreviousDir),
                     RemoveBackslashUnlessRoot(ExpandConstant('{app}'))) <> 0 then
    Result := '更新目录与原安装不一致，已取消更新。请手动运行安装包。';
end;
