// The crash report: when the game crashed last time, the next start shows over the title screen what went wrong
// (from Unreal's own crash record in Saved/Crashes: the error, where in the program, the engine and game versions) and
// what you were doing (the breadcrumb the game keeps on disk), with a button that writes it as an e-mail to
// crashlog@airportdynasty.com in your own mail program, where you see it and send it yourself.
#include "UI/ADPHUD.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"

namespace
{
	const TCHAR* CrashMail = TEXT("crashlog@airportdynasty.com");
	FString CrumbFile() { return FPaths::ProjectSavedDir() / TEXT("LastActivity.txt"); }
	FString Between(const FString& S, const TCHAR* A, const TCHAR* B)
	{
		const int32 I = S.Find(A);
		if (I == INDEX_NONE) return FString();
		const int32 From = I + FCString::Strlen(A);
		const int32 J = S.Find(B, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
		return J == INDEX_NONE ? FString() : S.Mid(From, J - From);
	}
	FString Unxml(FString S)
	{
		S.ReplaceInline(TEXT("&#10;"), TEXT("\n")); S.ReplaceInline(TEXT("&lt;"), TEXT("<")); S.ReplaceInline(TEXT("&gt;"), TEXT(">"));
		S.ReplaceInline(TEXT("&quot;"), TEXT("\"")); S.ReplaceInline(TEXT("&apos;"), TEXT("'")); S.ReplaceInline(TEXT("&amp;"), TEXT("&"));
		return S.TrimStartAndEnd();
	}
}

void AADPHUD::Breadcrumb(const FString& What)
{
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s  (%s, version %s)"), *What, *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")), *UpdaterVersion()), *CrumbFile());
}

bool AADPHUD::FindNewCrash()
{
	// the newest crash record; shown once (the one last shown is remembered)
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Crashes");
	TArray<FString> Found;
	IFileManager::Get().FindFiles(Found, *(Dir / TEXT("*")), false, true);
	FString Newest; FDateTime NewestAt;
	for (const FString& F : Found)
	{
		const FDateTime At = IFileManager::Get().GetTimeStamp(*(Dir / F / TEXT("CrashContext.runtime-xml")));
		if (At > NewestAt) { NewestAt = At; Newest = F; }
	}
	if (Newest.IsEmpty()) return false;
	FString Seen;
	GConfig->GetString(TEXT("ADP.Crash"), TEXT("LastShown"), Seen, GGameUserSettingsIni);
	if (Seen == Newest) return false;
	// shown once only, even if the box is closed without its buttons; a crash older than a week isn't news any more
	GConfig->SetString(TEXT("ADP.Crash"), TEXT("LastShown"), *Newest, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	if ((FDateTime::Now() - NewestAt).GetTotalDays() > 7.0) return false;
	FString Xml;
	if (!FFileHelper::LoadFileToString(Xml, *(Dir / Newest / TEXT("CrashContext.runtime-xml")))) return false;
	CrashId = Newest;
	CrashFolder = Dir / Newest;
	const FString Error = Unxml(Between(Xml, TEXT("<ErrorMessage>"), TEXT("</ErrorMessage>")));
	// where in the program: the lines of the call stack that are the game's own (and the first few of the rest)
	TArray<FString> Lines;
	Unxml(Between(Xml, TEXT("<CallStack>"), TEXT("</CallStack>"))).ParseIntoArrayLines(Lines);
	FString Stack; int32 Shown = 0;
	for (const FString& L : Lines)
	{
		const FString T = L.TrimStartAndEnd();
		if (T.IsEmpty() || T.StartsWith(TEXT("ntdll"))) continue;
		Stack += T + TEXT("\n");
		if (++Shown >= 12) break;
	}
	FString Crumb;
	FFileHelper::LoadFileToString(Crumb, *CrumbFile());
	CrashText = FString::Printf(TEXT("When: %s\nGame version: %s\nEngine: %s\nWhat you were doing: %s\n\nError: %s\n\nWhere in the program:\n%s"),
		*(NewestAt + (FDateTime::Now() - FDateTime::UtcNow())).ToString(TEXT("%Y-%m-%d %H:%M")), *UpdaterVersion(),
		*Unxml(Between(Xml, TEXT("<BuildVersion>"), TEXT("</BuildVersion>"))), Crumb.IsEmpty() ? TEXT("(not known)") : *Crumb,
		Error.IsEmpty() ? TEXT("(no message)") : *Error, Stack.IsEmpty() ? TEXT("(not recorded)\n") : *Stack);
	return true;
}

void AADPHUD::SendCrashMail()
{
	// your own mail program, with the report written in; you send it (or not)
	FString Body = TEXT("Airport Dynasty Pro (PC) crashed. The report:\n\n") + CrashText + TEXT("\nCrash record: ") + CrashId + TEXT("\n\n(Anything you were doing just before, add it here.)\n");
	if (Body.Len() > 1800) Body = Body.Left(1800) + TEXT("\n…");
	const FString Url = FString::Printf(TEXT("mailto:%s?subject=%s&body=%s"), CrashMail,
		*FGenericPlatformHttp::UrlEncode(FString::Printf(TEXT("Crash report, Airport Dynasty Pro PC %s"), *UpdaterVersion())), *FGenericPlatformHttp::UrlEncode(Body));
	FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

TSharedRef<SWidget> AADPHUD::MakeCrashPanel()
{
	const FLinearColor CrashInk(0.85f, 0.95f, 1.f);
	auto Btn = [CrashInk](const FString& Label, TFunction<void()> Click)
	{
		return SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.05f, 0.2f, 0.36f, 0.95f)).ContentPadding(FMargin(12, 6))
			.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); })
			[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)).ColorAndOpacity(CrashInk).Text(FText::FromString(Label))];
	};
	auto Done = [this]()
	{
		GConfig->SetString(TEXT("ADP.Crash"), TEXT("LastShown"), *CrashId, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		CrashPanel->SetVisibility(EVisibility::Collapsed);
	};
	return SNew(SBox).WidthOverride(760)
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.03f, 0.08f, 0.15f, 0.97f)).Padding(18)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 15)).ColorAndOpacity(FLinearColor(1.f, 0.6f, 0.45f)).Text(FText::FromString(TEXT("The game crashed last time")))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
			[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(CrashInk).AutoWrapText(true)
				.Text(FText::FromString(FString::Printf(TEXT("Sorry about that. Here is what went wrong. Send it to us and we can fix it: the button opens your mail program with the report written in, addressed to %s; you see it before you send it."), CrashMail)))]
			+ SVerticalBox::Slot().MaxHeight(360).Padding(0, 0, 0, 12)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.35f)).Padding(10)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Mono", 9)).ColorAndOpacity(CrashInk).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(CrashText); })]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[Btn(TEXT("SEND THE CRASH REPORT"), [this, Done]() { SendCrashMail(); Done(); })]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[Btn(TEXT("OPEN THE CRASH FOLDER"), [this]() { FPlatformProcess::ExploreFolder(*CrashFolder); })]
				+ SHorizontalBox::Slot().AutoWidth()[Btn(TEXT("CLOSE"), [Done]() { Done(); })]
			]
		]
	];
}
