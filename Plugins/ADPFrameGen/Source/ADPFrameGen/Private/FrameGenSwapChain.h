// The frame generation's display chain: Unreal draws each frame into our own buffers; we show an in-between frame made
// from it and the one before, then the real frame half a frame later, on the real display chain (see FrameGen.cpp).
#pragma once

#include "CoreMinimal.h"
#include "ADPFrameGen.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <d3d12.h>
#include <dxgi1_6.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

namespace ADPFrameGen
{

	/** Wraps the real display chain (its buffers handed out are ours). */
	HRESULT Create(IDXGIFactory2* Factory, IUnknown* Queue, HWND Wnd, const DXGI_SWAP_CHAIN_DESC1* Desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* Full, IDXGIOutput* Restrict, IDXGISwapChain1** Out);
}
