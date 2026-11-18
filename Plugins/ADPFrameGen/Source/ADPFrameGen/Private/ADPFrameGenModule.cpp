// Airport Dynasty Pro's own frame generation: handed to Unreal as its display chain maker at every start (to-do 111,
// owner 2026-11-18: switch it on and off live, no restart) - switched on or off from Settings while the game runs
// (ADP.Graphics OwnFrameGen); while off, each frame goes straight through as drawn. It goes first, ahead of any other
// maker (AMD's FSR plugin has one too, set aside while ours is in place) - except when AMD's frame generation was the
// one chosen at this start (ADP.Graphics FrameGeneration): then AMD's display chain is needed and ours stays out until
// the next start. Only in the game, never in the editor.
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Windows/IDXGISwapchainProvider.h"
#include "FrameGenSwapChain.h"

namespace ADPFrameGen { void SetEnabled(bool b); }   // (SetActive: ADPFrameGen.h)

class FADPFrameGenProvider : public IDXGISwapchainProvider
{
public:
	virtual bool SupportsRHI(ERHIInterfaceType RHIType) const override { return RHIType == ERHIInterfaceType::D3D12; }
	virtual const TCHAR* GetProviderName() const override { return TEXT("Airport Dynasty frame generation"); }
	virtual HRESULT CreateSwapChainForHwnd(IDXGIFactory2* F, IUnknown* Device, HWND Wnd, const DXGI_SWAP_CHAIN_DESC1* Desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* Full, IDXGIOutput* Restrict, IDXGISwapChain1** Out) override
	{
		return ADPFrameGen::Create(F, Device, Wnd, Desc, Full, Restrict, Out);
	}
	virtual HRESULT CreateSwapChain(IDXGIFactory* F, IUnknown* Device, DXGI_SWAP_CHAIN_DESC* Desc, IDXGISwapChain** Out) override
	{
		return F->CreateSwapChain(Device, Desc, Out);
	}
};

class FADPFrameGenModule : public IModuleInterface
{
	FADPFrameGenProvider Provider;
	bool bRegistered = false;
public:
	virtual void StartupModule() override
	{
		if (GIsEditor && !IsRunningGame()) return;
		// the Pixel Norma logo at the start (DefaultGame.ini StartupMovies, to-do 64b) is for players only: a test run
		// (any -UI.. switch, -Stress.., -TrafficTest, or -NoLogo) starts without it. This module loads early enough
		// (PostConfigInit): the movies are read from the settings only after it.
		{
			TArray<FString> Tokens, Switches;
			FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
			bool bTest = false;
			for (const FString& Sw : Switches)
				if (Sw.StartsWith(TEXT("UI")) || Sw.StartsWith(TEXT("Stress")) || Sw.StartsWith(TEXT("TrafficTest")) || Sw.Equals(TEXT("NoLogo"), ESearchCase::IgnoreCase)) { bTest = true; break; }
			if (bTest && GConfig) GConfig->SetArray(TEXT("/Script/MoviePlayer.MoviePlayerSettings"), TEXT("StartupMovies"), TArray<FString>(), GGameIni);
		}
		bool bOn = false, bAmd = false;
		if (GConfig) GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("OwnFrameGen"), bOn, GGameUserSettingsIni);
		if (GConfig) GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("FrameGeneration"), bAmd, GGameUserSettingsIni);
		if (FParse::Param(FCommandLine::Get(), TEXT("OwnFrameGen"))) { bOn = true; bAmd = false; }
		if (FParse::Param(FCommandLine::Get(), TEXT("NoOwnFrameGen"))) return;   // (for checking: the plain display chain)
		if (bAmd && !bOn) { UE_LOG(LogTemp, Log, TEXT("ADPFrameGen: AMD's frame generation chosen - ours stays out until the next start")); return; }
		ADPFrameGen::SetEnabled(true);
		ADPFrameGen::SetActive(bOn);
		// the only one: the makers already there step aside
		IModularFeatures& M = IModularFeatures::Get();
		const FName Name = IDXGISwapchainProvider::GetModularFeatureName();
		TArray<IDXGISwapchainProvider*> Others = M.GetModularFeatureImplementations<IDXGISwapchainProvider>(Name);
		for (IDXGISwapchainProvider* O : Others) M.UnregisterModularFeature(Name, O);
		M.RegisterModularFeature(Name, &Provider);
		bRegistered = true;
		// any maker that comes later (AMD's plugin starts after us) steps aside while ours runs
		M.OnModularFeatureRegistered().AddLambda([this, Name](const FName& Type, IModularFeature* F)
		{
			if (Type == Name && F != &Provider) IModularFeatures::Get().UnregisterModularFeature(Name, F);
		});
		UE_LOG(LogTemp, Log, TEXT("ADPFrameGen: our frame generation in place, switched %s (%d other makers set aside)"), bOn ? TEXT("on") : TEXT("off"), Others.Num());
		// AMD's frame generation stays off while ours is in place (its display chain is set aside)
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FI.Enabled"))) V->Set(0, ECVF_SetByCommandline);
	}
	virtual void ShutdownModule() override
	{
		if (bRegistered) IModularFeatures::Get().UnregisterModularFeature(IDXGISwapchainProvider::GetModularFeatureName(), &Provider);
	}
};

IMPLEMENT_MODULE(FADPFrameGenModule, ADPFrameGen)
