; Windows installer for Hindsight, built with Inno Setup 6 by
; .github/scripts/Package-Windows.ps1, which passes the defines below.
;
; It copies the plugin folder into C:\ProgramData\obs-studio\plugins, the same
; place the manual install uses, and registers an uninstaller under
; Settings > Apps. Running a newer installer updates in place: the AppId stays
; the same, and the streamer's keys live in OBS's plugin_config folder, which
; this never touches.

#ifndef PluginName
  #error PluginName must be defined (for example /DPluginName=obs-hindsight)
#endif
#ifndef DisplayName
  #error DisplayName must be defined
#endif
#ifndef AppVersion
  #error AppVersion must be defined
#endif
#ifndef FileVersion
  #error FileVersion must be defined (numbers only, for example 0.2.0)
#endif
#ifndef Publisher
  #error Publisher must be defined
#endif
#ifndef Website
  #error Website must be defined
#endif
#ifndef SourceDir
  #error SourceDir must be defined (the installed plugin folder)
#endif
#ifndef OutputDir
  #error OutputDir must be defined
#endif
#ifndef OutputName
  #error OutputName must be defined
#endif

[Setup]
; Never change the AppId: Windows uses it to recognize an update of the same
; app rather than a second install.
AppId={{88702AD4-C2BD-4955-B34C-261EE7D6F955}
AppName={#DisplayName} for OBS
AppVersion={#AppVersion}
AppVerName={#DisplayName} for OBS {#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL={#Website}
AppSupportURL={#Website}/issues
AppUpdatesURL={#Website}/releases/latest
VersionInfoVersion={#FileVersion}
DefaultDirName={commonappdata}\obs-studio\plugins\{#PluginName}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
; ProgramData\obs-studio\plugins is shared by every Windows user, so writing
; there needs administrator rights (one UAC prompt).
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; If OBS has the plugin loaded during an update, offer to close it.
CloseApplications=yes
RestartApplications=no
UninstallDisplayName={#DisplayName} for OBS
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes

[Messages]
WelcomeLabel2=This will install [name/ver] into OBS Studio.%n%nClose OBS before you continue. If it is open, Setup will offer to close it.
FinishedLabel=Setup has installed [name] into OBS Studio. Start OBS and add the Hindsight filter to your microphone.

[InstallDelete]
; Clear the previous version's files first so nothing stale is left behind.
Type: filesandordirs; Name: "{app}\bin"
Type: filesandordirs; Name: "{app}\data"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
