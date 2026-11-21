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
	static const TCHAR* Base = TEXT("https://github.com/DroneDirector/Airport-Dynasty-Pro-Unreal/releases/download/terrain-v3");
	// (0.18.0: terrain-v3 = the world + swissALTI3D round Chablais, the land cover, Chablais's 10 cm photos; an older set
	//  installed counts as not ready, so Settings asks for the update - only changed packs are fetched)
	static const TCHAR* Version = TEXT("3");
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

	bool Ready()
	{
		FString Flag;
		if (!FFileHelper::LoadFileToString(Flag, *(Dir() / TEXT("complete.flag"))) || !FPaths::FileExists(Dir() / TEXT("layer.json"))) return false;
		Flag.TrimStartAndEndInline();
		return Flag == Version;
	}

	static bool Read(TSharedPtr<FJsonObject>& O)
	{
		FString Text;
		return FFileHelper::LoadFileToString(Text, *(Dir() / TEXT("progress.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) && O.IsValid();
	}
	// a helper from an earlier start of the game still working (it writes progress.json every 2 s): don't start a second
	static bool HelperAlive()
	{
		TSharedPtr<FJsonObject> O;
		if (!Read(O)) return false;
		const FString State = O->GetStringField(TEXT("state"));
		if (State != TEXT("downloading") && State != TEXT("unpacking") && State != TEXT("checking")) return false;
		const FDateTime T = IFileManager::Get().GetTimeStamp(*(Dir() / TEXT("progress.json")));
		return T != FDateTime::MinValue() && (FDateTime::UtcNow() - T).GetTotalSeconds() < 120.0;
	}
	bool Busy() { return (Proc.IsValid() && FPlatformProcess::IsProcRunning(Proc)) || HelperAlive(); }

	// the bytes on disk in the download folder: what has arrived of the packs on the way (four at once)
	static double PartBytes()
	{
		double Sum = 0;
		IFileManager::Get().IterateDirectory(*(Dir() / TEXT("_download")), [&Sum](const TCHAR* Name, bool bDir)
		{
			if (!bDir && FString(Name).EndsWith(TEXT(".zip"))) Sum += double(IFileManager::Get().FileSize(Name));
			return true;
		});
		return Sum;
	}

	void Start()
	{
		if (Busy()) return;
		const FString D = Dir();
		IFileManager::Get().MakeDirectory(*D, true);
		// the helper: the manifest, then each pack not installed yet (or changed). Owner, 2026-11-17: the download was
		// very slow and stopped after a few percent, so: four packs come at once (Windows' own curl.exe, each its own
		// connection); a dropped connection is retried and carries on where it broke off (a half-fetched pack stays on
		// disk and is resumed, also after a restart of the game). Each pack is checked against its checksum (a damaged
		// one is fetched once more), unpacked next to the others (moved in place, so an update replaces the old tiles)
		// and noted as installed. It writes progress.json every 2 s, which is also how a restarted game sees it running.
		const FString Script = TEXT(R"PS1(param([string]$Base, [string]$Dir)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$prog = Join-Path $Dir 'progress.json'
function Say($o) { $o.pid = $PID; $o | ConvertTo-Json -Compress | Set-Content -Path $prog -Encoding UTF8 }
$running = @{}
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
  $total = [double](($todo | Measure-Object -Property size -Sum).Sum); $done = 0.0; $nDone = 0
  $curl = Join-Path $env:SystemRoot 'System32\curl.exe'
  $useCurl = Test-Path $curl
  $queue = New-Object System.Collections.Queue; $todo | ForEach-Object { $queue.Enqueue($_) }
  $running = @{}; $tries = @{}; $ready = New-Object System.Collections.Queue
  function Fetch($p) {
    $zip = Join-Path $dl $p.name
    if ($useCurl) {
      # -C -: carry on from what is already on disk; a connection under 1 KB/s for 60 s is dropped (and retried)
      $a = @('-L', '--fail', '-s', '-C', '-', '--retry', '10', '--retry-delay', '5', '--connect-timeout', '30', '--speed-limit', '1024', '--speed-time', '60', '-o', ('"' + $zip + '"'), ('"' + "$Base/$($p.name)" + '"'))
      return Start-Process -FilePath $curl -ArgumentList $a -WindowStyle Hidden -PassThru
    }
    Invoke-WebRequest "$Base/$($p.name)" -OutFile $zip -UseBasicParsing; return $null
  }
  function Beat($state) { Say @{ state = $state; packsDone = $nDone; packsTotal = $todo.Count; done = $done; total = $total } }
  while ($queue.Count -gt 0 -or $running.Count -gt 0 -or $ready.Count -gt 0) {
    while ($queue.Count -gt 0 -and $running.Count -lt 4) {
      $p = $queue.Dequeue(); $pr = Fetch $p
      if ($pr) { $running[$p.name] = @{ p = $p; proc = $pr } } else { $ready.Enqueue($p) }
    }
    foreach ($k in @($running.Keys)) {
      $r = $running[$k]
      if (-not $r.proc.HasExited) { continue }
      $running.Remove($k)
      $zip = Join-Path $dl $k
      $size = 0; if (Test-Path $zip) { $size = (Get-Item $zip).Length }
      # all of it there (a pack already whole when resumed can come back with an error code: the checksum decides)
      if ($size -ge [int64]$r.p.size) { $ready.Enqueue($r.p); continue }
      # broken off: again from where it stopped, up to 6 times (a server that can't carry on - curl 33/36 - from the start)
      if ($r.proc.ExitCode -eq 33 -or $r.proc.ExitCode -eq 36) { Remove-Item $zip -Force -ErrorAction SilentlyContinue }
      $tries[$k] = 1 + [int]$tries[$k]
      if ($tries[$k] -gt 6) { throw "the pack $k could not be downloaded (curl $($r.proc.ExitCode)); press the button again and it carries on from here" }
      Start-Sleep -Seconds (3 * $tries[$k]); $queue.Enqueue($r.p)
    }
    if ($ready.Count -gt 0) {
      $p = $ready.Dequeue(); $zip = Join-Path $dl $p.name
      Beat 'unpacking'
      if ((Get-FileHash $zip -Algorithm SHA256).Hash.ToLower() -ne $p.sha256) {
        Remove-Item $zip -Force
        $bad = '#' + $p.name; $tries[$bad] = 1 + [int]$tries[$bad]
        if ($tries[$bad] -gt 1) { throw "the pack $($p.name) arrived damaged twice" }
        $queue.Enqueue($p); continue
      }
      $tmp = Join-Path $dl '_unpack'
      if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
      [IO.Compression.ZipFile]::ExtractToDirectory($zip, $tmp)
      robocopy $tmp $Dir /E /MOVE /NFL /NDL /NJH /NJS /NP /MT:8 | Out-Null
      if ($LASTEXITCODE -ge 8) { throw "the pack $($p.name) couldn't be put in place" }
      Remove-Item $zip -Force
      $inst[$p.name] = $p.sha256
      $inst | ConvertTo-Json | Set-Content -Path $instFile -Encoding UTF8
      $done += [double]$p.size; $nDone++
      Beat 'downloading'
      continue
    }
    Beat 'downloading'; Start-Sleep -Seconds 2
  }
  Set-Content -Path (Join-Path $Dir 'complete.flag') -Value $man.version
  Say @{ state = 'ready'; packsDone = $todo.Count; packsTotal = $todo.Count; done = $total; total = $total }
} catch {
  foreach ($r in $running.Values) { try { $r.proc.Kill() } catch {} }
  Say @{ state = 'error'; error = $_.Exception.Message }
}
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

	float Fraction()
	{
		TSharedPtr<FJsonObject> O;
		if (!Read(O)) return -1.f;
		const FString State = O->GetStringField(TEXT("state"));
		if (State != TEXT("downloading") && State != TEXT("unpacking")) return -1.f;
		const double T = O->GetNumberField(TEXT("total")), D = O->GetNumberField(TEXT("done"));
		// the finished packs plus everything that has arrived of the ones on the way
		return T > 0 ? float(FMath::Clamp((D + PartBytes()) / T, 0.0, 1.0)) : -1.f;
	}

	double Speed()
	{
		static double LastT = 0, LastB = -1, Rate = 0;
		const float F = Fraction();
		if (F < 0) { LastB = -1; Rate = 0; return 0; }
		TSharedPtr<FJsonObject> O;
		if (!Read(O)) return Rate;
		const double B = F * O->GetNumberField(TEXT("total")), Now = FPlatformTime::Seconds();
		if (LastB < 0 || B < LastB) { LastB = B; LastT = Now; return Rate; }
		if (Now - LastT > 1.0) { const double R = (B - LastB) / (Now - LastT); Rate = Rate > 0 ? Rate * 0.6 + R * 0.4 : R; LastB = B; LastT = Now; }
		return Rate;
	}

	FString Status()
	{
		TSharedPtr<FJsonObject> O;
		const bool bHave = Read(O);
		const FString State = bHave ? O->GetStringField(TEXT("state")) : FString();
		if (Busy() || State == TEXT("downloading") || State == TEXT("unpacking") || State == TEXT("checking"))
		{
			if (!bHave || State == TEXT("checking")) return TEXT("Looking for the world's terrain…");
			// owner, 2026-11-17: the GB didn't count up (it only counted finished packs, ~50 MB each) - "it should increase
			// for every file it gets": now the bytes as they arrive, and the packs count up one by one as each is in
			const double Total = O->GetNumberField(TEXT("total")) / 1e9;
			const double Done = FMath::Min(Total, (O->GetNumberField(TEXT("done")) + PartBytes()) / 1e9);
			return FString::Printf(TEXT("Downloading the world's terrain: %.2f of %.1f GB  -  %d of %d packs in"),
				Done, Total, int32(O->GetNumberField(TEXT("packsDone"))), int32(O->GetNumberField(TEXT("packsTotal"))));
		}
		if (State == TEXT("moving")) return TEXT("Moving the world's terrain to ") + O->GetStringField(TEXT("to")) + TEXT("…");
		if (State == TEXT("moved")) return TEXT("The world's terrain is kept in ") + Dir() + TEXT(".");
		if (State == TEXT("error")) return TEXT("The terrain download stopped: ") + O->GetStringField(TEXT("error")) + TEXT(" Try again.");
		if (Ready() && State != TEXT("ready")) return TEXT("The world's terrain is installed, in ") + Dir() + TEXT(".");
		if (Ready()) return State == TEXT("ready") ? TEXT("The world's terrain is installed (mountains and valleys everywhere; used from the next start where it was just downloaded).") : TEXT("The world's terrain is installed.");
		if (FPaths::FileExists(Dir() / TEXT("complete.flag")))
			return TEXT("A new version of the world's terrain is out: Switzerland's heights round Chablais Airport and its 10 cm aerial photos. Press the button below - only the changed packs are downloaded. Until then the Earth is smooth.");
		return TEXT("The world's terrain isn't on this PC yet (about 37 GB, with Chablais Airport's 10 cm aerial photos). Without it the Earth is smooth.");
	}
}
