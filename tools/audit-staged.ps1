# Reviews the exact Git index. Reports file/rule only, never matched secret values.
param()
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$gitArgs=@('-c',("safe.directory="+$root),'-C',$root)
$files=@(& git @gitArgs diff --cached --name-only --diff-filter=ACMR)
if($LASTEXITCODE -ne 0 -or $files.Count -eq 0){throw 'No staged changes to review'}
$findings=[Collections.Generic.List[string]]::new()
foreach($file in $files){
 if($file -match '(?i)(^|/)(build|dist|vendor|\.vs|obj|Debug|Release|x64)/|\.(sav|png|pml|csv|dmp|mdmp|log|exe|dll|asi|zip|7z|obj|pdb|ilk|lib|exp|key|pem|pfx|p12)$'){$findings.Add("$file : forbidden artifact/state")}
 $size=& git @gitArgs cat-file -s (':'+$file)
 if($LASTEXITCODE -ne 0){throw 'Cannot inspect staged blob'}
 if([long]$size -gt 262144){$findings.Add("$file : unexpectedly large source file")}
 $lines=@(& git @gitArgs show (':'+$file))
 if($LASTEXITCODE -ne 0){throw 'Cannot read staged file'}
 $text=$lines -join "`n"
 if($text.Contains([char]0)){$findings.Add("$file : binary/NUL content")}
 $rules=@{
  'credential pattern'='(?i)gh[pousr]_[a-z0-9]{30,}|github_pat_[a-z0-9_]{40,}|AKIA[0-9A-Z]{16}|sk-[a-zA-Z0-9_-]{32,}|-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----'
  'assigned literal secret'='(?i)(password|api[_-]?key|access[_-]?token|client[_-]?secret)\s*[:=]\s*["''][^"''\r\n]{8,}["'']'
  'personal SID'='S-1-5-21-\d+-\d+-\d+-\d+'
  'historical memory address'='\b0x[0-9a-fA-F]{12,16}\b'
 }
 foreach($label in $rules.Keys){if($text -match $rules[$label]){$findings.Add("$file : $label")}}
 # Only explicit bug-example documents may contain a real profile path.
 $paths=[regex]::Matches($text,'(?i)[a-z]:[\\/]+Users[\\/]+([^\\/\s"`]+)')
 foreach($path in $paths){
  $synthetic=($file -eq 'src/tests.cpp' -and $path.Groups[1].Value -in @('Tester','u4e2d'))
  if($file -notin @('README.md','docs/TECHNICAL_ANALYSIS.md') -and -not $synthetic){$findings.Add("$file : profile path outside explicit bug example")}
 }
}
& git @gitArgs diff --cached --check
if($LASTEXITCODE -ne 0){$findings.Add('Whitespace/conflict-marker check failed')}
if($findings.Count){$findings | Sort-Object -Unique | Write-Output;throw 'Staged review failed'}
"PASS: $($files.Count) staged text files; no forbidden artifacts or matched credential/personal-state/address patterns. Manual review still required."
