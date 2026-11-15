#include "Pets.h"
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
		if (!Saved().Split(TEXT("/"), &Folder, &Id)) return;
		const FModel* M = Find(Folder, Id);
		if (!M || M->Id != Id) { ADP_WARN(TEXT("Pets: your pet %s isn't in the game"), *Saved()); return; }
		// for now just outside the terminal's main entrance (owner, 2026-11-15: "so I actually can see him"): beside its
		// doors, 3 m out on the pavement, facing out towards the kerb (a pet's own front is its +y). The terminal's real
		// front is found by looking in from the road (the built model reaches further out than the old floor plan)
		FVector At = FTerminalPlan::ToWorld(79.5f, 0.f, -3.f);
		{
			const FVector Road = FTerminalPlan::ToWorld(79.5f, 0.f, -60.f);
			FHitResult G; FCollisionQueryParams Q(TEXT("PetHomeFind"), true);
			float Z = 0.f;
			if (W->LineTraceSingleByChannel(G, Road + FVector(0, 0, 2000.f), Road - FVector(0, 0, 3000.f), ECC_Visibility, Q)) Z = G.ImpactPoint.Z;
			FHitResult H;
			const FVector From(Road.X, Road.Y, Z + 80.f), To(Road.X, Road.Y - 9000.f, Z + 80.f);
			if (W->LineTraceSingleByChannel(H, From, To, ECC_Visibility, Q)) At = FVector(H.ImpactPoint.X, H.ImpactPoint.Y + 300.f, Z);
			ADP_LOG(TEXT("Pets: the terminal's front at the main entrance found %s (y %.0f)"), H.bBlockingHit ? TEXT("by looking in from the road") : TEXT("nowhere - the old plan's place"), H.bBlockingHit ? H.ImpactPoint.Y : At.Y);
		}
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APet* Pet = APet::Spawn(W, *M, At, 0.f);
		if (!Pet) return;
		Pet->Tags.Add(TEXT("HomePet"));
		Pet->SetHome(At, 0.f, 1000.f);
		ADP_LOG(TEXT("Pets: your %s sits outside the terminal's main entrance"), *M->Name);
	}
}

APet::APet()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(true);
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
	Target = To; bSitAtEnd = bSit; EndYaw = InEndYaw; bGoneAtEnd = bGone;
	SetActorHiddenInGame(false);
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
	Super::Tick(Dt);
	StateT += Dt;
	switch (State)
	{
	case EState::StandingUp:
		if (StateT >= WaitFor) { State = EState::Walking; StateT = 0.f; Play(TEXT("WalkFast"), true, WalkRate); }
		break;
	case EState::SittingDown:
		if (StateT >= WaitFor) { State = EState::Sitting; StateT = 0.f; Play(TEXT("SitIdle"), true); }
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
