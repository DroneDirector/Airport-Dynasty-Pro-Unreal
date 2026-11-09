// The in-game updater (Settings → Updates), as in a released game: "Check for updates" asks GitHub for newer PC
// versions; each release carries a small update package from the version before it (only the files that changed:
// Unreal's patch pak with the changed content, the game's program when its code changed, the menus' changed files),
// so "Install update" downloads only those, one after another from the version you have to the newest. Then the game
// closes, a small script puts the files in place (each checked against its fingerprint), and the game starts again.
// The terrain has its own packs by region (only the regions that changed come down).
#include "UI/ADPHUD.h"
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
	return Lines.Num() && Lines.Last().StartsWith(TEXT("Update failed")) ? Lines.Last() : FString();
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
		struct FRel { FString Ver, From, UpdateUrl, SetupUrl, Page; int64 Size = 0; };
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
			if (Compare(Rel.Ver, At) > 0 && Rel.From == At && !Rel.UpdateUrl.IsEmpty()) { H->UpdateChain.Add({Rel.Ver, Rel.UpdateUrl}); Total += Rel.Size; At = Rel.Ver; }
		if (At != Newest->Ver)
		{
			// no unbroken chain (a version too old for patches): the full installer, which installs over this one
			H->UpdateChain.Reset();
			H->UpdateFullUrl = Newest->SetupUrl.IsEmpty() ? Newest->Page : Newest->SetupUrl;
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
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	IFileManager::Get().MakeDirectory(*Dir, true);
	DownloadNext(0, Dir);
}

void AADPHUD::DownloadNext(int32 I, const FString& Dir)
{
	if (I >= UpdateChain.Num()) { ApplyUpdate(Dir); return; }
	const FString Ver = UpdateChain[I].Key;
	UpdateStatus = FString::Printf(TEXT("Downloading the update to %s (%d of %d)…"), *Ver, I + 1, UpdateChain.Num());
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = Get(UpdateChain[I].Value);
	R->SetHeader(TEXT("Accept"), TEXT("application/octet-stream"));
	TWeakObjectPtr<AADPHUD> Me(this);
	R->OnRequestProgress64().BindLambda([Me, Ver, I](FHttpRequestPtr Req, uint64, uint64 Got)
	{
		if (AADPHUD* H = Me.Get())
		{
			const int64 Size = Req->GetResponse().IsValid() ? Req->GetResponse()->GetContentLength() : 0;
			H->UpdateStatus = Size > 0 ? FString::Printf(TEXT("Downloading the update to %s: %d%%"), *Ver, int32(Got * 100 / Size)) : FString::Printf(TEXT("Downloading the update to %s: %.0f MB"), *Ver, Got / 1048576.0);
		}
	});
	R->OnProcessRequestComplete().BindLambda([Me, I, Dir, Ver](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
	{
		AADPHUD* H = Me.Get();
		if (!H) return;
		if (!bOk || !Res.IsValid() || Res->GetResponseCode() != 200) { H->UpdateBusy = false; H->UpdateStatus = TEXT("The download stopped. Try again."); return; }
		const FString File = Dir / FString::Printf(TEXT("%02d-%s.zip"), I, *Ver);
		FFileHelper::SaveArrayToFile(Res->GetContent(), *File);
		H->UpdateFiles.Add(File);
		H->DownloadNext(I + 1, Dir);
	});
	R->ProcessRequest();
}

void AADPHUD::ApplyUpdate(const FString& Dir)
{
	// the script that puts the files in place once the game has closed, then starts it again
	const FString Install = FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	const FString Exe = Install / TEXT("AirportDynastyPro.exe");
	FString Zips;
	for (const FString& Z : UpdateFiles) Zips += (Zips.IsEmpty() ? TEXT("") : TEXT(",")) + FString::Printf(TEXT("'%s'"), *Z);
	const FString Script = FString::Printf(TEXT(R"PS1(# Airport Dynasty Pro: puts an update in place after the game has closed, then starts it again.
$ErrorActionPreference = 'Stop'
$install = '%s'; $exe = '%s'; $zips = @(%s); $log = '%s\update.log'
try {
  Wait-Process -Id %d -Timeout 120 -ErrorAction SilentlyContinue
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
      Copy-Item -LiteralPath $src -Destination $dst -Force
    }
    foreach ($d in $m.deleted) { $p = Join-Path $install $d; if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force } }
    $key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{3B7D9E21-6A4C-4F8E-9D15-2C7A0E4B8F63}_is1'
    if (Test-Path $key) { Set-ItemProperty $key DisplayVersion $m.to }
    "Updated to $($m.to)" | Add-Content $log
  }
} catch { "Update failed: $_" | Add-Content $log }
# started through Explorer: the game runs as you, not with the administrator rights this may have had
Start-Process explorer.exe -ArgumentList ('"' + $exe + '"')
)PS1"), *Install, *Exe, *Zips, *Dir, int32(FPlatformProcess::GetCurrentProcessId()), *Dir);
	const FString ScriptFile = Dir / TEXT("apply-update.ps1");
	FFileHelper::SaveStringToFile(Script, *ScriptFile, FFileHelper::EEncodingOptions::ForceUTF8);
	UpdateStatus = TEXT("Installing the update: the game closes and starts again in a moment…");
	// installed where only an administrator may write (Program Files, "for all users"): Windows asks once for permission
	const FString Probe = Install / TEXT("AirportDynastyPro") / TEXT(".write-test");
	const bool bWritable = FFileHelper::SaveStringToFile(TEXT("x"), *Probe);
	if (bWritable) IFileManager::Get().Delete(*Probe, false, true, true);
	FString Args = FString::Printf(TEXT("-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\""), *ScriptFile);
	if (!bWritable)
		Args = FString::Printf(TEXT("-NoProfile -WindowStyle Hidden -Command \"Start-Process powershell -Verb RunAs -WindowStyle Hidden -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File \\\"%s\\\"'\""), *ScriptFile);
	UE_LOG(LogTemp, Log, TEXT("Updater: installing %s"), bWritable ? TEXT("directly") : TEXT("with administrator permission (the game folder is protected)"));
	FPlatformProcess::CreateProc(TEXT("powershell.exe"), *Args, true, true, true, nullptr, 0, nullptr, nullptr);
	FGenericPlatformMisc::RequestExit(false);
}
