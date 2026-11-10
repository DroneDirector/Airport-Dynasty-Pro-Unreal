// The world's terrain packs: downloaded and unpacked by a hidden PowerShell helper (no extra programs: Windows' own),
// which writes its progress to progress.json for Settings (see TerrainPacks.h).
#include "TerrainPacks.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <shlobj.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace ADPTerrain
{
	static const TCHAR* Base = TEXT("https://github.com/DroneDirector/Airport-Dynasty-Pro-Unreal/releases/download/terrain-v1");
	static FProcHandle Proc;

	static FString Home()
	{
		FString Local = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
		if (Local.IsEmpty()) Local = FPaths::ProjectSavedDir();
		return FPaths::Combine(Local, TEXT("AirportDynastyPro"));
	}

	FString Dir()
	{
		// a folder the player moved it to (Settings), else the user folder
		FString Moved;
		if (FFileHelper::LoadFileToString(Moved, *(Home() / TEXT("terrain-location.txt"))))
		{
			Moved.TrimStartAndEndInline();
			if (!Moved.IsEmpty()) return Moved;
		}
		return Home() / TEXT("Terrain");
	}

	bool Ready() { return FPaths::FileExists(Dir() / TEXT("complete.flag")) && FPaths::FileExists(Dir() / TEXT("layer.json")); }

	bool Busy() { return Proc.IsValid() && FPlatformProcess::IsProcRunning(Proc); }

	void Start()
	{
		if (Busy()) return;
		const FString D = Dir();
		IFileManager::Get().MakeDirectory(*D, true);
		// the helper: the manifest, then each pack not installed yet (or changed): downloaded, its checksum checked,
		// unpacked next to the others (moved in place, so an update replaces the old tiles), noted as installed
		const FString Script = TEXT(R"PS1(param([string]$Base, [string]$Dir)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$prog = Join-Path $Dir 'progress.json'
function Say($o) { $o | ConvertTo-Json -Compress | Set-Content -Path $prog -Encoding UTF8 }
try {
  [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $dl = Join-Path $Dir '_download'; New-Item -ItemType Directory -Force $dl | Out-Null
  Say @{ state = 'checking' }
  $man = Invoke-RestMethod "$Base/manifest.json"
  $instFile = Join-Path $Dir 'installed.json'
  $inst = @{}
  if (Test-Path $instFile) { (Get-Content $instFile -Raw | ConvertFrom-Json).psobject.Properties | ForEach-Object { $inst[$_.Name] = $_.Value } }
  $todo = @($man.packs | Where-Object { $inst[$_.name] -ne $_.sha256 })
  $total = [double](($todo | Measure-Object -Property size -Sum).Sum); $done = 0.0; $i = 0
  foreach ($p in $todo) {
    $i++
    Say @{ state = 'downloading'; pack = $p.name; packsDone = $i - 1; packsTotal = $todo.Count; done = $done; total = $total }
    $zip = Join-Path $dl $p.name
    Invoke-WebRequest "$Base/$($p.name)" -OutFile $zip -UseBasicParsing
    if ((Get-FileHash $zip -Algorithm SHA256).Hash.ToLower() -ne $p.sha256) { throw "the pack $($p.name) arrived damaged" }
    Say @{ state = 'unpacking'; pack = $p.name; packsDone = $i - 1; packsTotal = $todo.Count; done = $done; total = $total }
    $tmp = Join-Path $dl '_unpack'
    if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
    [IO.Compression.ZipFile]::ExtractToDirectory($zip, $tmp)
    robocopy $tmp $Dir /E /MOVE /NFL /NDL /NJH /NJS /NP /MT:8 | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "the pack $($p.name) couldn't be put in place" }
    Remove-Item $zip -Force
    $inst[$p.name] = $p.sha256
    $inst | ConvertTo-Json | Set-Content -Path $instFile -Encoding UTF8
    $done += [double]$p.size
  }
  Set-Content -Path (Join-Path $Dir 'complete.flag') -Value $man.version
  Say @{ state = 'ready'; packsDone = $todo.Count; packsTotal = $todo.Count; done = $total; total = $total }
} catch { Say @{ state = 'error'; error = $_.Exception.Message } }
)PS1");
		const FString File = D / TEXT("get-terrain.ps1");
		FFileHelper::SaveStringToFile(Script, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		const FString Args = FString::Printf(TEXT("-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\" -Base \"%s\" -Dir \"%s\""), *File, Base, *D);
		Proc = FPlatformProcess::CreateProc(TEXT("powershell.exe"), *Args, true, true, true, nullptr, 0, nullptr, nullptr);
	}

	void ChooseAndMove()
	{
		if (Busy()) return;
		// Windows' own folder picker
		wchar_t Path[MAX_PATH] = {};
		BROWSEINFOW Bi = {};
		Bi.lpszTitle = L"Where should the world's terrain be kept? (about 6.6 GB)";
		Bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
		LPITEMIDLIST Item = SHBrowseForFolderW(&Bi);
		if (!Item) return;
		const bool bOk = SHGetPathFromIDListW(Item, Path) != 0;
		CoTaskMemFree(Item);
		if (!bOk) return;
		const FString From = Dir(), To = FPaths::Combine(FString(Path), TEXT("Airport Dynasty Pro Terrain"));
		if (FPaths::IsSamePath(From, To)) return;
		IFileManager::Get().MakeDirectory(*To, true);
		// moved by robocopy (resumable; quick on the same drive), then the new place noted for the game
		const FString Script = FString::Printf(TEXT(R"PS1($ErrorActionPreference = 'Stop'
$prog = Join-Path '%s' 'progress.json'
function Say($o) { $o | ConvertTo-Json -Compress | Set-Content -Path $prog -Encoding UTF8 }
try {
  Say @{ state = 'moving'; to = '%s' }
  if (Test-Path '%s') { robocopy '%s' '%s' /E /MOVE /NFL /NDL /NJH /NJS /NP /MT:16 | Out-Null; if ($LASTEXITCODE -ge 8) { throw 'some files could not be moved' } }
  Set-Content -Path '%s' -Value '%s' -Encoding UTF8
  $prog = Join-Path '%s' 'progress.json'
  Say @{ state = 'moved'; to = '%s' }
} catch { Say @{ state = 'error'; error = $_.Exception.Message } }
)PS1"), *From, *To, *From, *From, *To, *(Home() / TEXT("terrain-location.txt")), *To, *To, *To);
		const FString File = Home() / TEXT("move-terrain.ps1");
		FFileHelper::SaveStringToFile(Script, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		const FString Args = FString::Printf(TEXT("-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\""), *File);
		Proc = FPlatformProcess::CreateProc(TEXT("powershell.exe"), *Args, true, true, true, nullptr, 0, nullptr, nullptr);
	}

	FString Status()
	{
		FString Text;
		TSharedPtr<FJsonObject> O;
		const bool bHave = FFileHelper::LoadFileToString(Text, *(Dir() / TEXT("progress.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) && O.IsValid();
		const FString State = bHave ? O->GetStringField(TEXT("state")) : FString();
		if (Busy() || State == TEXT("downloading") || State == TEXT("unpacking") || State == TEXT("checking"))
		{
			if (!bHave || State == TEXT("checking")) return TEXT("Looking for the world's terrain…");
			const double Done = O->GetNumberField(TEXT("done")) / 1e9, Total = O->GetNumberField(TEXT("total")) / 1e9;
			return FString::Printf(TEXT("%s the world's terrain: pack %d of %d, %.1f of %.1f GB"), State == TEXT("unpacking") ? TEXT("Unpacking") : TEXT("Downloading"),
				int32(O->GetNumberField(TEXT("packsDone"))) + 1, int32(O->GetNumberField(TEXT("packsTotal"))), Done, Total);
		}
		if (State == TEXT("moving")) return TEXT("Moving the world's terrain to ") + O->GetStringField(TEXT("to")) + TEXT("…");
		if (State == TEXT("moved")) return TEXT("The world's terrain is kept in ") + Dir() + TEXT(".");
		if (State == TEXT("error")) return TEXT("The terrain download stopped: ") + O->GetStringField(TEXT("error")) + TEXT(" Try again.");
		if (Ready() && State != TEXT("ready")) return TEXT("The world's terrain is installed, in ") + Dir() + TEXT(".");
		if (Ready()) return State == TEXT("ready") ? TEXT("The world's terrain is installed (mountains and valleys everywhere; used from the next start where it was just downloaded).") : TEXT("The world's terrain is installed.");
		return TEXT("The world's terrain isn't on this PC yet (about 6.6 GB). Without it the Earth is smooth.");
	}
}
