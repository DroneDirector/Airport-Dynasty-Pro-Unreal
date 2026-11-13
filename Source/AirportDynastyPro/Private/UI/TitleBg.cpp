// The title screen's film drawn by Unreal (TitleBg.h). The shots, cameras, lights and timing are splash.ts's own.
#include "UI/TitleBg.h"
#include "ADPLog.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Rendering/DrawElements.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformTime.h"

namespace
{
	// splash.ts: the music's tempo and the film's shots (picture, kind, beats, camera from a to b: centre x, y, width)
	constexpr float TS_BPM = 101.63f, TS_OFF = 0.322f, TS_BEAT = 60.f / TS_BPM, TS_DUR = 257.16f;
	struct FShot { const TCHAR* Img; TCHAR Kind; float Beats; float A[3], B[3]; };
	const FShot Shots[] = {
		{TEXT("fly"), 'f', 16, {700, 384, 1408}, {720, 380, 1220}},
		{TEXT("fly"), 'd', 8, {1128, 262, 360}, {1100, 252, 300}},
		{TEXT("t1"), 'd', 8, {660, 488, 420}, {722, 492, 340}},
		{TEXT("t1"), 'd', 8, {1040, 528, 300}, {1028, 510, 250}},
		{TEXT("t1"), 'w', 16, {704, 384, 1300}, {905, 520, 880}},
		{TEXT("t2"), 'd', 8, {600, 530, 340}, {548, 518, 280}},
		{TEXT("t2"), 'd', 8, {900, 470, 330}, {860, 462, 270}},
		{TEXT("t2"), 'w', 16, {800, 404, 1340}, {620, 450, 1000}},
		{TEXT("t3"), 'c', 16, {792, 336, 1584}, {792, 322, 1420}},
		{TEXT("fly"), 'd', 8, {690, 492, 340}, {740, 488, 290}},
		{TEXT("fly"), 'd', 8, {300, 372, 420}, {350, 366, 360}},
		{TEXT("fly"), 'd', 8, {1215, 425, 320}, {1245, 418, 260}},
		{TEXT("fly"), 'f', 16, {740, 384, 1340}, {690, 380, 1150}},
		{TEXT("t5"), 'w', 16, {688, 384, 1300}, {722, 330, 760}},
	};
	constexpr int32 NumShots = UE_ARRAY_COUNT(Shots);
	float RoundBeats() { float B = 0; for (const FShot& S : Shots) B += S.Beats; return B; }
	float Ease(float K) { K = FMath::Clamp(K, 0.f, 1.f); return K * K * (3.f - 2.f * K); }

	bool bFilmOn = false, bFilmReady = false;
	float ClockT = 0.f; double ClockAt = 0.0;
	float Now()
	{
		const float T = ClockT + float(FPlatformTime::Seconds() - ClockAt);
		return FMath::Fmod(FMath::Max(T, 0.f), TS_DUR);
	}
}

void ADPTitleBg::Clock(bool bNowOn, float Seconds)
{
	if (bNowOn != bFilmOn) ADP_LOG(TEXT("Title film: %s (Unreal draws it)"), bNowOn ? TEXT("on") : TEXT("off"));
	bFilmOn = bNowOn;
	// the page's music clock: taken when it drifts (the page sends it a few times a second)
	if (FMath::Abs(Now() - Seconds) > 0.08f && FMath::Abs(Now() - Seconds) < TS_DUR - 0.08f) { ClockT = Seconds; ClockAt = FPlatformTime::Seconds(); }
}
bool ADPTitleBg::IsOn() { return bFilmOn; }
bool ADPTitleBg::Ready() { return bFilmReady; }

void STitleBg::Construct(const FArguments& Args)
{
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Title/M_TitleBg.M_TitleBg"));
	if (!M) { ADP_WARN(TEXT("Title film: M_TitleBg is missing (Tools/set_title_bg.py); the page draws it")); return; }
	Mid.Reset(UMaterialInstanceDynamic::Create(M, GetTransientPackage()));
	for (const TCHAR* N : {TEXT("t1"), TEXT("t2"), TEXT("t3o"), TEXT("t3i"), TEXT("t4a"), TEXT("t4b"), TEXT("t5")})
		for (const TCHAR* Sfx : {TEXT(""), TEXT("_blur")})
		{
			const FString Key = FString(N) + Sfx;
			if (UTexture* T = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Title/T_Title_%s.T_Title_%s"), *Key, *Key))) Tex.Add(Key, TStrongObjectPtr<UTexture>(T));
			else { ADP_WARN(TEXT("Title film: picture %s missing; the page draws it"), *Key); Mid.Reset(); return; }
		}
	Brush.SetResourceObject(Mid.Get());
	Brush.ImageSize = FVector2D(1920, 1080);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	// the music's loudness and kick (the page's own file: 25 values a second)
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Web/assets/title/env.json")))
		|| FFileHelper::LoadFileToString(Json, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Airline-Manager-Pro-Babylon/www/assets/title/env.json"))))
	{
		TSharedPtr<FJsonObject> O;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) && O.IsValid())
		{
			FBase64::Decode(O->GetStringField(TEXT("e")), Env);
			FBase64::Decode(O->GetStringField(TEXT("k")), Kick);
		}
	}
	bFilmReady = true;
	ADP_LOG(TEXT("Title film: ready (%d pictures, beat levels %d)"), Tex.Num(), Env.Num());
}

STitleBg::~STitleBg() {}

float STitleBg::Level(const TArray<uint8>& A, float T) const
{
	if (!A.Num()) return 0.5f;
	return A[FMath::Clamp(FMath::FloorToInt(T * 25.f), 0, A.Num() - 1)] / 255.f;
}

void STitleBg::Frame(float W, float H, float Dt) const
{
	UMaterialInstanceDynamic* M = Mid.Get();
	static float LastT = 0.f;
	const float t = Now();
	if (t + 1.f < LastT) Flash = 1.f;   // the music starts over: a flash
	LastT = t;
	const float Beat = (t - TS_OFF) / TS_BEAT, kick = Level(Kick, t), lvl = Level(Env, t);
	auto V4 = [M](const TCHAR* N, float X, float Y, float Z, float A) { M->SetVectorParameterValue(N, FLinearColor(X, Y, Z, A)); };
	auto NoLights = [&]() { V4(TEXT("L0"), 0, 0, 0, 0); V4(TEXT("L1"), 0, 0, 0, 0); V4(TEXT("L2"), 0, 0, 0, 0); };
	float Mode = -1.f, iw = 1408, ih = 768, s = 1, ox = 0, oy = 0, rk = -1, flip = 0;
	FLinearColor P4(0, 0, 0, 0);
	NoLights();
	if (Beat >= 0.f)
	{
		const float RB = RoundBeats();
		const int32 Round = FMath::FloorToInt(Beat / RB);
		float b = Beat - Round * RB; int32 i = 0;
		while (i < NumShots - 1 && b >= Shots[i].Beats) { b -= Shots[i].Beats; ++i; }
		const FShot& Sh = Shots[i];
		const float k = FMath::Min(1.f, b / Sh.Beats);
		flip = Round % 2 == 1 ? 1.f : 0.f;
		const int32 Id = Round * 100 + i;
		if (Id != LastShot) { LastShot = Id; if (Sh.Kind != 'd') Flash = FMath::Max(Flash, 0.55f + kick * 0.4f); Glitch = 1.f; }
		const float e = flip > 0.5f ? 1.f - Ease(k) : Ease(k);
		const float v[3] = {Sh.A[0] + (Sh.B[0] - Sh.A[0]) * e, Sh.A[1] + (Sh.B[1] - Sh.A[1]) * e, Sh.A[2] + (Sh.B[2] - Sh.A[2]) * e};
		const FString Img(Sh.Img);
		const bool bFly = Img == TEXT("fly"), bT3 = Img == TEXT("t3");
		const FString Main = bFly ? TEXT("t4b") : bT3 ? TEXT("t3i") : Img;
		UTexture* Tm = Tex.FindRef(Main).Get();
		iw = Tm ? Tm->GetSurfaceWidth() : 1408.f; ih = Tm ? Tm->GetSurfaceHeight() : 768.f;
		M->SetTextureParameterValue(TEXT("Img"), Tm);
		M->SetTextureParameterValue(TEXT("Img2"), Tex.FindRef(bFly ? TEXT("t4a") : bT3 ? TEXT("t3o") : *Main).Get());
		M->SetTextureParameterValue(TEXT("Blur"), Tex.FindRef(Main + TEXT("_blur")).Get());
		M->SetTextureParameterValue(TEXT("Blur2"), Tex.FindRef(bFly ? TEXT("t4a_blur") : *(Main + TEXT("_blur"))).Get());
		// the camera: the picture's part in view, a little off-centre for the closer shots
		const float vw = FMath::Min3(v[2], iw, ih * W / H), vh = vw * H / W;
		const float Side = Sh.Kind == 'd' ? 0.14f : Sh.Kind == 'c' ? 0.f : 0.06f;
		const float cx = FMath::Max(vw / 2, FMath::Min(iw - vw / 2, v[0] - vw * Side)), cy = FMath::Max(vh / 2, FMath::Min(ih - vh / 2, v[1]));
		s = W / vw; ox = W / 2 - cx * s; oy = H / 2 - cy * s;
		Mode = bT3 ? 1.f : bFly ? 2.f : 0.f;
		if (bT3) { const float pe = Ease(k); P4 = FLinearColor(70.f - 110.f * pe, 1.24f - 0.24f * pe, -0.012f * pe, FMath::Sin(t * 9.f) * 0.6f * (1.f - k)); }
		if (bFly) P4 = FLinearColor(-120.f + 240.f * k, FMath::Sin(t * 0.8f) * 4.f + FMath::Sin(t * 2.1f) * 1.2f, FMath::Sin(t * 0.55f) * 0.006f, 0.f);
		rk = Sh.Kind == 'd' ? k : -1.f;
		// the glints (titleLights)
		struct FL { float X, Y; FLinearColor C; float A; };
		TArray<FL> L;
		auto Col = [](int R, int G, int B) { return FLinearColor(R / 255.f, G / 255.f, B / 255.f); };
		if (Img == TEXT("t2"))
		{
			if (FMath::Fmod(t, 1.1f) < 0.12f) L.Add({748, 299, Col(255, 40, 40), 1.f});
			L.Add({228, 571, Col(255, 170, 40), 0.55f + 0.45f * FMath::Max(0.f, FMath::Sin(t * 7.f))});
		}
		if (bFly)
		{
			const float Ph = FMath::Fmod(t, 1.25f);
			if (Ph < 0.06f || (Ph > 0.14f && Ph < 0.2f)) L.Add({1206, 456, Col(255, 255, 255), 1.2f});
			if (FMath::Fmod(t, 1.05f) < 0.12f) L.Add({690, 312, Col(255, 50, 50), 0.9f});
			L.Add({-40, 120, Col(255, 190, 120), 0.6f + kick * 0.5f});
		}
		for (int32 j = 0; j < 3; ++j)
		{
			const TCHAR* LN[3] = {TEXT("L0"), TEXT("L1"), TEXT("L2")}; const TCHAR* CN[3] = {TEXT("LC0"), TEXT("LC1"), TEXT("LC2")};
			if (j < L.Num()) { V4(LN[j], L[j].X, L[j].Y, L[j].A, 0); M->SetVectorParameterValue(CN[j], L[j].C); }
		}
	}
	V4(TEXT("P0"), W, H, iw, ih);
	V4(TEXT("P1"), s, ox, oy, Mode);
	V4(TEXT("P2"), t, kick, lvl, rk);
	V4(TEXT("P3"), flip, Flash, Glitch, FMath::FRand() * 100.f);
	M->SetVectorParameterValue(TEXT("P4"), P4);
	// the flash and the glitch fade as the page's do (per frame at 60 a second)
	Flash *= FMath::Pow(0.84f, Dt * 60.f);
	Glitch *= FMath::Pow(0.7f, Dt * 60.f);
}

int32 STitleBg::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	if (!Mid.IsValid() || !bFilmOn) return Layer;
	const double T = FPlatformTime::Seconds();
	const float Dt = LastPaint > 0 ? float(FMath::Clamp(T - LastPaint, 0.0, 0.1)) : 1.f / 60.f;
	LastPaint = T;
	const FVector2D Px = Geo.GetAbsoluteSize();
	static bool bSaid = false; if (!bSaid) { bSaid = true; ADP_LOG(TEXT("Title film: first frame drawn, %.0f x %.0f px"), Px.X, Px.Y); }
	Frame(FMath::Max(1.f, float(Px.X)), FMath::Max(1.f, float(Px.Y)), Dt);
	FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, FLinearColor::White);
	return Layer + 1;
}
