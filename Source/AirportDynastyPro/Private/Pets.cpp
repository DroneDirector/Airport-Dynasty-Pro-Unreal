#include "Pets.h"
#include "PerfLog.h"
#include "ADPLog.h"
#include "TerminalPlan.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ADPPets
{
	static FString Spaced(const FString& Id)
	{
		FString S;
		for (int32 i = 0; i < Id.Len(); ++i)
		{
			if (i > 0 && FChar::IsUpper(Id[i]) && !FChar::IsUpper(Id[i - 1])) S.AppendChar(' ');
			S.AppendChar(Id[i]);
		}
		return S;
	}

	const TArray<FModel>& All()
	{
		ADP_PERF_SCOPE("the pets: finding every pet model", 4.f);
		static TArray<FModel> List;
		static bool bDone = false;
		if (bDone) return List;
		bDone = true;
		IAssetRegistry& R = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
#if WITH_EDITOR
		R.ScanPathsSynchronous({TEXT("/Game/Pets")}, false);   // (in the editor the registry may still be gathering)
#endif
		TArray<FAssetData> Found;
		R.GetAssetsByPath(FName(TEXT("/Game/Pets")), Found, true);
		for (const FAssetData& A : Found)
		{
			if (A.AssetClassPath != USkeletalMesh::StaticClass()->GetClassPathName()) continue;
			const FString Name = A.AssetName.ToString();
			if (!Name.StartsWith(TEXT("SK_"))) continue;
			TArray<FString> Parts; A.PackagePath.ToString().ParseIntoArray(Parts, TEXT("/"));   // Game, Pets, Dogs, <Name>
			if (Parts.Num() < 4) continue;
			FModel M;
			M.Folder = Parts[2];
			M.Kind = M.Folder == TEXT("Dogs") ? TEXT("dog") : M.Folder == TEXT("Cats") ? TEXT("cat") : FString();
			if (M.Kind.IsEmpty()) continue;
			M.Id = Name.Mid(3);
			M.Name = Spaced(M.Id);
			List.Add(M);
		}
		List.Sort([](const FModel& A, const FModel& B) { return A.Kind != B.Kind ? A.Kind > B.Kind : A.Name < B.Name; });   // dogs first
		FString Names; for (const FModel& M : List) Names += FString::Printf(TEXT(" %s/%s"), *M.Folder, *M.Id);
		ADP_LOG(TEXT("Pets: %d models found:%s"), List.Num(), *Names);
		return List;
	}

	const FModel* Find(const FString& Kind, const FString& Id)
	{
		const FModel* First = nullptr;
		for (const FModel& M : All())
		{
			if (M.Kind != Kind && M.Folder != Kind) continue;
			if (!First) First = &M;
			if (M.Id == Id) return &M;
		}
		return First;
	}

	FString ListJson()
	{
		FString Dogs, Cats;
		for (const FModel& M : All())
		{
			FString& Out = M.Kind == TEXT("dog") ? Dogs : Cats;
			Out += FString::Printf(TEXT("%s{\"id\":\"%s\",\"name\":\"%s\"}"), Out.IsEmpty() ? TEXT("") : TEXT(","), *M.Id, *M.Name);
		}
		return FString::Printf(TEXT("{\"dog\":[%s],\"cat\":[%s]}"), *Dogs, *Cats);
	}

	FString Saved()
	{
		FString S; GConfig->GetString(TEXT("ADP.Player"), TEXT("Pet"), S, GGameUserSettingsIni);
		return S == TEXT("none") ? FString() : S;
	}
	void Save(const FString& KindSlashId)
	{
		GConfig->SetString(TEXT("ADP.Player"), TEXT("Pet"), KindSlashId.IsEmpty() ? TEXT("none") : *KindSlashId, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	void PlaceHome(UWorld* W)
	{
		if (!W) return;
		for (TActorIterator<APet> It(W); It; ++It) if (It->ActorHasTag(TEXT("HomePet"))) It->Destroy();
		FString Folder, Id;
		if (!Saved().Split(TEXT("/"), &Folder, &Id)) { ADP_LOG(TEXT("Pets: you have no pet")); return; }
		const FModel* M = Find(Folder, Id);
		if (!M || M->Id != Id) { ADP_WARN(TEXT("Pets: your pet %s isn't in the game"), *Saved()); return; }
		// its home is your office (owner, 2026-11-26): it lies in its bed there. Only if your office is furnished (Offices.cpp
		// tags its model "OwnerOffice"; once, at loading)
		bool bOffice = false;
		for (TActorIterator<AActor> It(W); It && !bOffice; ++It) bOffice = It->ActorHasTag(TEXT("OwnerOffice"));
		if (!bOffice) { ADP_WARN(TEXT("Pets: your office isn't furnished - your %s stays away"), *M->Name); return; }
		// the bed's lie point (plan x, height, z in metres; the cushion's top) from the office's own points
		FVector Lie(121.f, 6.17f, 1.3f);
		{
			FString Text; TSharedPtr<FJsonObject> R;
			const FString F = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("owner-office-points.json");
			if ((FFileHelper::LoadFileToString(Text, *F) || FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data") / TEXT("owner-office-points.json"))))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), R) && R.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* V = nullptr;
				if (R->TryGetArrayField(TEXT("DB_LiePoint"), V) && V->Num() == 3) Lie = FVector((*V)[0]->AsNumber(), (*V)[1]->AsNumber(), (*V)[2]->AsNumber());
				else ADP_WARN(TEXT("Pets: owner-office-points.json has no DB_LiePoint - the bed's place from the model's script"));
			}
			else ADP_WARN(TEXT("Pets: owner-office-points.json not found - the bed's place from the model's script"));
		}
		const float FloorY = 6.01f;   // Floor 2 (Offices.cpp PlaceYours)
		const FVector BedAt = FTerminalPlan::ToWorld(Lie.X, Lie.Y, Lie.Z);
		APet* Pet = APet::Spawn(W, *M, BedAt, 0.f);
		if (!Pet) return;
		Pet->Tags.Add(TEXT("HomePet"));
		Pet->SetOffice(BedAt, FloorY * 100.f);
		ADP_LOG(TEXT("Pets: your %s lies in its bed in your office (plan %.2f, %.2f, %.2f)"), *M->Name, Lie.X, Lie.Y, Lie.Z);
	}
}

APet::APet()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(true);
	// a solid box round its body, for people only (their capsules): nobody walks through it; traces and cameras ignore it
	Solid = CreateDefaultSubobject<UBoxComponent>(TEXT("Solid"));
	Solid->SetupAttachment(Body);
	Solid->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Solid->SetCollisionObjectType(ECC_WorldDynamic);
	Solid->SetCollisionResponseToAllChannels(ECR_Ignore);
	Solid->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Solid->SetCanEverAffectNavigation(false);
	Solid->SetGenerateOverlapEvents(false);
}

APet* APet::Spawn(UWorld* W, const ADPPets::FModel& Model, const FVector& At, float Yaw)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Pets/%s/%s/SK_%s.SK_%s"), *Model.Folder, *Model.Id, *Model.Id, *Model.Id));
	if (!Mesh) { ADP_WARN(TEXT("Pets: no mesh for %s/%s"), *Model.Folder, *Model.Id); return nullptr; }
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APet* Pet = W->SpawnActor<APet>(At, FRotator(0.f, Yaw, 0.f), P);
	if (!Pet) return nullptr;
	Pet->M = Model;
	Pet->Body->SetSkeletalMesh(Mesh);
	Pet->Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Pet->Length = FMath::Max(30.f, Mesh->GetBounds().BoxExtent.Y * 2.f);
	// its walking pace from its own size: the moves were made at ~1.8 body lengths a second (Art/Pets/anim_pet.py),
	// shown slower here, an easy walk
	Pet->Speed = 1.8f * Pet->Length * Pet->WalkRate;
	{
		const FBoxSphereBounds B = Mesh->GetBounds();
		Pet->Solid->SetBoxExtent(FVector(FMath::Max(8.f, B.BoxExtent.X * 0.8f), FMath::Max(15.f, B.BoxExtent.Y * 0.8f), FMath::Max(10.f, B.BoxExtent.Z * 0.5f)));
		Pet->Solid->SetRelativeLocation(FVector(B.Origin.X, B.Origin.Y, FMath::Max(10.f, B.BoxExtent.Z * 0.5f)));
	}
	for (const TCHAR* Mv : {TEXT("Idle"), TEXT("WalkFast"), TEXT("SitDown"), TEXT("SitIdle"), TEXT("StandUp"), TEXT("LieDown"), TEXT("LieIdle"), TEXT("GetUp"), TEXT("Run"), TEXT("RollOver")})
		if (UAnimSequence* A = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Pets/%s/%s/A_%s_%s.A_%s_%s"), *Model.Folder, *Model.Id, *Model.Id, Mv, *Model.Id, Mv), nullptr, LOAD_Quiet | LOAD_NoWarn))
			Pet->Moves.Add(Mv, A);
	Pet->Play(TEXT("Idle"), true);
	return Pet;
}

void APet::Play(const TCHAR* Move, bool bLoop, float Rate)
{
	UAnimSequence* A = this->Move(Move);
	if (!A) { ADP_WARN(TEXT("Pets: %s has no move %s"), *M.Id, Move); return; }
	Body->PlayAnimation(A, bLoop);
	Body->SetPlayRate(Rate);
}

float APet::MoveLength(const TCHAR* Move) const
{
	UAnimSequence* A = this->Move(Move);
	return A ? A->GetPlayLength() : 1.f;
}

void APet::WalkTo(const FVector& To, bool bSit, float InEndYaw, bool bGone)
{
	Target = To; bSitAtEnd = bSit; EndYaw = InEndYaw; bGoneAtEnd = bGone; bLieAtEnd = false;
	SetActorHiddenInGame(false);
	if (State == EState::Lying || State == EState::LyingDown)
	{
		State = EState::GettingUp; StateT = 0.f; WaitFor = MoveLength(TEXT("GetUp"));
		Play(TEXT("GetUp"), false);
		return;
	}
	if (State == EState::Sitting || State == EState::SittingDown)
	{
		State = EState::StandingUp; StateT = 0.f; WaitFor = MoveLength(TEXT("StandUp"));
		Play(TEXT("StandUp"), false);
		return;
	}
	State = EState::Walking; StateT = 0.f;
	Play(TEXT("WalkFast"), true, WalkRate);
}

void APet::SitHere()
{
	if (State == EState::Sitting || State == EState::SittingDown) return;
	State = EState::SittingDown; StateT = 0.f; WaitFor = MoveLength(TEXT("SitDown"));
	Play(TEXT("SitDown"), false);
}

void APet::LieHere()
{
	if (State == EState::Lying || State == EState::LyingDown) return;
	State = EState::LyingDown; StateT = 0.f; WaitFor = MoveLength(TEXT("LieDown"));
	Play(TEXT("LieDown"), false);
}

void APet::SetHome(const FVector& Home, float Yaw, float InMaxAway)
{
	bHome = true; bGround = true; MaxAway = InMaxAway; HomeYaw = Yaw;
	HomeAt = FVector(Home.X, Home.Y, Ground(Home));
	SetActorLocationAndRotation(HomeAt, FRotator(0.f, Yaw, 0.f));
	State = EState::Sitting; Play(TEXT("SitIdle"), true);
	Leg = 0; NextWander = FParse::Param(FCommandLine::Get(), TEXT("PetWanderSoon")) ? 8.f : FMath::FRandRange(20.f, 60.f);
}

float APet::Ground(const FVector& At) const
{
	FHitResult H; FCollisionQueryParams Q(TEXT("PetGround"), true, this);
	if (GetWorld()->LineTraceSingleByChannel(H, At + FVector(0, 0, 150.f), At - FVector(0, 0, 400.f), ECC_Visibility, Q)) return H.ImpactPoint.Z;
	return At.Z;
}

bool APet::Clear(const FVector& From, const FVector& To) const
{
	// a ball the pet's width, at its chest's height, along the way: anything in it (a wall, a door, a pillar, a bench)
	// and the way is shut
	const float R = FMath::Clamp(Length * 0.18f, 10.f, 30.f);
	const FVector Up(0, 0, FMath::Max(R + 5.f, Length * 0.35f));
	FHitResult H; FCollisionQueryParams Q(TEXT("PetWay"), false, this);
	if (GetWorld()->SweepSingleByChannel(H, From + Up, To + Up, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(R), Q)) return false;
	// and ground all the way, no steps up or down bigger than a kerb
	const int32 N = FMath::Max(2, FMath::CeilToInt((To - From).Size2D() / 100.f));
	float Z = Ground(From);
	for (int32 i = 1; i <= N; ++i)
	{
		const float G = Ground(FMath::Lerp(From, To, float(i) / N));
		if (FMath::Abs(G - Z) > 25.f) return false;
		Z = G;
	}
	return true;
}

void APet::Arrived()
{
	if (bGoneAtEnd) { Destroy(); return; }
	State = EState::Turning; StateT = 0.f;
}

void APet::Tick(float Dt)
{
	ADP_PERF_SCOPE("Pet", 0.5f);
	Super::Tick(Dt);
	StateT += Dt;
	switch (State)
	{
	case EState::GettingUp:
	case EState::StandingUp:
		if (StateT >= WaitFor) { State = EState::Walking; StateT = 0.f; Play(TEXT("WalkFast"), true, WalkRate); }
		break;
	case EState::SittingDown:
		if (StateT >= WaitFor) { State = EState::Sitting; StateT = 0.f; Play(TEXT("SitIdle"), true); }
		break;
	case EState::LyingDown:
		if (StateT >= WaitFor) { State = EState::Lying; StateT = 0.f; Play(TEXT("LieIdle"), true); }
		break;
	case EState::Walking:
	{
		FVector At = GetActorLocation();
		FVector D = Target - At; D.Z = 0.f;
		const float Dist = D.Size();
		if (Dist < 5.f) { Arrived(); break; }
		// turn towards the mark (its own front is +y: heading = yaw + 90), walk once roughly facing it
		const float Want = D.Rotation().Yaw - 90.f;
		const float Yaw = GetActorRotation().Yaw;
		const float Diff = FMath::FindDeltaAngleDegrees(Yaw, Want);
		SetActorRotation(FRotator(0.f, Yaw + FMath::Clamp(Diff, -200.f * Dt, 200.f * Dt), 0.f));
		if (FMath::Abs(Diff) < 50.f)
		{
			const float Step = FMath::Min(Dist, Speed * Dt * (1.f - FMath::Abs(Diff) / 70.f));
			FVector Next = At + D.GetSafeNormal() * Step;
			if (bGround)
			{
				// looking ahead: a wall or a step in the way and it stops there (and, out wandering, turns back)
				if (!Clear(At, At + D.GetSafeNormal() * FMath::Min(Dist, 60.f)))
				{
					Target = At; if (bHome && Leg == 1) EndYaw = Yaw;
					Arrived(); break;
				}
				Next.Z = FMath::FInterpTo(At.Z, Ground(Next), Dt, 12.f);
			}
			SetActorLocation(Next);
		}
		break;
	}
	case EState::Turning:
	{
		const float Yaw = GetActorRotation().Yaw;
		const float Diff = FMath::FindDeltaAngleDegrees(Yaw, EndYaw);
		if (FMath::Abs(Diff) > 2.f && StateT < 3.f) { SetActorRotation(FRotator(0.f, Yaw + FMath::Clamp(Diff, -150.f * Dt, 150.f * Dt), 0.f)); break; }
		if (bSitAtEnd) SitHere();
		else { State = EState::Standing; StateT = 0.f; Play(TEXT("Idle"), true); }
		break;
	}
	default: break;
	}

	if (bOffice) { OfficeTick(Dt); return; }
	if (!bHome) return;
	// at home: a short walk now and then (owner, 2026-11-15: "not walk further than 10 meters in either direction and
	// then back to the spot where he was ... aware of his surroundings so he doesn't clip into walls")
	if (Leg == 0 && State == EState::Sitting && (NextWander -= Dt) <= 0.f)
	{
		for (int32 Try = 0; Try < 16; ++Try)
		{
			const float H = FMath::FRandRange(0.f, 360.f), Away = FMath::FRandRange(300.f, MaxAway);
			FVector To = HomeAt + FRotator(0.f, H, 0.f).Vector() * Away;
			To.Z = Ground(To);
			if (!Clear(HomeAt, To)) continue;
			Leg = 1; WalkTo(To, false, H - 90.f);
			break;
		}
		if (Leg == 0) NextWander = FMath::FRandRange(15.f, 40.f);
	}
	else if (Leg == 1 && State == EState::Standing) { Leg = 2; StateT = 0.f; WaitFor = FMath::FRandRange(3.f, 8.f); }
	else if (Leg == 2 && State == EState::Standing && StateT >= WaitFor) { Leg = 3; WalkTo(HomeAt, true, HomeYaw); }
	else if (Leg == 3 && State == EState::Sitting) { Leg = 0; NextWander = FMath::FRandRange(25.f, 70.f); }
}

UAnimSequence* APet::Move(const TCHAR* Name) const
{
	const TObjectPtr<UAnimSequence>* A = Moves.Find(Name);
	return A ? A->Get() : nullptr;
}

// -- Its life in your office (owner, 2026-11-26) --
// Room metres: x = plan x - 114 (the Grand Hall's glass at 0, its guard 0.36 in), y = plan z (the forecourt windows at
// 0, the corridor wall with the door at 9); 12.3 x 9. The furniture's footprints from Art/OwnerOffice/make_owneroffice.py
// (min x, min y, max x, max y); its ways keep its own clearance from each and from the walls and the door's swing.
namespace
{
	struct FBox2 { float X0, Y0, X1, Y1; const TCHAR* Name; };
	const FBox2 GOfficeThings[] = {
		{1.4f, 0.375f, 5.0f, 0.825f, TEXT("sideboard")}, {7.9f, 0.44f, 8.5f, 0.76f, TEXT("bowls")}, {6.04f, 1.79f, 6.26f, 2.01f, TEXT("football")},
		{4.75f, 3.3f, 5.65f, 5.3f, TEXT("desk")}, {5.8f, 3.85f, 6.5f, 4.55f, TEXT("your chair")}, {3.9f, 3.95f, 4.6f, 4.65f, TEXT("visitor's chair")},
		{11.27f, 3.1f, 12.3f, 5.5f, TEXT("sofa")}, {9.97f, 3.85f, 10.87f, 4.75f, TEXT("coffee table")}, {8.77f, 3.87f, 9.63f, 4.73f, TEXT("armchair")},
		{1.1f, 5.7f, 3.7f, 8.3f, TEXT("meeting table")}, {0.55f, 8.15f, 1.15f, 8.75f, TEXT("big plant")}, {11.35f, 0.65f, 11.85f, 1.15f, TEXT("corner plant")},
		{11.0f, 6.1f, 12.3f, 7.3f, TEXT("wardrobe")}, {9.9f, 7.8f, 11.1f, 9.0f, TEXT("door")}, {6.42f, 0.72f, 7.58f, 1.88f, TEXT("its bed")}};
	const float GRoomX0 = 0.36f, GRoomX1 = 12.3f, GRoomY0 = 0.f, GRoomY1 = 9.f;
	const FVector2D GBedExit(7.0f, 2.5f);
	// open floor: the corners of its ways
	const FVector2D GOfficeNodes[] = {{7.0f, 2.5f}, {7.6f, 2.6f}, {4.0f, 2.2f}, {5.3f, 1.9f}, {1.0f, 2.5f}, {1.0f, 4.6f}, {7.6f, 5.1f}, {7.6f, 6.6f},
		{5.0f, 6.6f}, {9.6f, 6.0f}, {10.1f, 2.6f}};

	bool SegHitsBox(const FVector2D& A, const FVector2D& B, const FBox2& Bx, float C)
	{
		// the slab test against the box grown by C
		float T0 = 0.f, T1 = 1.f;
		const FVector2D D = B - A;
		const float Lo[2] = {Bx.X0 - C, Bx.Y0 - C}, Hi[2] = {Bx.X1 + C, Bx.Y1 + C}, P[2] = {float(A.X), float(A.Y)}, V[2] = {float(D.X), float(D.Y)};
		for (int32 i = 0; i < 2; ++i)
		{
			if (FMath::Abs(V[i]) < 1e-6f) { if (P[i] < Lo[i] || P[i] > Hi[i]) return false; continue; }
			float Ta = (Lo[i] - P[i]) / V[i], Tb = (Hi[i] - P[i]) / V[i];
			if (Ta > Tb) Swap(Ta, Tb);
			T0 = FMath::Max(T0, Ta); T1 = FMath::Min(T1, Tb);
			if (T0 > T1) return false;
		}
		return true;
	}
	// a room direction's yaw for the pet (its front is its +y; room (dx, dy) is the world's (dx, -dy))
	float YawFacing(const FVector2D& D) { return FMath::RadiansToDegrees(FMath::Atan2(-D.Y, D.X)) - 90.f; }
}

FVector APet::OfficeWorld(const FVector2D& P) const
{
	return FVector((114.f + P.X) * 100.f, -P.Y * 100.f, FloorZ);
}

FVector2D APet::OfficeRoom(const FVector& W) const
{
	return FVector2D(W.X / 100.f - 114.f, -W.Y / 100.f);
}

bool APet::OfficePointFree(const FVector2D& P) const
{
	if (P.X < GRoomX0 + Clearance || P.X > GRoomX1 - Clearance || P.Y < GRoomY0 + Clearance || P.Y > GRoomY1 - Clearance) return false;
	for (const FBox2& B : GOfficeThings)
		if (P.X > B.X0 - Clearance && P.X < B.X1 + Clearance && P.Y > B.Y0 - Clearance && P.Y < B.Y1 + Clearance) return false;
	return true;
}

bool APet::OfficeFree(const FVector2D& A, const FVector2D& B) const
{
	if (!OfficePointFree(A) || !OfficePointFree(B)) return false;   // (the room is a box: both ends in it, the way is)
	for (const FBox2& Bx : GOfficeThings) if (SegHitsBox(A, B, Bx, Clearance)) return false;
	return true;
}

bool APet::PersonAhead(const FVector& At, const FVector& Dir) const
{
	// a person's capsule just ahead of its nose (you, anyone walking in the office): one small query
	const FVector C = At + Dir * (Length * 0.5f + 35.f) + FVector(0, 0, 50.f);
	FCollisionQueryParams Q(TEXT("PetPeople"), false, this);
	return GetWorld()->OverlapAnyTestByObjectType(C, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(35.f), Q);
}

bool APet::OfficeGo(const FVector2D& To, EEnd End, float FaceYaw)
{
	const FVector2D From = OfficeRoom(GetActorLocation());
	// out of its bed (and into it) by the bed's front: the only way it crosses its own bed's footprint
	const bool bFromBed = FVector2D::Distance(From, Bed) < 0.6f, bToBed = FVector2D::Distance(To, Bed) < 0.05f;
	const FVector2D S = bFromBed ? GBedExit : From, G = bToBed ? GBedExit : To;
	TArray<FVector2D> Way;
	if (S.Equals(G, 0.05f)) Way.Add(G);
	else if (OfficeFree(S, G)) Way.Add(G);
	else
	{
		// the shortest way over the open floor's corners (a dozen points: Dijkstra on the spot, no world query)
		TArray<FVector2D> P; P.Add(S); for (const FVector2D& N : GOfficeNodes) P.Add(N); P.Add(G);
		const int32 Num = P.Num(), Goal = Num - 1;
		TArray<float> Dist; Dist.Init(1e9f, Num); TArray<int32> Prev; Prev.Init(-1, Num); TArray<bool> Done; Done.Init(false, Num);
		Dist[0] = 0.f;
		for (int32 It = 0; It < Num; ++It)
		{
			int32 U = -1;
			for (int32 i = 0; i < Num; ++i) if (!Done[i] && Dist[i] < 1e8f && (U < 0 || Dist[i] < Dist[U])) U = i;
			if (U < 0 || U == Goal) break;
			Done[U] = true;
			for (int32 V = 0; V < Num; ++V)
			{
				if (Done[V] || V == U) continue;
				const float L = FVector2D::Distance(P[U], P[V]);
				if (Dist[U] + L < Dist[V] && OfficeFree(P[U], P[V])) { Dist[V] = Dist[U] + L; Prev[V] = U; }
			}
		}
		if (Prev[Goal] < 0) return false;
		for (int32 i = Goal; i > 0; i = Prev[i]) Way.Insert(P[i], 0);
	}
	if (bFromBed && !S.Equals(From, 0.05f)) Way.Insert(S, 0);
	if (bToBed) Way.Add(Bed);
	Route = Way;
	const FVector2D First = Route[0]; Route.RemoveAt(0);
	WalkTo(OfficeWorld(First), End == EEnd::Sit, FaceYaw);
	bLieAtEnd = End == EEnd::Lie;
	BlockedT = 0.f; bBlocked = false; PeopleT = 0.f;
	return true;
}

void APet::SetOffice(const FVector& BedAt, float InFloorZ)
{
	bOffice = true; bHome = false; bGround = false;
	Bed = OfficeRoom(BedAt); BedZ = BedAt.Z; FloorZ = InFloorZ;
	Clearance = FMath::Clamp(Length / 200.f + 0.05f, 0.25f, 0.45f);
	// the floor checked once, on open floor in front of the bed (the model's floor is Floor 2's)
	{
		const FVector F = OfficeWorld(GBedExit);
		FHitResult H; FCollisionQueryParams Q(TEXT("PetFloor"), true, this);
		if (GetWorld()->LineTraceSingleByChannel(H, F + FVector(0, 0, 100.f), F - FVector(0, 0, 200.f), ECC_Visibility, Q) && FMath::Abs(H.ImpactPoint.Z - FloorZ) < 30.f) FloorZ = H.ImpactPoint.Z;
		ADP_LOG(TEXT("Pets: office floor at z %.0f (%s), its clearance %.2f m"), FloorZ, H.bBlockingHit ? *GetNameSafe(H.GetActor()) : TEXT("nothing found - Floor 2's height"), Clearance);
	}
	SetActorLocationAndRotation(BedAt, FRotator(0.f, YawFacing(FVector2D(0.f, 1.f)), 0.f));   // looking into the room
	State = EState::Lying; StateT = 0.f; Play(TEXT("LieIdle"), true);
	Errand = 0; ErrandT = 0.f; Route.Reset();
	StayFor = FParse::Param(FCommandLine::Get(), TEXT("PetWanderSoon")) ? 8.f : FMath::FRandRange(20.f, 60.f);
}

void APet::OfficeTick(float Dt)
{
	ErrandT += Dt;
	const bool bWalking = State == EState::Walking || State == EState::Turning || State == EState::GettingUp || State == EState::StandingUp;
	if (bWalking)
	{
		// someone stands in its way for long: it gives up the errand and goes back to its bed (another way if there is one)
		if (BlockedT > 8.f)
		{
			ADP_LOG(TEXT("Pets: %s waited 8 s for someone in its way - back to its bed"), *M.Id);
			BlockedT = 0.f;
			const FVector2D Here = OfficeRoom(GetActorLocation());
			if (!OfficeGo(Bed, EEnd::Lie, YawFacing(FVector2D(0.f, 1.f)))) ADP_WARN(TEXT("Pets: %s finds no way back to its bed from room (%.1f, %.1f)"), *M.Id, Here.X, Here.Y);
			Errand = 3;
		}
		return;
	}
	const bool bSettled = State == EState::Standing || State == EState::Sitting || State == EState::Lying;
	if (!bSettled) return;

	auto GoOut = [this]() -> bool
	{
		struct FSpot { FVector2D P, Away, Face; EEnd End; float Min, Max; const TCHAR* What; };
		TArray<FSpot> Spots = {
			{FVector2D(8.2f, 0.76f + Clearance + 0.05f), FVector2D(0, 1), FVector2D(0, -1), EEnd::Stand, 5.f, 10.f, TEXT("its bowls")},
			{FVector2D(5.72f, Clearance + 0.05f), FVector2D(0, 1), FVector2D(0, -1), EEnd::Sit, 10.f, 25.f, TEXT("the forecourt window")},
			{FVector2D(GRoomX0 + Clearance + 0.05f, 3.5f), FVector2D(1, 0), FVector2D(-1, 0), EEnd::Sit, 10.f, 25.f, TEXT("the Grand Hall's glass")},
			{FVector2D(4.0f, 3.0f), FVector2D(0, -1), FVector2D(1, 0.4f), EEnd::Lie, 20.f, 45.f, TEXT("the rug")},
			{FVector2D(7.1f, 4.3f), FVector2D(1, 0), FVector2D(-1, 0), EEnd::Lie, 20.f, 50.f, TEXT("beside your chair")},
			{FVector2D(7.0f, 7.3f), FVector2D(0, -1), FVector2D(-1, 0.3f), EEnd::Stand, 4.f, 8.f, TEXT("a sniff at the back of the room")},
			{FVector2D(10.6f, 0.95f), FVector2D(-1, 0), FVector2D(1, 0), EEnd::Stand, 4.f, 8.f, TEXT("a sniff at the corner plant")},
			{FVector2D(5.4f, 2.6f), FVector2D(-1, 0.5f), FVector2D(0.75f, -0.7f), EEnd::Sit, 6.f, 12.f, TEXT("its football")},
			{FVector2D(10.4f, 2.9f), FVector2D(0, -1), FVector2D(1, 0.5f), EEnd::Stand, 4.f, 8.f, TEXT("a sniff at the sofa")}};
		for (int32 i = 0; i < Spots.Num(); ++i) Spots.Swap(i, FMath::RandRange(i, Spots.Num() - 1));
		// you in your office: it comes to you now and then (sits a step away, looking at you), first of all
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (const APawn* You = PC->GetPawn())
			{
				const FVector2D R = OfficeRoom(You->GetActorLocation()), Me = OfficeRoom(GetActorLocation());
				if (R.X > GRoomX0 && R.X < GRoomX1 && R.Y > GRoomY0 && R.Y < GRoomY1 && FMath::Abs(You->GetActorLocation().Z - FloorZ) < 250.f && FMath::FRand() < 0.6f)
				{
					const FVector2D Away = (Me - R).IsNearlyZero() ? FVector2D(1, 0) : (Me - R).GetSafeNormal();
					Spots.Insert({R + Away * (Clearance + 0.55f), Away, -Away, EEnd::Sit, 8.f, 20.f, TEXT("you")}, 0);
				}
			}
		for (const FSpot& S : Spots)
		{
			FVector2D P = S.P;
			int32 k = 0;
			while (k < 7 && !OfficePointFree(P)) { P += S.Away.GetSafeNormal() * 0.1f; ++k; }
			if (k == 7) continue;
			if (OfficeGo(P, S.End, YawFacing(S.Face)))
			{
				StayFor = FMath::FRandRange(S.Min, S.Max);
				ADP_LOG(TEXT("Pets: %s gets up - to %s (room %.1f, %.1f)"), *M.Id, S.What, P.X, P.Y);
				return true;
			}
			ADP_LOG(TEXT("Pets: %s finds no way to %s"), *M.Id, S.What);
		}
		return false;
	};

	switch (Errand)
	{
	case 0:   // in its bed
		if (ErrandT >= StayFor) { ErrandT = 0.f; if (GoOut()) Errand = 1; else StayFor = 30.f; }
		break;
	case 1:   // arrived where it went
		Errand = 2; ErrandT = 0.f;
		break;
	case 2:   // there for a while; then now and then another errand, or back to its bed
		if (ErrandT >= StayFor)
		{
			ErrandT = 0.f;
			if (FMath::FRand() < 0.3f && GoOut()) { Errand = 1; break; }
			if (OfficeGo(Bed, EEnd::Lie, YawFacing(FVector2D(0.f, 1.f)))) { Errand = 3; ADP_LOG(TEXT("Pets: %s goes back to its bed"), *M.Id); }
			else { StayFor = 10.f; ADP_WARN(TEXT("Pets: %s finds no way back to its bed"), *M.Id); }
		}
		break;
	default:  // back in its bed (or, after waiting for someone, wherever it stopped)
		if (State == EState::Lying && FVector2D::Distance(OfficeRoom(GetActorLocation()), Bed) < 0.3f)
		{ Errand = 0; ErrandT = 0.f; StayFor = FParse::Param(FCommandLine::Get(), TEXT("PetWanderSoon")) ? 15.f : FMath::FRandRange(40.f, 150.f); }
		else { Errand = 2; ErrandT = 0.f; StayFor = 2.f; }
		break;
	}
}
