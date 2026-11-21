// The in-game updater (Settings → Updates), as in a released game: "Check for updates" asks GitHub for newer PC
// versions; each release carries a small update package from the version before it (only the files that changed:
// Unreal's patch pak with the changed content, the game's program when its code changed, the menus' changed files),
// so "Install update" downloads only those, one after another from the version you have to the newest. Then the game
// closes, a small script puts the files in place (each checked against its fingerprint), and the game starts again.
// The terrain has its own packs by region (only the regions that changed come down).
#include "UI/ADPHUD.h"
#include "ADPLog.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Runtime/Launch/Resources/Version.h"

namespace
{
	const TCHAR* Releases = TEXT("https://api.github.com/repos/DroneDirector/Airport-Dynasty-Pro-Unreal/releases?per_page=60");

	/** 0.12.1 → {0, 12, 1}; compares versions part by part. */
	int32 Compare(const FString& A, const FString& B)
	{
		TArray<FString> X, Y;
		A.ParseIntoArray(X, TEXT(".")); B.ParseIntoArray(Y, TEXT("."));
		for (int32 i = 0; i < 3; ++i)
		{
			const int32 a = X.IsValidIndex(i) ? FCString::Atoi(*X[i]) : 0, b = Y.IsValidIndex(i) ? FCString::Atoi(*Y[i]) : 0;
			if (a != b) return a < b ? -1 : 1;
		}
		return 0;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Get(const FString& Url)
	{
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
		R->SetURL(Url);
		R->SetVerb(TEXT("GET"));
		R->SetHeader(TEXT("User-Agent"), TEXT("AirportDynastyPro-Updater"));
		R->SetHeader(TEXT("Accept"), TEXT("application/vnd.github+json"));
		return R;
	}
}

FString AADPHUD::UpdaterVersion() { return TEXT(ADP_PC_VERSION); }

FString AADPHUD::LastUpdateProblem()
{
	// the update script's own log: its last line, if that says the last install failed
	FString Log;
	FFileHelper::LoadFileToString(Log, *(FPaths::ProjectSavedDir() / TEXT("Update") / TEXT("update.log")));
	TArray<FString> Lines;
	Log.ParseIntoArrayLines(Lines);
	// (each line starts with its date and time; the message after it)
	if (!Lines.Num()) return FString();
	const int32 At = Lines.Last().Find(TEXT("Update failed"));
	return At != INDEX_NONE ? Lines.Last().RightChop(At) : FString();
}

void AADPHUD::CheckForUpdates()
{
	if (UpdateBusy) return;
	UpdateBusy = true;
	UpdateStatus = TEXT("Checking for updates…");
	UpdateChain.Reset();
	UpdateFullUrl.Empty();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = Get(Releases);
	TWeakObjectPtr<AADPHUD> Me(this);
	R->OnProcessRequestComplete().BindLambda([Me](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
	{
		AADPHUD* H = Me.Get();
		if (!H) return;
		H->UpdateBusy = false;
		TArray<TSharedPtr<FJsonValue>> List;
		if (!bOk || !Res.IsValid() || Res->GetResponseCode() != 200 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Res->GetContentAsString()), List))
		{ H->UpdateStatus = TEXT("Couldn't reach the update server. Check the internet connection and try again."); return; }
		// every PC release: its version, its update package (from which version) and its full installer
		struct FRel { FString Ver, From, UpdateUrl, SetupUrl, Page; int64 Size = 0; bool bSplit = false; };
		TArray<FRel> Rels;
		for (const TSharedPtr<FJsonValue>& V : List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FString Tag; if (!O.IsValid() || !O->TryGetStringField(TEXT("tag_name"), Tag) || !Tag.StartsWith(TEXT("pc-unreal-v"))) continue;
			FRel Rel; Rel.Ver = Tag.RightChop(11); O->TryGetStringField(TEXT("html_url"), Rel.Page);
			const TArray<TSharedPtr<FJsonValue>>* Assets;
			if (O->TryGetArrayField(TEXT("assets"), Assets))
				for (const TSharedPtr<FJsonValue>& AV : *Assets)
				{
					const TSharedPtr<FJsonObject> A = AV->AsObject();
					const FString Name = A->GetStringField(TEXT("name")), Url = A->GetStringField(TEXT("browser_download_url"));
					// AirportDynastyPro-Unreal-PC-Update-<from>-to-<to>.zip
					FString L, Rt;
					if (Name.Split(TEXT("-Update-"), &L, &Rt) && Rt.EndsWith(TEXT(".zip")))
					{
						FString From, To;
						if (Rt.LeftChop(4).Split(TEXT("-to-"), &From, &To) && To == Rel.Ver) { Rel.From = From; Rel.UpdateUrl = Url; Rel.Size = int64(A->GetNumberField(TEXT("size"))); }
					}
					else if (Name.Contains(TEXT("-Setup-")) && Name.EndsWith(TEXT(".exe"))) Rel.SetupUrl = Url;
					// (an installer split in pieces - Setup.exe + Setup-N.bin, GitHub's 2 GiB limit: the release page, to get them all)
					else if (Name.Contains(TEXT("-Setup-")) && Name.EndsWith(TEXT(".bin"))) Rel.bSplit = true;
				}
			Rels.Add(Rel);
		}
		Rels.Sort([](const FRel& A, const FRel& B) { return Compare(A.Ver, B.Ver) < 0; });
		const FString Have = UpdaterVersion();
		const FRel* Newest = Rels.Num() ? &Rels.Last() : nullptr;
		if (!Newest || Compare(Newest->Ver, Have) <= 0) { H->UpdateStatus = FString::Printf(TEXT("You have the newest version (%s)."), *Have); return; }
		// the chain of update packages from the version you have to the newest
		FString At = Have;
		int64 Total = 0;
		for (const FRel& Rel : Rels)
			if (Compare(Rel.Ver, At) > 0 && Rel.From == At && !Rel.UpdateUrl.IsEmpty()) { H->UpdateChain.Add({Rel.Ver, Rel.UpdateUrl}); H->UpdateSizes.Add(Rel.Size); Total += Rel.Size; At = Rel.Ver; }
		if (At != Newest->Ver)
		{
			// no unbroken chain (a version too old for patches): the full installer, which installs over this one
			H->UpdateChain.Reset(); H->UpdateSizes.Reset();
			H->UpdateFullUrl = Newest->SetupUrl.IsEmpty() || Newest->bSplit ? Newest->Page : Newest->SetupUrl;
			H->UpdateStatus = FString::Printf(TEXT("Version %s is out. This version is too old for a small update: Install downloads the full installer (it installs over this one; your saves stay)."), *Newest->Ver);
			return;
		}
		H->UpdateStatus = FString::Printf(TEXT("Version %s is ready to install (%.0f MB to download). Install update: the game closes, updates and starts again."), *Newest->Ver, Total / 1048576.0);
	});
	R->ProcessRequest();
}

void AADPHUD::InstallUpdate()
{
	if (UpdateBusy) return;
	if (!UpdateFullUrl.IsEmpty()) { FPlatformProcess::LaunchURL(*UpdateFullUrl, nullptr, nullptr); return; }
	if (!UpdateChain.Num()) return;
	UpdateBusy = true;
	UpdateFiles.Reset();
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Update"));
	// (to-do 163, owner 2026-11-20: a download never starts over - the folder and whatever is already in it stay, every
	// package carries on from its last byte, a finished one is never fetched again; only files of other updates go)
	IFileManager::Get().MakeDirectory(*Dir, true);
	TArray<FString> Have; IFileManager::Get().FindFiles(Have, *(Dir / TEXT("*.zip")), true, false);
	for (const FString& F : Have)
	{
		bool bMine = false;
		for (int32 i = 0; i < UpdateChain.Num(); ++i) if (F == FString::Printf(TEXT("%02d-%s.zip"), i, *UpdateChain[i].Key)) bMine = true;
		if (!bMine) IFileManager::Get().Delete(*(Dir / F));
	}
	DownloadNext(0, Dir);
}

void AADPHUD::DownloadNext(int32 I, const FString& Dir)
{
	if (I >= UpdateChain.Num()) { UpdateIdx = -1; ApplyUpdate(Dir); return; }
	const FString Ver = UpdateChain[I].Key;
	const FString File = Dir / FString::Printf(TEXT("%02d-%s.zip"), I, *Ver);
	const int64 Size = UpdateSizes.IsValidIndex(I) ? UpdateSizes[I] : 0;
	UpdateIdx = I; UpdateDir = Dir; UpdateFile = File; UpdateTries = 0;
	UpdateFrac = 0.f; UpdateSpeed = 0; UpdateT0 = 0; UpdateGot0 = 0;
	// already whole (downloaded before the game was closed): straight on to the next
	if (Size > 0 && IFileManager::Get().FileSize(*File) == Size) { UpdateFiles.Add(File); DownloadNext(I + 1, Dir); return; }
	UpdateStatus = FString::Printf(TEXT("Downloading the update to %s (%d of %d)…"), *Ver, I + 1, UpdateChain.Num());
	// Windows' own curl: -C - carries on from the file's end; retries by itself; a stalled connection is dropped and retried
	const FString Curl = FPlatformMisc::GetEnvironmentVariable(TEXT("SystemRoot")) / TEXT("System32/curl.exe");
	const FString Args = FString::Printf(TEXT("-L --fail -s -C - --retry 10 --retry-delay 5 --retry-all-errors --connect-timeout 30 --speed-limit 1024 --speed-time 60 -H \"Accept: application/octet-stream\" -A AirportDynastyPro-Updater -o \"%s\" \"%s\""), *File, *UpdateChain[I].Value);
	UpdateProc = FPlatformProcess::CreateProc(*Curl, *Args, true, true, true, nullptr, 0, nullptr, nullptr);
	if (!UpdateProc.IsValid()) { UpdateBusy = false; UpdateIdx = -1; UpdateStatus = TEXT("The download couldn't start (no curl on this PC)."); }
}

void AADPHUD::TickUpdateDownload(float Dt)
{
	if (UpdateIdx < 0 || !UpdateProc.IsValid()) return;
	if ((UpdatePoll += Dt) < 0.5f) return;
	UpdatePoll = 0.f;
	const int64 Got = FMath::Max<int64>(0, IFileManager::Get().FileSize(*UpdateFile));
	const int64 Size = UpdateSizes.IsValidIndex(UpdateIdx) ? UpdateSizes[UpdateIdx] : 0;
	const FString Ver = UpdateChain.IsValidIndex(UpdateIdx) ? UpdateChain[UpdateIdx].Key : FString();
	const double Now = FPlatformTime::Seconds();
	if (UpdateT0 <= 0) { UpdateT0 = Now; UpdateGot0 = Got; }
	else if (Now - UpdateT0 > 1.0) { const double Rate = double(Got - UpdateGot0) / (Now - UpdateT0); UpdateSpeed = UpdateSpeed > 0 ? UpdateSpeed * 0.6 + Rate * 0.4 : Rate; UpdateT0 = Now; UpdateGot0 = Got; }
	if (Size > 0) UpdateFrac = FMath::Max(UpdateFrac, FMath::Clamp(float(double(Got) / double(Size)), 0.f, 1.f));
	UpdateStatus = Size > 0 ? FString::Printf(TEXT("Downloading the update to %s: %.0f of %.0f MB"), *Ver, Got / 1048576.0, Size / 1048576.0) : FString::Printf(TEXT("Downloading the update to %s: %.0f MB"), *Ver, Got / 1048576.0);
	if (FPlatformProcess::IsProcRunning(UpdateProc)) return;
	int32 Code = 0; FPlatformProcess::GetProcReturnCode(UpdateProc, &Code); FPlatformProcess::CloseProc(UpdateProc);
	const bool bWhole = Size > 0 ? Got >= Size : Code == 0;
	if (bWhole) { UpdateFiles.Add(UpdateFile); DownloadNext(UpdateIdx + 1, UpdateDir); return; }
	// broken off: again from where it stopped (the part on disk stays), up to 8 times now - and the next press of the
	// button, or the next start of the game, carries on from there too
	if (++UpdateTries <= 8)
	{
		const int32 I = UpdateIdx, Tries = UpdateTries; DownloadNext(I, UpdateDir); UpdateTries = Tries;
		UpdateStatus = FString::Printf(TEXT("The connection dropped - carrying on (%.0f MB already here)…"), Got / 1048576.0);
		return;
	}
	UpdateBusy = false; UpdateIdx = -1;
	UpdateStatus = FString::Printf(TEXT("The download stopped at %.0f MB. Press Install again - it carries on from there."), Got / 1048576.0);
}

void AADPHUD::ApplyUpdate(const FString& Dir)
{
	// the script that puts the files in place once the game has closed, then starts it again
	const FString Install = FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	const FString Exe = Install / TEXT("AirportDynastyPro.exe");
	// (the script turns both into Windows' own backslash form itself - [IO.Path]::GetFullPath - before Explorer gets them)
	FString Zips;
	for (const FString& Z : UpdateFiles) Zips += (Zips.IsEmpty() ? TEXT("") : TEXT(",")) + FString::Printf(TEXT("'%s'"), *Z);
	const FString Script = FString::Printf(TEXT(R"PS1(# Airport Dynasty Pro: puts an update in place after the game has closed, then starts it again.
$ErrorActionPreference = 'Stop'
$install = '%s'; $exe = '%s'; $zips = @(%s); $log = '%s\update.log'
function Note($t) { ((Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + '  ' + $t) | Add-Content $log }
# (to-do 139: a file still held by the closing game made the copy fail, the old program stayed, and the same update was
# offered again on the next start - so: wait for every program running from the game's folder to end, copy each file with
# retries, and check every installed file's fingerprint before calling it done)
function Copy-Retry($src, $dst) {
  for ($t = 1; $t -le 40; $t++) {
    try { Copy-Item -LiteralPath $src -Destination $dst -Force; return } catch { if ($t -eq 40) { throw "still in use after 60 s: $dst" }; Start-Sleep -Milliseconds 1500 }
  }
}
try {
  Note "Installing: $($zips.Count) package(s)"
  Wait-Process -Id %d -Timeout 180 -ErrorAction SilentlyContinue
  $root = [IO.Path]::GetFullPath($install)
  for ($t = 0; $t -lt 120; $t++) {
    $busy = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { try { $_.Path -and $_.Path.StartsWith($root, 'OrdinalIgnoreCase') } catch { $false } })
    if ($busy.Count -eq 0) { break }
    if ($t -eq 0) { Note ("Waiting for " + (($busy | ForEach-Object { $_.Name }) -join ', ') + " to close") }
    Start-Sleep -Milliseconds 500
  }
  Start-Sleep -Milliseconds 800
  foreach ($z in $zips) {
    $stage = Join-Path '%s' ('stage-' + [IO.Path]::GetFileNameWithoutExtension($z))
    if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
    Expand-Archive -LiteralPath $z -DestinationPath $stage -Force
    $m = Get-Content (Join-Path $stage 'update.json') -Raw | ConvertFrom-Json
    foreach ($f in $m.files) {
      $src = Join-Path $stage ('files\' + $f.path); $dst = Join-Path $install $f.path
      $h = (Get-FileHash -Algorithm SHA256 -LiteralPath $src).Hash
      if ($h -ne $f.sha256.ToUpper()) { throw "Damaged file in the update: $($f.path)" }
      New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
      Copy-Retry $src $dst
    }
    foreach ($d in $m.deleted) { $p = Join-Path $install $d; if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force } }
    foreach ($f in $m.files) {
      $dst = Join-Path $install $f.path
      if ((Get-FileHash -Algorithm SHA256 -LiteralPath $dst).Hash -ne $f.sha256.ToUpper()) { throw "Not replaced: $($f.path)" }
    }
    foreach ($key in @('HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{3B7D9E21-6A4C-4F8E-9D15-2C7A0E4B8F63}_is1', 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{3B7D9E21-6A4C-4F8E-9D15-2C7A0E4B8F63}_is1')) {
      try { if (Test-Path $key) { Set-ItemProperty $key DisplayVersion $m.to } } catch { }
    }
    Note "Updated to $($m.to) ($($m.files.Count) files, every one checked)"
  }
} catch { Note "Update failed: $_" }
# started through Explorer: the game runs as you, not with the administrator rights this may have had. The path in
# Windows' own form (backslashes): given forward slashes, Explorer doesn't know it as a file and opens Documents instead
# (owner, 2026-11-19: "it brings up a documents folder")
$exeWin = [IO.Path]::GetFullPath($exe)
if (Test-Path -LiteralPath $exeWin) { Start-Process explorer.exe -ArgumentList ('"' + $exeWin + '"') } else { Note "Restart: no game at $exeWin" }
)PS1"), *Install, *Exe, *Zips, *Dir, int32(FPlatformProcess::GetCurrentProcessId()), *Dir);
	const FString ScriptFile = Dir / TEXT("apply-update.ps1");
	FFileHelper::SaveStringToFile(Script, *ScriptFile, FFileHelper::EEncodingOptions::ForceUTF8);
	UpdateStatus = TEXT("Installing the update: the game closes and starts again in a moment…");
	UpdateFrac = 1.f;
	// installed where only an administrator may write (Program Files, "for all users"): Windows asks once for permission
	const FString Probe = Install / TEXT("AirportDynastyPro") / TEXT(".write-test");
	const bool bWritable = FFileHelper::SaveStringToFile(TEXT("x"), *Probe);
	if (bWritable) IFileManager::Get().Delete(*Probe, false, true, true);
	FString Args = FString::Printf(TEXT("-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\""), *ScriptFile);
	if (!bWritable)
		Args = FString::Printf(TEXT("-NoProfile -WindowStyle Hidden -Command \"Start-Process powershell -Verb RunAs -WindowStyle Hidden -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File \\\"%s\\\"'\""), *ScriptFile);
	ADP_LOG(TEXT("Updater: installing %s"), bWritable ? TEXT("directly") : TEXT("with administrator permission (the game folder is protected)"));
	FPlatformProcess::CreateProc(TEXT("powershell.exe"), *Args, true, true, true, nullptr, 0, nullptr, nullptr);
	FGenericPlatformMisc::RequestExit(false);
}
