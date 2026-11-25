// The remote stands (to-do 277): their pads, throats, the bus road, the bus depot and the bus bays, drawn from the "remote"
// block of Data/airfield.json. Every outline, line and light is worked out by Tools/make_remote_stands.py and proved by
// Tools/check_remote_stands.py; here they are only laid into the airfield's sheets, level with the terminal's apron. The
// pads' paved shoulders fall to the grass over their 3 m, the bus road ramps down onto the links over 10 m (the links lie
// with the rest of the airfield, 25 cm lower with the terminal's model).
#include "Airfield.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ADPLog.h"
#include "TerminalPlan.h"
#include "Components/TextRenderComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#pragma region Loading

namespace
{
	TArray<FVector2f> RemotePts(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		TArray<FVector2f> P;
		for (int32 i = 0; i + 1 < A.Num(); i += 2) P.Add(FVector2f(A[i]->AsNumber(), A[i + 1]->AsNumber()));
		return P;
	}
	float RemoteSegDist(const FVector2f& P, const FVector2f& A, const FVector2f& B)
	{
		const FVector2f AB = B - A;
		const float T = FMath::Clamp(FVector2f::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1e-6f), 0.f, 1.f);
		return (P - (A + AB * T)).Size();
	}
}

bool FAirfieldData::RemoteOn()
{
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("RemoteStands"));
	return bOn;
}

void FAirfieldData::StandLines(bool bCargo, TArray<FStandLine>& Lines, TArray<TPair<const FStand*, FBox2f>>& Zones) const
{
	constexpr float Wall = 82.6f;   // (the terminal's apron side: its stands' lines begin there)
	TArray<const FStand*> Row; for (const FStand& St : Stands) if (!St.bRemote && (bCargo || !St.bCargo)) Row.Add(&St);
	// the boundary lines as Airfield.cpp draws them (wingtips + 4.5 m, one shared line between close neighbours)
	Row.Sort([](const FStand& A, const FStand& B) { return A.Nose != B.Nose ? A.Nose < B.Nose : A.X < B.X; });
	TArray<float> Lf, Rt; for (const FStand* S : Row) { Lf.Add(S->X - S->Span / 2 - 4.5f); Rt.Add(S->X + S->Span / 2 + 4.5f); }
	for (int32 k = 0; k + 1 < Row.Num(); ++k)
		if (Row[k]->Nose == Row[k + 1]->Nose && Row[k]->Dir == Row[k + 1]->Dir && Rt[k] > Lf[k + 1] - 6.f) { const float Mid = (Row[k]->X + Row[k + 1]->X) / 2; Rt[k] = Mid; Lf[k + 1] = Mid; }
	for (int32 k = 0; k < Row.Num(); ++k)
	{
		const FStand& St = *Row[k]; const float B = -St.Dir, Head = St.Dir < 0 ? Wall : St.Nose, Te = St.Nose + B * (St.Len + 3.f);
		for (const float Bx : {Lf[k], Rt[k]}) Lines.Add({&St, FVector2f(Bx, Head), FVector2f(Bx, Te)});
		if (St.bJetway) Zones.Add(TPair<const FStand*, FBox2f>(&St, FBox2f(FVector2f(St.X + 3.f, FMath::Min(Wall, St.Nose + B * 9.f)), FVector2f(St.X + 16.f, FMath::Max(Wall, St.Nose + B * 9.f)))));
	}
	// a remote stand's: the two sides of its red restraint line (Tools/make_remote_stands.py; its head and tail ends are crossed)
	for (const FStand& St : Stands)
	{
		if (!St.bRemote) continue;
		for (const float k : {-1.f, 1.f})
		{
			float Room = 1e9f; for (const FStand& O : Stands) if (O.bRemote && (O.X - St.X) * k > 0.f) Room = FMath::Min(Room, (FMath::Abs(O.X - St.X) + St.Span / 2 - O.Span / 2) / 2);
			const float Bx = St.X + k * FMath::Min(St.Span / 2 + 3.f, Room - 0.35f);
			Lines.Add({&St, FVector2f(Bx, St.NoseEnd + St.Dir * 2.f), FVector2f(Bx, St.NoseEnd - St.Dir * (St.Len + 3.f))});
		}
	}
}

void FAirfieldData::LoadRemote(const TSharedPtr<FJsonObject>& Json)
{
	const TSharedPtr<FJsonObject>* R = nullptr;
	if (!Json->TryGetObjectField(TEXT("remote"), R)) return;
	const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
	double V = 0;
	if ((*R)->TryGetNumberField(TEXT("linkHalf"), V)) Remote.LinkHalf = float(V);
	if ((*R)->TryGetNumberField(TEXT("rampTo"), V)) Remote.RampTo = float(V);
	if ((*R)->TryGetArrayField(TEXT("linkX"), L)) for (const TSharedPtr<FJsonValue>& J : *L) Remote.LinkX.Add(float(J->AsNumber()));
	if ((*R)->TryGetArrayField(TEXT("polys"), L))
		for (const TSharedPtr<FJsonValue>& J : *L)
		{
			const TSharedPtr<FJsonObject> O = J->AsObject();
			FRemote::FPoly& P = Remote.Polys.AddDefaulted_GetRef();
			P.Mat = FName(*O->GetStringField(TEXT("mat")));
			O->TryGetBoolField(TEXT("ring"), P.bRing);
			P.Outer = RemotePts(O->GetArrayField(TEXT("outer")));
			P.Pts = RemotePts(O->GetArrayField(TEXT("pts")));
			if (!P.Pts.Num()) P.Pts = P.Outer;
			for (const TSharedPtr<FJsonValue>& T : O->GetArrayField(TEXT("tri"))) P.Tri.Add(int32(T->AsNumber()));
			for (const TSharedPtr<FJsonValue>& Q : O->GetArrayField(TEXT("runs"))) P.Runs.Add(RemotePts(Q->AsArray()));
			P.Min = P.Max = P.Outer.Num() ? P.Outer[0] : FVector2f::ZeroVector;
			for (const FVector2f& Q : P.Outer) { P.Min = FVector2f::Min(P.Min, Q); P.Max = FVector2f::Max(P.Max, Q); }
		}
	if ((*R)->TryGetArrayField(TEXT("paint"), L))
		for (const TSharedPtr<FJsonValue>& J : *L)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = J->AsArray();
			if (A.Num() >= 4) Remote.Paint.Add({FName(*A[0]->AsString()), float(A[1]->AsNumber()), float(A[2]->AsNumber()), RemotePts(A[3]->AsArray())});
		}
	if ((*R)->TryGetArrayField(TEXT("texts"), L))
		for (const TSharedPtr<FJsonValue>& J : *L)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = J->AsArray();
			if (A.Num() >= 6) Remote.Texts.Add({A[0]->AsString(), float(A[1]->AsNumber()), float(A[2]->AsNumber()), float(A[3]->AsNumber()), float(A[4]->AsNumber()), A[5]->AsString() == TEXT("white")});
		}
	if ((*R)->TryGetArrayField(TEXT("signs"), L))
		for (const TSharedPtr<FJsonValue>& J : *L)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = J->AsArray();
			if (A.Num() >= 3) Remote.Signs.Add({A[0]->AsString(), float(A[1]->AsNumber()), float(A[2]->AsNumber()), 0.f, 0.f, false});
		}
	if ((*R)->TryGetArrayField(TEXT("lights"), L))
		for (const TSharedPtr<FJsonValue>& J : *L) { const TArray<FVector2f> Q = RemotePts(J->AsArray()); if (Q.Num() == 2) Remote.Lights.Add({Q[0], Q[1]}); }
	if ((*R)->TryGetArrayField(TEXT("masts"), L))
		for (const TSharedPtr<FJsonValue>& J : *L) { const TArray<FVector2f> Q = RemotePts(J->AsArray()); if (Q.Num() == 1) Remote.Masts.Add(Q[0]); }
}

bool FAirfieldData::OnRemote(const FVector2f& P, float Margin) const
{
	for (const FRemote::FPoly& G : Remote.Polys)
	{
		if (P.X < G.Min.X - Margin || P.X > G.Max.X + Margin || P.Y < G.Min.Y - Margin || P.Y > G.Max.Y + Margin) continue;
		bool bIn = false;
		for (int32 i = 0, j = G.Outer.Num() - 1; i < G.Outer.Num(); j = i++)
		{
			const FVector2f &A = G.Outer[j], &B = G.Outer[i];
			if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X) bIn = !bIn;
			if (Margin > 0.f && RemoteSegDist(P, A, B) < Margin) return true;
		}
		if (bIn) return true;
	}
	return false;
}

#pragma endregion

#pragma region Laying it

void AAirfield::RemoteApron(bool bModelSite)
{
	const FAirfieldData::FRemote& R = D.Remote;
	if (!R.Polys.Num()) return;
	const float Base = bModelSite ? 0.02f - Sink : -0.04f;   // the terminal apron's level
	constexpr float Low = -0.045f, OnLink = -0.03f;          // the shoulders' outer edge; the bus road's foot on a link (asphalt -0.0225, shoulder -0.036)
	auto Level = [&](const FVector2f& P)
	{
		float T = 1.f;
		for (const float X : R.LinkX) T = FMath::Min(T, FMath::Clamp((FMath::Abs(P.X - X) - R.LinkHalf) / FMath::Max(R.RampTo - R.LinkHalf, 1.f), 0.f, 1.f));
		return FMath::Lerp(OnLink, Base, T);
	};
	// a pad's outer edge: down at the grass, up again where it meets the taxilane's apron (south) and the bus road (north)
	auto OuterLift = [&](const FAirfieldData::FRemote::FPoly& G, const FVector2f& P)
	{
		return G.bRing ? FMath::Lerp(Base, Low, FMath::SmoothStep(0.f, 3.f, FMath::Min(P.Y - G.Min.Y, G.Max.Y - P.Y))) : Level(P);
	};
	for (const FAirfieldData::FRemote::FPoly& G : R.Polys)
	{
		FSheet& S = Sheets.FindOrAdd(G.Mat);
		const int32 B0 = S.V.Num();
		for (const FVector2f& P : G.Pts) Vertex(S, P.X, P.Y, G.bRing ? Base : Level(P));
		for (int32 i = 0; i + 2 < G.Tri.Num(); i += 3) Tri(S, B0 + G.Tri[i], B0 + G.Tri[i + 1], B0 + G.Tri[i + 2]);
		// the paved shoulder between the concrete and the outline, and the double yellow line along the concrete's edge
		// (the sheets are looked up again for each piece: the map may move them when a new material comes)
		if (G.bRing && G.Pts.Num() == G.Outer.Num())
		{
			const int32 N = G.Pts.Num();
			for (int32 i = 0; i < N; ++i)
			{
				const int32 j = (i + 1) % N;
				const float Wi = (G.Pts[i] - G.Outer[i]).Size(), Wj = (G.Pts[j] - G.Outer[j]).Size();
				if (Wi < 0.05f && Wj < 0.05f) continue;
				FSheet& Sh = Sheets.FindOrAdd(TEXT("shoulder"));
				const int32 C0 = Sh.V.Num();
				for (const int32 k : {i, j}) { Vertex(Sh, G.Pts[k].X, G.Pts[k].Y, Base); Vertex(Sh, G.Outer[k].X, G.Outer[k].Y, OuterLift(G, G.Outer[k])); }
				if (Wi >= 0.05f) Tri(Sh, C0, C0 + 1, C0 + 3);
				if (Wj >= 0.05f) Tri(Sh, C0, C0 + 3, C0 + 2);
				const FVector2f U = (G.Pts[j] - G.Pts[i]).GetSafeNormal(), Nm(-U.Y, U.X);   // (counter-clockwise: the left is inside)
				for (const float In : {0.225f, 0.525f}) Band("paintyellow", G.Pts[i] + Nm * In, G.Pts[j] + Nm * In, 0.15f, Base + 0.0065f, 5.f);
			}
		}
		// the soft edge into the grass
		for (const TArray<FVector2f>& Run : G.Runs)
		{
			TArray<FVector2f> Out, Flags;
			TArray<float> Lift, Edge;
			for (int32 k = 0; k < Run.Num(); ++k)
			{
				const FVector2f U = (Run[FMath::Min(k + 1, Run.Num() - 1)] - Run[FMath::Max(k - 1, 0)]).GetSafeNormal();
				Out.Add(FVector2f(U.Y, -U.X));
				Flags.Add(FVector2f(G.Mat == TEXT("asphalt") ? 1.f : 0.f, 0.f));
				const float E = OuterLift(G, Run[k]);
				Edge.Add(E);
				Lift.Add(G.bRing ? FMath::Lerp(E, Base, 0.4f) : E);   // (the band's inner edge lies 1.2 m up the 3 m shoulder)
			}
			EdgePoly(Run, Out, Flags, Lift, true, &Edge);
		}
	}
	// ---- the paint: lines along their points, a little over the surface, riding the ramps
	auto Line = [&](const FName Mat, const TArray<FVector2f>& P, float W, float Up)
	{
		FSheet& S = Sheets.FindOrAdd(Mat);
		for (int32 i = 0; i + 1 < P.Num(); ++i)
		{
			const FVector2f U = (P[i + 1] - P[i]).GetSafeNormal(), Nm(-U.Y, U.X);
			const float L = (P[i + 1] - P[i]).Size();
			if (L < 0.01f) continue;
			const int32 N = FMath::Max(1, FMath::CeilToInt(L / 2.f)), B0 = S.V.Num();
			for (int32 k = 0; k <= N; ++k)
			{
				const FVector2f Q = P[i] + U * (L * k / N);
				for (const float s : {-0.5f, 0.5f}) { const FVector2f At = Q + Nm * (W * s); Vertex(S, At.X, At.Y, Level(At) + Up); }
			}
			for (int32 k = 0; k < N; ++k) { const int32 a = B0 + 2 * k; Tri(S, a, a + 1, a + 3); Tri(S, a, a + 3, a + 2); }
		}
	};
	for (const FAirfieldData::FRemote::FPaint& P : R.Paint)
	{
		if (P.Mat == TEXT("paintred")) Line("paintwhite", P.Pts, P.W + 0.2f, P.Up - 0.002f);   // (edged in white, as the gates' red lines)
		Line(P.Mat, P.Pts, P.W, P.Up);
	}
	auto Text = [&](const FString& T, const FVector& At, const FRotator& Rot, float H, const FColor& C)
	{
		UTextRenderComponent* Tx = NewObject<UTextRenderComponent>(this);
		Tx->SetupAttachment(Root);
		Tx->SetText(FText::FromString(T));
		Tx->SetHorizontalAlignment(EHTA_Center);
		Tx->SetVerticalAlignment(EVRTA_TextCenter);
		Tx->SetWorldSize(H * 100.f);
		Tx->SetTextRenderColor(C);
		Tx->SetRelativeLocationAndRotation(At, Rot);
		Tx->RegisterComponent();
		AddInstanceComponent(Tx);
	};
	const FColor Yellow(240, 200, 40), White(235, 235, 230);
	for (const FAirfieldData::FRemote::FText& T : R.Texts)
		Text(T.T, FTerminalPlan::ToWorld(T.X, GroundAt(T.X, T.Y) + Level(FVector2f(T.X, T.Y)) + 0.014f + Sink, T.Y), FRotator(90.f, T.Yaw + 180.f, 0.f), T.H, T.bWhite ? White : Yellow);
	// the stand signs on two legs behind the bus road, their faces to the pilots (south)
	for (const FAirfieldData::FRemote::FText& T : R.Signs)
	{
		const float W = 0.8f + 0.52f * T.T.Len(), G = GroundAt(T.X, T.Y);
		Slab("black", T.X - W / 2, T.Y - 0.06f, T.X + W / 2, T.Y + 0.06f, G + 1.3f, G + 2.5f);
		for (const float s : {-0.35f, 0.35f}) Slab("black", T.X + s * W - 0.05f, T.Y - 0.04f, T.X + s * W + 0.05f, T.Y + 0.04f, G - 0.5f, G + 1.3f);
		Text(T.T, FTerminalPlan::ToWorld(T.X, G + 1.9f + Sink, T.Y - 0.075f), FRotator(0.f, 90.f, 0.f), 0.85f, Yellow);
	}
	ADP_LOG(TEXT("Airfield: the remote stands laid: %d pieces, %d lines, %d texts, %d signs"), R.Polys.Num(), R.Paint.Num(), R.Texts.Num(), R.Signs.Num());
}

#pragma endregion
