#ifndef ProjectRoot
  #define ProjectRoot SourcePath
#endif
[Setup]
AppId={{714A2DA7-E923-468F-A84F-0450CA09D88E}
AppName=W3UnicodeSaveFix
AppVersion=0.1.0
AppPublisher=W3UnicodeSaveFix contributors
DefaultDirName={autopf}\W3UnicodeSaveFix
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
WizardStyle=modern
OutputDir={#ProjectRoot}\dist
OutputBaseFilename=W3UnicodeSaveFix_Setup
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=W3UnicodeSaveFix
CloseApplications=no
RestartApplications=no
SetupLogging=yes
LicenseFile={#ProjectRoot}\LICENSE
InfoBeforeFile={#ProjectRoot}\installer\INSTALL-NOTICE.txt
UninstallFilesDir={app}
[Files]
Source: "{#ProjectRoot}\build\W3UnicodeSaveFixHelper.exe"; DestDir: "{app}"; Flags: ignoreversion notimestamp
Source: "{#ProjectRoot}\dist\W3UnicodeSaveFix.asi"; DestDir: "{app}\payload"; Flags: ignoreversion notimestamp
Source: "{#ProjectRoot}\W3UnicodeSaveFix.ini"; DestDir: "{app}\payload"; Flags: ignoreversion notimestamp
Source: "{#ProjectRoot}\vendor\dinput8.dll"; DestDir: "{app}\payload"; Flags: ignoreversion notimestamp
Source: "{#ProjectRoot}\README.md"; DestDir: "{app}"; Flags: ignoreversion notimestamp
Source: "{#ProjectRoot}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion notimestamp
Source: "{#ProjectRoot}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion notimestamp
Source: "{#ProjectRoot}\BUILDING.md"; DestDir: "{app}"; Flags: ignoreversion notimestamp
Source: "{#ProjectRoot}\docs\*.md"; DestDir: "{app}\docs"; Flags: ignoreversion notimestamp
Source: "{#ProjectRoot}\build\W3UnicodeSaveFixHelper.exe"; Flags: dontcopy notimestamp
[Code]
var
  GamePage: TInputDirWizardPage;
  ReviewPage: TOutputMsgMemoWizardPage;
  ProbeFile, GamePath, SourcePath, TargetPath, BackupPath, LoaderStatus: String;
  ProbeCounter: Integer;
function Quote(S: String): String;
begin Result := '"' + S + '"'; end;
function RunHelper(Args: String; Installed: Boolean): Boolean;
var Code: Integer; Exe: String;
begin
  if Installed then Exe := ExpandConstant('{app}\W3UnicodeSaveFixHelper.exe')
  else Exe := ExpandConstant('{tmp}\W3UnicodeSaveFixHelper.exe');
  Result := Exec(Exe, Args, '', SW_HIDE, ewWaitUntilTerminated, Code) and (Code = 0);
end;
function Probe(Selected: String): Boolean;
var Args, Text: String; Lines: TArrayOfString; I, P: Integer; Key, Value: String;
begin
  ProbeCounter := ProbeCounter + 1;
  ProbeFile := ExpandConstant('{tmp}\w3-probe-') + IntToStr(ProbeCounter) + '.ini';
  Args := 'probe ' + Quote(ProbeFile);
  if Selected <> '' then Args := Args + ' ' + Quote(Selected);
  Result := RunHelper(Args, False); if not Result then exit;
  { Helper writes UTF-8; load explicitly rather than ANSI GetIniString. }
  if not LoadStringsFromFile(ProbeFile, Lines) then begin Result := False; exit; end;
  GamePath := ''; SourcePath := ''; TargetPath := ''; BackupPath := ''; LoaderStatus := '';
  for I := 0 to GetArrayLength(Lines)-1 do begin
    Text := Lines[I]; P := Pos('=', Text); if P > 0 then begin
      Key := Copy(Text,1,P-1); Value := Copy(Text,P+1,Length(Text));
      if Key='Game' then GamePath:=Value;
      if Key='Source' then SourcePath:=Value;
      if Key='Target' then TargetPath:=Value;
      if Key='Backup' then BackupPath:=Value;
      if Key='Loader' then LoaderStatus:=Value;
    end;
  end;
end;
procedure InitializeWizard;
begin
  ExtractTemporaryFile('W3UnicodeSaveFixHelper.exe');
  GamePage := CreateInputDirPage(wpLicense, 'The Witcher 3 installation', 'Select the DX12 executable folder', 'Detected Steam/GOG locations are suggested. Browse to bin\x64_dx12 if necessary. The game must be closed by you.', False, '');
  GamePage.Add('Folder containing witcher3.exe:');
  ReviewPage := CreateOutputMsgMemoPage(GamePage.ID, 'Summary', 'Review before any game/save changes', 'Backups and save migration are performed only after you click Install.', '');
  if Probe('') then GamePage.Values[0] := GamePath;
end;
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID=GamePage.ID then begin
    Result := Probe(GamePage.Values[0]); if not Result then exit;
    if (GamePath='') or (Pos('BLOCKED',LoaderStatus)=1) then begin MsgBox('A valid DX12 installation and compatible loader are required.' + #13#10 + LoaderStatus, mbError, MB_OK); Result:=False; exit; end;
    ReviewPage.RichEditViewer.Lines.Text := 'Game: The Witcher 3 (x64 DX12)' + #13#10 + 'Detected game path: ' + GamePath + #13#10 + 'Original save path: ' + SourcePath + #13#10 + 'New ASCII save path: ' + TargetPath + #13#10 + 'Backup path: ' + BackupPath + #13#10 + 'ASI loader status: ' + LoaderStatus + #13#10#13#10 + 'Original saves are never deleted. Different destination files block installation. This is an experimental runtime workaround; review README limits.';
  end;
end;
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if FileExists(ExpandConstant('{app}\installation-state.ini')) then Result := 'This mod is already registered. Uninstall it first; saves will be retained.';
end;
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep=ssPostInstall then begin
    if not RunHelper('install ' + Quote(GamePath) + ' ' + Quote(BackupPath) + ' ' + Quote(ExpandConstant('{app}\payload')) + ' ' + Quote(ExpandConstant('{app}\installation-state.ini')), True) then
      RaiseException('Game installation was stopped. Original saves and verified backups are retained. See README recovery instructions.');
  end;
end;
function InitializeUninstall: Boolean;
var State, Arg: String;
begin
  Result := True; State := ExpandConstant('{app}\installation-state.ini');
  if not FileExists(State) then exit;
  Arg := 'keep';
  if MsgBox('Copy missing/newer saves from the ASCII folder back to the ORIGINAL Documents save folder? Existing different files require a separate confirmation each. All ASCII saves and backups are kept in either case.', mbConfirmation, MB_YESNO or MB_DEFBUTTON2)=IDYES then Arg := 'restore';
  Result := RunHelper('uninstall ' + Quote(State) + ' ' + Arg, True);
end;
