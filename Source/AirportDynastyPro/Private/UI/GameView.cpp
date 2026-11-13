// The airline game in a web view: a scheme handler serving the built game's files at https://game.adp.local/, and the
// view itself.
#include "UI/GameView.h"
#include "UI/TitleBg.h"
#include "Widgets/SOverlay.h"
#include "ADPLog.h"
#include "IWebBrowserSchemeHandler.h"
#include "IWebBrowserSingleton.h"
#include "WebBrowserModule.h"
#include "SWebBrowser.h"
#include "IWebBrowserWindow.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Runtime/Launch/Resources/Version.h"

#pragma region Serving the files
namespace
{
	const TCHAR* Host = TEXT("game.adp.local");

	const TCHAR* MimeOf(const FString& Path)
	{
		const FString E = FPaths::GetExtension(Path).ToLower();
		if (E == TEXT("html")) return TEXT("text/html");
		if (E == TEXT("js") || E == TEXT("mjs")) return TEXT("text/javascript");
		if (E == TEXT("css")) return TEXT("text/css");
		if (E == TEXT("json")) return TEXT("application/json");
		if (E == TEXT("png")) return TEXT("image/png");
		if (E == TEXT("jpg") || E == TEXT("jpeg")) return TEXT("image/jpeg");
		if (E == TEXT("webp")) return TEXT("image/webp");
		if (E == TEXT("svg")) return TEXT("image/svg+xml");
		if (E == TEXT("wasm")) return TEXT("application/wasm");
		if (E == TEXT("mp3")) return TEXT("audio/mpeg");
		if (E == TEXT("ogg")) return TEXT("audio/ogg");
		if (E == TEXT("wav")) return TEXT("audio/wav");
		if (E == TEXT("woff2")) return TEXT("font/woff2");
		if (E == TEXT("woff")) return TEXT("font/woff");
		if (E == TEXT("ttf")) return TEXT("font/ttf");
		if (E == TEXT("glb")) return TEXT("model/gltf-binary");
		if (E == TEXT("onnx") || E == TEXT("bin")) return TEXT("application/octet-stream");
		return TEXT("application/octet-stream");
	}

	/** One request: the file read whole, then handed over in pieces. */
	class FGameFile : public IWebBrowserSchemeHandler
	{
	public:
		virtual bool ProcessRequest(const FString& Verb, const FString& Url, const FSimpleDelegate& OnHeadersReady) override
		{
			FString Path = Url;
			const int32 At = Path.Find(Host);
			Path = At != INDEX_NONE ? Path.RightChop(At + FCString::Strlen(Host)) : TEXT("/");
			int32 Q;
			if (Path.FindChar(TEXT('?'), Q)) Path = Path.Left(Q);
			if (Path.FindChar(TEXT('#'), Q)) Path = Path.Left(Q);
			Path.ReplaceInline(TEXT("%20"), TEXT(" ")); // the game's file names have no other escapes
			if (Path.IsEmpty() || Path.EndsWith(TEXT("/"))) Path += TEXT("index.html");
			// the voice pack (Content/Voice) at /voicepack/: the pronunciation dictionary for the announcer's words
			const bool bVoice = Path.StartsWith(TEXT("/voicepack/"));
			const FString Base = FPaths::ConvertRelativePathToFull(bVoice ? FPaths::ProjectContentDir() / TEXT("Voice") : ADPGameView::Folder());
			File = FPaths::ConvertRelativePathToFull(Base / (bVoice ? Path.RightChop(11) : Path));
			// never outside the game's (or the voice pack's) folder
			bFound = File.StartsWith(Base) && !File.Contains(TEXT("..")) && FFileHelper::LoadFileToArray(Data, *File);
			OnHeadersReady.Execute();
			return true;
		}
		virtual void GetResponseHeaders(IHeaders& H) override
		{
			H.SetStatusCode(bFound ? 200 : 404);
			H.SetMimeType(bFound ? MimeOf(File) : TEXT("text/plain"));
			H.SetContentLength(bFound ? Data.Num() : 0);
			H.SetHeader(TEXT("Cache-Control"), TEXT("no-store"));   // never an old copy after an update
		}
		virtual bool ReadResponse(uint8* Out, int32 ToRead, int32& Read, const FSimpleDelegate& OnMore) override
		{
			Read = FMath::Min(ToRead, Data.Num() - Offset);
			if (Read <= 0) return false;
			FMemory::Memcpy(Out, Data.GetData() + Offset, Read);
			Offset += Read;
			return true;
		}
		virtual void Cancel() override {}

	private:
		FString File;
		TArray<uint8> Data;
		int32 Offset = 0;
		bool bFound = false;
	};

	class FGameFiles : public IWebBrowserSchemeHandlerFactory
	{
	public:
		virtual TUniquePtr<IWebBrowserSchemeHandler> Create(FString Verb, FString Url) override { return MakeUnique<FGameFile>(); }
	};

	void Register()
	{
		static bool bDone = false;
		static FGameFiles Factory;
		if (bDone || !IWebBrowserModule::IsAvailable()) return;
		if (IWebBrowserSingleton* B = IWebBrowserModule::Get().GetSingleton())
			bDone = B->RegisterSchemeHandlerFactory(TEXT("https"), Host, &Factory);
	}
}
#pragma endregion

#pragma region The view

FString ADPGameView::Folder()
{
	// packaged: staged with the game; run from the editor: always the latest build of the menus (never an old copy
	// left in Content/Web by a release build)
	const FString Packed = FPaths::ProjectContentDir() / TEXT("Web");
	const FString Dev = FPaths::ProjectDir() / TEXT("Airline-Manager-Pro-Babylon/www");
	if (!FPlatformProperties::RequiresCookedData() && FPaths::FileExists(Dev / TEXT("index.html"))) return Dev;
	if (FPaths::FileExists(Packed / TEXT("index.html"))) return Packed;
	if (FPaths::FileExists(Dev / TEXT("index.html"))) return Dev;
	return FString();
}

TSharedRef<SWidget> ADPGameView::Make(UObject* Bridge, TSharedPtr<SWebBrowser>* OutBrowser)
{
	if (Folder().IsEmpty())
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.024f, 0.094f, 0.17f, 0.92f))
			.Padding(30)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FLinearColor(0.75f, 0.92f, 1.f))
				.AutoWrapText(true)
				.Text(FText::FromString(TEXT("The airline game's screens come with the release build.\nTo see them in the editor, build the Android edition once: npm install and npm run build in Airline-Manager-Pro-Babylon.\n\nG or Esc goes back to the terminal.")))
			];
	}
	Register();
	TSharedRef<SWebBrowser> B = SNew(SWebBrowser)
		// the app's own title screen and main menu, told it is the PC version (its number and the engine's)
		.InitialURL(FString::Printf(TEXT("https://%s/index.html?pc=%s&ue=%d.%d"), Host, TEXT(ADP_PC_VERSION), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION))
		.ShowControls(false)
		.ShowAddressBar(false)
		.ShowErrorMessage(true)
		// see-through where the page is (the phone or a computer over the 3D room: Devices.cpp); the menus themselves are
		// opaque, their page has its own background
		.SupportsTransparency(true)
		.BackgroundColor(FColor(5, 3, 12, 0))   // (see-through until the page draws: the title film behind it shows)
		.BrowserFrameRate(60)
		// the page's own messages and errors go to the game's log (Saved/Logs), to find problems in the menus
		.OnConsoleMessage_Lambda([](const FString& Msg, const FString& Src, int32 Line, EWebBrowserConsoleLogSeverity Sev)
		{
			if (Sev >= EWebBrowserConsoleLogSeverity::Warning || Msg.StartsWith(TEXT("[ue]"))) ADP_LOG(TEXT("Menus: %s (%s:%d)"), *Msg, *FPaths::GetCleanFilename(Src), Line);
		});
	// the menus reach Unreal through window.ue.adp (the title screen's Terminal, and more later)
	if (Bridge) B->BindUObject(TEXT("adp"), Bridge, true);
	if (OutBrowser) *OutBrowser = B;
	// the title screen's film, drawn by Unreal behind the page (TitleBg.h); the page leaves its canvas see-through then
	return SNew(SOverlay)
		+ SOverlay::Slot()[SNew(STitleBg).Visibility(EVisibility::HitTestInvisible)]
		+ SOverlay::Slot()[B];
}

#pragma endregion

