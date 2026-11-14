// The airport's crash tenders, drivable (FireTruck.h); the fire station's bay doors.
#include "FireTruck.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ADPLog.h"
#include "BabylonModel.h"
#include "FireStation.h"
#include "SunClock.h"
#include "Puddles.h"
#include "Airfield.h"
#include "ADPWalker.h"
#include "DriverPoseAnim.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SpotLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include <atomic>
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

TArray<TWeakObjectPtr<AFireTruck>> AFireTruck::All;

namespace
{
	// the truck's own measures (Art/FireTruck/make_panther.py, Blender metres) and its physics
	constexpr float AxleY[3] = {2.85f, -2.55f, -4.2f};
	constexpr float TrackHalfW = 1.22f, WheelR = 0.75f, KingpinOff = 0.12f;
	constexpr float TruckMass = 28000.f, Gravity = 9.81f;
	constexpr float KSpring = 458000.f, CDamp = 42000.f, AntiRoll = 180000.f;   // N/m, N s/m
	constexpr float BumpCm = 15.f, DroopCm = 10.f, StrokeCm = BumpCm + DroopCm, WheelRCm = WheelR * 100.f;
	constexpr float TyreMu = 0.9f, DrivePower = 780000.f, TopSpeed = 39.3f, ReverseTop = 4.2f;   // (owner, 2026-11-14: tops out at 135 km/h, a big crash tender's ~1050 hp)
	constexpr float MaxSteer = 25.f, Wheelbase = 5.4f, RearOff = 1.65f, TrackWidth = 2.44f;
	FVector BlenderToRoot(float X, float Y, float Z) { return FVector(X, -Y, Z) * 100.f; }
	FTransform About(const FVector& P, const FQuat& Q) { return FTransform(-P) * FTransform(Q) * FTransform(P); }
	FVector DirToRoot(float X, float Y) { return FVector(X, -Y, 0.f).GetSafeNormal(); }

	// ---- the sounds, made here (no recordings): 44.1 kHz mono
	constexpr int32 SndRate = 44100;
	float SndNoise(uint32& Seed) { Seed = Seed * 1664525u + 1013904223u; return ((Seed >> 9) & 0xFFFF) / 32767.5f - 1.f; }
	TArray<int16> SndPcm(const TArray<float>& F) { TArray<int16> P; P.SetNumUninitialized(F.Num()); for (int32 I = 0; I < F.Num(); ++I) P[I] = int16(FMath::Clamp(FMath::Tanh(F[I] * 1.2f) * 30000.f, -32767.f, 32767.f)); return P; }
	/** the door opening: the handle's latch letting go (two quick metal clicks), the seal's soft pop, the hinge */
	const TArray<int16>& DoorOpenPcm()
	{
		static TArray<int16> P;
		if (P.Num()) return P;
		TArray<float> F; F.SetNumZeroed(SndRate * 45 / 100);
		uint32 Sd = 7u; float Hp = 0.f, Prev = 0.f, Lp = 0.f;
		for (int32 I = 0; I < F.Num(); ++I)
		{
			const float T = float(I) / SndRate, N = SndNoise(Sd);
			Hp = 0.7f * (Hp + N - Prev); Prev = N;                              // bright noise (the metal)
			Lp += 0.08f * (N - Lp);                                             // dull noise (the rubber)
			float V = 0.f;
			for (const float C : {0.f, 0.034f}) if (T >= C) V += 0.75f * Hp * FMath::Exp(-(T - C) / 0.0045f) + 0.35f * FMath::Sin(2.f * PI * 2350.f * (T - C)) * FMath::Exp(-(T - C) / 0.006f);
			if (T >= 0.05f) V += 0.55f * Lp * FMath::Exp(-(T - 0.05f) / 0.035f) + 0.3f * FMath::Sin(2.f * PI * 150.f * (T - 0.05f)) * FMath::Exp(-(T - 0.05f) / 0.03f);
			if (T >= 0.12f) { const float H = T - 0.12f; V += 0.05f * FMath::Sin(2.f * PI * (330.f * H + 120.f * H * H)) * FMath::Sin(PI * FMath::Min(1.f, H / 0.3f)) * (0.6f + 0.4f * FMath::Sin(2.f * PI * 23.f * H)); }
			F[I] = V;
		}
		P = SndPcm(F);
		return P;
	}
	/** the door shutting: a heavy cab door's low thump, the latch catching, a little rattle */
	const TArray<int16>& DoorShutPcm()
	{
		static TArray<int16> P;
		if (P.Num()) return P;
		TArray<float> F; F.SetNumZeroed(SndRate * 7 / 10);
		uint32 Sd = 11u; float Lp = 0.f, Hp = 0.f, Prev = 0.f;
		for (int32 I = 0; I < F.Num(); ++I)
		{
			const float T = float(I) / SndRate, N = SndNoise(Sd);
			Lp += 0.05f * (N - Lp); Hp = 0.7f * (Hp + N - Prev); Prev = N;
			float V = 0.95f * FMath::Sin(2.f * PI * 62.f * T) * FMath::Exp(-T / 0.11f) + 0.5f * FMath::Sin(2.f * PI * 124.f * T + 0.6f) * FMath::Exp(-T / 0.07f);
			V += 1.6f * Lp * FMath::Exp(-T / 0.025f);
			if (T >= 0.014f) V += 0.55f * Hp * FMath::Exp(-(T - 0.014f) / 0.004f) + 0.25f * FMath::Sin(2.f * PI * 1900.f * (T - 0.014f)) * FMath::Exp(-(T - 0.014f) / 0.008f);
			V += 0.08f * FMath::Sin(2.f * PI * 870.f * T) * FMath::Exp(-T / 0.06f) * (0.5f + 0.5f * FMath::Sin(2.f * PI * 37.f * T));
			F[I] = V * FMath::Min(1.f, T / 0.002f);
		}
		P = SndPcm(F);
		return P;
	}
	/** the siren: an electronic wail, its tone rising and falling every 4.4 s, the horn speaker's hard edge */
	struct FSirenState { double Phase = 0.0, T = 0.0; };
}

// ---- the engine's voice: what the game sets each frame (speed, load) and the phases the sound thread carries on
struct FEngineVoice
{
	std::atomic<float> Kmh{0.f}, Load{0.f};
	double Rpm = 750.0, Diesel = 0.0, Whine = 0.0, Mesh = 0.0, Noise = 0.0, Lp = 0.0, Lp2 = 0.0, Rumble = 0.0;
	uint32 Seed = 0x9e3779b9u;
	float Rnd() { Seed = Seed * 1664525u + 1013904223u; return float(Seed >> 8) / 8388608.f - 1.f; }
	void Fill(TArray<int16>& Out, int32 N)
	{
		Out.SetNumUninitialized(N);
		const float Kmh_ = Kmh.load(), Load_ = Load.load();
		// the diesel's speed: idling at 750, pulled up by the load and the road speed (a generator: it follows the power asked
		// of it, not the wheels), smoothly
		const double RpmWant = 750.0 + 1400.0 * Load_ + 6.0 * FMath::Min(Kmh_, 100.f);
		for (int32 I = 0; I < N; ++I)
		{
			Rpm += (RpmWant - Rpm) * 0.00004;
			// the diesel: a V8's firing (4 bangs a revolution), each a short thump with a little ring, plus the exhaust's rumble
			const double Fire = Rpm / 60.0 * 4.0;
			Diesel += Fire / SndRate; if (Diesel >= 1.0) Diesel -= 1.0;
			const double Thump = FMath::Exp(-Diesel * 9.0) * FMath::Sin(Diesel * 2.0 * PI * 1.0) + 0.35 * FMath::Sin(Diesel * 2.0 * PI * 2.0) * FMath::Exp(-Diesel * 4.0);
			Rumble += ((Fire * 0.5) / SndRate); if (Rumble >= 1.0) Rumble -= 1.0;
			const double Exhaust = 0.5 * FMath::Sin(Rumble * 2.0 * PI) + 0.25 * FMath::Sin(Rumble * 4.0 * PI + 0.7);
			// the electric drive: a whine whose pitch follows the wheels (about 12 Hz a km/h, two harmonics), louder under load,
			// and the gear mesh above it
			const double WhineHz = 40.0 + 12.0 * Kmh_;
			Whine += WhineHz / SndRate; if (Whine >= 1.0) Whine -= 1.0;
			Mesh += (WhineHz * 3.0) / SndRate; if (Mesh >= 1.0) Mesh -= 1.0;
			const double Ev = FMath::Clamp(Kmh_ / 30.0, 0.0, 1.0) * (0.35 + 0.65 * Load_);
			const double Electric = Ev * (0.6 * FMath::Sin(Whine * 2.0 * PI) + 0.25 * FMath::Sin(Whine * 4.0 * PI) + 0.15 * FMath::Sin(Mesh * 2.0 * PI));
			// the tyres and the air: filtered noise swelling with speed
			const double W = Rnd();
			Lp += (W - Lp) * 0.08; Lp2 += (Lp - Lp2) * 0.08;
			const double Road = Lp2 * 3.0 * FMath::Clamp(Kmh_ / 60.0, 0.0, 1.2);
			const double Idle = 0.55 + 0.45 * Load_;
			const double V = Idle * (0.9 * Thump + 0.5 * Exhaust) + 0.7 * Electric + 0.6 * Road;
			Out[I] = int16(FMath::Clamp(FMath::Tanh(V * 0.9) * 22000.0, -32000.0, 32000.0));
		}
	}
};

namespace
{
	void SirenFill(FSirenState& S, TArray<int16>& Out, int32 N)
	{
		Out.SetNumUninitialized(N);
		for (int32 I = 0; I < N; ++I)
		{
			const double C = FMath::Fmod(S.T, 4.4) / 4.4;
			const double Hz = 640.0 + 820.0 * (C < 0.55 ? FMath::Sin(C / 0.55 * HALF_PI) : FMath::Cos((C - 0.55) / 0.45 * HALF_PI));
			S.Phase = FMath::Fmod(S.Phase + 2.0 * PI * Hz / SndRate, 2.0 * PI);
			const double Ph = S.Phase;
			const double V = 0.6 * FMath::Sin(Ph) + 0.24 * FMath::Sin(3 * Ph) + 0.12 * FMath::Sin(5 * Ph) + 0.05 * FMath::Sin(7 * Ph);
			Out[I] = int16(FMath::Tanh(1.8 * V) * 22000.0);
			S.T += 1.0 / SndRate;
		}
	}

	/** a bay door's motor: the start (relay click, the motor's kick, the sections jerking), the stop (the brake catching,
	 *  the sections' heavy clank, a rattle dying away) */
	TArray<int16> GarageClunk(bool bStop)
	{
		TArray<float> F; F.SetNumZeroed(SndRate * (bStop ? 70 : 45) / 100);
		uint32 Sd = bStop ? 23u : 19u; float Lp = 0.f, Hp = 0.f, Prev = 0.f;
		for (int32 I = 0; I < F.Num(); ++I)
		{
			const float T = float(I) / SndRate, N = SndNoise(Sd);
			Lp += 0.06f * (N - Lp); Hp = 0.75f * (Hp + N - Prev); Prev = N;
			float V = 0.f;
			if (!bStop) V += 0.4f * Hp * FMath::Exp(-T / 0.003f);                                    // the relay
			const float T0 = bStop ? 0.f : 0.03f;
			if (T >= T0)
			{
				const float U = T - T0;
				V += (bStop ? 0.9f : 0.6f) * FMath::Sin(2.f * PI * (bStop ? 52.f : 68.f) * U) * FMath::Exp(-U / (bStop ? 0.16f : 0.1f));
				V += (bStop ? 1.3f : 0.8f) * Lp * FMath::Exp(-U / 0.03f);
				V += (bStop ? 0.35f : 0.2f) * Hp * FMath::Exp(-U / 0.012f) * (0.6f + 0.4f * FMath::Sin(2.f * PI * 9.f * U));
				V += 0.12f * FMath::Sin(2.f * PI * 610.f * U) * FMath::Exp(-U / (bStop ? 0.12f : 0.05f)) * (0.5f + 0.5f * FMath::Sin(2.f * PI * 27.f * U));   // the sections' rattle
			}
			F[I] = V;
		}
		return SndPcm(F);
	}
	/** the motor running: its hum, the chain, the sections' rollers knocking past the track's joints */
	struct FHumState { double T = 0.0, Ph = 0.0; uint32 Seed = 5u; float Hp = 0.f, Prev = 0.f; };
	void HumFill(FHumState& S, TArray<int16>& Out, int32 N)
	{
		Out.SetNumUninitialized(N);
		for (int32 I = 0; I < N; ++I)
		{
			S.Ph = FMath::Fmod(S.Ph + 2.0 * PI * 96.0 / SndRate, 2.0 * PI);
			const float Nz = SndNoise(S.Seed); S.Hp = 0.8f * (S.Hp + Nz - S.Prev); S.Prev = Nz;
			const float Knock = FMath::Pow(0.5f + 0.5f * float(FMath::Sin(2.0 * PI * 3.1 * S.T)), 18.f);   // a roller over a joint, three a second
			const float V = 0.22f * float(FMath::Sin(S.Ph) + 0.45 * FMath::Sin(2 * S.Ph) + 0.2 * FMath::Sin(3 * S.Ph)) + 0.07f * S.Hp + 0.35f * Knock * S.Hp;
			Out[I] = int16(FMath::Tanh(V * 1.4f) * 20000.f);
			S.T += 1.0 / SndRate;
		}
	}

	/** the reversing beeper: the classic truck alarm, about 1.1 kHz, half a second on, half a second off, its small
	 *  horn's hard square-ish tone */
	struct FBeepState { double T = 0.0, Ph = 0.0; };
	void BeepFill(FBeepState& S, TArray<int16>& Out, int32 N)
	{
		Out.SetNumUninitialized(N);
		for (int32 I = 0; I < N; ++I)
		{
			const double C = FMath::Fmod(S.T, 1.0);
			const double Env = C < 0.5 ? FMath::Min(1.0, C / 0.008) * FMath::Min(1.0, (0.5 - C) / 0.01) : 0.0;
			S.Ph = FMath::Fmod(S.Ph + 2.0 * PI * 1120.0 / SndRate, 2.0 * PI);
			const double V = FMath::Sin(S.Ph) + 0.3 * FMath::Sin(3 * S.Ph) + 0.12 * FMath::Sin(5 * S.Ph);
			Out[I] = int16(FMath::Tanh(1.6 * V) * 16000.0 * Env);
			S.T += 1.0 / SndRate;
		}
	}

	/** the indicator relay: a dry click as the lamps come on, a softer one as they go off */
	const TArray<int16>& TickPcm(bool bOn)
	{
		static TArray<int16> P[2];
		TArray<int16>& Out = P[bOn ? 1 : 0];
		if (Out.Num()) return Out;
		TArray<float> F; F.SetNumZeroed(SndRate * 4 / 100);
		uint32 Sd = bOn ? 31u : 37u; float Hp = 0.f, Prev = 0.f;
		for (int32 I = 0; I < F.Num(); ++I)
		{
			const float T = float(I) / SndRate, N = SndNoise(Sd);
			Hp = 0.8f * (Hp + N - Prev); Prev = N;
			F[I] = (bOn ? 0.55f : 0.32f) * (Hp * FMath::Exp(-T / 0.0025f) + 0.6f * FMath::Sin(2.f * PI * (bOn ? 2900.f : 2300.f) * T) * FMath::Exp(-T / 0.004f));
		}
		Out = SndPcm(F);
		return Out;
	}

	/** The lamps' settings (owner, 2026-11-11: adjustable later without rebuilding the game): Content/Settings/
	 *  TruckLamps.ini, a plain text file staged loose beside the game; read once; every value falls back to the
	 *  game's own if the file or the line is missing. */
	struct FLampIni
	{
		TMap<FString, FString> V;
		FLampIni()
		{
			TArray<FString> Lines;
			const FString Path = FPaths::ProjectContentDir() / TEXT("Settings/TruckLamps.ini");
			if (!FFileHelper::LoadFileToStringArray(Lines, *Path)) { ADP_WARN(TEXT("Truck lamps: %s not found; the built-in values are used"), *Path); return; }
			FString Sec;
			for (FString L : Lines)
			{
				L.TrimStartAndEndInline();
				if (L.IsEmpty() || L.StartsWith(TEXT(";"))) continue;
				if (L.StartsWith(TEXT("[")) && L.EndsWith(TEXT("]"))) { Sec = L.Mid(1, L.Len() - 2); continue; }
				FString K, Val;
				if (L.Split(TEXT("="), &K, &Val)) V.Add(Sec + TEXT(".") + K.TrimStartAndEnd(), Val.TrimStartAndEnd());
			}
			ADP_LOG(TEXT("Truck lamps: %d settings from %s"), V.Num(), *Path);
		}
		float F(const TCHAR* Key, float Def) const { const FString* S = V.Find(Key); return S && S->IsNumeric() ? FCString::Atof(**S) : Def; }
		FLinearColor C(const TCHAR* Key, const FLinearColor& Def) const
		{
			const FString* S = V.Find(Key);
			TArray<FString> P;
			if (!S || S->ParseIntoArray(P, TEXT(",")) != 3) return Def;
			return FLinearColor(FColor(uint8(FMath::Clamp(FCString::Atoi(*P[0]), 0, 255)), uint8(FMath::Clamp(FCString::Atoi(*P[1]), 0, 255)), uint8(FMath::Clamp(FCString::Atoi(*P[2]), 0, 255))));
		}
	};
	const FLampIni& LampIni() { static FLampIni I; return I; }

	// ---- getting in: where your pelvis, feet and hands are (Blender metres, the truck's own measures) at each moment.
	// The driver's door is on the left: the hung step outside it (top 0.40 m, x -1.6, y 4.3-4.9), the step inside the
	// doorway (top 0.75 m), the cab floor at 1.04 m; a handle either side of the doorway inside (rear y 3.78, front
	// y 5.12, about 1.25-1.9 m up); the middle seat's cushion at 1.6 m (y 4.08); the pedals.
	enum class EHand : uint8 { Free, At, Wheel, Handle };   // Handle: on the door's outside handle, riding with the door
	struct FKey { float T; FVector Pelvis; FVector2D Face; float Lean; FVector Foot[2]; EHand HM[2]; FVector Hand[2]; float Grip[2]; };
	const FKey ClimbKeys[] = {
		// standing at the foot of the steps (replaced by where you really are when you press E)
		{0.00f, {-2.15f, 4.60f, 0.95f}, {1, 0}, 0.f,  {{-2.15f, 4.72f, 0.f}, {-2.15f, 4.48f, 0.f}}, {EHand::Free, EHand::Free}, {}, {0.f, 0.f}},
		// the right hand to the door's handle (the door swings open from here, the hand riding with it), a half step back
		{0.50f, {-2.15f, 4.40f, 0.95f}, {1, 0}, 5.f,  {{-2.15f, 4.62f, 0.f}, {-2.15f, 4.30f, 0.f}}, {EHand::Free, EHand::Handle}, {}, {0.f, 1.f}},
		// hands up to the handles either side of the doorway
		{1.00f, {-2.02f, 4.60f, 0.95f}, {1, 0}, 8.f,  {{-2.08f, 4.72f, 0.f}, {-2.08f, 4.48f, 0.f}}, {EHand::At, EHand::At}, {{-1.42f, 5.12f, 1.45f}, {-1.42f, 3.78f, 1.40f}}, {1.f, 1.f}},
		// the right foot on the step outside
		{1.50f, {-1.95f, 4.58f, 1.10f}, {1, 0}, 14.f, {{-2.08f, 4.72f, 0.f}, {-1.62f, 4.50f, 0.40f}}, {EHand::At, EHand::At}, {{-1.42f, 5.12f, 1.55f}, {-1.42f, 3.78f, 1.50f}}, {1.f, 1.f}},
		// pulling up: the left foot on the step inside the doorway
		{2.05f, {-1.72f, 4.60f, 1.48f}, {1, 0}, 16.f, {{-1.30f, 4.74f, 0.75f}, {-1.62f, 4.50f, 0.40f}}, {EHand::At, EHand::At}, {{-1.42f, 5.12f, 1.70f}, {-1.42f, 3.78f, 1.70f}}, {1.f, 1.f}},
		// the right foot onto the cab floor
		{2.55f, {-1.36f, 4.58f, 1.88f}, {1, 0.1f}, 14.f, {{-1.30f, 4.74f, 0.75f}, {-0.98f, 4.48f, 1.04f}}, {EHand::At, EHand::At}, {{-1.42f, 5.12f, 1.85f}, {-1.42f, 3.78f, 1.85f}}, {1.f, 1.f}},
		// in: both feet on the floor, turning to the front, letting go
		{3.05f, {-0.88f, 4.62f, 1.98f}, {0.6f, 0.8f}, 8.f, {{-0.92f, 4.78f, 1.04f}, {-0.98f, 4.48f, 1.04f}}, {EHand::At, EHand::Free}, {{-1.38f, 5.12f, 1.75f}, {}}, {1.f, 0.f}},
		// past the left seat's front, to the middle
		{3.55f, {-0.40f, 4.66f, 1.90f}, {0.3f, 1.f}, 4.f, {{-0.40f, 4.82f, 1.04f}, {-0.52f, 4.56f, 1.04f}}, {EHand::Free, EHand::Free}, {}, {0.f, 0.f}},
		// sat down in the middle, feet to the pedals, hands on the wheel
		{4.15f, {0.00f, 4.02f, 1.70f}, {0, 1}, -8.f, {{-0.14f, 4.70f, 1.04f}, {0.20f, 4.62f, 1.04f}}, {EHand::Wheel, EHand::Wheel}, {}, {1.f, 1.f}},
	};
	constexpr int32 NumClimbKeys = UE_ARRAY_COUNT(ClimbKeys);
	constexpr float ClimbLen = 4.15f;
	constexpr float HandleT = 0.50f;   // when the hand reaches the door's handle (in), or lets go of it (out)
}

AFireTruck::AFireTruck()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetBoxExtent(FVector(605.f, 150.f, 140.f));
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetSimulatePhysics(false);
	Body->SetLinearDamping(0.005f);   // (the air's drag at speed: with the power, it settles at about 135 km/h)
	Body->SetAngularDamping(0.6f);
	Body->BodyInstance.SetMassOverride(TruckMass, true);
	Body->BodyInstance.COMNudge = FVector(0, 0, -80.f);   // the weight low (the tank, the chassis)
	Body->SetHiddenInGame(true);
	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(Body);
	Boom->SetRelativeLocation(FVector(0, 0, 130.f));
	Boom->SetRelativeRotation(FRotator(-12.f, 0, 0));
	Boom->TargetArmLength = 1700.f;
	Boom->bDoCollisionTest = false;   // kept in front of walls by Tick's own trace instead (the buildings' meshes collide by their triangles)
	Boom->SetTickGroup(TG_PostPhysics);   // follows the body after physics has moved it (no lag jitter)
	Boom->bUsePawnControlRotation = false;
	Boom->bEnableCameraLag = true; Boom->CameraLagSpeed = 5.f;
	Boom->bEnableCameraRotationLag = true; Boom->CameraRotationLagSpeed = 6.f;
	ChaseCam = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCam"));
	ChaseCam->SetupAttachment(Boom);
	ChaseCam->SetFieldOfView(80.f);
	DriverCam = CreateDefaultSubobject<UCameraComponent>(TEXT("DriverCam"));
	DriverCam->SetupAttachment(Body);
	DriverCam->SetFieldOfView(90.f);
	ChaseCam->SetAutoActivate(false);
	MoveCam = CreateDefaultSubobject<UCameraComponent>(TEXT("MoveCam"));
	MoveCam->SetupAttachment(Body);
	MoveCam->SetFieldOfView(85.f);
	MoveCam->SetAutoActivate(false);
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

bool AFireTruck::Setup(const FString& ModelName)
{
	UWorld* W = GetWorld();
	if (!W) return false;
	FActorSpawnParameters S; S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; S.Owner = this;
	Model = W->SpawnActor<ABabylonModel>(GetActorLocation(), GetActorRotation(), S);
	if (!Model || !Model->Place(ModelName)) { if (Model) Model->Destroy(); Model = nullptr; return false; }
	Model->SetPartsCollision(false);   // the body collides for it
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	// the body round the model: forward from the rear axle to the front, its middle at the truck's middle
	const FVector Front = RX.TransformPosition(BlenderToRoot(0, AxleY[0], WheelR)), Back = RX.TransformPosition(BlenderToRoot(0, AxleY[2], WheelR));
	const FVector Fw = (Front - Back).GetSafeNormal2D();
	SetActorLocationAndRotation(RX.TransformPosition(BlenderToRoot(0.f, 0.025f, 2.0f)), FRotationMatrix::MakeFromXZ(Fw, FVector::UpVector).Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
	Model->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform);
	const FTransform BT = Body->GetComponentTransform();
	// which way the turns go in the model's root space (its forward is root -y, its right root +x)
	SpinSign = FQuat(FVector::ForwardVector, 0.1f).RotateVector(FVector::UpVector).Y < 0.f ? 1.f : -1.f;
	SteerSign = FQuat(FVector::UpVector, 0.1f).RotateVector(FVector(0, -1, 0)).X > 0.f ? 1.f : -1.f;
	DoorSign = FQuat(FVector::UpVector, 0.1f).RotateVector(FVector(0, 1, 0)).X < 0.f ? 1.f : -1.f;
	int32 Bound = 0;
	for (int32 A = 0; A < 3; ++A)
		for (const float Sd : {-1.f, 1.f})
		{
			FWheel Wh; Wh.Axle = A; Wh.Side = Sd;
			const FString T = FString::Printf(TEXT("%s%d"), Sd < 0 ? TEXT("L") : TEXT("R"), A + 1);
			Wh.CentreRoot = BlenderToRoot(Sd * TrackHalfW, AxleY[A], WheelR);
			Wh.Rest = BT.InverseTransformPosition(RX.TransformPosition(Wh.CentreRoot));
			auto Bind = [&](const TArray<FString>& Names, FTransform& Base, int32& Group)
			{
				if (Model->NodeBase(Names[0], Base)) { Group = Model->BindGroup(Names, false, Base); Bound += Group != INDEX_NONE; }
			};
			Bind({TEXT("Wheel_") + T}, Wh.BWheel, Wh.GWheel);
			Bind({TEXT("LowerArm_") + T}, Wh.BLower, Wh.GLower);
			Bind({TEXT("UpperArm_") + T}, Wh.BUpper, Wh.GUpper);
			Bind({TEXT("Spring_") + T}, Wh.BSpring, Wh.GSpring);
			Bind({TEXT("DamperBody_") + T, TEXT("SpringSeat_") + T}, Wh.BDamper, Wh.GDamper);
			Bind({TEXT("DamperRod_") + T}, Wh.BRod, Wh.GRod);
			Bind({TEXT("DriveShaft_") + T, TEXT("InnerJoint_") + T}, Wh.BShaft, Wh.GShaft);
			// what rides on the wheel's upright (steering with it)
			Wh.GFollow = Model->BindGroup({TEXT("Knuckle_") + T, TEXT("BrakeDisc_") + T, TEXT("Caliper_") + T, TEXT("BrakeHose_") + T,
				TEXT("WheelSensor_") + T, TEXT("SteerArm_") + T, TEXT("TieRodEnd_") + T}, false, FTransform(Wh.CentreRoot));
			if (A == 0 && Sd < 0) ADP_LOG(TEXT("Fire truck: wheel L1 node at %s, expected %s (root space)"), *Wh.BWheel.GetLocation().ToString(), *Wh.CentreRoot.ToString());
			Wheels.Add(Wh);
		}
	// the cab doors (hinged at their front edge), the steering wheel, the pedals
	HingeRoot[0] = BlenderToRoot(-1.53f, 5.0f, 0.f); HingeRoot[1] = BlenderToRoot(1.53f, 5.0f, 0.f);
	GDoor[0] = Model->BindGroup({TEXT("DoorL"), TEXT("Glass_DoorL"), TEXT("DoorHandleL"), TEXT("DoorStripeL")}, false, FTransform(HingeRoot[0]));
	GDoor[1] = Model->BindGroup({TEXT("DoorR"), TEXT("Glass_DoorR"), TEXT("DoorHandleR"), TEXT("DoorStripeR")}, false, FTransform(HingeRoot[1]));
	if (Model->NodeBase(TEXT("SteeringWheel"), BSteeringWheel)) GSteeringWheel = Model->BindGroup({TEXT("SteeringWheel")}, false, BSteeringWheel);
	if (Model->NodeBase(TEXT("Pedal_Accel"), BAccel)) GAccel = Model->BindGroup({TEXT("Pedal_Accel")}, false, BAccel);
	FTransform Pin;
	if (Model->NodeBase(TEXT("Pedal_BrakePin"), Pin)) { BrakePin = Pin.GetLocation(); GBrake = Model->BindGroup({TEXT("Pedal_Brake"), TEXT("Pedal_BrakeBracket"), TEXT("Pedal_BrakeArm")}, false, FTransform(BrakePin)); }
	FTransform PadT;
	if (Model->NodeBase(TEXT("Pedal_Brake"), PadT)) BrakePad = PadT.GetLocation();
	WheelNow = BSteeringWheel; AccelNow = BAccel; BrakeNow = FTransform(BrakePin);
	{
		static const TCHAR* NeedleNames[6] = {TEXT("Speed"), TEXT("Rev"), TEXT("Temp"), TEXT("Oil"), TEXT("Fuel"), TEXT("Volts")};
		static const float Rest[6] = {-120.f, -120.f, -60.f, -60.f, -40.f, 0.f};   // as modelled (Art/FireTruck/make_panther.py)
		int32 NN = 0;
		for (int32 i = 0; i < 6; ++i)
		{
			FTransform B;
			const FString N = FString(TEXT("Needle_")) + NeedleNames[i];
			if (!Model->NodeBase(N, B)) continue;
			// (the pivot taken back to the needle's zero: the dials' lowest mark)
			BNeedle[i] = B; NeedleAng[i] = Rest[i];
			GNeedle[i] = Model->BindGroup({N, N + TEXT("Tip")}, false, B);
			NN += GNeedle[i] != INDEX_NONE;
			TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> F;
			Model->InstancesNamed(N, false, F); Model->InstancesNamed(N + TEXT("Tip"), false, F);
			for (const auto& X : F) NeedleLens.Add(TPair<UInstancedStaticMeshComponent*, int32>(X.Get<0>(), X.Get<1>()));
		}
		// a soft backlight in the cluster's hood (night only)
		FTransform CF;
		if (Model->NodeBase(TEXT("ClusterFace"), CF) || Model->NodeBase(TEXT("ClusterRoot"), CF))
		{
			ClusterLight = NewObject<UPointLightComponent>(this);
			ClusterLight->SetupAttachment(Model->GetModelRoot());
			ClusterLight->SetRelativeLocation(CF.GetLocation() + FVector(0.f, -9.f, 4.f));
			ClusterLight->SetIntensityUnits(ELightUnits::Candelas);
			ClusterLight->SetIntensity(0.25f);
			ClusterLight->SetAttenuationRadius(45.f);
			ClusterLight->SetLightColor(FLinearColor(0.75f, 0.85f, 1.f));
			ClusterLight->SetCastShadows(false);
			ClusterLight->SetVisibility(false);
			ClusterLight->RegisterComponent();
		}
		ADP_LOG(TEXT("Fire truck %s: %d gauge needles, cluster light %d"), *ModelName, NN, ClusterLight ? 1 : 0);
	}
	// your body for getting in and driving: the walker's own mannequin, posed by hand (TruckDriverRig.h)
	if (USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")))
	{
		Rider = NewObject<UPoseableMeshComponent>(this, TEXT("Rider"));
		Rider->SetSkinnedAssetAndUpdate(Manny);
		MannyMesh = Manny;
		Rider->SetupAttachment(Model->GetModelRoot());
		Rider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rider->SetCastShadow(true);
		Rider->RegisterComponent();
		Rider->SetRelativeTransform(FTransform::Identity);
		Rider->SetVisibility(false);
		if (!Rig.Init(Rider)) ADP_ERROR(TEXT("Fire truck: the driver's body couldn't be rigged"));
	}
	else ADP_ERROR(TEXT("Fire truck: the driver's body (SKM_Manny_Simple) is missing"));
	// how far its sounds carry: the doors across the forecourt, the siren far over the airport
	auto Att = [this](float Inner, float Falloff)
	{
		USoundAttenuation* A = NewObject<USoundAttenuation>(this);
		A->Attenuation.bAttenuate = true; A->Attenuation.bSpatialize = true;
		A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		A->Attenuation.AttenuationShapeExtents = FVector(Inner, 0, 0);
		A->Attenuation.FalloffDistance = Falloff;
		A->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		return A;
	};
	NearSound = Att(150.f, 3500.f);
	FarSound = Att(1500.f, 120000.f);
	BuildLamps();
	DressMaterials();
	// the reversing lamps (Light_Reverse, either side at the back): the lens shining white, the ground behind barely
	// touched (owner, 2026-11-11: reversing lamps hardly light the ground)
	for (const TCHAR* N : {TEXT("Light_Reverse"), TEXT("Light_Reverse.001")})
	{
		FTransform LT;
		if (!Model->NodeBase(N, LT)) continue;
		const FLampIni& Li = LampIni();
		if (USceneComponent* G = LensGlow(LT.GetLocation() + FVector(0, 4.f, 0), Li.C(TEXT("Reverse.Color"), FLinearColor(1.f, 0.97f, 0.92f)), Li.F(TEXT("Reverse.Glow"), 30.f), Li.F(TEXT("Reverse.GlowSize"), 26.f))) ReverseLights.Add(G);
		USpotLightComponent* Sp = NewObject<USpotLightComponent>(this);
		Sp->SetupAttachment(Model->GetModelRoot());
		Sp->SetRelativeLocationAndRotation(LT.GetLocation() + FVector(0, 30.f, 0), FRotator(-35.f, 90.f, 0.f));
		Sp->SetIntensityUnits(ELightUnits::Lumens); Sp->SetIntensity(Li.F(TEXT("Reverse.Brightness"), 140.f));
		Sp->SetOuterConeAngle(50.f); Sp->SetInnerConeAngle(25.f); Sp->SetAttenuationRadius(Li.F(TEXT("Reverse.Reach"), 5.f) * 100.f);
		Sp->SetLightColor(Li.C(TEXT("Reverse.Color"), FLinearColor(1.f, 0.97f, 0.92f)));
		Sp->SetCastShadows(false); Sp->SetVolumetricScatteringIntensity(0.f);
		Sp->SetVisibility(false);
		Sp->RegisterComponent();
		ReverseLights.Add(Sp);
	}
	// the driver's eyes: in the middle seat, looking out through the windscreen
	DriverCam->SetWorldLocation(RX.TransformPosition(BlenderToRoot(0.f, 3.95f, 2.33f)));
	DriverCam->SetRelativeRotation(FRotator::ZeroRotator);
	All.Add(this);
	ADP_LOG(TEXT("Fire truck %s ready: %d moving parts bound, doors %d/%d, steering wheel %d, pedals %d/%d"), *ModelName, Bound, GDoor[0], GDoor[1], GSteeringWheel, GAccel, GBrake);
	return true;
}

void AFireTruck::EndPlay(const EEndPlayReason::Type Why)
{
	All.RemoveAll([this](const TWeakObjectPtr<AFireTruck>& P) { return !P.IsValid() || P.Get() == this; });
	Super::EndPlay(Why);
}

AFireTruck* AFireTruck::FromActor(AActor* A)
{
	if (!A) return nullptr;
	if (AFireTruck* T = Cast<AFireTruck>(A)) return T;
	if (AFireTruck* T = Cast<AFireTruck>(A->GetOwner())) return T;
	return Cast<AFireTruck>(A->GetAttachParentActor());
}

AFireTruck* AFireTruck::Beside(const APawn* P)
{
	if (!P) return nullptr;
	AFireTruck* Best = nullptr; float BestD = 250.f;
	for (const TWeakObjectPtr<AFireTruck>& W : All)
	{
		AFireTruck* T = W.Get();
		if (!T || !T->Body || T->Driver || T->Seq != ESeq::None) continue;
		// how far you stand outside its body's box (in the truck's own frame), and whether you face that nearest point
		const FTransform BX = T->Body->GetComponentTransform();
		const FVector L = BX.InverseTransformPosition(P->GetActorLocation()), E = T->Body->GetUnscaledBoxExtent();
		const FVector Near(FMath::Clamp(L.X, -E.X, E.X), FMath::Clamp(L.Y, -E.Y, E.Y), L.Z);
		const float D = FVector::Dist2D(L, Near);
		FVector To = BX.TransformPosition(Near) - P->GetActorLocation(); To.Z = 0.f;
		const FVector Fw = FRotator(0.f, (P->GetController() ? P->GetController()->GetControlRotation() : P->GetActorRotation()).Yaw, 0.f).Vector();
		if (D < BestD && (D < 40.f || FVector::DotProduct(To.GetSafeNormal(), Fw) > 0.25f)) { BestD = D; Best = T; }
	}
	return Best;
}

float AFireTruck::SpeedKmh() const { return bSim ? Body->GetPhysicsLinearVelocity().Size() * 0.036f : 0.f; }

void AFireTruck::SetPhysics(bool bOn)
{
	if (bOn == bSim) return;
	bSim = bOn;
	Body->SetSimulatePhysics(bOn);
	if (bOn) Body->WakeRigidBody();
}

void AFireTruck::SetView(bool bNewChase)
{
	bChase = bNewChase;
	LookYaw = LookPitch = 0.f;
	if (Seq != ESeq::None) return;   // (climbing in or out: the view you'll have once in)
	DriverCam->SetActive(!bChase);
	ChaseCam->SetActive(bChase);
	if (APlayerController* PC = Cast<APlayerController>(GetController())) if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();   // no smear across the switch
	LookYaw = LookPitch = 0.f;
}

void AFireTruck::SetupPlayerInputComponent(UInputComponent* In)
{
	Super::SetupPlayerInputComponent(In);
	In->BindAxis(TEXT("MoveForward"), this, &AFireTruck::Fwd);
	In->BindAxis(TEXT("MoveRight"), this, &AFireTruck::Right);
	In->BindAxis(TEXT("Turn"), this, &AFireTruck::Turn);
	In->BindAxis(TEXT("LookUp"), this, &AFireTruck::LookUp);
	In->BindAxis(TEXT("TruckThrottle"), this, &AFireTruck::PadT);
	In->BindAxis(TEXT("TruckBrake"), this, &AFireTruck::PadB);
	In->BindAction(TEXT("Jump"), IE_Pressed, this, &AFireTruck::HandOn);
	In->BindAction(TEXT("Jump"), IE_Released, this, &AFireTruck::HandOff);
	In->BindAction(TEXT("ChangeView"), IE_Pressed, this, &AFireTruck::ToggleView);
	In->BindAction(TEXT("Siren"), IE_Pressed, this, &AFireTruck::ToggleSiren);
	In->BindAction(TEXT("HighBeam"), IE_Pressed, this, &AFireTruck::ToggleHighBeam);
	In->BindAction(TEXT("BlueLights"), IE_Pressed, this, &AFireTruck::ToggleBlue);
	In->BindAction(TEXT("IndicatorLeft"), IE_Pressed, this, &AFireTruck::TurnLeft);
	In->BindAction(TEXT("IndicatorRight"), IE_Pressed, this, &AFireTruck::TurnRight);
	In->BindAction(TEXT("Hazards"), IE_Pressed, this, &AFireTruck::Hazards);
	// the pad: hold B (circle) and use the D-pad for the indicators (B alone does nothing in a truck)
	In->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &AFireTruck::PadModOn);
	In->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Released, this, &AFireTruck::PadModOff);
	In->BindKey(EKeys::Gamepad_DPad_Left, IE_Pressed, this, &AFireTruck::PadLeft);
	In->BindKey(EKeys::Gamepad_DPad_Right, IE_Pressed, this, &AFireTruck::PadRight).bConsumeInput = false;   // (also the bay doors' button)
	In->BindKey(EKeys::Gamepad_DPad_Down, IE_Pressed, this, &AFireTruck::PadDown);
}

void AFireTruck::Enter(APawn* Walker)
{
	if (Driver || Seq != ESeq::None || !Walker || !Model) return;
	APlayerController* PC = Cast<APlayerController>(Walker->GetController());
	if (!PC) return;
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	const float Away = FVector::Dist2D(RX.InverseTransformPosition(Walker->GetActorLocation()), BlenderToRoot(ClimbKeys[0].Pelvis.X, ClimbKeys[0].Pelvis.Y, 0.f));
	if (Away > 1500.f)   // (anywhere along the truck: you walk round to the door)
	{
		ADP_LOG(TEXT("Fire truck: too far from the driver's door to get in (%s, %.1f m)"), *GetName(), Away / 100.f);
		return;
	}
	// not at the door yet: you walk over first (your keys do nothing meanwhile); the climb starts when you get there
	if (Away > 40.f)
	{
		Driver = Walker;
		if (AADPWalker* Wk = Cast<AADPWalker>(Walker)) Wk->SetLed(true);
		Seq = ESeq::Walk; WalkClock = 0.f;
		ADP_LOG(TEXT("Fire truck: walking to the driver's door (%s, %.1f m)"), *GetName(), Away / 100.f);
		return;
	}
	BeginClimb(Walker);
}

void AFireTruck::BeginClimb(APawn* Walker)
{
	APlayerController* PC = Cast<APlayerController>(Walker->GetController());
	if (!PC || !Model) return;
	AADPWalker* Me = Cast<AADPWalker>(Walker);
	if (Me) Me->SetLed(false);
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	// the body that climbs: your own MetaHuman when you have one (owner, 2026-11-13), else the mannequin; the rig built on
	// that body's own skeleton, so feet, hands and seat fit its own size
	HumanPose = nullptr;
	// the view you had stays: through your own eyes = the cab, from behind yourself = outside (as far back as last time)
	if (Me) bChase = Me->IsThirdPerson();
	Boom->TargetArmLength = ArmWant();
	USkeletalMesh* Own = Me ? Me->BodyMesh() : nullptr;
	if (Rider && (Own || MannyMesh))
	{
		Rider->SetSkinnedAssetAndUpdate(Own ? Own : MannyMesh.Get());
		if (!Rig.Init(Rider)) ADP_ERROR(TEXT("Fire truck: the driver's body couldn't be rigged (%s)"), Own ? *Own->GetName() : TEXT("mannequin"));
	}
	// where you are and look from, in the truck's own space: the climb starts there
	FVector EyeAt; FRotator EyeRot;
	Walker->GetActorEyesViewPoint(EyeAt, EyeRot);
	if (PC->PlayerCameraManager) { EyeAt = PC->PlayerCameraManager->GetCameraLocation(); EyeRot = PC->PlayerCameraManager->GetCameraRotation(); }
	CamFrom = FTransform(EyeRot, EyeAt).GetRelativeTransform(RX);
	StartPelvis = RX.InverseTransformPosition(Walker->GetActorLocation());
	StartPelvis.Z = Rig.IsReady() ? Rig.PelvisHeight() : 95.f;
	StartFacing = RX.InverseTransformVector(Walker->GetActorForwardVector()); StartFacing.Z = 0.f; StartFacing.Normalize();
	Driver = Walker;
	Walker->SetActorHiddenInGame(true);
	Walker->SetActorEnableCollision(false);
	if (ACharacter* Ch = Cast<ACharacter>(Walker)) { Ch->GetCharacterMovement()->DisableMovement(); if (Ch->GetMesh()) Ch->GetMesh()->bCastHiddenShadow = false; }
	Walker->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform);
	PC->Possess(this);
	DriverCam->SetActive(false); ChaseCam->SetActive(false); MoveCam->SetActive(true);
	MoveCam->SetWorldTransform(CamFrom * RX);
	if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
	SetPhysics(true);
	Still = 0.f; bTestInput = false; bHandbrake = true;
	Seq = ESeq::In; SeqT = 0.f;
	if (Own && Rig.IsReady()) HumanPose = Me->LendBody(Rider);
	if (HumanPose) { Me->ShowHead(true); ADP_LOG(TEXT("Fire truck: your own character climbs in (%s)"), *Own->GetName()); }
	else if (Own) ADP_WARN(TEXT("Fire truck: your character couldn't be lent to the truck, the mannequin climbs instead"));
	bHeadHidden = false;
	if (Rider) { Rider->UnHideBoneByName(TEXT("head")); Rider->SetVisibility(!HumanPose); }
	Sequence(0.f);
	ADP_LOG(TEXT("Fire truck: getting in (%s)"), *GetName());
}

FVector AFireTruck::DoorSpot() const
{
	return Model ? Model->GetModelRoot()->GetComponentTransform().TransformPosition(BlenderToRoot(ClimbKeys[0].Pelvis.X - 0.5f, ClimbKeys[0].Pelvis.Y - 0.3f, 1.0f)) : GetActorLocation();
}

void AFireTruck::Exit()
{
	if (!Driver || !Model || Seq != ESeq::None) return;
	// at a standstill you climb out; moving, it stops first
	if (SpeedKmh() > 4.f) { bExitPending = true; return; }
	StartExit();
}

void AFireTruck::ExitNow()
{
	if (!Driver || !Model) return;
	StartExit();
	FinishExit();
	MoveCam->SetActive(false);
}

void AFireTruck::StartExit()
{
	bExitPending = false;
	// and out again the same way: the cab = your own eyes, outside = from behind yourself
	if (AADPWalker* Me = Cast<AADPWalker>(Driver.Get())) Me->SetThirdPerson(bChase);
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	UCameraComponent* From = bChase ? ChaseCam.Get() : DriverCam.Get();
	CamFrom = From->GetComponentTransform().GetRelativeTransform(RX);
	DriverCam->SetActive(false); ChaseCam->SetActive(false); MoveCam->SetActive(true);
	MoveCam->SetWorldTransform(From->GetComponentTransform());
	InThrottle = InBrake = InSteer = KeyFwd = KeySteer = PadThrottle = PadBrake = 0.f;
	bHandbrake = true;   // left with the handbrake on
	// you end a step out from the foot of the steps, turned along the truck towards its front (it stays in view)
	StartPelvis = BlenderToRoot(ClimbKeys[0].Pelvis.X - 0.25f, ClimbKeys[0].Pelvis.Y - 0.1f, 0.f); StartPelvis.Z = Rig.IsReady() ? Rig.PelvisHeight() : 95.f;
	StartFacing = DirToRoot(-0.35f, 1.f);
	Seq = ESeq::Out; SeqT = 0.f;
	DoorTarget = 1.f;
	ADP_LOG(TEXT("Fire truck: getting out (%s)"), *GetName());
}

void AFireTruck::FinishExit()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	APawn* Wk = Driver;
	Driver = nullptr;
	Seq = ESeq::None;
	EngineOn(false);
	if (Rider) Rider->SetVisibility(false);
	DoorTarget = 0.f;
	if (HumanPose) { if (AADPWalker* Me = Cast<AADPWalker>(Wk)) Me->TakeBodyBack(); HumanPose = nullptr; }
	if (!Wk) return;
	Wk->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// you stand where the climb ended, facing the truck, looking the way the camera looked
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	ACharacter* Ch = Cast<ACharacter>(Wk);
	const float Half = Ch ? Ch->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	FVector At = RX.TransformPosition(FVector(StartPelvis.X, StartPelvis.Y, Half + 2.f));
	const FRotator Look = MoveCam->GetComponentRotation();
	FRotator Rot(0.f, RX.TransformVectorNoScale(StartFacing).Rotation().Yaw, 0.f);
	GetWorld()->FindTeleportSpot(Wk, At, Rot);
	Wk->TeleportTo(At, Rot);
	Wk->SetActorEnableCollision(true);
	Wk->SetActorHiddenInGame(false);
	if (Ch) { Ch->GetCharacterMovement()->SetMovementMode(MOVE_Walking); if (Ch->GetMesh()) Ch->GetMesh()->bCastHiddenShadow = true; }
	if (PC) { PC->Possess(Wk); PC->SetControlRotation(FRotator(Look.Pitch, Rot.Yaw, 0.f)); }
	MoveCam->SetActive(false); DriverCam->SetActive(false); ChaseCam->SetActive(false);
	ADP_LOG(TEXT("Fire truck: you got out (%s), %.0f km/h"), *GetName(), SpeedKmh());
}

USceneComponent* AFireTruck::LensGlow(const FVector& At, const FLinearColor& Col, float Brightness, float Size)
{
	// a lamp's own shine: a soft glowing disc at the lens, always facing you (as games draw lamps), lighting nothing
	// round it, so the paint beside a lamp stays as it is (owner, 2026-11-11)
	static UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Lamps/M_LampGlow.M_LampGlow"));
	if (!Base) { static bool bTold = false; if (!bTold) { bTold = true; ADP_ERROR(TEXT("Fire truck: the lamp glow material is missing (Tools/make_lamp_glow.py)")); } return nullptr; }
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetVectorParameterValue(TEXT("Color"), Col);
	// (the lens itself lights up now, M_TruckLens: the disc only a soft halo round it)
	M->SetScalarParameterValue(TEXT("Brightness"), Brightness * 0.35f);
	Size *= 0.55f;
	UMaterialBillboardComponent* B = NewObject<UMaterialBillboardComponent>(this);
	B->SetupAttachment(Model->GetModelRoot());
	B->SetRelativeLocation(At);
	B->AddElement(M, nullptr, false, Size, Size, nullptr);
	B->SetCastShadow(false);
	B->SetVisibility(false);
	B->RegisterComponent();
	return B;
}

void AFireTruck::BuildLamps()
{
	// each lamp: its lens shining (LensGlow), and only where a lamp really lights the world (the headlamps' beams on
	// the road ahead, the blue lights round about) a light, placed clear of the truck so the bodywork isn't washed.
	// The lamps' places are the model's own Light_* parts. Forward is root -y.
	auto Keep = [this](USceneComponent* C, ELamp Kind, uint8 Side) { if (C) { Lamps.Add(C); LampKind.Add(Kind); LampSide.Add(Side); LampFull.Add(1.f); } };
	auto SideOf = [](const FTransform& T) { return uint8(T.GetLocation().X < 0.f ? 0 : 1); };
	auto Light = [this](ULocalLightComponent* L, const FVector& At, const FLinearColor& Col, float Lumens, float Radius)
	{
		L->SetupAttachment(Model->GetModelRoot());
		L->SetRelativeLocation(At);
		L->SetIntensityUnits(ELightUnits::Lumens); L->SetIntensity(Lumens);
		L->SetLightColor(Col); L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		L->SetVisibility(false);
		L->RegisterComponent();
		return L;
	};
	const FLampIni& Li = LampIni();
	const FLinearColor Red = Li.C(TEXT("TailLight.Color"), FLinearColor(1.f, 0.03f, 0.015f)), BrakeRed = Li.C(TEXT("BrakeLight.Color"), Red), Blue = Li.C(TEXT("BlueLights.Color"), FLinearColor(0.02f, 0.12f, 1.f)),
		Amber = Li.C(TEXT("Indicator.Color"), FLinearColor(1.f, 0.42f, 0.02f)), MarkerAmber = Li.C(TEXT("SideMarker.Color"), Amber);
	TArray<FTransform> N;
	// the headlamps: each lens shining; per side one beam onto the road for the low beams and one for the high, set
	// out in front of the bumper so the light falls on the ground, not the truck's own front
	for (const TCHAR* Nm : {TEXT("Light_HeadLow"), TEXT("Light_HeadHigh")})
	{
		const ELamp K = Nm[10] == TEXT('L') ? ELamp::Low : ELamp::High;   // ("Light_Head" then L or H)
		const bool bLo = K == ELamp::Low;
		auto Hf = [&](const TCHAR* Key, float Lo, float Hi) { return Li.F(*FString::Printf(TEXT("%s.%s"), bLo ? TEXT("LowBeam") : TEXT("HighBeam"), Key), bLo ? Lo : Hi); };
		const FLinearColor White = Li.C(bLo ? TEXT("LowBeam.Color") : TEXT("HighBeam.Color"), FLinearColor(1.f, 0.96f, 0.9f));
		N.Reset(); Model->NodesNamed(Nm, N);
		FVector Sum[2] = {FVector::ZeroVector, FVector::ZeroVector}; int32 Cnt[2] = {0, 0};
		for (const FTransform& T : N) { Keep(LensGlow(T.GetLocation() + FVector(0, -4.f, 0), White, Hf(TEXT("Glow"), 40.f, 60.f), Hf(TEXT("GlowSize"), 30.f, 30.f)), K, SideOf(T)); Sum[SideOf(T)] += T.GetLocation(); ++Cnt[SideOf(T)]; }
		for (int32 Sd = 0; Sd < 2; ++Sd)
		{
			if (!Cnt[Sd]) continue;
			USpotLightComponent* Sp = NewObject<USpotLightComponent>(this);
			Sp->SetRelativeRotation(FRotator(-Hf(TEXT("Aim"), 6.f, 1.5f), -90.f, 0.f));
			Sp->SetOuterConeAngle(Hf(TEXT("BeamWidth"), 36.f, 15.f)); Sp->SetInnerConeAngle(Hf(TEXT("Core"), 20.f, 6.f));
			Sp->SetVolumetricScatteringIntensity(0.25f);
			Keep(Light(Sp, Sum[Sd] / Cnt[Sd] + FVector(0, -45.f, 0), White, Hf(TEXT("Brightness"), 3200.f, 9000.f), Hf(TEXT("Reach"), 55.f, 160.f) * 100.f), K, uint8(Sd));
		}
	}
	// the rear lamps: red tail lights, the same lenses far brighter as you brake (the lens only; a brake light barely
	// lights the road)
	N.Reset(); Model->NodesNamed(TEXT("Light_Brake"), N);
	for (const FTransform& T : N)
	{
		Keep(LensGlow(T.GetLocation() + FVector(0, 4.f, 0), Red, Li.F(TEXT("TailLight.Glow"), 12.f), Li.F(TEXT("TailLight.GlowSize"), 24.f)), ELamp::Tail, SideOf(T));
		Keep(LensGlow(T.GetLocation() + FVector(0, 5.f, 0), BrakeRed, Li.F(TEXT("BrakeLight.Glow"), 70.f), Li.F(TEXT("BrakeLight.GlowSize"), 34.f)), ELamp::Brake, SideOf(T));
	}
	// the indicators: front corners, rear, and the side repeaters (amber); their sides from their names
	for (const TCHAR* Nm : {TEXT("Light_TurnL"), TEXT("Light_TurnR"), TEXT("Light_TurnSideL"), TEXT("Light_TurnSideR")})
	{
		N.Reset(); Model->NodesNamed(Nm, N);
		const uint8 Sd = FString(Nm).EndsWith(TEXT("L")) ? 0 : 1;
		for (const FTransform& T : N)
		{
			const FVector P = T.GetLocation();
			const FVector Out = FMath::Abs(P.X) > 140.f ? FVector(P.X < 0.f ? -4.f : 4.f, 0, 0) : FVector(0, P.Y < 0.f ? -4.f : 4.f, 0);
			Keep(LensGlow(P + Out, Amber, Li.F(TEXT("Indicator.Glow"), 55.f), Li.F(TEXT("Indicator.GlowSize"), 26.f)), ELamp::Turn, Sd);
		}
	}
	// the indicator stalk on the column's left: it swings down for left, up for right (about the column's axis)
	FTransform StalkT;
	if (Model->NodeBase(TEXT("Stalk_Indicator"), StalkT))
	{
		StalkPivot = BlenderToRoot(0.f, 4.68f, 1.731f);
		StalkTip = StalkT.GetLocation();
		GStalk = Model->BindGroup({TEXT("Stalk_Indicator"), TEXT("Stalk_IndicatorKnob")}, false, FTransform(StalkPivot));
	}
	// the amber side markers
	N.Reset(); Model->NodesNamed(TEXT("Light_MarkerSide"), N);
	for (const FTransform& T : N) Keep(LensGlow(T.GetLocation() + FVector(T.GetLocation().X < 0.f ? -3.f : 3.f, 0, 0), MarkerAmber, Li.F(TEXT("SideMarker.Glow"), 10.f), Li.F(TEXT("SideMarker.GlowSize"), 12.f)), ELamp::Marker, SideOf(T));
	// the blue warning lights (owner: natural, as in other games): every lens a hard bright flash; the light round
	// about from just outside the truck (above the roof bar, ahead of the front, behind the rear), so the blue sweeps
	// the ground and the walls and doesn't blow out the cab
	for (const TCHAR* Nm : {TEXT("Light_BlueBar"), TEXT("Light_BlueFront"), TEXT("Light_BlueRear"), TEXT("Light_BlueSide")})
	{
		N.Reset(); Model->NodesNamed(Nm, N);
		const bool bBar = Nm[10] == TEXT('B');   // ("Light_Blue" then Bar / Front / Rear / Side)
		for (const FTransform& T : N) Keep(LensGlow(T.GetLocation() + FVector(0, 0, bBar ? 6.f : 0.f), Blue, Li.F(TEXT("BlueLights.Glow"), 120.f), bBar ? Li.F(TEXT("BlueLights.GlowSize"), 46.f) : Li.F(TEXT("BlueLights.SideGlowSize"), 30.f)), ELamp::Blue, SideOf(T));
		FVector Sum[2] = {FVector::ZeroVector, FVector::ZeroVector}; int32 Cnt[2] = {0, 0};
		for (const FTransform& T : N) { Sum[SideOf(T)] += T.GetLocation(); ++Cnt[SideOf(T)]; }
		if (Nm[10] == TEXT('S')) continue;   // (the side lamps: their shine only)
		// their light thrown outward and down, away from the truck (the roof bar's to each side, the front's ahead, the
		// rear's behind): the ground and walls round about go blue, the cab and the bodywork barely
		for (int32 Sd = 0; Sd < 2; ++Sd)
			if (Cnt[Sd])
			{
				const float Away = Sd ? 1.f : -1.f;   // root x: the truck's left is -x
				const FVector From = Sum[Sd] / Cnt[Sd] + (bBar ? FVector(Away * 60.f, 0, 10.f) : Nm[10] == TEXT('F') ? FVector(0, -40.f, 0) : FVector(0, 40.f, 0));
				const float Yaw = bBar ? (Sd ? 0.f : 180.f) : Nm[10] == TEXT('F') ? -90.f : 90.f;
				USpotLightComponent* Sp = NewObject<USpotLightComponent>(this);
				Sp->SetRelativeRotation(FRotator(bBar ? -28.f : -15.f, Yaw, 0.f));
				Sp->SetOuterConeAngle(bBar ? 75.f : 60.f); Sp->SetInnerConeAngle(bBar ? 35.f : 25.f);
				Sp->SetSourceRadius(10.f);
				Sp->SetVolumetricScatteringIntensity(0.5f);
				Keep(Light(Sp, From, Blue, bBar ? Li.F(TEXT("BlueLights.Brightness"), 9000.f) : Li.F(TEXT("BlueLights.FrontRearBrightness"), 3000.f),
					(bBar ? Li.F(TEXT("BlueLights.Reach"), 26.f) : Li.F(TEXT("BlueLights.FrontRearReach"), 12.f)) * 100.f), ELamp::BlueWide, uint8(Sd));
			}
	}
	ADP_LOG(TEXT("Fire truck: %d lamp parts (headlamps, tail, brake, markers, blue warning lights)"), Lamps.Num());
}

void AFireTruck::UpdateLamps(float Dt)
{
	// the lights are on whenever someone is at the wheel (or climbing in or out); the blue lights as they were left
	const bool bRunning = Driver != nullptr;
	const bool bBraking = bRunning && ((bExitPending) || (bReverse ? InThrottle : InBrake) > 0.05f);
	BlueClock += Dt;
	// the warning lights' double flash: each side twice quickly, then the other side
	auto Flash = [this](uint8 Side)
	{
		const float Round = FMath::Max(0.2f, LampIni().F(TEXT("BlueLights.Round"), 0.75f)), C = FMath::Fmod(BlueClock + (Side ? Round * 0.5f : 0.f), Round) / Round * 0.75f;
		return (C < 0.06f) || (C > 0.11f && C < 0.17f);   // two quick flashes a side, the sides in turn (as LED beacons do)
	};
	// the indicators: about 90 flashes a minute; a turn signal cancels itself once the wheel has gone over that way
	// and come back
	if (!bRunning && Signal != ETurn::Hazard) Signal = ETurn::Off;
	if (Signal == ETurn::Left || Signal == ETurn::Right)
	{
		const float Way = Signal == ETurn::Left ? -1.f : 1.f;
		if (InSteer * Way > 0.45f) bTurnArmed = true;
		else if (bTurnArmed && InSteer * Way < 0.1f) { Signal = ETurn::Off; bTurnArmed = false; ADP_LOG(TEXT("Fire truck: the indicator cancelled itself after the turn")); }
	}
	TurnClock += Dt;
	const float TurnPeriod = 60.f / FMath::Clamp(LampIni().F(TEXT("Indicator.FlashesPerMinute"), 90.f), 30.f, 200.f);
	const bool bTurnOn = Signal != ETurn::Off && FMath::Fmod(TurnClock, TurnPeriod) < TurnPeriod * FMath::Clamp(LampIni().F(TEXT("Indicator.OnPart"), 0.55f), 0.1f, 0.9f);
	if (bTurnOn != bTurnLit && Driver && Model)
	{
		bTurnLit = bTurnOn;
		Sound(TickPcm(bTurnOn), BlenderToRoot(0.f, 4.3f, 1.6f), false);   // (the relay behind the dash)
	}
	if (Signal == ETurn::Off) bTurnLit = false;
	if (GStalk >= 0)
	{
		// the stalk: down for left, up for right, back to the middle when off (hazards: its own switch)
		const float Ang = FMath::DegreesToRadians(Signal == ETurn::Left ? -9.f : Signal == ETurn::Right ? 9.f : 0.f);
		const FVector Arm = (StalkTip - StalkPivot).GetSafeNormal();
		const FVector Bent = (Arm * FMath::Cos(Ang) + FVector(0, 0, 1) * FMath::Sin(Ang)).GetSafeNormal();
		Model->MoveGroup(GStalk, FTransform(FQuat::FindBetweenNormals(Arm, Bent), StalkPivot));
	}
	for (int32 I = 0; I < Lamps.Num(); ++I)
	{
		USceneComponent* L = Lamps[I];
		if (!L) continue;
		bool bOn = false;
		switch (LampKind[I])
		{
		case ELamp::Low: case ELamp::Tail: case ELamp::Marker: bOn = bRunning; break;
		case ELamp::High: bOn = bRunning && bHighBeam; break;
		case ELamp::Brake: bOn = bBraking; break;
		case ELamp::Blue: case ELamp::BlueWide: bOn = bBlue && Flash(LampSide[I]); break;
		case ELamp::Turn: bOn = bTurnOn && (Signal == ETurn::Hazard || (Signal == ETurn::Left) == (LampSide[I] == 0)); break;
		}
		if (L->IsVisible() != bOn) L->SetVisibility(bOn);
	}
	UpdateLenses(bRunning, bBraking, bTurnOn);
	// the paint's dirt and spray measured from where the truck stands
	if (Model) for (UMaterialInstanceDynamic* D : PaintMids) if (D) D->SetScalarParameterValue(TEXT("GroundZ"), Model->GetModelRoot()->GetComponentLocation().Z);
	PushDirt(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f);
}

void AFireTruck::UpdateGauges(float Dt)
{
	if (!Model) return;
	const bool bOn = Driver != nullptr;
	// the engine: idling at 750, up with the throttle and the road speed (as its sound), off when nobody's in
	const float RpmWant = bOn ? 750.f + 1400.f * InThrottle + 6.f * FMath::Min(SpeedKmh(), 100.f) : 0.f;
	RevShown = FMath::FInterpTo(RevShown, RpmWant, Dt, bOn ? 3.f : 1.2f);
	EngineTemp = FMath::FInterpConstantTo(EngineTemp, bOn ? 85.f : 15.f, Dt, bOn ? 1.1f : 0.05f);   // (warm in about a minute)
	// what each needle shows, degrees from its dial's middle
	float Want[6];
	Want[0] = -120.f + 240.f * FMath::Clamp(SpeedKmh() / 140.f, 0.f, 1.f);                       // km/h
	Want[1] = -120.f + 240.f * FMath::Clamp(RevShown / 3000.f, 0.f, 1.f);                         // rpm
	Want[2] = -60.f + 120.f * FMath::Clamp((EngineTemp - 40.f) / 80.f, 0.f, 1.f);                 // 40-120 C
	Want[3] = -60.f + 120.f * FMath::Clamp((bOn ? 1.6f + RevShown * 0.0016f : 0.f) / 8.f, 0.f, 1.f);   // oil, bar
	Want[4] = -60.f + 120.f * (bOn ? 0.83f : 0.f);                                                // fuel
	Want[5] = -60.f + 120.f * FMath::Clamp(((bOn ? 27.8f : 24.6f) - 18.f) / 12.f, 0.f, 1.f);     // volts
	static const float Rest[6] = {-120.f, -120.f, -60.f, -60.f, -40.f, 0.f};
	for (int32 i = 0; i < 6; ++i)
	{
		if (GNeedle[i] < 0) continue;
		// a light spring, a little under-damped: it swings to the value and settles
		// (small fixed steps, so a long frame never throws it round the dial; and never past its stops)
		const float K = i < 2 ? 160.f : 30.f, C = i < 2 ? 18.f : 9.f;
		for (float Left = FMath::Min(Dt, 0.25f); Left > 0.f; Left -= 1.f / 240.f)
		{
			const float H = FMath::Min(Left, 1.f / 240.f);
			NeedleVel[i] += ((Want[i] - NeedleAng[i]) * K - NeedleVel[i] * C) * H;
			NeedleAng[i] += NeedleVel[i] * H;
		}
		const float Lo = i < 2 ? -124.f : -64.f, Hi = i < 2 ? 124.f : 64.f;
		if (NeedleAng[i] < Lo || NeedleAng[i] > Hi || !FMath::IsFinite(NeedleAng[i])) { NeedleAng[i] = FMath::Clamp(FMath::IsFinite(NeedleAng[i]) ? NeedleAng[i] : Want[i], Lo, Hi); NeedleVel[i] = 0.f; }
		// turned about its own hub's axis (the dial's normal), from the angle it was modelled at
		const FVector Axis = BNeedle[i].GetRotation().GetAxisY();
		const FQuat Q(Axis, FMath::DegreesToRadians(-(NeedleAng[i] - Rest[i])));
		Model->MoveGroup(GNeedle[i], FTransform(Q * BNeedle[i].GetRotation(), BNeedle[i].GetLocation()));
	}
	// in the dark, with the engine on: the dials' backlight and the needles glowing softly
	const float Glow = bOn && ASunClock::SunElevation < 4.f ? FMath::Clamp((4.f - ASunClock::SunElevation) / 8.f, 0.f, 1.f) : 0.f;
	if (FMath::Abs(Glow - DashGlow) > 0.02f)
	{
		DashGlow = Glow;
		for (const auto& L : NeedleLens) if (L.Key) L.Key->SetCustomDataValue(L.Value, 0, 0.06f * Glow, true);
		if (ClusterLight) { ClusterLight->SetVisibility(Glow > 0.f); ClusterLight->SetIntensity(0.3f * Glow); }
	}
}

void AFireTruck::PushDirt(float Dt)
{
	if (!Model || PaintMids.Num() == 0) return;
	// the truck's middle, once, in its own frame (its parts' bounds)
	if (!bDirtCenter)
	{
		// the truck's middle and its length, from its parts in the model's own frame (the model's nose isn't along the
		// actor's forward)
		const FBox LB = Model->CalculateComponentsBoundingBoxInLocalSpace(true);
		const FVector E = LB.GetExtent();
		const FVector AxisM = E.X >= E.Y ? FVector::ForwardVector : FVector::RightVector;
		const FTransform MT = Model->GetActorTransform();
		DirtCenter = GetActorTransform().InverseTransformPosition(MT.TransformPosition(LB.GetCenter()));
		DirtAxis = GetActorTransform().InverseTransformVectorNoScale(MT.TransformVectorNoScale(AxisM)).GetSafeNormal();
		bDirtCenter = true;
		ADP_LOG(TEXT("Fire truck %s: %.1f m long, %.1f m wide (the dirt's areas along its length)"), *GetName(), FMath::Max(E.X, E.Y) / 50.f, FMath::Min(E.X, E.Y) / 50.f);
	}
	// driving muddies it again: about 2 % a kilometre
	const float Ms = SpeedKmh() / 3.6f;
	// what's under the wheels, twice a second: the airfield's paving or a building's floor, or grass and bare ground
	GroundCheck -= Dt;
	if (GroundCheck <= 0.f && Ms > 0.5f && GetWorld())
	{
		GroundCheck = 0.5f;
		const FVector C = GetActorTransform().TransformPosition(DirtCenter);
		FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPTruckGround), false, this); Q.AddIgnoredActor(Model);
		bOffRoad = false;
		if (GetWorld()->LineTraceSingleByChannel(H, C + FVector(0, 0, 50.f), C - FVector(0, 0, 400.f), ECC_Visibility, Q))
		{
			const UMaterialInterface* M = H.GetComponent() ? H.GetComponent()->GetMaterial(0) : nullptr;
			const FString MN = M ? M->GetName() : FString();
			const bool bPaved = (Cast<AAirfield>(H.GetActor()) && !MN.Contains(TEXT("grass"), ESearchCase::IgnoreCase)) || Cast<ABabylonModel>(H.GetActor());
			bOffRoad = !bPaved;
		}
	}
	// dirt builds only off the paving (fully muddy after about 4 km) or in the wet (about 20 km; off-road in the wet twice
	// as fast); on clean dry paving it stays as it is
	const float Wet = APuddles::Get() ? APuddles::Get()->GetWetness() : 0.f;
	const float PerM = (bOffRoad ? 0.00025f : 0.f) * (1.f + Wet) + (Wet > 0.3f ? 0.00005f * Wet : 0.f);
	const float Most = bOffRoad ? 2.f : 1.f;   // (off-road, driven a lot: as filthy as a truck gets)
	if (Ms > 0.5f && PerM > 0.f) for (float& Z : DirtGoal) if (Z < Most) Z = FMath::Min(Most, Z + Ms * Dt * PerM);
	// the paint follows: the dirt runs off over a few seconds
	for (int32 i = 0; i < 12; ++i) DirtZone[i] = FMath::FInterpConstantTo(DirtZone[i], DirtGoal[i], Dt, DirtZone[i] > DirtGoal[i] ? 0.3f : 0.05f);
	// the beads: drying (about 20 minutes), and blown off by the airflow once you drive - slowly in town, fast on the road
	for (float& B : Beads) if (B > 0.f) B = FMath::Max(0.f, B - Dt / 1200.f - Dt * FMath::Max(0.f, Ms - 3.f) / 45.f);
	Blow += Ms * Dt * 0.6f;
	const FVector O = GetActorTransform().TransformPosition(DirtCenter), F = DirtAxisWorld();
	for (UMaterialInstanceDynamic* D : GlassMids)
	{
		if (!D) continue;
		D->SetVectorParameterValue(TEXT("Origin"), FLinearColor(O.X, O.Y, O.Z, 0.f));
		D->SetVectorParameterValue(TEXT("AxisF"), FLinearColor(F.X, F.Y, F.Z, 0.f));
		D->SetVectorParameterValue(TEXT("B0"), FLinearColor(Beads[0], Beads[1], Beads[2], Beads[3]));
		D->SetVectorParameterValue(TEXT("B1"), FLinearColor(Beads[4], Beads[5], Beads[6], Beads[7]));
		D->SetVectorParameterValue(TEXT("B2"), FLinearColor(Beads[8], Beads[9], Beads[10], Beads[11]));
		D->SetScalarParameterValue(TEXT("Blow"), Blow);
	}
	for (UMaterialInstanceDynamic* D : PaintMids)
	{
		if (!D) continue;
		D->SetVectorParameterValue(TEXT("B0"), FLinearColor(Beads[0], Beads[1], Beads[2], Beads[3]));
		D->SetVectorParameterValue(TEXT("B1"), FLinearColor(Beads[4], Beads[5], Beads[6], Beads[7]));
		D->SetVectorParameterValue(TEXT("B2"), FLinearColor(Beads[8], Beads[9], Beads[10], Beads[11]));
		D->SetScalarParameterValue(TEXT("Blow"), Blow);
		D->SetVectorParameterValue(TEXT("Origin"), FLinearColor(O.X, O.Y, O.Z, 0.f));
		D->SetVectorParameterValue(TEXT("AxisF"), FLinearColor(F.X, F.Y, F.Z, 0.f));
		D->SetVectorParameterValue(TEXT("Z0"), FLinearColor(DirtZone[0], DirtZone[1], DirtZone[2], DirtZone[3]));
		D->SetVectorParameterValue(TEXT("Z1"), FLinearColor(DirtZone[4], DirtZone[5], DirtZone[6], DirtZone[7]));
		D->SetVectorParameterValue(TEXT("Z2"), FLinearColor(DirtZone[8], DirtZone[9], DirtZone[10], DirtZone[11]));
	}
}

void AFireTruck::Wash(const FVector& At, float Amount)
{
	// the same areas as the paint's (Tools/set_truck_materials.py): 2 m lengths over 12 m, the two sides blended over 40 cm
	if (!bDirtCenter) return;
	const FVector Lp = At - GetActorTransform().TransformPosition(DirtCenter), F = DirtAxisWorld();
	const FVector Ar = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal();
	const float Fa = FMath::Clamp(float(FVector::DotProduct(Lp, F)) / 100.f + 6.f, 0.f, 11.999f) / 2.f;
	const float Sd = FMath::Clamp(float(FVector::DotProduct(Lp, Ar)) / 40.f + 0.5f, 0.f, 1.f);
	const int32 I0 = FMath::Min(int32(Fa), 5), I1 = FMath::Min(I0 + 1, 5);
	const float Ft = FMath::Clamp(Fa - I0, 0.f, 1.f);
	auto Take = [&](int32 I, float Wt) { DirtGoal[I] = FMath::Max(0.f, DirtGoal[I] - Amount * Wt * 2.f); };
	Take(I0, (1.f - Ft) * (1.f - Sd)); Take(I1, Ft * (1.f - Sd)); Take(6 + I0, (1.f - Ft) * Sd); Take(6 + I1, Ft * Sd);
	// and beads of water left behind there (the water runs a little way round: the neighbours a little wet too)
	auto Wet = [&](int32 I, float Wt) { Beads[I] = FMath::Min(1.f, Beads[I] + Amount * 40.f * Wt); };
	Wet(I0, 1.f - Sd); Wet(I1, 1.f - Sd); Wet(6 + I0, Sd); Wet(6 + I1, Sd);
	if (I0 > 0) Wet(I0 - 1, 0.25f * (1.f - Sd));
	if (I1 < 5) Wet(I1 + 1, 0.25f * (1.f - Sd));
}

void AFireTruck::FollowRoute()
{
	// the nearest point on the route, a point 10 m on from it, and the wheel turned toward it; the pace held, slower
	// into a bend
	const FVector2D P(GetActorLocation().X / 100.0, -GetActorLocation().Y / 100.0);
	double Best = 1e9; int32 Seg = 0; double T = 0;
	for (int32 i = 0; i + 1 < TestRoute.Num(); ++i)
	{
		const FVector2D A = TestRoute[i], AB = TestRoute[i + 1] - A;
		const double U = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1e-6), 0.0, 1.0);
		const double D = (A + AB * U - P).Size();
		if (D < Best) { Best = D; Seg = i; T = U; }
	}
	double Ahead = 10.0; FVector2D Aim = TestRoute.Last(); int32 i = Seg; FVector2D From = TestRoute[Seg] + (TestRoute[Seg + 1] - TestRoute[Seg]) * T;
	for (; i + 1 < TestRoute.Num(); ++i)
	{
		const FVector2D To = TestRoute[i + 1]; const double L = (To - From).Size();
		if (L >= Ahead) { Aim = From + (To - From) / L * Ahead; break; }
		Ahead -= L; From = To;
	}
	const FVector Fw = GetActorForwardVector();
	const FVector2D F(Fw.X, -Fw.Y), To = (Aim - P).GetSafeNormal();
	const double Cross = F.X * To.Y - F.Y * To.X, Dot = FVector2D::DotProduct(F, To);
	const double Ang = FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot));      // + to the left in the plan
	InSteer = float(FMath::Clamp(-Ang / 25.0, -1.0, 1.0));
	const float Want = TestKmh * (FMath::Abs(Ang) > 20.0 ? 0.55f : 1.f), Now = SpeedKmh();
	const bool bEnd = (TestRoute.Last() - P).Size() < 8.0;
	InThrottle = bEnd ? 0.f : Now < Want - 15.f ? 1.f : Now < Want ? 0.55f : 0.f;
	InBrake = bEnd ? 1.f : Now > Want + 6.f ? 0.4f : 0.f;
}

void AFireTruck::DressMaterials()
{
	if (!Model) return;
	UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_TruckPaint.M_TruckPaint"));
	UMaterialInterface* Lens = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_TruckLens.M_TruckLens"));
	if (!Paint || !Lens) { ADP_WARN(TEXT("Fire truck: its own materials are missing (Tools/set_truck_materials.py)")); return; }
	// the painted and worn parts: how much road dirt each gathers and how worn it gets
	struct FKind { const TCHAR* Name; float Dirt, Wear; };
	static const FKind Painted[] = {{TEXT("TruckPaint"), 0.45f, 0.35f}, {TEXT("TruckTeal"), 0.45f, 0.35f}, {TEXT("TruckWarningStripes"), 0.4f, 0.3f},
		{TEXT("TruckLettering"), 0.4f, 0.2f}, {TEXT("TruckBlack"), 0.55f, 0.4f}, {TEXT("TruckRim"), 0.75f, 0.5f}, {TEXT("TruckTreadPlate"), 0.6f, 0.8f}, {TEXT("TruckSatin"), 0.45f, 0.3f}, {TEXT("TruckRubber"), 0.75f, 0.1f}};
	TMap<UMaterialInterface*, UMaterialInterface*> Swap;
	TArray<UInstancedStaticMeshComponent*> Parts; Model->Parts(Parts);
	auto V = [](UMaterialInterface* M, const TCHAR* N, const FLinearColor& D) { FLinearColor C = D; M->GetVectorParameterValue(FHashedMaterialParameterInfo(N), C); return C; };
	auto S = [](UMaterialInterface* M, const TCHAR* N, float D) { float F = D; M->GetScalarParameterValue(FHashedMaterialParameterInfo(N), F); return F; };
	int32 NPaint = 0, NLens = 0;
	TSet<UInstancedStaticMeshComponent*> LensLists;
	TSet<UMaterialInterface*> LensMids;
	for (UInstancedStaticMeshComponent* K : Parts)
		for (int32 Si = 0; Si < K->GetNumMaterials(); ++Si)
		{
			UMaterialInterface* M = K->GetMaterial(Si);
			if (!M) continue;
			if (UMaterialInterface** F = Swap.Find(M)) { if (*F) { K->SetMaterial(Si, *F); if (LensMids.Contains(*F)) LensLists.Add(K); } continue; }
			// (a material the model already copied, toned down, is known by the one it was copied from)
			const UMaterialInstanceDynamic* Md = Cast<UMaterialInstanceDynamic>(M);
			const FString MN = Md && Md->Parent ? Md->Parent->GetName() : M->GetName();
			UMaterialInstanceDynamic* D = nullptr;
			for (const FKind& Kd : Painted)
				if (MN == Kd.Name)
				{
					D = UMaterialInstanceDynamic::Create(Paint, this);
					D->SetVectorParameterValue(TEXT("BC"), V(M, TEXT("BaseColorFactor"), FLinearColor::White));
					D->SetVectorParameterValue(TEXT("OS"), V(M, TEXT("BaseColorTexture_OffsetScale"), FLinearColor(0, 0, 1, 1)));
					UTexture* T = nullptr;
					if (M->GetTextureParameterValue(FHashedMaterialParameterInfo(TEXT("BaseColorTexture")), T) && T) D->SetTextureParameterValue(TEXT("Base"), T);
					D->SetScalarParameterValue(TEXT("R"), S(M, TEXT("RoughnessFactor"), 0.4f));
					D->SetScalarParameterValue(TEXT("M"), S(M, TEXT("MetallicFactor"), 0.f));
					D->SetScalarParameterValue(TEXT("CC"), S(M, TEXT("ClearCoatFactor"), 0.f));
					D->SetScalarParameterValue(TEXT("CCR"), S(M, TEXT("ClearCoatRoughnessFactor"), 0.1f));
					D->SetScalarParameterValue(TEXT("Dirt"), Kd.Dirt); D->SetScalarParameterValue(TEXT("Wear"), Kd.Wear);
					PaintMids.Add(D); ++NPaint;
				}
			if (!D && MN.Contains(TEXT("Glass")))
			{
				static UMaterialInterface* GlassM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Truck/M_TruckGlass.M_TruckGlass"));
				if (GlassM)
				{
					D = UMaterialInstanceDynamic::Create(GlassM, this);
					D->SetVectorParameterValue(TEXT("BC"), V(M, TEXT("BaseColorFactor"), FLinearColor(0.015f, 0.022f, 0.026f, 0.38f)));
					GlassMids.Add(D);
				}
			}
			if (!D && MN.StartsWith(TEXT("TruckLens")))
			{
				D = UMaterialInstanceDynamic::Create(Lens, this);
				D->SetVectorParameterValue(TEXT("BC"), V(M, TEXT("BaseColorFactor"), FLinearColor::White));
				const float Glow = MN.EndsWith(TEXT("Blue")) ? 70.f : MN.EndsWith(TEXT("Brake")) ? 30.f : MN.Contains(TEXT("Turn")) ? 45.f : 28.f;
				D->SetScalarParameterValue(TEXT("Glow"), Glow);
				D->SetScalarParameterValue(TEXT("Dots"), MN.Contains(TEXT("Head")) || MN.Contains(TEXT("High")) ? 3.f : MN.EndsWith(TEXT("Blue")) ? 7.f : 5.f);
				LensLists.Add(K); LensMids.Add(D); ++NLens;
			}
			Swap.Add(M, D);
			if (D) K->SetMaterial(Si, D);
		}
	for (UInstancedStaticMeshComponent* K : LensLists) K->SetNumCustomDataFloats(1);
	// every lens, which lamp it is and its side; the roof bar's modules counted from the left
	struct FWhich { const TCHAR* Name; ELens Kind; int8 Side; };
	static const FWhich Which[] = {{TEXT("Light_BlueBar"), ELens::BlueBar, -1}, {TEXT("Light_BlueFront"), ELens::Blue, -1}, {TEXT("Light_BlueRear"), ELens::Blue, -1},
		{TEXT("Light_BlueSide"), ELens::Blue, -1}, {TEXT("Light_HeadLow"), ELens::Low, -1}, {TEXT("Light_HeadHigh"), ELens::High, -1}, {TEXT("Light_Brake"), ELens::Rear, -1},
		{TEXT("Light_TurnL"), ELens::Turn, 0}, {TEXT("Light_TurnR"), ELens::Turn, 1}, {TEXT("Light_TurnSideL"), ELens::Turn, 0}, {TEXT("Light_TurnSideR"), ELens::Turn, 1},
		{TEXT("Light_MarkerSide"), ELens::Marker, -1}, {TEXT("Light_Reverse"), ELens::Reverse, -1}};
	TArray<TPair<float, int32>> BarAt;
	for (const FWhich& Wh : Which)
	{
		TArray<TTuple<UInstancedStaticMeshComponent*, int32, FString>> Found;
		Model->InstancesNamed(Wh.Name, false, Found);
		for (const auto& F : Found)
		{
			UInstancedStaticMeshComponent* K = F.Get<0>();
			if (!LensLists.Contains(K)) continue;
			FTransform T; K->GetInstanceTransform(F.Get<1>(), T, false);
			FLens L; L.K = K; L.Ix = F.Get<1>(); L.Kind = Wh.Kind;
			L.Side = Wh.Side >= 0 ? uint8(Wh.Side) : uint8(T.GetLocation().X < 0.f ? 0 : 1);
			if (Wh.Kind == ELens::BlueBar) BarAt.Add(TPair<float, int32>(T.GetLocation().X, Lenses.Num()));
			Lenses.Add(L);
		}
	}
	BarAt.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	for (int32 I = 0; I < BarAt.Num(); ++I) Lenses[BarAt[I].Value].Bar = I;
	BarCount = BarAt.Num();
	ADP_LOG(TEXT("Fire truck: its own paint on %d materials, %d lens materials, %d lenses lit on their own (%d in the roof bar)"), NPaint, NLens, Lenses.Num(), BarCount);
}

void AFireTruck::UpdateLenses(bool bRunning, bool bBraking, bool bTurnOn)
{
	// the side lamps' double flash (as the halo and the light thrown round about), and the roof bar's own rhythm: its
	// modules in two groups that take turns, each a quick triple flash - the left half against the right, then (every
	// other second) the outer modules against the inner ones, as real light bars run
	auto Double = [this](uint8 Side)
	{
		const float Round = FMath::Max(0.2f, LampIni().F(TEXT("BlueLights.Round"), 0.75f)), C = FMath::Fmod(BlueClock + (Side ? Round * 0.5f : 0.f), Round) / Round * 0.75f;
		return (C < 0.06f) || (C > 0.11f && C < 0.17f);
	};
	auto Bar = [this](int32 I)
	{
		const float P = 1.0f, C = FMath::Fmod(BlueClock, P);
		const int32 Cyc = int32(BlueClock / P) % 2, N = FMath::Max(BarCount, 2);
		const bool bA = Cyc == 0 ? I < N / 2 : (I <= 1 || I >= N - 2);
		const float G = bA ? C : FMath::Fmod(C + P * 0.5f, P);
		return (G < 0.05f) || (G > 0.09f && G < 0.14f) || (G > 0.18f && G < 0.23f);
	};
	for (FLens& L : Lenses)
	{
		float V = 0.f;
		switch (L.Kind)
		{
		case ELens::Blue: V = bBlue && Double(L.Side) ? 1.f : 0.f; break;
		case ELens::BlueBar: V = bBlue && Bar(L.Bar) ? 1.f : 0.f; break;
		case ELens::Low: V = bRunning ? 0.8f : 0.f; break;
		case ELens::High: V = bRunning && bHighBeam ? 1.f : 0.f; break;
		case ELens::Rear: V = bBraking ? 1.f : bRunning ? 0.3f : 0.f; break;
		case ELens::Turn: V = bTurnOn && (Signal == ETurn::Hazard || (Signal == ETurn::Left) == (L.Side == 0)) ? 1.f : 0.f; break;
		case ELens::Marker: V = bRunning ? 0.5f : 0.f; break;
		case ELens::Reverse: V = bReverseLit ? 1.f : 0.f; break;
		}
		if (V != L.Now && L.K) { L.Now = V; L.K->SetCustomDataValue(L.Ix, 0, V, true); }
	}
}

void AFireTruck::ReverseGear(bool bOn)
{
	if (bOn == bReverseLit || !Model) return;
	bReverseLit = bOn;
	for (USceneComponent* L : ReverseLights) if (L) L->SetVisibility(bOn);
	ADP_LOG(TEXT("Fire truck: reverse gear %s (%d lamps)"), bOn ? TEXT("in") : TEXT("out"), ReverseLights.Num());
	if (bOn)
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
		W->SetSampleRate(SndRate); W->NumChannels = 1; W->Duration = INDEFINITELY_LOOPING_DURATION; W->bLooping = false;
		W->SoundGroup = SOUNDGROUP_Effects;
		TSharedPtr<FBeepState> St = MakeShared<FBeepState>();
		W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([St](USoundWaveProcedural* Wv, int32 Samples)
		{
			TArray<int16> Buf; BeepFill(*St, Buf, FMath::Max(Samples, SndRate / 20));
			Wv->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Buf.Num() * sizeof(int16));
		});
		// (the alarm sits under the rear bumper)
		BeeperOut = UGameplayStatics::SpawnSoundAttached(W, Model->GetModelRoot(), NAME_None, BlenderToRoot(0.f, -5.9f, 0.7f), EAttachLocation::KeepRelativeOffset, false, 0.7f, 1.f, 0.f, NearSound);
	}
	else if (BeeperOut) { BeeperOut->Stop(); BeeperOut = nullptr; }
}

void AFireTruck::EngineOn(bool bOn)
{
	if (bOn == (EngineOut != nullptr) || !Model) return;
	if (bOn)
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
		W->SetSampleRate(SndRate); W->NumChannels = 1; W->Duration = INDEFINITELY_LOOPING_DURATION; W->bLooping = false; W->SoundGroup = SOUNDGROUP_Effects;
		Engine = MakeShared<FEngineVoice>();
		TSharedPtr<FEngineVoice> St = Engine;
		W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([St](USoundWaveProcedural* Wv, int32 Samples)
		{
			TArray<int16> Buf; St->Fill(Buf, FMath::Max(Samples, SndRate / 20));
			Wv->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Buf.Num() * sizeof(int16));
		});
		// from under the cab's front (the power unit sits behind it), heard a good way off
		EngineOut = UGameplayStatics::SpawnSoundAttached(W, Model->GetModelRoot(), NAME_None, BlenderToRoot(0.f, 3.6f, 0.9f), EAttachLocation::KeepRelativeOffset, false, 0.55f, 1.f, 0.f, FarSound);
	}
	else if (EngineOut) { EngineOut->Stop(); EngineOut = nullptr; Engine.Reset(); }
}

void AFireTruck::ToggleSiren()
{
	if (!Model || bPadMod) return;   // (B + D-pad left is the left indicator)
	bSiren = !bSiren;
	if (bSiren)
	{
		USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
		W->SetSampleRate(SndRate); W->NumChannels = 1; W->Duration = INDEFINITELY_LOOPING_DURATION; W->bLooping = false;
		W->SoundGroup = SOUNDGROUP_Effects;
		TSharedPtr<FSirenState> St = MakeShared<FSirenState>();
		W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([St](USoundWaveProcedural* Wv, int32 Samples)
		{
			TArray<int16> Buf; SirenFill(*St, Buf, FMath::Max(Samples, SndRate / 20));
			Wv->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Buf.Num() * sizeof(int16));
		});
		// (the speakers on the cab's roof, at the front)
		SirenOut = UGameplayStatics::SpawnSoundAttached(W, Model->GetModelRoot(), NAME_None, BlenderToRoot(0.f, 5.4f, 3.2f), EAttachLocation::KeepRelativeOffset, false, 0.9f, 1.f, 0.f, FarSound);
	}
	else if (SirenOut) { SirenOut->Stop(); SirenOut = nullptr; }
	ADP_LOG(TEXT("Fire truck: siren %s"), bSiren ? TEXT("on") : TEXT("off"));
}

void AFireTruck::Sound(const TArray<int16>& Pcm, const FVector& RootAt, bool bFar)
{
	if (!Model || Pcm.Num() == 0) return;
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
	W->SetSampleRate(SndRate); W->NumChannels = 1; W->Duration = Pcm.Num() / float(SndRate); W->bLooping = false;
	W->SoundGroup = SOUNDGROUP_Effects;
	W->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
	UGameplayStatics::SpawnSoundAttached(W, Model->GetModelRoot(), NAME_None, RootAt, EAttachLocation::KeepRelativeOffset, true, 1.f, 1.f, 0.f, bFar ? FarSound.Get() : NearSound.Get());
}

FDriverPose AFireTruck::PoseAt(float T) const
{
	// the keys either side, eased between; the first one where you really stood
	auto KeyPelvis = [this](int32 I) { return I == 0 ? StartPelvis : BlenderToRoot(ClimbKeys[I].Pelvis.X, ClimbKeys[I].Pelvis.Y, ClimbKeys[I].Pelvis.Z); };
	auto KeyFace = [this](int32 I) { return I == 0 ? StartFacing : DirToRoot(ClimbKeys[I].Face.X, ClimbKeys[I].Face.Y); };
	T = FMath::Clamp(T, 0.f, ClimbLen);
	int32 A = 0;
	while (A < NumClimbKeys - 2 && T > ClimbKeys[A + 1].T) ++A;
	const int32 B = A + 1;
	const float U = FMath::Clamp((T - ClimbKeys[A].T) / (ClimbKeys[B].T - ClimbKeys[A].T), 0.f, 1.f), E = FMath::SmoothStep(0.f, 1.f, U);
	FDriverPose P;
	// the pelvis on a smooth curve through the keys
	const FVector P0 = KeyPelvis(FMath::Max(A - 1, 0)), P1 = KeyPelvis(A), P2 = KeyPelvis(B), P3 = KeyPelvis(FMath::Min(B + 1, NumClimbKeys - 1));
	P.Pelvis = 0.5f * ((2.f * P1) + (-P0 + P2) * E + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * E * E + (-P0 + 3.f * P1 - 3.f * P2 + P3) * E * E * E);
	P.Facing = FMath::Lerp(KeyFace(A), KeyFace(B), E).GetSafeNormal();
	P.Lean = FMath::Lerp(ClimbKeys[A].Lean, ClimbKeys[B].Lean, E);
	const FVector Up(0, 0, 1);
	for (int32 S = 0; S < 2; ++S)
	{
		// which side is this one's: the rig's own left (a vector across the body)
		const FVector Across = FVector::CrossProduct(Up, P.Facing).GetSafeNormal();
		const FVector StartAcross = FVector::CrossProduct(Up, StartFacing).GetSafeNormal();
		const float Sgn = S == 0 ? -1.f : 1.f;
		// a foot that moves lifts on its way (the start: under your hips)
		auto KeyFoot = [&](int32 I) { return I == 0 ? StartPelvis * FVector(1, 1, 0) + Sgn * 12.f * StartAcross : BlenderToRoot(ClimbKeys[I].Foot[S].X, ClimbKeys[I].Foot[S].Y, ClimbKeys[I].Foot[S].Z); };
		const FVector Fa = KeyFoot(A), Fb = KeyFoot(B);
		P.Foot[S] = FMath::Lerp(Fa, Fb, E) + (FVector::Dist(Fa, Fb) > 3.f ? Up * 14.f * FMath::Sin(PI * E) : FVector::ZeroVector);
		// the hands: what they hold, or hanging by your side
		const FVector Rest = P.Pelvis + Sgn * Across * 24.f + P.Facing * 4.f - Up * 10.f;
		auto KeyHand = [&](int32 I) -> FVector
		{
			if (ClimbKeys[I].HM[S] == EHand::At) return BlenderToRoot(ClimbKeys[I].Hand[S].X, ClimbKeys[I].Hand[S].Y, ClimbKeys[I].Hand[S].Z);
			if (ClimbKeys[I].HM[S] == EHand::Handle) return HandleNow();
			if (ClimbKeys[I].HM[S] == EHand::Wheel)
			{
				// on the rim at ten to two, carried round as the wheel turns
				const FVector WC(0.f, 4.6f, 1.87f), Rt(1.f, 0.f, 0.f), Top(0.f, 0.866f, 0.5f), Ax(0.f, -0.5f, 0.866f);
				const float Ang = FMath::DegreesToRadians(S == 0 ? -62.f : 62.f);
				const FVector Rim = WC + (Rt * FMath::Sin(Ang) + Top * FMath::Cos(Ang)) * 0.23f;
				const FVector Wrist = Rim + (Rim - WC).GetSafeNormal() * 0.045f + Ax * 0.03f;
				return WheelNow.TransformPosition(BSteeringWheel.InverseTransformPosition(BlenderToRoot(Wrist.X, Wrist.Y, Wrist.Z)));
			}
			return Rest;
		};
		P.Hand[S] = FMath::Lerp(KeyHand(A), KeyHand(B), E);
		P.bHand[S] = true;
		P.Grip[S] = FMath::Lerp(ClimbKeys[A].Grip[S], ClimbKeys[B].Grip[S], E);
	}
	return P;
}

FVector AFireTruck::HandleNow() const
{
	// the driver's door's outside handle (Art/FireTruck/make_panther.py: on the lower panel, 1.04-1.09 m up, 0.15-0.4 m
	// behind the door's rear edge), the wrist 5 cm out from it, carried round the hinge as the door swings
	const FVector H0 = BlenderToRoot(-1.60f, 3.96f, 1.065f);
	const float A = DoorTarget > 0.f ? FMath::Sin(DoorK * HALF_PI) : DoorK * DoorK * (2.f - DoorK);
	return HingeRoot[0] + FQuat(FVector::UpVector, FMath::DegreesToRadians(DoorSign * 70.f * A)).RotateVector(H0 - HingeRoot[0]);
}

FTransform AFireTruck::CamPath(float T) const
{
	// from where you looked, out to behind your left shoulder (the door and steps in view), and in to where you'll look
	const FVector TpAt = BlenderToRoot(-5.3f, 1.3f, 2.75f), Aim = BlenderToRoot(-1.35f, 4.3f, 1.35f);
	const FTransform Tp(FRotationMatrix::MakeFromX(Aim - TpAt).ToQuat(), TpAt);
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	FTransform To;
	if (Seq == ESeq::In) To = (bChase ? ChaseCam.Get() : DriverCam.Get())->GetComponentTransform().GetRelativeTransform(RX);
	else
	{
		// exactly where your own view will be once you stand there (your eyes, or behind you)
		const FRotator Look(-6.f, StartFacing.Rotation().Yaw, 0.f);
		const AADPWalker* Wk = Cast<AADPWalker>(Driver.Get());
		const float Half = 90.f;
		To = Wk ? Wk->ViewFrom(FVector(StartPelvis.X, StartPelvis.Y, Half), Look) : FTransform(Look, FVector(StartPelvis.X, StartPelvis.Y, 160.f));
	}
	const float L = ClimbLen + (Seq == ESeq::Out ? 0.25f : 0.f);
	auto Blend = [](const FTransform& A, const FTransform& B, float K)
	{
		const float E = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(K, 0.f, 1.f));
		return FTransform(FQuat::Slerp(A.GetRotation(), B.GetRotation(), E).GetNormalized(), FMath::Lerp(A.GetLocation(), B.GetLocation(), E));
	};
	if (T < 0.8f) return Blend(CamFrom, Tp, T / 0.8f);
	if (T < L - 0.8f) return Tp;
	return Blend(Tp, To, (T - (L - 0.8f)) / 0.8f);
}

void AFireTruck::Sequence(float Dt)
{
	const FTransform RX = Model->GetModelRoot()->GetComponentTransform();
	if (Seq == ESeq::Walk)
	{
		// walking you to the foot of the steps: along the ground, turning to face the way you go; there (or after a
		// while, if something blocks the way) the climb begins
		WalkClock += Dt;
		APawn* Wk = Driver;
		if (!Wk) { Seq = ESeq::None; return; }
		const FVector Spot = RX.TransformPosition(BlenderToRoot(ClimbKeys[0].Pelvis.X, ClimbKeys[0].Pelvis.Y, 0.f));
		FVector To = Spot - Wk->GetActorLocation(); To.Z = 0.f;
		const float Left = To.Size();
		if (Left < 30.f || WalkClock > 14.f) { if (Wk->GetMovementComponent()) Wk->GetMovementComponent()->StopMovementImmediately(); BeginClimb(Wk); return; }
		const FVector Dir = To / Left;
		Wk->AddMovementInput(Dir, Left < 120.f ? 0.6f : 1.f);
		if (AController* C = Wk->GetController())
		{
			const FRotator Rc = C->GetControlRotation();
			const float Pitch = Rc.Pitch > 180.f ? Rc.Pitch - 360.f : Rc.Pitch;
			C->SetControlRotation(FRotator(FMath::FInterpTo(Pitch, -8.f, Dt, 3.f), FMath::FixedTurn(Rc.Yaw, Dir.Rotation().Yaw, 240.f * Dt), 0.f));
		}
		return;
	}
	if (Seq != ESeq::None)
	{
		SeqT += Dt;
		MoveCam->SetWorldTransform(CamPath(SeqT) * RX);
		if (Seq == ESeq::In && SeqT >= HandleT) DoorTarget = 1.f;  // swung open by its handle
		if (Seq == ESeq::In && SeqT >= 3.65f) DoorTarget = 0.f;    // pulled shut behind you
		if (Seq == ESeq::Out && ClimbLen - SeqT <= HandleT) DoorTarget = 0.f;   // standing outside: pulled shut by its handle
		if (Seq == ESeq::In && SeqT >= ClimbLen)
		{
			Seq = ESeq::None;
			MoveCam->SetActive(false);
			SetView(bChase);
			EngineOn(true);
			ADP_LOG(TEXT("Fire truck: you got in (%s)"), *GetName());
		}
		else if (Seq == ESeq::Out && SeqT >= ClimbLen + 0.25f) { FinishExit(); return; }
	}
	if (!Rider || !Rig.IsReady() || !Driver) return;
	// the body: climbing (in: forwards through the keys, out: backwards), or at the wheel
	const float KeyT = Seq == ESeq::Out ? ClimbLen - SeqT : Seq == ESeq::In ? SeqT : ClimbLen;
	FDriverPose P = PoseAt(KeyT);
	const float SitT = ClimbKeys[NumClimbKeys - 2].T;
	if (KeyT > SitT)
	{
		// at the pedals: the right foot's ball on the accelerator, or across on the brake; the left foot on the floor
		const bool bOnBrake = (bReverse ? InThrottle : InBrake) > 0.05f;
		const FVector Pad = bOnBrake ? BrakeNow.TransformPosition(FTransform(BrakePin).InverseTransformPosition(BrakePad)) : AccelNow.GetLocation();
		const float Pitch = bOnBrake ? 18.f : 32.f - InThrottle * 12.f;
		const FVector Up(0, 0, 1);
		const FVector Toe = (P.Facing * FMath::Cos(FMath::DegreesToRadians(Pitch)) + Up * FMath::Sin(FMath::DegreesToRadians(Pitch))).GetSafeNormal();
		const FVector Sole = Pad - Toe * 13.f + Up * 1.5f;
		const float K = FMath::SmoothStep(0.f, 1.f, (KeyT - SitT) / (ClimbLen - SitT));
		P.Foot[1] = FMath::Lerp(P.Foot[1], Sole, K);
		P.FootPitch[1] = Pitch * K;
	}
	Rig.Pose(P);
	if (HumanPose) HumanPose->Local = Rig.LocalPose();
	// in the driver's seat your own head stays out of the way of your eyes
	APlayerController* PC = Cast<APlayerController>(GetController());
	const bool bHide = PC && PC->PlayerCameraManager && FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), RX.TransformPosition(Rig.Eyes())) < 45.f;
	if (bHide != bHeadHidden)
	{
		bHeadHidden = bHide;
		if (HumanPose) { if (AADPWalker* Me = Cast<AADPWalker>(Driver.Get())) Me->ShowHead(!bHide); }
		else if (bHide) Rider->HideBoneByName(TEXT("head"), PBO_None); else Rider->UnHideBoneByName(TEXT("head"));
	}
}

void AFireTruck::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Model || Dt <= 0.f) return;
	if (bExitPending)
	{
		InThrottle = 0.f; InBrake = 0.f; InSteer = 0.f; StopHold = 0.f;
		if (SpeedKmh() < 4.f) StartExit();
	}
	else if (Seq != ESeq::None) { InThrottle = InBrake = InSteer = 0.f; }
	else if (Driver && !bTestInput)
	{
		InThrottle = FMath::Max(FMath::Max(KeyFwd, 0.f), PadThrottle);
		InBrake = FMath::Max(FMath::Max(-KeyFwd, 0.f), PadBrake);
		InSteer = FMath::Clamp(KeySteer, -1.f, 1.f);
	}
	// the parking brake lets itself off as you pull away (as on today's trucks); held with Space / pad A it stays on
	if (InThrottle > 0.1f && !bHandHeld) bHandbrake = false;
	if (TestRoute.Num() >= 2 && Driver) FollowRoute();
	if (bSim) Simulate(FMath::Min(Dt, 1.f / 20.f));
	if (bSim || DoorK > 0.f || DoorTarget > 0.f) Animate(Dt);
	if (Driver || Seq != ESeq::None) Sequence(Dt);
	DriverCam->SetRelativeRotation(FRotator(LookPitch - 11.f, LookYaw, 0.f));   // a driver's eyes rest a little down the road, the wheel's rim in view
	Boom->SetRelativeRotation(FRotator((bFarView ? -16.f : -12.f) + LookPitch * 0.5f, LookYaw, 0.f));
	if (bChase)
	{
		// the chase camera stays on this side of walls and ceilings (in the bay it would sit in the back wall)
		const FVector From = Boom->GetComponentLocation(), To = From - Boom->GetForwardVector() * (ArmWant() + 50.f);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPTruckCam), true, this);
		Q.AddIgnoredActor(Model);
		if (Driver) Q.AddIgnoredActor(Driver);
		FHitResult H;
		const float Want = GetWorld()->LineTraceSingleByChannel(H, From, To, ECC_Visibility, Q) ? FMath::Max(250.f, H.Distance - 60.f) : ArmWant();
		// in at once (never through the wall), back out gently
		Boom->TargetArmLength = Want < Boom->TargetArmLength ? Want : FMath::FInterpTo(Boom->TargetArmLength, Want, Dt, 2.f);
	}
	// in reverse gear the reversing lamps light and the beeper sounds (owner, 2026-11-11), until you drive forward again
	ReverseGear(Driver && bSim && bReverse && Seq == ESeq::None);
	if (Engine.IsValid()) { Engine->Kmh.store(SpeedKmh()); Engine->Load.store(FMath::Clamp(InThrottle, 0.f, 1.f)); }
	UpdateLamps(Dt);
	// once you've got out and it stands still a while, it rests (no physics running; it stays exactly where it stopped)
	if (bSim && !Driver) { Still = SpeedKmh() < 0.5f ? Still + Dt : 0.f; if (Still > 3.f) SetPhysics(false); }
}

float AFireTruck::TurnRate() const { return Body ? Body->GetPhysicsAngularVelocityInDegrees().Size() : 0.f; }

void AFireTruck::Simulate(float Dt)
{
	const FTransform BT = Body->GetComponentTransform();
	const FVector Up = BT.GetUnitAxis(EAxis::Z), F = BT.GetUnitAxis(EAxis::X), R = BT.GetUnitAxis(EAxis::Y);
	const FVector V = Body->GetPhysicsLinearVelocity() / 100.f;
	const float Vf = FVector::DotProduct(V, F), Speed = FMath::Abs(Vf);
	// the steering: towards the wheel's target at a steady rate, less lock the faster it goes
	const float Lock = MaxSteer / (1.f + Speed / 12.f);
	Steer = FMath::FInterpConstantTo(Steer, InSteer * Lock, Dt, 45.f);
	// forward on the throttle; reverse once stopped with the brake held; the other pedal brakes
	StopHold = (InBrake > 0.1f && InThrottle < 0.1f && Vf < 0.6f) ? StopHold + Dt : 0.f;
	if (StopHold > 0.9f) bReverse = true;
	if (InThrottle > 0.1f && Vf > -0.6f) bReverse = false;
	// (getting out while moving: it simply brakes to a stop, whichever way it's going)
	const float Drive = bExitPending ? 0.f : bReverse ? -InBrake : InThrottle, BrakeIn = bExitPending ? 1.f : bReverse ? InThrottle : InBrake;
	float DriveForce = 0.f;
	if (Drive > 0.f) DriveForce = Drive * FMath::Min(DrivePower / FMath::Max(Speed, 2.f), 0.7f * TruckMass * Gravity) * FMath::Clamp((TopSpeed - Vf) / 3.f, 0.f, 1.f);
	else if (Drive < 0.f) DriveForce = Drive * 0.45f * TruckMass * Gravity * FMath::Clamp((ReverseTop + Vf) / 1.5f, 0.f, 1.f);
	const float BrakeForce = BrakeIn * 0.75f * TruckMass * Gravity;

	// the suspension: a ray down from each wheel's top of travel
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ADPTruckWheel), true, this);   // (the buildings' floors by their own triangles)
	Q.AddIgnoredActor(Model);
	if (Driver) Q.AddIgnoredActor(Driver);
	TArray<FVector> Mount, Contact; Mount.SetNum(Wheels.Num()); Contact.SetNum(Wheels.Num());
	for (int32 I = 0; I < Wheels.Num(); ++I)
	{
		FWheel& Wh = Wheels[I];
		Mount[I] = BT.TransformPosition(Wh.Rest + FVector(0, 0, BumpCm));
		FHitResult H;
		float S = StrokeCm;
		Wh.bContact = GetWorld()->LineTraceSingleByChannel(H, Mount[I], Mount[I] - Up * (StrokeCm + WheelRCm + 5.f), ECC_Visibility, Q);
		if (Wh.bContact) { S = FMath::Clamp(H.Distance - WheelRCm, 0.f, StrokeCm); Contact[I] = H.ImpactPoint; Wh.bContact = H.Distance - WheelRCm <= StrokeCm; }
		const float X = StrokeCm - S;                       // compression, cm
		const float Vx = (X - Wh.X) / Dt; Wh.X = X;
		Wh.Travel = BumpCm - S;                             // up from rest, cm
		float Fs = KSpring * X / 100.f + CDamp * Vx / 100.f;
		if (S < 2.f) Fs += KSpring * 4.f * (2.f - S) / 100.f;   // the bump stop
		Wh.Load = Wh.bContact ? FMath::Max(Fs, 0.f) : 0.f;
	}
	// anti-roll: across each axle, the more compressed side carries more
	for (int32 A = 0; A < 3; ++A)
	{
		FWheel& L = Wheels[A * 2]; FWheel& Rt = Wheels[A * 2 + 1];
		const float Far = AntiRoll * (L.X - Rt.X) / 100.f;
		if (L.bContact) L.Load = FMath::Max(L.Load + Far, 0.f);
		if (Rt.bContact) Rt.Load = FMath::Max(Rt.Load - Far, 0.f);
	}
	float SumLoad = 0.f;
	for (int32 I = 0; I < Wheels.Num(); ++I) if (Wheels[I].Load > 0.f) { SumLoad += Wheels[I].Load; Body->AddForceAtLocation(Up * Wheels[I].Load * 100.f, Mount[I]); }

	// the tyres
	for (int32 I = 0; I < Wheels.Num(); ++I)
	{
		FWheel& Wh = Wheels[I];
		// Ackermann: each wheel turned to the same turning centre (on the middle axle's line); the rear axle the other way
		float Delta = 0.f;
		if (FMath::Abs(Steer) > 0.05f && Wh.Axle != 1)
		{
			const float Rr = Wheelbase / FMath::Tan(FMath::DegreesToRadians(FMath::Abs(Steer)));
			const float Inside = FMath::Sign(Steer) * Wh.Side;   // +1 on the inside of the turn
			const float Rw2 = FMath::Max(1.f, Rr - Inside * TrackWidth * 0.5f);
			Delta = FMath::Sign(Steer) * FMath::RadiansToDegrees(FMath::Atan((Wh.Axle == 0 ? Wheelbase : RearOff) / Rw2)) * (Wh.Axle == 0 ? 1.f : -1.f);
		}
		Wh.Steer = Delta;
		const float Sn = FMath::Sin(FMath::DegreesToRadians(Delta)), Cs = FMath::Cos(FMath::DegreesToRadians(Delta));
		const FVector WF = F * Cs + R * Sn, WR = R * Cs - F * Sn;
		const FVector At = Wh.bContact ? Contact[I] : BT.TransformPosition(Wh.Rest);
		const FVector Vp = Body->GetPhysicsLinearVelocityAtPoint(At) / 100.f;
		const float VF = FVector::DotProduct(Vp, WF), VL = FVector::DotProduct(Vp, WR);
		Wh.Spin += SpinSign * VF / WheelR * Dt;
		if (!Wh.bContact || Wh.Load <= 0.f) continue;
		const float Fz = Wh.Load, Share = SumLoad > 0.f ? Fz / SumLoad : 0.f;
		// the most a resisting force may do in one step is stop this wheel's share of the truck: any more and it pushes
		// back the other way the next frame, over and over (the truck shook standing and at low speed)
		const float Stop = TruckMass * Share / Dt;
		float Resist = BrakeForce * Share * FMath::Tanh(VF / 0.5f);
		Resist += 0.012f * Fz * FMath::Tanh(VF / 0.3f);                                 // rolling
		if (bHandbrake && Wh.Axle > 0) Resist += TyreMu * Fz * FMath::Tanh(VF / 0.3f);       // the handbrake on the rear axles
		if (FMath::IsNearlyZero(Drive) && Speed < 0.8f) Resist += 0.6f * TyreMu * Fz * FMath::Tanh(VF / 0.15f);   // standing still holds
		Resist = FMath::Clamp(Resist, -Stop * FMath::Abs(VF), Stop * FMath::Abs(VF));
		float Fl = DriveForce * Share - Resist;
		const float Slip = FMath::Atan2(VL, FMath::Max(FMath::Abs(VF), 1.f));
		float Fy = -TyreMu * Fz * FMath::Tanh(Slip * 6.f);
		Fy = FMath::Clamp(Fy, -Stop * FMath::Abs(VL), Stop * FMath::Abs(VL));
		const float Mag = FMath::Sqrt(Fl * Fl + Fy * Fy), Lim = TyreMu * Fz;
		if (Mag > Lim) { Fl *= Lim / Mag; Fy *= Lim / Mag; }
		Body->AddForceAtLocation((WF * Fl + WR * Fy) * 100.f, At);
	}
	// the air
	Body->AddForce(-V.GetSafeNormal() * 0.5f * 1.2f * 0.9f * 8.5f * V.SizeSquared() * 100.f);
}

void AFireTruck::Animate(float Dt)
{
	// the wheels and everything that moves with them (all in the model's root space)
	for (FWheel& Wh : Wheels)
	{
		const FVector C0 = Wh.CentreRoot, Kp = C0 - FVector(Wh.Side * KingpinOff * 100.f, 0, 0);
		const FTransform SteerT = About(Kp, FQuat(FVector::UpVector, FMath::DegreesToRadians(SteerSign * Wh.Steer)));
		const FTransform Lift(FVector(0, 0, Wh.Travel));
		if (Wh.GWheel >= 0) Model->MoveGroup(Wh.GWheel, Wh.BWheel * About(C0, FQuat(FVector::ForwardVector, Wh.Spin)) * SteerT * Lift);
		if (Wh.GFollow >= 0) Model->MoveGroup(Wh.GFollow, FTransform(C0) * SteerT * Lift);
		auto Swing = [&](const FVector& P, const FVector& O0) { return FQuat::FindBetweenNormals((O0 - P).GetSafeNormal(), (O0 + FVector(0, 0, Wh.Travel) - P).GetSafeNormal()); };
		const FVector PL = Wh.BLower.GetLocation(), PU = Wh.BUpper.GetLocation();
		const FQuat QL = Swing(PL, C0 + FVector(-Wh.Side * 41.f, 0, -19.f));
		if (Wh.GLower >= 0) Model->MoveGroup(Wh.GLower, Wh.BLower * About(PL, QL));
		if (Wh.GUpper >= 0) Model->MoveGroup(Wh.GUpper, Wh.BUpper * About(PU, Swing(PU, C0 + FVector(-Wh.Side * 41.f, 0, 25.f))));
		// the coil-over: its top fixed, its lower eye riding on the lower arm
		const FVector Top = Wh.BSpring.GetLocation(), E0 = Wh.BDamper.GetLocation(), E1 = PL + QL.RotateVector(E0 - PL);
		const FQuat QS = FQuat::FindBetweenNormals((E0 - Top).GetSafeNormal(), (E1 - Top).GetSafeNormal());
		const float Sc = (E1 - Top).Size() / FMath::Max(1.f, float((E0 - Top).Size()));
		if (Wh.GSpring >= 0) Model->MoveGroup(Wh.GSpring, FTransform(FQuat::Identity, FVector::ZeroVector, FVector(1, 1, Sc)) * Wh.BSpring * About(Top, QS));
		if (Wh.GDamper >= 0) Model->MoveGroup(Wh.GDamper, Wh.BDamper * FTransform(-E0) * FTransform(QS) * FTransform(E1));
		if (Wh.GRod >= 0) Model->MoveGroup(Wh.GRod, Wh.BRod * About(Top, QS));
		const FVector D = Wh.BShaft.GetLocation();
		if (Wh.GShaft >= 0) Model->MoveGroup(Wh.GShaft, Wh.BShaft * About(D, FQuat::FindBetweenNormals((C0 - D).GetSafeNormal(), (C0 + FVector(0, 0, Wh.Travel) - D).GetSafeNormal())));
	}
	// the steering wheel (one and a half turns lock to lock), the pedals
	WheelNow = FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(-Steer / MaxSteer * 270.f))) * BSteeringWheel;
	AccelNow = BAccel * About(BAccel.TransformPosition(FVector(0, 0, -11.f)), FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-InThrottle * 14.f)));
	BrakeNow = FTransform(BrakePin) * About(BrakePin, FQuat(FVector::ForwardVector, FMath::DegreesToRadians(InBrake * 16.f)));
	if (GSteeringWheel >= 0) Model->MoveGroup(GSteeringWheel, WheelNow);
	if (GAccel >= 0) Model->MoveGroup(GAccel, AccelNow);
	if (GBrake >= 0) Model->MoveGroup(GBrake, BrakeNow);
	UpdateGauges(Dt);
	// the driver's door: swung open as you reach for it, pulled shut behind you (with their sounds)
	if (DoorK != DoorTarget)
	{
		const float Was = DoorK;
		DoorK = FMath::FInterpConstantTo(DoorK, DoorTarget, Dt, DoorTarget > DoorK ? 2.2f : 1.9f);
		const FVector DoorAt = BlenderToRoot(-1.55f, 4.35f, 1.5f);
		if (Was == 0.f && DoorK > 0.f) Sound(DoorOpenPcm(), DoorAt, false);
		if (Was > 0.f && DoorK == 0.f) Sound(DoorShutPcm(), DoorAt, false);
		// opening: quick, then easing out; shutting: swung, fastest at the end
		const float A = DoorTarget > 0.f ? FMath::Sin(DoorK * HALF_PI) : DoorK * DoorK * (2.f - DoorK);
		if (GDoor[0] >= 0) Model->MoveGroup(GDoor[0], FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(DoorSign * 70.f * A)), HingeRoot[0]));
	}
	Model->FlushMoves();
}

#pragma region StationDoors
AFireStationDoors::AFireStationDoors()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool AFireStationDoors::bTestOpen = false;
TWeakObjectPtr<AFireStationDoors> AFireStationDoors::One;

void AFireStationDoors::Command(const FVector& At, bool bOpen)
{
	AFireStationDoors* D = One.Get();
	if (!D) return;
	int32 Best = INDEX_NONE; double BD = 1e12;
	for (int32 i = 0; i < D->Bays.Num(); ++i) if (D->Bays[i].Group >= 0) { const double Dd = FVector::DistSquared2D(D->Bays[i].World, At); if (Dd < BD) { BD = Dd; Best = i; } }
	if (Best != INDEX_NONE) { D->Bays[Best].bOpen = bOpen; ADP_LOG(TEXT("Fire station: bay door %d sent %s"), Best, bOpen ? TEXT("up") : TEXT("down")); }
}

void AFireStationDoors::CommandAll(bool bOpen)
{
	if (AFireStationDoors* D = One.Get()) for (FBay& B : D->Bays) B.bOpen = bOpen;
}

bool AFireStationDoors::InReach(const FVector& At)
{
	// the station's outline in plan metres (its walls and doors, as on the holographic map): the nearest of them
	TArray<FVector4f> Walls, Doors;
	ADPFireStation::MapOutline(Walls, Doors);
	Walls.Append(Doors);
	const FVector2D P(At.X / 100.0, -At.Y / 100.0);
	double Best = 1e9;
	for (const FVector4f& S : Walls)
	{
		const FVector2D A(S.X, S.Y), B(S.Z, S.W), AB = B - A;
		const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1e-6), 0.0, 1.0);
		Best = FMath::Min(Best, (A + AB * T - P).Size());
	}
	return Best <= 25.0;
}

void AFireStationDoors::Setup(ABabylonModel* S)
{
	Station = S;
	if (!S) return;
	const FTransform RX = S->GetModelRoot()->GetComponentTransform();
	int32 Found = 0;
	for (int32 I = 0; I < 5; ++I)
	{
		const float Bx = ADPFireStation::BayX(float(I));
		FBay B;
		B.Pivot = FVector(-Bx * 100.f, 0.f, 520.f);   // the leaf's top edge (the sketch: doors 5.2 m high on the front line)
		// the leaf, its window band, glass and bars: the sketch names them BayDoor<bay>_<n> (docs/fire-station/sketch/station.ts)
		const FString Prefix = FString::Printf(TEXT("BayDoor%d_"), I);
		B.Group = S->BindGroupWhere([&Prefix](const FString& Name, int32, const FVector&) { return Name.StartsWith(Prefix); }, FTransform(B.Pivot));
		B.World = RX.TransformPosition(FVector(-Bx * 100.f, 0.f, 200.f));
		Found += B.Group != INDEX_NONE;
		Bays.Add(B);
	}
	ADP_LOG(TEXT("Fire station: %d bay doors that open"), Found);
}

void AFireStationDoors::Sound(const FBay& B, const TArray<int16>& Pcm)
{
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
	W->SetSampleRate(SndRate); W->NumChannels = 1; W->Duration = Pcm.Num() / float(SndRate); W->bLooping = false;
	W->SoundGroup = SOUNDGROUP_Effects;
	W->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
	UGameplayStatics::SpawnSoundAtLocation(this, W, B.World + FVector(0, 0, 300.f), FRotator::ZeroRotator, 1.f, 1.f, 0.f, Heard);
}

void AFireStationDoors::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Station) return;
	if (!Heard)
	{
		Heard = NewObject<USoundAttenuation>(this);
		Heard->Attenuation.bAttenuate = true; Heard->Attenuation.bSpatialize = true;
		Heard->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Heard->Attenuation.AttenuationShapeExtents = FVector(400.f, 0, 0);
		Heard->Attenuation.FalloffDistance = 6000.f;
	}
	const FTransform SX = Station->GetModelRoot()->GetComponentTransform();
	bool bMoved = false;
	for (FBay& B : Bays)
	{
		if (B.Group < 0) continue;
		// open for a truck in this bay's own lane (owner, 2026-11-11: only the door in front of your truck, as soon as
		// you sit at the wheel; the others stay shut; as many open as there are trucks going), within 20 m of the door,
		// inside or out. Shut otherwise: shut when you start, shut as you walk or drive up.
		// (owner, 2026-11-14: never by themselves any more - all of them up or down by the button, O / D-pad right, within
		// 25 m of the station)
		One = this;
		const bool bWant = bTestOpen || B.bOpen;   // (owner, 2026-11-14: only by its own panel, and it stays)
		// never down onto anything (owner: not while a truck or anyone is under it): the door comes down only in its
		// opening (its top runs along the ceiling, high over the trucks), so anything across the opening holds it
		bool bBlocked = false;
		for (const TWeakObjectPtr<AFireTruck>& T : AFireTruck::All)
		{
			if (!T.IsValid()) continue;
			const FVector F = T->GetActorForwardVector() * 620.f;
			const FVector A = SX.InverseTransformPosition(T->GetActorLocation() - F), C = SX.InverseTransformPosition(T->GetActorLocation() + F);
			const float Mx = 0.5f * (A.X + C.X);
			if (FMath::Min(A.Y, C.Y) < 90.f && FMath::Max(A.Y, C.Y) > -90.f && FMath::Abs(Mx - B.Pivot.X) < 290.f + 160.f) { bBlocked = true; break; }
		}
		if (!bBlocked)
			for (TActorIterator<APawn> It(GetWorld()); It; ++It)
			{
				if (Cast<AFireTruck>(*It) || It->IsHidden()) continue;
				const FVector L = SX.InverseTransformPosition(It->GetActorLocation());
				if (FMath::Abs(L.Y) < 110.f && FMath::Abs(L.X - B.Pivot.X) < 330.f) { bBlocked = true; break; }
			}
		B.Hold = 0.f;
		// where it's going: up for a truck (and a few seconds after); down when nothing wants it, unless something is in
		// the opening (then it stops where it is, or stays up)
		const float Target = (bWant || B.Hold > 0.f) ? 1.f : (bBlocked ? B.K : 0.f);
		const int32 Want = Target > B.K + 1e-4f ? 1 : Target < B.K - 1e-4f ? -1 : 0;
		auto StopMotor = [&]()
		{
			Sound(B, GarageClunk(true));
			if (B.Hum.IsValid()) B.Hum->Stop();
			B.Hum = nullptr; B.Dir = 0;
		};
		if (B.Dir != 0 && Want != B.Dir) { StopMotor(); B.Pause = Want != 0 ? 1.0f : 0.f; }   // (changing its way: stops, about a second, then goes)   // stopped, or turning back: a hard stop first
		if (B.Pause > 0.f) { B.Pause -= Dt; continue; }
		if (Want != 0 && B.Dir == 0)
		{
			// the motor starts: its clunk, then its hum and the sections' rattle while it runs
			Sound(B, GarageClunk(false));
			USoundWaveProcedural* W = NewObject<USoundWaveProcedural>(this);
			W->SetSampleRate(SndRate); W->NumChannels = 1; W->Duration = INDEFINITELY_LOOPING_DURATION; W->bLooping = false;
			W->SoundGroup = SOUNDGROUP_Effects;
			TSharedPtr<FHumState> St = MakeShared<FHumState>();
			W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([St](USoundWaveProcedural* Wv, int32 Samples)
			{
				TArray<int16> Buf; HumFill(*St, Buf, FMath::Max(Samples, SndRate / 20));
				Wv->QueueAudio(reinterpret_cast<const uint8*>(Buf.GetData()), Buf.Num() * sizeof(int16));
			});
			B.Hum = UGameplayStatics::SpawnSoundAtLocation(this, W, B.World + FVector(0, 0, 300.f), FRotator::ZeroRotator, 0.8f, 1.f, 0.f, Heard, nullptr, false);
			B.Dir = Want;
		}
		if (B.Dir == 0) continue;
		// about 5 s from shut to open (a fire station's fast doors)
		B.K = FMath::Clamp(B.K + B.Dir * Dt / 5.f, 0.f, 1.f);
		if ((B.Dir > 0 && B.K >= Target) || (B.Dir < 0 && B.K <= Target)) { B.K = Target; StopMotor(); }
		// a sectional door: its foot rises in the opening, its top runs back along the ceiling track (it never swings
		// down into the hall)
		const float Phi = HALF_PI * B.K;
		Station->MoveGroup(B.Group, FTransform(FQuat(FVector::ForwardVector, -Phi), B.Pivot + FVector(0.f, 520.f * FMath::Sin(Phi), 0.f)));
		bMoved = true;
	}
	if (bMoved) Station->FlushMoves();
}
#pragma endregion
