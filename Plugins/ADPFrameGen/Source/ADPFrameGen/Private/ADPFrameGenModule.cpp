// Airport Dynasty Pro's own frame generation: handed to Unreal as its display chain maker when it is switched on in
// Settings (ADP.Graphics OwnFrameGen; read once at start, so switching needs a restart). It goes first, ahead of any
// other maker (AMD's FSR plugin has one too, set aside while ours runs), and only in the game, never in the editor.
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "Misc/ConfigCacheIni.h"
#include "Windows/IDXGISwapchainProvider.h"
#include "FrameGenSwapChain.h"

namespace ADPFrameGen { void SetEnabled(bool b); }

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
		bool bOn = false;
		if (GConfig) GConfig->GetBool(TEXT("ADP.Graphics"), TEXT("OwnFrameGen"), bOn, GGameUserSettingsIni);
		if (FParse::Param(FCommandLine::Get(), TEXT("OwnFrameGen"))) bOn = true;
		if (!bOn) return;
		ADPFrameGen::SetEnabled(true);
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
		UE_LOG(LogTemp, Log, TEXT("ADPFrameGen: our frame generation is on (%d other makers set aside)"), Others.Num());
		// AMD's frame generation stays off while ours runs (one at a time)
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FidelityFX.FI.Enabled"))) V->Set(0, ECVF_SetByCommandline);
	}
	virtual void ShutdownModule() override
	{
		if (bRegistered) IModularFeatures::Get().UnregisterModularFeature(IDXGISwapchainProvider::GetModularFeatureName(), &Provider);
	}
};

IMPLEMENT_MODULE(FADPFrameGenModule, ADPFrameGen)
