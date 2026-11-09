// The airline game in a web view: a scheme handler serving the built game's files at https://game.adp.local/, and the
// view itself.
#include "UI/GameView.h"
#include "IWebBrowserSchemeHandler.h"
#include "IWebBrowserSingleton.h"
#include "WebBrowserModule.h"
#include "SWebBrowser.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

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
			File = FPaths::ConvertRelativePathToFull(ADPGameView::Folder() / Path);
			// never outside the game's folder
			bFound = File.StartsWith(FPaths::ConvertRelativePathToFull(ADPGameView::Folder())) && FFileHelper::LoadFileToArray(Data, *File);
			OnHeadersReady.Execute();
			return true;
		}
		virtual void GetResponseHeaders(IHeaders& H) override
		{
			H.SetStatusCode(bFound ? 200 : 404);
			H.SetMimeType(bFound ? MimeOf(File) : TEXT("text/plain"));
			H.SetContentLength(bFound ? Data.Num() : 0);
			H.SetHeader(TEXT("Cache-Control"), TEXT("no-cache"));
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
	// packaged: staged with the game; in the editor: the Android edition's own build output
	const FString Packed = FPaths::ProjectContentDir() / TEXT("Web");
	if (FPaths::FileExists(Packed / TEXT("game/index.html"))) return Packed;
	const FString Dev = FPaths::ProjectDir() / TEXT("Airline-Manager-Pro-Babylon/www");
	if (FPaths::FileExists(Dev / TEXT("game/index.html"))) return Dev;
	return FString();
}

TSharedRef<SWidget> ADPGameView::Make()
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
	return SNew(SWebBrowser)
		.InitialURL(FString::Printf(TEXT("https://%s/game/index.html"), Host))
		.ShowControls(false)
		.ShowAddressBar(false)
		.ShowErrorMessage(true)
		.SupportsTransparency(false)
		.BackgroundColor(FColor(5, 3, 12))
		.BrowserFrameRate(60);
}

#pragma endregion
