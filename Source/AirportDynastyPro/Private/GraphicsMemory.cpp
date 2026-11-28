#include "GraphicsMemory.h"
#include "ADPLog.h"
#include "RHI.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <dxgi1_4.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

ADPGraphicsMemory::FNow ADPGraphicsMemory::Now()
{
	FNow R;
#if PLATFORM_WINDOWS
	// (the adapter the game draws on: the one with the engine's own device id, kept from the first asking)
	static IDXGIAdapter3* Card = nullptr; static bool bAsked = false;
	if (!bAsked)
	{
		bAsked = true;
		IDXGIFactory1* F = nullptr;
		if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&F)) && F)
		{
			IDXGIAdapter1* A = nullptr;
			for (uint32 i = 0; !Card && F->EnumAdapters1(i, &A) != DXGI_ERROR_NOT_FOUND; ++i)
			{
				DXGI_ADAPTER_DESC1 D; A->GetDesc1(&D);
				if (D.VendorId == GRHIVendorId && D.DeviceId == GRHIDeviceId) A->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&Card);
				A->Release();
			}
			F->Release();
		}
	}
	if (!Card) return R;
	DXGI_QUERY_VIDEO_MEMORY_INFO L, S;
	if (FAILED(Card->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &L)) || FAILED(Card->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &S))) return R;
	R.UsedMB = int64(L.CurrentUsage >> 20); R.BudgetMB = int64(L.Budget >> 20); R.SharedMB = int64(S.CurrentUsage >> 20); R.bKnown = true;
#endif
	return R;
}

FString ADPGraphicsMemory::Line()
{
	const FNow N = Now();
	return N.bKnown ? FString::Printf(TEXT("graphics memory %lld MB used of the %lld MB Windows gives the game%s, %lld MB in main memory"), N.UsedMB, N.BudgetMB, N.UsedMB > N.BudgetMB ? TEXT(" (OVER)") : TEXT(""), N.SharedMB) : FString(TEXT("graphics memory not known"));
}

namespace ADPGraphicsMemoryInternal
{
	constexpr int64 KeepFreeMB = 450, RoomMB = 800, LeastMB = 1000;
	int32 Most = 0, Pool = 0, Full = 0;

	bool Watch(float)
	{
		const ADPGraphicsMemory::FNow N = ADPGraphicsMemory::Now();
		IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.PoolSize"));
		if (!N.bKnown || !V || Most <= 0) return true;
		const int64 Free = N.BudgetMB - N.UsedMB;
		int32 Want = Pool;
		if (Free < KeepFreeMB)
		{
			FTextureMemoryStats M; RHIGetTextureMemoryStats(M);
			Want = int32(FMath::Max(FMath::Min<int64>(LeastMB, Most), FMath::Min<int64>(Pool, M.StreamingMemorySize / 1048576) - (KeepFreeMB - Free)));
		}
		else if (Free > RoomMB) Want = FMath::Min(Most, Pool + 50);
		if (Want < Pool) ADP_LOG(TEXT("Graphics memory: nearly full (%s); texture pool would be %d -> %d MB (left as it is while playing)"), *ADPGraphicsMemory::Line(), Pool, Want);
		else if (Want == Most && Pool < Most) ADP_LOG(TEXT("Graphics memory: room again, texture pool %d MB"), Want);
		else if (Free < 0 && Want == Pool && (Full++ % 15) == 0) ADP_WARN(TEXT("Graphics memory: over the limit with the texture pool at its least (%s)"), *ADPGraphicsMemory::Line());
		// (NOT set while playing: resizing r.Streaming.PoolSize mid-game killed the game with a fast fail (0xC0000409) a
		//  moment after each change - 11-28, six views runs at the town hub. Only watched and written down now; the pool is
		//  set once from your setting, before the world.)
		if (Want != Pool) { Pool = Want; if (!GWorld || !GWorld->HasBegunPlay()) V->Set(Pool, ECVF_SetByGameSetting); }
		return true;
	}
}

void ADPGraphicsMemory::Guard(int32 MaxPoolMB)
{
	using namespace ADPGraphicsMemoryInternal;
	const bool bFirst = Most == 0;
	Most = MaxPoolMB; Pool = bFirst ? MaxPoolMB : FMath::Min(Pool, MaxPoolMB);
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.PoolSize"))) V->Set(Pool, ECVF_SetByGameSetting);
	if (bFirst) FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Watch), 2.f);
}
