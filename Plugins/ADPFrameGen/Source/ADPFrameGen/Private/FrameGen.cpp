// Airport Dynasty Pro's own frame generation (written from scratch; made for NVIDIA cards first, the owner's RTX
// 3070 Ti, and working on any DirectX 12 card).
//
// How it works: Unreal draws each frame into our own buffers instead of the screen's. When a frame is finished, our
// own thread on the graphics card (a queue of its own) copies it into a working picture, finds how everything moved
// since the frame before (a motion search on brightness at a sixteenth, an eighth and a quarter of the size, each
// level refining the coarser one), makes the in-between picture half-way along that motion, shows it at once, and
// shows the real frame half a frame later. So the screen gets two pictures for every one drawn, evenly spaced. What
// didn't change between the two frames (the menus, the information boxes) is shown exactly as drawn.
// It only starts at a fair frame rate (from 20 drawn frames a second); below that each frame is shown as it is.
#include "FrameGenSwapChain.h"
#include "FrameGenShaders.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/CriticalSection.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <d3dcompiler.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogADPFrameGen, Log, All);

namespace ADPFrameGen
{
	static bool GEnabled = false;
	static TAtomic<int32> GMultiplier(1);
	bool Enabled() { return GEnabled; }
	int32 Multiplier() { return GMultiplier.Load(); }
	void SetEnabled(bool b) { GEnabled = b; }

	template <class T> static void Rel(T*& P) { if (P) { P->Release(); P = nullptr; } }

	enum EPass { PConvert, PLuma4, PDown2, PFlow, PSmooth, PInterp, PCount };
	static const char* GEntry[PCount] = {"Convert", "Luma4", "Down2", "Flow", "Smooth", "Interp"};

	class FChain final : public IDXGISwapChain4
	{
	public:
		int32 Refs = 1;
		IDXGISwapChain4* Real = nullptr;
		ID3D12Device* Dev = nullptr;
		ID3D12CommandQueue* EngineQ = nullptr;
		ID3D12CommandQueue* Q = nullptr;
		ID3D12Fence* FE = nullptr; uint64 NE = 0;          // the engine's frames finished on its queue
		ID3D12Fence* FO = nullptr; uint64 NO = 0;          // our work finished
		HANDLE FOEvent = nullptr;
		ID3D12CommandAllocator* Alloc[4] = {};
		uint64 AllocDone[4] = {};
		ID3D12GraphicsCommandList* List = nullptr;
		ID3D12RootSignature* Root = nullptr;
		ID3D12PipelineState* Pso[PCount] = {};
		ID3D12DescriptorHeap* Heap = nullptr; UINT Inc = 0; UINT HeapAt = 0;

		static constexpr UINT FakeCount = 3, RealCount = 3;
		ID3D12Resource* Fake[FakeCount] = {};
		ID3D12Resource* RealBuf[RealCount] = {};
		ID3D12Resource* Conv[2] = {}; ID3D12Resource* Out = nullptr;
		ID3D12Resource* L4[2] = {}; ID3D12Resource* L8[2] = {}; ID3D12Resource* L16[2] = {};
		ID3D12Resource* F16 = nullptr; ID3D12Resource* F8 = nullptr; ID3D12Resource* F4 = nullptr; ID3D12Resource* F4s = nullptr;
		UINT W = 0, H = 0, W4 = 0, H4 = 0, W8 = 0, H8 = 0, W16 = 0, H16 = 0;
		DXGI_FORMAT EngFmt = DXGI_FORMAT_B8G8R8A8_UNORM, RealFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
		UINT RealFlags = 0;
		UINT FakeIdx = 0, Par = 0; bool bHavePrev = false; uint32 Frame = 0;

		// the pacing thread
		HANDLE Thread = nullptr, JobEvent = nullptr, IdleEvent = nullptr;
		volatile bool bQuit = false;
		struct FJob { uint64 Fence; UINT Fake; double T; UINT Sync; UINT Flags; } Job{};
		double LastT = 0, Interval = 1.0 / 60;
		HRESULT LastHr = S_OK;
		FCriticalSection Lock;

		static DXGI_FORMAT RealOf(DXGI_FORMAT F) { return F == DXGI_FORMAT_B8G8R8A8_UNORM ? DXGI_FORMAT_R8G8B8A8_UNORM : F; }

		// ---- making things ------------------------------------------------------------------------------------
		ID3D12Resource* Tex(UINT w, UINT h, DXGI_FORMAT F, D3D12_RESOURCE_FLAGS Fl, D3D12_RESOURCE_STATES St)
		{
			D3D12_HEAP_PROPERTIES Hp = {}; Hp.Type = D3D12_HEAP_TYPE_DEFAULT;
			D3D12_RESOURCE_DESC D = {};
			D.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; D.Width = FMath::Max(1u, w); D.Height = FMath::Max(1u, h); D.DepthOrArraySize = 1; D.MipLevels = 1;
			D.Format = F; D.SampleDesc.Count = 1; D.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; D.Flags = Fl;
			ID3D12Resource* R = nullptr;
			Dev->CreateCommittedResource(&Hp, D3D12_HEAP_FLAG_NONE, &D, St, nullptr, IID_PPV_ARGS(&R));
			return R;
		}

		bool MakePipelines()
		{
			D3D12_DESCRIPTOR_RANGE Ranges[2] = {};
			Ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; Ranges[0].NumDescriptors = 3; Ranges[0].OffsetInDescriptorsFromTableStart = 0;
			Ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; Ranges[1].NumDescriptors = 1; Ranges[1].OffsetInDescriptorsFromTableStart = 3;
			D3D12_ROOT_PARAMETER Params[2] = {};
			Params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; Params[0].Constants.Num32BitValues = 8; Params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			Params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; Params[1].DescriptorTable.NumDescriptorRanges = 2; Params[1].DescriptorTable.pDescriptorRanges = Ranges; Params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			D3D12_STATIC_SAMPLER_DESC Smp = {};
			Smp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; Smp.AddressU = Smp.AddressV = Smp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; Smp.MaxLOD = D3D12_FLOAT32_MAX; Smp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			D3D12_ROOT_SIGNATURE_DESC Rd = {}; Rd.NumParameters = 2; Rd.pParameters = Params; Rd.NumStaticSamplers = 1; Rd.pStaticSamplers = &Smp;
			ID3DBlob* Blob = nullptr; ID3DBlob* Err = nullptr;
			if (FAILED(D3D12SerializeRootSignature(&Rd, D3D_ROOT_SIGNATURE_VERSION_1, &Blob, &Err))) { Rel(Err); return false; }
			const HRESULT Hr = Dev->CreateRootSignature(0, Blob->GetBufferPointer(), Blob->GetBufferSize(), IID_PPV_ARGS(&Root));
			Rel(Blob);
			if (FAILED(Hr)) return false;
			for (int32 i = 0; i < PCount; ++i)
			{
				ID3DBlob* Code = nullptr; ID3DBlob* Msg = nullptr;
				if (FAILED(D3DCompile(GFrameGenHlsl, strlen(GFrameGenHlsl), "FrameGen", nullptr, nullptr, GEntry[i], "cs_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &Code, &Msg)))
				{
					UE_LOG(LogADPFrameGen, Warning, TEXT("Shader %hs: %hs"), GEntry[i], Msg ? (const char*)Msg->GetBufferPointer() : "?");
					Rel(Msg); return false;
				}
				D3D12_COMPUTE_PIPELINE_STATE_DESC Pd = {}; Pd.pRootSignature = Root; Pd.CS = {Code->GetBufferPointer(), Code->GetBufferSize()};
				const HRESULT H2 = Dev->CreateComputePipelineState(&Pd, IID_PPV_ARGS(&Pso[i]));
				Rel(Code); Rel(Msg);
				if (FAILED(H2)) return false;
			}
			D3D12_DESCRIPTOR_HEAP_DESC Hd = {}; Hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; Hd.NumDescriptors = 256; Hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			if (FAILED(Dev->CreateDescriptorHeap(&Hd, IID_PPV_ARGS(&Heap)))) return false;
			Inc = Dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			for (int32 i = 0; i < 4; ++i) if (FAILED(Dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Alloc[i])))) return false;
			if (FAILED(Dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Alloc[0], nullptr, IID_PPV_ARGS(&List)))) return false;
			List->Close();
			return true;
		}

		void FreeSized()
		{
			for (auto*& R : Fake) Rel(R);
			for (auto*& R : RealBuf) Rel(R);
			for (int32 i = 0; i < 2; ++i) { Rel(Conv[i]); Rel(L4[i]); Rel(L8[i]); Rel(L16[i]); }
			Rel(Out); Rel(F16); Rel(F8); Rel(F4); Rel(F4s);
		}

		bool MakeSized()
		{
			DXGI_SWAP_CHAIN_DESC1 D; Real->GetDesc1(&D);
			W = D.Width; H = D.Height;
			W4 = (W + 3) / 4; H4 = (H + 3) / 4; W8 = (W4 + 1) / 2; H8 = (H4 + 1) / 2; W16 = (W8 + 1) / 2; H16 = (H8 + 1) / 2;
			for (UINT i = 0; i < FakeCount; ++i) { Fake[i] = Tex(W, H, EngFmt, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT); if (!Fake[i]) return false; Fake[i]->SetName(L"ADPFrameGen buffer"); }
			for (UINT i = 0; i < RealCount; ++i) if (FAILED(Real->GetBuffer(i, IID_PPV_ARGS(&RealBuf[i])))) return false;
			const D3D12_RESOURCE_FLAGS U = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			const D3D12_RESOURCE_STATES S = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			for (int32 i = 0; i < 2; ++i)
			{
				Conv[i] = Tex(W, H, RealFmt, U, S);
				L4[i] = Tex(W4, H4, DXGI_FORMAT_R16_FLOAT, U, S); L8[i] = Tex(W8, H8, DXGI_FORMAT_R16_FLOAT, U, S); L16[i] = Tex(W16, H16, DXGI_FORMAT_R16_FLOAT, U, S);
			}
			Out = Tex(W, H, RealFmt, U, S);
			F16 = Tex(W16, H16, DXGI_FORMAT_R16G16B16A16_FLOAT, U, S); F8 = Tex(W8, H8, DXGI_FORMAT_R16G16B16A16_FLOAT, U, S);
			F4 = Tex(W4, H4, DXGI_FORMAT_R16G16B16A16_FLOAT, U, S); F4s = Tex(W4, H4, DXGI_FORMAT_R16G16B16A16_FLOAT, U, S);
			FakeIdx = 0; bHavePrev = false;
			return Conv[0] && Conv[1] && Out && F4s && L16[1];
		}

		// ---- recording ----------------------------------------------------------------------------------------
		void Barrier(ID3D12Resource* R, D3D12_RESOURCE_STATES A, D3D12_RESOURCE_STATES B)
		{
			if (A == B) return;
			D3D12_RESOURCE_BARRIER Bar = {}; Bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Bar.Transition.pResource = R; Bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; Bar.Transition.StateBefore = A; Bar.Transition.StateAfter = B;
			List->ResourceBarrier(1, &Bar);
		}
		void UavBarrier()
		{
			D3D12_RESOURCE_BARRIER Bar = {}; Bar.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; Bar.UAV.pResource = nullptr;
			List->ResourceBarrier(1, &Bar);
		}
		void Srv(UINT Slot, ID3D12Resource* R)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC D = {}; D.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; D.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; D.Texture2D.MipLevels = 1;
			D.Format = R ? R->GetDesc().Format : DXGI_FORMAT_R8G8B8A8_UNORM;
			D3D12_CPU_DESCRIPTOR_HANDLE C = Heap->GetCPUDescriptorHandleForHeapStart(); C.ptr += SIZE_T(Slot) * Inc;
			Dev->CreateShaderResourceView(R, &D, C);
		}
		/** One pass: up to three pictures read (made readable for it), one written. */
		void Pass(EPass P, ID3D12Resource* T0, D3D12_RESOURCE_STATES S0, ID3D12Resource* T1, ID3D12Resource* T2, ID3D12Resource* Dst, UINT w, UINT h, UINT sw, UINT sh, float p0 = 0, float p1 = 0, float p3 = 0)
		{
			const D3D12_RESOURCE_STATES Read = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, Uav = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			Barrier(T0, S0, Read); if (T1) Barrier(T1, Uav, Read); if (T2) Barrier(T2, Uav, Read);
			const UINT Base = (Frame % 2) * 128 + HeapAt; HeapAt += 4;
			Srv(Base, T0); Srv(Base + 1, T1); Srv(Base + 2, T2);
			D3D12_UNORDERED_ACCESS_VIEW_DESC U = {}; U.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D; U.Format = Dst->GetDesc().Format;
			D3D12_CPU_DESCRIPTOR_HANDLE C = Heap->GetCPUDescriptorHandleForHeapStart(); C.ptr += SIZE_T(Base + 3) * Inc;
			Dev->CreateUnorderedAccessView(Dst, nullptr, &U, C);
			List->SetPipelineState(Pso[P]);
			const float K[8] = {0, 0, 0, 0, p0, p1, 0, p3};
			uint32 Kc[8]; FMemory::Memcpy(Kc, K, sizeof(K));
			Kc[0] = w; Kc[1] = h; Kc[2] = sw; Kc[3] = sh;
			List->SetComputeRoot32BitConstants(0, 8, Kc, 0);
			D3D12_GPU_DESCRIPTOR_HANDLE G = Heap->GetGPUDescriptorHandleForHeapStart(); G.ptr += UINT64(Base) * Inc;
			List->SetComputeRootDescriptorTable(1, G);
			List->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
			UavBarrier();
			Barrier(T0, Read, S0); if (T1) Barrier(T1, Read, Uav); if (T2) Barrier(T2, Read, Uav);
		}
		void Begin(UINT A)
		{
			if (FO->GetCompletedValue() < AllocDone[A]) { FO->SetEventOnCompletion(AllocDone[A], FOEvent); WaitForSingleObject(FOEvent, 1000); }
			Alloc[A]->Reset();
			List->Reset(Alloc[A], nullptr);
			ID3D12DescriptorHeap* Heaps[] = {Heap};
			List->SetDescriptorHeaps(1, Heaps);
			List->SetComputeRootSignature(Root);
		}
		void Submit(UINT A)
		{
			List->Close();
			ID3D12CommandList* L[] = {List};
			Q->ExecuteCommandLists(1, L);
			Q->Signal(FO, ++NO);
			AllocDone[A] = NO;
		}
		/** A picture onto the real display chain's next buffer. */
		void CopyToScreen(ID3D12Resource* Src)
		{
			ID3D12Resource* B = RealBuf[Real->GetCurrentBackBufferIndex() % RealCount];
			Barrier(Src, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
			Barrier(B, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
			List->CopyResource(B, Src);
			Barrier(B, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
			Barrier(Src, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		// ---- one frame: the in-between picture (when there's an earlier one), then the real one -----------------
		void Run(const FJob& J)
		{
			const UINT A = (Frame % 2) * 2;
			HeapAt = 0;
			Begin(A);
			Q->Wait(FE, J.Fence);   // Unreal's drawing of the frame finished first
			const UINT P = Par, O = Par ^ 1;
			Pass(PConvert, Fake[J.Fake], D3D12_RESOURCE_STATE_PRESENT, nullptr, nullptr, Conv[P], W, H, W, H);
			Pass(PLuma4, Conv[P], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, nullptr, L4[P], W4, H4, W, H);
			Pass(PDown2, L4[P], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, nullptr, L8[P], W8, H8, W4, H4);
			Pass(PDown2, L8[P], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, nullptr, L16[P], W16, H16, W8, H8);
			const bool bGen = bHavePrev && Interval < 1.0 / 20;
			if (bGen)
			{
				Pass(PFlow, L16[P], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L16[O], nullptr, F16, W16, H16, W16, H16, 6, 0.02f, 0);
				Pass(PFlow, L8[P], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L8[O], F16, F8, W8, H8, W8, H8, 2, 0.02f, 1);
				Pass(PFlow, L4[P], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L4[O], F8, F4, W4, H4, W4, H4, 2, 0.02f, 1);
				Pass(PSmooth, F4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, nullptr, F4s, W4, H4, W4, H4);
				Pass(PInterp, Conv[O], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, Conv[P], F4s, Out, W, H, W, H);
				CopyToScreen(Out);
				Submit(A);
				const HRESULT Hr = Real->Present(J.Sync, J.Flags);
				if (FAILED(Hr)) LastHr = Hr;
				// (-FGOnlyGen, for checking: only the in-between pictures shown)
				static const bool bOnlyGen = FParse::Param(FCommandLine::Get(), TEXT("FGOnlyGen"));
				if (bOnlyGen) { GMultiplier = 2; bHavePrev = true; Par ^= 1; ++Frame; return; }
				// the real frame half a frame later
				const double At = FPlatformTime::Seconds() + Interval * 0.5;
				Begin(A + 1);
				CopyToScreen(Conv[P]);
				Submit(A + 1);
				for (double Now = FPlatformTime::Seconds(); Now < At - 0.0015; Now = FPlatformTime::Seconds()) FPlatformProcess::SleepNoStats(0.001f);
				while (FPlatformTime::Seconds() < At) FPlatformProcess::YieldThread();
				GMultiplier = 2;
			}
			else
			{
				CopyToScreen(Conv[P]);
				Submit(A);
				GMultiplier = 1;
			}
			const HRESULT Hr = Real->Present(J.Sync, J.Flags);
			if (FAILED(Hr)) LastHr = Hr;
			bHavePrev = true; Par ^= 1; ++Frame;
		}

		static DWORD WINAPI ThreadMain(LPVOID Me)
		{
			FChain* C = (FChain*)Me;
			for (;;)
			{
				WaitForSingleObject(C->JobEvent, INFINITE);
				if (C->bQuit) break;
				C->Run(C->Job);
				SetEvent(C->IdleEvent);
			}
			return 0;
		}
		void WaitIdle() { if (IdleEvent) WaitForSingleObject(IdleEvent, 2000); if (IdleEvent) SetEvent(IdleEvent); }
		void FlushQueue() { Q->Signal(FO, ++NO); FO->SetEventOnCompletion(NO, FOEvent); WaitForSingleObject(FOEvent, 2000); }

		bool Init(IDXGIFactory2* Factory, IUnknown* Queue, HWND Wnd, const DXGI_SWAP_CHAIN_DESC1* Desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* Full, IDXGIOutput* Restrict)
		{
			if (FAILED(Queue->QueryInterface(IID_PPV_ARGS(&EngineQ)))) return false;
			if (FAILED(EngineQ->GetDevice(IID_PPV_ARGS(&Dev)))) return false;
			D3D12_COMMAND_QUEUE_DESC Qd = {}; Qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; Qd.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
			if (FAILED(Dev->CreateCommandQueue(&Qd, IID_PPV_ARGS(&Q)))) return false;
			Q->SetName(L"ADPFrameGen");
			if (FAILED(Dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&FE))) || FAILED(Dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&FO)))) return false;
			FOEvent = CreateEventW(nullptr, 0, 0, nullptr);
			if (!MakePipelines()) return false;
			EngFmt = Desc->Format; RealFmt = RealOf(EngFmt);
			DXGI_SWAP_CHAIN_DESC1 R = *Desc;
			R.Format = RealFmt; R.BufferCount = RealCount;
			R.Flags &= ~DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;   // we pace the frames ourselves
			RealFlags = R.Flags;
			IDXGISwapChain1* S1 = nullptr;
			if (FAILED(Factory->CreateSwapChainForHwnd(Q, Wnd, &R, Full, Restrict, &S1))) return false;
			const HRESULT Hr = S1->QueryInterface(IID_PPV_ARGS(&Real));
			S1->Release();
			if (FAILED(Hr) || !MakeSized()) return false;
			JobEvent = CreateEventW(nullptr, 0, 0, nullptr);
			IdleEvent = CreateEventW(nullptr, 0, 1, nullptr);
			Thread = CreateThread(nullptr, 0, &FChain::ThreadMain, this, 0, nullptr);
			SetThreadPriority(Thread, THREAD_PRIORITY_HIGHEST);
			UE_LOG(LogADPFrameGen, Log, TEXT("Frame generation on: %ux%u"), W, H);
			return true;
		}

		~FChain()
		{
			if (Thread) { WaitIdle(); bQuit = true; SetEvent(JobEvent); WaitForSingleObject(Thread, 2000); CloseHandle(Thread); }
			if (Q && FO) FlushQueue();
			FreeSized();
			Rel(Real);
			for (auto*& P : Pso) Rel(P);
			for (auto*& A : Alloc) Rel(A);
			Rel(List); Rel(Root); Rel(Heap); Rel(FE); Rel(FO); Rel(Q); Rel(EngineQ); Rel(Dev);
			if (FOEvent) CloseHandle(FOEvent);
			if (JobEvent) CloseHandle(JobEvent);
			if (IdleEvent) CloseHandle(IdleEvent);
			GMultiplier = 1;
		}

		// ---- IUnknown -----------------------------------------------------------------------------------------
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID Riid, void** Obj) override
		{
			if (Riid == __uuidof(IUnknown) || Riid == __uuidof(IDXGIObject) || Riid == __uuidof(IDXGIDeviceSubObject) || Riid == __uuidof(IDXGISwapChain) ||
				Riid == __uuidof(IDXGISwapChain1) || Riid == __uuidof(IDXGISwapChain2) || Riid == __uuidof(IDXGISwapChain3) || Riid == __uuidof(IDXGISwapChain4))
			{ *Obj = this; AddRef(); return S_OK; }
			*Obj = nullptr; return E_NOINTERFACE;
		}
		ULONG STDMETHODCALLTYPE AddRef() override { return ULONG(FPlatformAtomics::InterlockedIncrement(&Refs)); }
		ULONG STDMETHODCALLTYPE Release() override { const int32 R = FPlatformAtomics::InterlockedDecrement(&Refs); if (!R) delete this; return R; }
		// ---- IDXGIObject ----------------------------------------------------------------------------------------
		HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID N, UINT S, const void* D) override { return Real->SetPrivateData(N, S, D); }
		HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID N, const IUnknown* U) override { return Real->SetPrivateDataInterface(N, U); }
		HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID N, UINT* S, void* D) override { return Real->GetPrivateData(N, S, D); }
		HRESULT STDMETHODCALLTYPE GetParent(REFIID R, void** P) override { return Real->GetParent(R, P); }
		HRESULT STDMETHODCALLTYPE GetDevice(REFIID R, void** D) override { return Real->GetDevice(R, D); }
		// ---- IDXGISwapChain ----------------------------------------------------------------------------------------
		HRESULT STDMETHODCALLTYPE Present(UINT Sync, UINT Flags) override
		{
			if (Flags & DXGI_PRESENT_TEST) return Real->Present(Sync, Flags);
			WaitForSingleObject(IdleEvent, 1000);   // the frame before is on screen (never more than one waiting)
			if (FAILED(LastHr)) { const HRESULT H2 = LastHr; LastHr = S_OK; SetEvent(IdleEvent); return H2; }
			EngineQ->Signal(FE, ++NE);
			const double T = FPlatformTime::Seconds();
			if (LastT > 0) Interval = FMath::Clamp(Interval * 0.8 + (T - LastT) * 0.2, 1.0 / 400, 0.5);
			LastT = T;
			Job = {NE, FakeIdx, T, Sync, (Sync ? Flags & ~DXGI_PRESENT_ALLOW_TEARING : Flags)};
			FakeIdx = (FakeIdx + 1) % FakeCount;
			SetEvent(JobEvent);
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE GetBuffer(UINT I, REFIID R, void** S) override { return I < FakeCount && Fake[I] ? Fake[I]->QueryInterface(R, S) : DXGI_ERROR_INVALID_CALL; }
		HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL F, IDXGIOutput* T) override { WaitIdle(); return Real->SetFullscreenState(F, T); }
		HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* F, IDXGIOutput** T) override { return Real->GetFullscreenState(F, T); }
		HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* D) override { const HRESULT Hr = Real->GetDesc(D); if (SUCCEEDED(Hr)) { D->BufferCount = FakeCount; D->BufferDesc.Format = EngFmt; } return Hr; }
		HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT Count, UINT Width, UINT Height, DXGI_FORMAT Fmt, UINT Flags) override
		{
			WaitIdle(); FlushQueue(); FreeSized();
			if (Fmt != DXGI_FORMAT_UNKNOWN) { EngFmt = Fmt; RealFmt = RealOf(Fmt); }
			const HRESULT Hr = Real->ResizeBuffers(RealCount, Width, Height, Fmt == DXGI_FORMAT_UNKNOWN ? DXGI_FORMAT_UNKNOWN : RealFmt, (Flags & ~DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) | (RealFlags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING));
			if (FAILED(Hr)) return Hr;
			return MakeSized() ? S_OK : E_FAIL;
		}
		HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* M) override { return Real->ResizeTarget(M); }
		HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** O) override { return Real->GetContainingOutput(O); }
		HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* S) override { return Real->GetFrameStatistics(S); }
		HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* C) override { return Real->GetLastPresentCount(C); }
		// ---- IDXGISwapChain1 ---------------------------------------------------------------------------------------
		HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* D) override { const HRESULT Hr = Real->GetDesc1(D); if (SUCCEEDED(Hr)) { D->BufferCount = FakeCount; D->Format = EngFmt; } return Hr; }
		HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* D) override { return Real->GetFullscreenDesc(D); }
		HRESULT STDMETHODCALLTYPE GetHwnd(HWND* W2) override { return Real->GetHwnd(W2); }
		HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID R, void** U) override { return Real->GetCoreWindow(R, U); }
		HRESULT STDMETHODCALLTYPE Present1(UINT Sync, UINT Flags, const DXGI_PRESENT_PARAMETERS*) override { return Present(Sync, Flags); }
		BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override { return Real->IsTemporaryMonoSupported(); }
		HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** O) override { return Real->GetRestrictToOutput(O); }
		HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA* C) override { return Real->SetBackgroundColor(C); }
		HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA* C) override { return Real->GetBackgroundColor(C); }
		HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION R) override { return Real->SetRotation(R); }
		HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION* R) override { return Real->GetRotation(R); }
		// ---- IDXGISwapChain2 ---------------------------------------------------------------------------------------
		HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Wd, UINT Ht) override { return Real->SetSourceSize(Wd, Ht); }
		HRESULT STDMETHODCALLTYPE GetSourceSize(UINT* Wd, UINT* Ht) override { return Real->GetSourceSize(Wd, Ht); }
		HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT L) override { return S_OK; }
		HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* L) override { if (L) *L = 1; return S_OK; }
		HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override { return nullptr; }
		HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F* M) override { return Real->SetMatrixTransform(M); }
		HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F* M) override { return Real->GetMatrixTransform(M); }
		// ---- IDXGISwapChain3 ---------------------------------------------------------------------------------------
		UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override { return FakeIdx; }
		HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE C, UINT* S) override { return Real->CheckColorSpaceSupport(C, S); }
		HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE C) override { return Real->SetColorSpace1(C); }
		HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT Count, UINT Wd, UINT Ht, DXGI_FORMAT F, UINT Flags, const UINT*, IUnknown* const*) override { return ResizeBuffers(Count, Wd, Ht, F, Flags); }
		// ---- IDXGISwapChain4 ---------------------------------------------------------------------------------------
		HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE T, UINT S, void* M) override { return Real->SetHDRMetaData(T, S, M); }
	};

	HRESULT Create(IDXGIFactory2* Factory, IUnknown* Queue, HWND Wnd, const DXGI_SWAP_CHAIN_DESC1* Desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* Full, IDXGIOutput* Restrict, IDXGISwapChain1** Out)
	{
		FChain* C = new FChain();
		if (C->Init(Factory, Queue, Wnd, Desc, Full, Restrict)) { *Out = C; return S_OK; }
		UE_LOG(LogADPFrameGen, Warning, TEXT("Frame generation couldn't start on this PC: the game runs without it"));
		C->Release();
		return Factory->CreateSwapChainForHwnd(Queue, Wnd, Desc, Full, Restrict, Out);
	}
}
