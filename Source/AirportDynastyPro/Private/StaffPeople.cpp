// Your hired staff in the 3D airport - see StaffPeople.h and docs/Hired-Crew-Plan.md.
#include "StaffPeople.h"
#include "PeopleLayers.h"
#include "PaxAnimInstance.h"
#include "People.h"
#include "PaxNav.h"
#include "Tugs.h"
#include "Airfield.h"
#include "RoadNetwork.h"
#include "BabylonModel.h"
#include "TerminalPlan.h"
#include "ADPLog.h"
#include "ServiceVehicles.h"
#include "PerfLog.h"
#include "Sim/ADPSim.h"
#include "Sim/ADPBoards.h"
#include "Sim/ADPSchedule.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Crc.h"
#include "Sim/ADPCrew.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Unseen.h"

static TAutoConsoleVariable<int32> CVarHiredCrew(TEXT("adp.HiredCrew"), 1, TEXT("To-do 206: 1 = your hired ground staff are the people in the 3D airport; 0 = the fixed stand-in team, as before"));
static FAutoConsoleCommandWithWorld CmdCrewList(TEXT("adp.CrewList"), TEXT("Your hired ground staff: shift, face, place and job, into the log"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (const UStaffPeople* S = UStaffPeople::Get(W)) S->Dump(); }));

namespace
{
	// the roles that are people in 3D (ADPCrew.cpp: b3D - the ramp role; the others when their jobs exist in 3D)
	const TArray<const TCHAR*>& SpRoles() { static TArray<const TCHAR*> L; if (!L.Num()) for (const ADPCrew::FRole& R : ADPCrew::Roles()) if (R.b3D) L.Add(R.Id); return L; }
	FString SpArea(const FString& Role) { if (const ADPCrew::FRole* R = ADPCrew::Role(Role)) for (const ADPCrew::FArea& X : ADPCrew::Areas()) if (FCString::Strcmp(X.Id, R->Area) == 0) return X.Label; return Role; }
	bool SpIsRole(const FString& R) { for (const TCHAR* X : SpRoles()) if (R == X) return true; return false; }
	using FSpShift = ADPCrew::FShift;
	const FSpShift* SpShift(const FString& N) { return ADPCrew::Shift(N); }
	bool SpInShift(const FSpShift& S, float H) { return ADPCrew::InShift(S, H); }
	float SpHoursToStart(const FSpShift& S, float H) { return FMath::Fmod(S.From - H + 24.f, 24.f); }
	FVector SpPlan(float X, float Z) { return FTerminalPlan::ToWorld(X, Z < 82.6f ? 0.f : AAirfield::GroundAt(X, Z), Z); }   // (the ramp rooms' floor; outside the apron's surface - as ATugs::Plan)
	float SpPlanZ(const FVector& W) { return -W.Y / 100.f; }
	// landside (the car park, the crossing, the pavement): the roads' ground, as the passengers (PaxFlow GroundZ)
	FVector SpLand(float X, float Z) { return FTerminalPlan::ToWorld(X, float(ARoadNetwork::GroundAt(X, Z)) + 0.03f, Z); }
	constexpr float SpLandCrossX = 48.f;                       // (the west zebra crossing, PaxFlow PXCROSS[0])
	constexpr float SpLandCarX = 46.25f, SpLandCarZ = -58.5f;  // (the car park's front row, its last space of x -87.5 .. 47.5, 2.5 m in front of it)
	constexpr float SpLandX = -42.5f, SpLandZ = -19.5f;        // (their place: the pavement behind the taxi bus - its rear x -45, the kerb z -21.5)
	const TCHAR* SpMove = TEXT("/Game/People/Anims/BS_Idle_Walk_Run_MH.BS_Idle_Walk_Run_MH");

	struct FSpFace { FString Name; bool bWoman = false; int32 Group = 0; };   // Group 0: the ground people; 1: the fire people (airside too, not used elsewhere)
	const TArray<FSpFace>& SpFaces()
	{
		static TArray<FSpFace> F; static bool bRead = false;
		if (bRead) return F;
		bRead = true;
		FString Text; TArray<TSharedPtr<FJsonValue>> Rows;
		if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/people-roster.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Rows))
			for (const TSharedPtr<FJsonValue>& V : Rows)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject(); if (!O) continue;
				const FString R = O->GetStringField(TEXT("role")); FSpFace X; X.Name = O->GetStringField(TEXT("name")); X.bWoman = O->GetBoolField(TEXT("female"));
				if (R != TEXT("ground") && R != TEXT("fire")) continue;
				X.Group = R == TEXT("ground") ? 0 : 1;
				if (FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/People/Built/%s/BP_%s"), *X.Name, *X.Name))) F.Add(X);
			}
		F.Sort([](const FSpFace& A, const FSpFace& B) { return A.Group != B.Group ? A.Group < B.Group : A.Name < B.Name; });
		ADP_LOG(TEXT("StaffPeople: %d built faces for the ground staff"), F.Num());
		return F;
	}
	// (hired in a talk: their own face, a passenger's perhaps - looked up once)
	bool SpBuilt(const FString& Face)
	{
		static TMap<FString, bool> Known;
		if (const bool* b = Known.Find(Face)) return *b;
		return Known.Add(Face, FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/People/Built/%s/BP_%s"), *Face, *Face)));
	}
	USkeletalMeshComponent* SpBodyOf(UWorld* W, AActor* A) { AADPPeople* P = AADPPeople::Get(W); AADPPeople::FWho* Who = P && A ? P->Find(A) : nullptr; return Who ? Who->Body : nullptr; }
	FADPPerson* SpRec(FADPWorld& W, const FString& Id) { return W.People.FindByPredicate([&](const FADPPerson& X) { return X.Id == Id; }); }
}

UStaffPeople* UStaffPeople::Get(const UWorld* W) { return W ? W->GetSubsystem<UStaffPeople>() : nullptr; }
bool UStaffPeople::On() { return CVarHiredCrew.GetValueOnGameThread() != 0; }

void UStaffPeople::Deinitialize() { ADPSchedule::Waiting3D = nullptr; Asks.Reset(); Staff.Reset(); Super::Deinitialize(); }

// ------------------------------------------------------------------ the pool

const TCHAR* UStaffPeople::RoleOf(EStaffDuty D) { return D == EStaffDuty::CleanDriver || D == EStaffDuty::Cleaner ? TEXT("cleaning") : D == EStaffDuty::FuelDriver ? TEXT("fuel") : D == EStaffDuty::CaterDriver || D == EStaffDuty::CaterLoader ? TEXT("catering") : D == EStaffDuty::WaterDriver ? TEXT("water") : D == EStaffDuty::TaxiDriver ? TEXT("taxi") : D == EStaffDuty::BusDriver ? TEXT("bus") : D == EStaffDuty::DeicerDriver || D == EStaffDuty::DeicerOperator ? TEXT("deice") : TEXT("ground"); }   // (one ramp role does all that has no role of its own)
const TCHAR* UStaffPeople::DutyName(EStaffDuty D)
{
	switch (D)
	{
	case EStaffDuty::CleanDriver: return TEXT("cleaning van driver");
	case EStaffDuty::Cleaner: return TEXT("aircraft cleaner");
	case EStaffDuty::FuelDriver: return TEXT("fuel truck driver");
	case EStaffDuty::CaterDriver: return TEXT("catering truck driver");
	case EStaffDuty::CaterLoader: return TEXT("catering loader");
	case EStaffDuty::WaterDriver: return TEXT("water and toilet truck driver");
	case EStaffDuty::StairsDriver: return TEXT("passenger stairs driver");
	case EStaffDuty::StairsHelper: return TEXT("passenger stairs helper");
	case EStaffDuty::AmbuDriver: return TEXT("ambulift driver");
	case EStaffDuty::AmbuHelper: return TEXT("ambulift helper");
	case EStaffDuty::TaxiDriver: return TEXT("taxi bus driver");
	case EStaffDuty::BusDriver: return TEXT("apron bus driver");
	case EStaffDuty::DeicerDriver: return TEXT("de-icing truck driver");
	case EStaffDuty::DeicerOperator: return TEXT("de-icing operator");
	case EStaffDuty::TugDriver: return TEXT("tug driver");
	case EStaffDuty::PushbackHandler: return TEXT("pushback handler");
	case EStaffDuty::WingWalker: return TEXT("wing walker");
	case EStaffDuty::GpuHandler: return TEXT("ground power handler");
	case EStaffDuty::TractorDriver: return TEXT("tractor driver");
	case EStaffDuty::BaggageHandler: return TEXT("baggage handler");
	default: return TEXT("belt loader operator");
	}
}
const TCHAR* UStaffPeople::TaskName(EStaffTask T)
{
	switch (T)
	{
	case EStaffTask::Cleaning: case EStaffTask::CleanRelief: return TEXT("cabin cleaning");
	case EStaffTask::Fuel: return TEXT("refuelling");
	case EStaffTask::Water: return TEXT("fresh water");
	case EStaffTask::Toilet: return TEXT("toilet service");
	case EStaffTask::Stairs: return TEXT("passenger stairs");
	case EStaffTask::Ambulift: return TEXT("ambulift");
	case EStaffTask::Taxi: return TEXT("the taxi bus");
	case EStaffTask::ApronBus: return TEXT("an apron bus");
	case EStaffTask::BusCharge: return TEXT("a bus charger");
	case EStaffTask::PushHelp: case EStaffTask::PushDriver: return TEXT("the pushback");
	case EStaffTask::Deice: case EStaffTask::DeiceRelief: return TEXT("de-icing");
	case EStaffTask::Catering: case EStaffTask::CaterRelief: return TEXT("catering");
	case EStaffTask::Gpu: return TEXT("ground power");
	case EStaffTask::BagsOut: return TEXT("bags out to the aircraft");
	case EStaffTask::BagsIn: return TEXT("bags in from the aircraft");
	case EStaffTask::Loader: return TEXT("belt loader");
	case EStaffTask::Chocks: return TEXT("chocks and cones");
	default: return TEXT("pushback");
	}
}

void UStaffPeople::Needs(EStaffTask T, TArray<FNeed>& Out)
{
	Out.Reset();
	auto Add = [&](EStaffDuty D, int32 Min, int32 Full) { FNeed N; N.Duty = D; N.Min = Min; N.Full = Full; Out.Add(N); };
	switch (T)
	{
	case EStaffTask::Gpu: case EStaffTask::Chocks: Add(EStaffDuty::GpuHandler, 1, 1); break;
	case EStaffTask::BagsOut: case EStaffTask::BagsIn: Add(EStaffDuty::TractorDriver, 1, 1); Add(EStaffDuty::BaggageHandler, 0, 1); break;
	case EStaffTask::Loader: Add(EStaffDuty::LoaderOperator, 1, 1); Add(EStaffDuty::BaggageHandler, 0, 1); break;
	case EStaffTask::Bags: Add(EStaffDuty::TractorDriver, 1, 1); Add(EStaffDuty::LoaderOperator, 1, 1); Add(EStaffDuty::BaggageHandler, 0, 2); break;
	case EStaffTask::Cleaning: Add(EStaffDuty::CleanDriver, 1, 1); Add(EStaffDuty::Cleaner, 1, 3); break;
	case EStaffTask::Fuel: Add(EStaffDuty::FuelDriver, 1, 1); break;
	case EStaffTask::Stairs: Add(EStaffDuty::StairsDriver, 1, 1); Add(EStaffDuty::StairsHelper, 1, 1); break;
	case EStaffTask::Ambulift: Add(EStaffDuty::AmbuDriver, 1, 1); Add(EStaffDuty::AmbuHelper, 1, 1); break;
	case EStaffTask::ApronBus: Add(EStaffDuty::BusDriver, 1, 1); break;
	case EStaffTask::BusCharge: Add(EStaffDuty::GpuHandler, 1, 1); break;
	case EStaffTask::Taxi: Add(EStaffDuty::TaxiDriver, 1, 1); break;   // (its driver at the wheel; the passengers are no crew)
	case EStaffTask::Deice: Add(EStaffDuty::DeicerDriver, 1, 1); Add(EStaffDuty::DeicerOperator, 1, 1); break;
	case EStaffTask::Water: case EStaffTask::Toilet: Add(EStaffDuty::WaterDriver, 1, 1); break;
	case EStaffTask::Catering: Add(EStaffDuty::CaterDriver, 1, 1); Add(EStaffDuty::CaterLoader, 1, 1); break;
	case EStaffTask::CaterRelief: Add(EStaffDuty::CaterLoader, 1, 1); break;
	case EStaffTask::DeiceRelief: Add(EStaffDuty::DeicerOperator, 1, 1); break;
	case EStaffTask::CleanRelief: Add(EStaffDuty::Cleaner, 1, 1); break;
	case EStaffTask::Relief: Add(EStaffDuty::BaggageHandler, 1, 1); break;
	case EStaffTask::PushHelp: Add(EStaffDuty::PushbackHandler, 1, 1); break;   // (a push of your own: its handler; its driver when you leave it)
	case EStaffTask::PushDriver: Add(EStaffDuty::TugDriver, 1, 1); break;
	case EStaffTask::Pushback: Add(EStaffDuty::TugDriver, 1, 1); Add(EStaffDuty::PushbackHandler, 1, 1); Add(EStaffDuty::WingWalker, 0, 3); break;
	}
}

const UStaffPeople::FStaff* UStaffPeople::Find(const FString& Id) const { return Staff.FindByPredicate([&](const FStaff& S) { return S.Id == Id; }); }

bool UStaffPeople::IsFree(const FStaff& S, const FString& Role) const
{
	return (S.State == EState::Ready || (S.State == EState::Back && S.A.IsValid() && (S.bLand || SpPlanZ(S.A->GetActorLocation()) > 83.f))) && S.bWanted && S.Role == Role && S.A.IsValid();
}

FString UStaffPeople::FindFree(EStaffDuty Duty, const FVector& Near) const
{
	const FStaff* Best = nullptr; double Bd = 1e18;
	if (On()) for (const FStaff& S : Staff)
	{
		if (!IsFree(S, RoleOf(Duty))) continue;
		const double D = FVector::DistSquared2D(S.A->GetActorLocation(), Near);
		if (D < Bd) { Bd = D; Best = &S; }
	}
	return Best ? Best->Id : FString();
}

int32 UStaffPeople::NumFree(EStaffDuty Duty) const
{
	int32 N = 0;
	if (On()) for (const FStaff& S : Staff) if (IsFree(S, RoleOf(Duty))) ++N;
	return N;
}

AActor* UStaffPeople::Claim(const FString& StaffId, const FString& Job)
{
	FStaff* S = Find(StaffId);
	if (!On() || !S || !IsFree(*S, S->Role)) return nullptr;
	S->State = EState::Busy; S->Job = Job; S->Way.Reset(); S->Speed = 0.f; S->BlockedT = S->SideT = 0.f; S->bOvertime = false;
	ADP_LOG(TEXT("StaffPeople: %s takes %s"), *S->Name, *Job);
	return S->A.Get();
}

bool UStaffPeople::OnShift(const FString& StaffId) const { const FStaff* S = Find(StaffId); return S && S->bWanted; }

void UStaffPeople::Release(const FString& StaffId)
{
	FStaff* S = Find(StaffId);
	if (!S || S->State != EState::Busy) return;
	ADP_LOG(TEXT("StaffPeople: %s done with %s"), *S->Name, *S->Job);
	S->Job.Reset(); S->Speed = 0.f; S->BlockedT = S->SideT = 0.f; S->Way.Reset(); S->State = EState::Back;
	AActor* A = S->A.Get(); if (!A) return;
	// (the walk and idle again, upright, whatever the job's poses were)
	if (USkeletalMeshComponent* B = SpBodyOf(GetWorld(), A))
		if (!B->GetSingleNodeInstance())
			if (UBlendSpace* Mv = Cast<UBlendSpace>(FSoftObjectPath(SpMove).ResolveObject())) { B->SetAnimationMode(EAnimationMode::AnimationSingleNode); B->SetAnimation(Mv); B->Play(true); UPaxAnimInstance::Install(B); }
	A->SetActorRotation(FRotator(0.f, A->GetActorRotation().Yaw, 0.f));
	// (landside: released on the pavement by the taxi bus - along it to their place)
	if (S->bLand) { FLeg L; L.At = LandSpot(*S); S->Way.Add(L); return; }
	if (SpPlanZ(A->GetActorLocation()) < 83.f)
	{
		// (never through a wall: only from the apron)
		ADP_WARN(TEXT("StaffPeople: %s was released inside the terminal (plan z %.1f) - stays there, free"), *S->Name, SpPlanZ(A->GetActorLocation()));
		S->State = EState::Ready; return;
	}
	AlongWall(*S, SpotOf(*S));
	FLeg L; L.At = SpotOf(*S); S->Way.Add(L);
}

// a far walk: in to the terminal's front, along it at plan z 85.45 - under the jetways' fixed corridors (their columns stand
// at z 88), past the ramp rooms' open door leaves (to z 85) and the stand-ins' waiting places (z 86) - and out again
void UStaffPeople::AlongWall(FStaff& S, const FVector& To) const
{
	const AActor* A = S.A.Get(); if (!A) return;
	const FVector Me = A->GetActorLocation();
	if (FMath::Abs(Me.X - To.X) < 1200.f || SpPlanZ(Me) < 83.f) return;
	for (const double X : {Me.X, To.X}) { FLeg L; L.At = SpPlan(float(X / 100.0), 85.45f); S.Way.Add(L); }
}

// ------------------------------------------------------------------ the queue (part 2)

int32 UStaffPeople::OnDuty(const FString& Role) const { int32 N = 0; for (const FStaff& S : Staff) if (S.bWanted && S.Role == Role) ++N; return N; }

// the smallest number of a role that can do every job of the role (the biggest of its jobs' smallest teams): one for a bus,
// the taxi bus, the fuel and the water; two for the apron, the catering, the cleaning and the de-icing
int32 UStaffPeople::LeastOf(const FString& Role)
{
	static TMap<FString, int32> Known;
	if (const int32* K = Known.Find(Role)) return *K;
	int32 Least = 1; TArray<FNeed> N;
	for (int32 t = 0; t < NumTasks; ++t)
	{
		Needs(EStaffTask(t), N);
		int32 Sum = 0; for (const FNeed& X : N) if (Role == RoleOf(X.Duty)) Sum += X.Min;
		Least = FMath::Max(Least, Sum);
	}
	return Known.Add(Role, Least);
}

// who takes over from somebody of this role (one person of the same role, never of another)
EStaffTask UStaffPeople::ReliefOf(const FString& Role)
{
	return Role == TEXT("water") ? EStaffTask::Water : Role == TEXT("catering") ? EStaffTask::CaterRelief : Role == TEXT("fuel") ? EStaffTask::Fuel : Role == TEXT("cleaning") ? EStaffTask::CleanRelief
		: Role == TEXT("deice") ? EStaffTask::DeiceRelief : Role == TEXT("bus") ? EStaffTask::ApronBus : Role == TEXT("taxi") ? EStaffTask::Taxi : EStaffTask::Relief;
}

// Once, at the start (to-do 300; Tools/check_crew_roles.py does the same on the source): every duty's role is there and comes in
// 3D, every task has its people, all of one role (AskTeam counts one role), every vehicle asks for the role of its own team,
// every role you can hire has a job or says that it has none.
void UStaffPeople::CheckRoles() const
{
	int32 Bad = 0; TSet<FString> Used; TArray<FNeed> N;
	for (int32 d = 0; d < NumDuties; ++d)
	{
		const FString Role = RoleOf(EStaffDuty(d)); const ADPCrew::FRole* R = ADPCrew::Role(Role); Used.Add(Role);
		if (!R) { ++Bad; ADP_ERROR(TEXT("StaffPeople: the duty %s belongs to the role %s, which nobody can be hired for"), DutyName(EStaffDuty(d)), *Role); }
		else if (!R->b3D || !R->bShift) { ++Bad; ADP_ERROR(TEXT("StaffPeople: the duty %s belongs to the role %s, which never comes (3D %d, shifts %d)"), DutyName(EStaffDuty(d)), *Role, R->b3D, R->bShift); }
		bool bAny = false; for (int32 t = 0; t < NumTasks && !bAny; ++t) { Needs(EStaffTask(t), N); for (const FNeed& X : N) if (X.Duty == EStaffDuty(d)) bAny = true; }
		if (!bAny) { ++Bad; ADP_ERROR(TEXT("StaffPeople: no task needs the duty %s"), DutyName(EStaffDuty(d))); }
	}
	for (int32 t = 0; t < NumTasks; ++t)
	{
		Needs(EStaffTask(t), N); int32 Min = 0; bool bOne = true;
		for (const FNeed& X : N) { Min += X.Min; if (FCString::Strcmp(RoleOf(X.Duty), RoleOf(N[0].Duty)) != 0) bOne = false; }
		if (!N.Num() || Min < 1) { ++Bad; ADP_ERROR(TEXT("StaffPeople: the task %d (%s) has no duties"), t, TaskName(EStaffTask(t))); }
		else if (!bOne) { ++Bad; ADP_ERROR(TEXT("StaffPeople: the task %d (%s) mixes roles"), t, TaskName(EStaffTask(t))); }
	}
	for (const FServiceSpec& S : ADPService::All())
	{
		Needs(S.Team, N);
		if (!N.Num() || FCString::Strcmp(S.Role, RoleOf(N[0].Duty)) != 0) { ++Bad; ADP_ERROR(TEXT("StaffPeople: the %s asks for the role %s, its team is %s"), S.Label, S.Role, N.Num() ? RoleOf(N[0].Duty) : TEXT("nobody")); }
	}
	FString None;
	for (const ADPCrew::FRole& R : ADPCrew::Roles())
	{
		if (R.b3D && !Used.Contains(R.Id)) { ++Bad; ADP_ERROR(TEXT("StaffPeople: the role %s comes in 3D and has no duty"), R.Id); }
		if (R.Work) continue;
		None += FString::Printf(TEXT("%s%s"), None.IsEmpty() ? TEXT("") : TEXT(", "), R.Id);
		if (R.b3D || R.bSim || Used.Contains(R.Id)) { ++Bad; ADP_ERROR(TEXT("StaffPeople: the role %s has a job but its table line says none"), R.Id); }
	}
	if (!Rooms.Num()) { ++Bad; ADP_ERROR(TEXT("StaffPeople: no ramp room has a staff door onto the apron - nobody can come out")); }
	if (!None.IsEmpty()) ADP_WARN(TEXT("StaffPeople: can be hired and have no job in this version (the hire list says so): %s"), *None);
	ADP_LOG(TEXT("StaffPeople: %d duties, %d tasks, %d roles checked, %d wrong"), NumDuties, NumTasks, ADPCrew::Roles().Num(), Bad);
}

void UStaffPeople::NeverCame(const FADPPerson& P, const TCHAR* Why)
{
	if (SaidNever.Contains(P.Id)) return;
	SaidNever.Add(P.Id);
	ADP_ERROR(TEXT("StaffPeople: %s (%s) is on shift and cannot come: %s"), *P.Name, *P.Role, Why);
}

bool UStaffPeople::HiresRole(const TCHAR* Role) const
{
	const UADPSim* Sim = UADPSim::Get(GetWorld());
	if (!On() || !Sim || !Sim->IsRunning()) return false;
	const FString Here = ADPBoards::HereIcao(); int32 N = 0;
	for (const FADPPerson& P : Sim->World.People) if (P.AirlineId == UADPSim::Me() && P.Role == Role && P.Base == Here) ++N;
	const int32 Least = LeastOf(Role);
	const bool bOk = N >= Least;
	const bool* Was = SaidHires.Find(Role);
	if (!Was || *Was != bOk)
	{
		SaidHires.Add(Role, bOk);
		if (bOk) ADP_LOG(TEXT("StaffPeople: you employ %d people of the role %s here - they work your own flights"), N, Role);
		else ADP_LOG(TEXT("StaffPeople: you employ %d people of the role %s here, %d %s the smallest team - the handling agent's people work your flights until you hire"), N, Role, Least, Least == 1 ? TEXT("is") : TEXT("are"));
	}
	return bOk;
}

void UStaffPeople::CheckClaims()
{
	// (a claim always ends: somebody who is claimed but whom no vehicle's crew list has is free again)
	for (FStaff& S : Staff)
	{
		if (S.State != EState::Busy) { S.Idle = 0; continue; }
		if (Tugs.IsValid() && Tugs->Crew.ContainsByPredicate([&](const ATugs::FCrew& C) { return C.bHired && !C.bSlotFree && C.StaffId == S.Id; })) { S.Idle = 0; continue; }
		if (++S.Idle < 4) continue;
		ADP_WARN(TEXT("StaffPeople: %s was claimed for %s but is at no job - free again"), *S.Name, *S.Job);
		S.Idle = 0; Release(S.Id);
	}
}

void UStaffPeople::DropOldAsks()
{
	const double Now = GetWorld()->GetTimeSeconds();
	Asks.RemoveAll([&](const FAsk& A) { return Now - A.Last > 10.0; });
}

bool UStaffPeople::AskTeam(const FString& Key, EStaffTask Task, const FDateTime& When, const FVector& Near, const FString& What, TArray<FTaken>& Out)
{
	Out.Reset();
	UADPSim* Sim = UADPSim::Get(GetWorld());
	if (!On() || !Sim) return false;
	DropOldAsks();
	TArray<FNeed> N; Needs(Task, N);
	if (!N.Num()) return false;
	int32 Need = 0; for (const FNeed& X : N) Need += X.Min;
	const FString Role = RoleOf(N[0].Duty);
	FAsk* Me = Asks.FindByPredicate([&](const FAsk& A) { return A.Key == Key; });
	if (!Me) { Me = &Asks.AddDefaulted_GetRef(); Me->Key = Key; Me->Since = Sim->Now(); }
	Me->What = What; Me->Role = Role; Me->Task = Task; Me->Need = Need; Me->When = When; Me->Last = GetWorld()->GetTimeSeconds();
	Asks.Sort([](const FAsk& A, const FAsk& B) { return A.When != B.When ? A.When < B.When : A.Key < B.Key; });
	// the earlier departures first: the people they need are theirs (a job that needs more than are on shift at all holds
	// nobody up); never half a team
	int32 Free = NumFree(N[0].Duty); const int32 All = Free, Duty = OnDuty(Role);
	FString Before;
	int32 Mine = INDEX_NONE;
	for (int32 k = 0; k < Asks.Num(); ++k)
	{
		if (Asks[k].Key == Key) { Mine = k; break; }
		if (Asks[k].Role == Role && Asks[k].Need <= Duty) { Free -= Asks[k].Need; if (Before.IsEmpty()) Before = Asks[k].What; }
	}
	if (Mine == INDEX_NONE) return false;
	FAsk& A = Asks[Mine];
	const double Min = (Sim->Now() - A.Since).GetTotalMinutes();
	if (Free < Need)
	{
		const FString M = Missing(Task);
		A.Why = !M.IsEmpty() ? M : FString::Printf(TEXT("%d free (%s) go to %s first"), All, *ADPCrew::RoleName(Role), *Before);
		if (Min >= 1.0) ADPSchedule::Tell(Sim->World, Sim->Now(), TEXT("ground:") + Key, FString::Printf(TEXT("Nobody free: %s"), *ADPCrew::RoleName(Role)), FString::Printf(TEXT("%s waits: %s. Hire more under Crew, %s."), *What, *A.Why, *SpArea(Role)));
		return false;
	}
	for (const FNeed& X : N)
		for (int32 i = 0; i < X.Min; ++i)
		{
			const FString Id = FindFree(X.Duty, Near);
			AActor* B = Id.IsEmpty() ? nullptr : Claim(Id, FString::Printf(TEXT("%s (%s)"), *What, DutyName(X.Duty)));
			if (!B) { for (const FTaken& T : Out) Release(T.Id); Out.Reset(); return false; }
			FTaken T; T.Duty = X.Duty; T.Id = Id; T.A = B; Out.Add(T);
		}
	if (Min >= 1.0) ADP_LOG(TEXT("StaffPeople: %s waited %.0f min for its people"), *What, Min);
	Asks.RemoveAt(Mine);
	return true;
}

bool UStaffPeople::AskExtra(EStaffDuty Duty, const FVector& Near, const FString& What, FTaken& Out)
{
	if (!On()) return false;
	DropOldAsks();
	int32 Free = NumFree(Duty); const FString Role = RoleOf(Duty); const int32 Num = OnDuty(Role);
	for (const FAsk& A : Asks) if (A.Role == Role && A.Need <= Num) Free -= A.Need;
	if (Free < 1) return false;
	Out = FTaken(); Out.Duty = Duty; Out.Id = FindFree(Duty, Near);
	if (!Out.Id.IsEmpty()) Out.A = Claim(Out.Id, FString::Printf(TEXT("%s (%s)"), *What, DutyName(Duty)));
	return Out.A.IsValid();
}

FString UStaffPeople::WaitsFor(const FString& Key) const { const FAsk* A = Asks.FindByPredicate([&](const FAsk& X) { return X.Key == Key; }); return A ? A->Why : FString(); }

FString UStaffPeople::Waiting() const
{
	TArray<FString> L; const double Now = GetWorld()->GetTimeSeconds();
	for (const FAsk& A : Asks) if (Now - A.Last <= 10.0 && !A.Why.IsEmpty()) L.Add(FString::Printf(TEXT("%s waits - %s"), *A.What, *A.Why));
	return FString::Join(L, TEXT("; "));
}

void UStaffPeople::Faces(TSet<FString>& Out)
{
	UADPSim* Sim = UADPSim::Get(GetWorld());
	if (!Sim || !Sim->IsRunning()) return;
	const FString Here = ADPBoards::HereIcao();
	TArray<FString> Ids;
	for (const FADPPerson& P : Sim->World.People) if (P.AirlineId == UADPSim::Me() && SpIsRole(P.Role) && P.Base == Here) Ids.Add(P.Id);
	for (const FString& Id : Ids)
	{
		GiveShift(Sim->World, Id); GiveFace(Sim->World, Id);
		if (const FADPPerson* P = SpRec(Sim->World, Id)) if (!P->Face.IsEmpty()) Out.Add(P->Face);
	}
}

FString UStaffPeople::Missing(EStaffTask Task) const
{
	TArray<FNeed> N; Needs(Task, N);
	TMap<FString, int32> Left;
	FString Out;
	for (const FNeed& X : N)
	{
		const FString Role = RoleOf(X.Duty);
		if (!Left.Contains(Role)) Left.Add(Role, NumFree(X.Duty));
		int32& L = Left[Role];
		const int32 Short = FMath::Max(0, X.Min - L); L = FMath::Max(0, L - X.Min);
		if (!Short) continue;
		int32 OnDuty = 0; for (const FStaff& S : Staff) if (S.bWanted && S.Role == Role) ++OnDuty;
		Out += FString::Printf(TEXT("%s%d %s (%s)"), Out.IsEmpty() ? TEXT("") : TEXT(", "), Short, DutyName(X.Duty), OnDuty ? *FString::Printf(TEXT("%s: %d on shift, not enough free"), *ADPCrew::RoleName(Role), OnDuty) : *FString::Printf(TEXT("%s: nobody on shift"), *ADPCrew::RoleName(Role)));
	}
	return Out;
}

FString UStaffPeople::NameOf(const FString& Id) const { const FStaff* S = Find(Id); return S ? S->Name : FString(); }
AActor* UStaffPeople::ActorOf(const FString& Id) const { const FStaff* S = Find(Id); return S ? S->A.Get() : nullptr; }
FString UStaffPeople::IdOf(const AActor* A) const { if (A) for (const FStaff& S : Staff) if (S.A.Get() == A) return S.Id; return FString(); }

const TCHAR* UStaffPeople::StateName(EState S)
{
	switch (S)
	{
	case EState::Load: return TEXT("coming (loading)");
	case EState::Wait: return TEXT("coming (not in the ramp room yet)");
	case EState::Out: return TEXT("coming out");
	case EState::Back: return TEXT("walking back");
	case EState::Ready: return TEXT("standing by");
	case EState::Busy: return TEXT("at work");
	default: return TEXT("going off shift");
	}
}

bool UStaffPeople::Status(const FString& Id, FString& Doing, FString& Where) const
{
	const FStaff* S = Find(Id);
	if (!S) return false;
	Doing = S->Job.IsEmpty() ? FString(StateName(S->State)) : S->Job;
	if (S->bCalledIn) Doing += TEXT(" (called in)");
	Where.Reset();
	const AActor* A = S->A.Get();
	if (!A) return true;
	const FVector L = A->GetActorLocation();
	const FRoom* Near = nullptr; double Bd = 1e18;
	for (const FRoom& R : Rooms) { const double D = FMath::Abs(L.X / 100.0 - R.X0); if (D < Bd) { Bd = D; Near = &R; } }
	if (SpPlanZ(L) < 83.f) Where = Near ? FString::Printf(TEXT("in Ramp services %d"), Near->Gate) : FString(TEXT("in the terminal"));
	else Where = Near && Bd < 60.0 ? FString::Printf(TEXT("on the apron by Ramp services %d"), Near->Gate) : FString(TEXT("on the apron"));
	return true;
}

void UStaffPeople::Dump() const
{
	const UADPSim* Sim = UADPSim::Get(GetWorld());
	if (!Sim) { ADP_LOG(TEXT("CrewList: no career")); return; }
	const FDateTime Now = Sim->Now(), Here = ADPBoards::Local(Now);
	int32 N = 0;
	for (const FADPPerson& P : Sim->World.People)
	{
		if (P.AirlineId != UADPSim::Me() || !SpIsRole(P.Role)) continue;
		++N;
		const FStaff* S = Find(P.Id);
		FString At = TEXT("not here"), Doing = S ? FString(StateName(S->State)) : FString(TEXT("off shift"));
		if (S && S->A.IsValid()) { const FVector L = S->A->GetActorLocation(); At = FString::Printf(TEXT("plan %.1f, %.1f"), L.X / 100.0, -L.Y / 100.0); }
		if (S && !S->Job.IsEmpty()) Doing += TEXT(", job: ") + S->Job;
		const FString Shift = P.Shift.IsEmpty() ? FString(TEXT("-")) : P.Shift + (S && S->bCalledIn ? TEXT(" (called in)") : TEXT(""));
		ADP_LOG(TEXT("CrewList: %s %s (%s, %s, %s at %s) - shift %s, face %s, %s, %s"), *P.Id, *P.Name, *P.Role, P.bWoman ? TEXT("woman") : TEXT("man"), *P.Status, *P.Base, *Shift, P.Face.IsEmpty() ? TEXT("-") : *P.Face, *Doing, *At);
	}
	ADP_LOG(TEXT("CrewList: %d hired ground staff, %d in 3D or on their way, the simulation counts %d on shift, local time %02d:%02d, adp.HiredCrew %d"), N, Staff.Num(), ADPSchedule::OnShift(Sim->World, TEXT("ground"), Now), Here.GetHour(), Here.GetMinute(), On() ? 1 : 0);
	for (const EStaffTask T : {EStaffTask::Gpu, EStaffTask::BagsOut, EStaffTask::BagsIn, EStaffTask::Loader, EStaffTask::Bags, EStaffTask::Pushback})
	{
		const FString M = Missing(T);
		ADP_LOG(TEXT("CrewList: %s - %s"), TaskName(T), M.IsEmpty() ? TEXT("enough people free") : *(TEXT("missing ") + M));
	}
	// the jobs that wait, in their order
	int32 K = 0; const double Ts = GetWorld()->GetTimeSeconds();
	for (const FAsk& A : Asks)
		if (Ts - A.Last <= 10.0)
			ADP_LOG(TEXT("CrewList: waiting job %d: %s (%s, %d needed) - departure %s, waited %.0f min, %s"), ++K, *A.What, TaskName(A.Task), A.Need, *ADPBoards::Local(A.When).ToString(TEXT("%H:%M")), (Now - A.Since).GetTotalMinutes(), A.Why.IsEmpty() ? TEXT("being given out") : *A.Why);
	if (!K) ADP_LOG(TEXT("CrewList: no job waits for people"));
}

// ------------------------------------------------------------------ who should be here

void UStaffPeople::FindRooms()
{
	bRooms = true;
	// the ramp rooms under the gates (F1-RAMP-n): their staff door onto the apron is 5 m from the room's west end
	FTerminalPlan Plan; if (!Plan.Load()) return;
	for (const FPlanDoor& D : Plan.Doors)
		if (D.Id.StartsWith(TEXT("D-F1-RAMP-")) && D.B == TEXT("APRON") && D.Type == TEXT("secure"))
		{ FRoom R; R.Gate = FCString::Atoi(*D.Id.Mid(10)); R.X0 = D.X - 5.f; Rooms.Add(R); }
	ADP_LOG(TEXT("StaffPeople: %d ramp rooms with a staff door onto the apron"), Rooms.Num());
}

void UStaffPeople::GiveShift(FADPWorld& W, const FString& Id) const
{
	// the shift with the fewest people for its share (two early and two late for one night), kept in their record
	if (FADPPerson* P = SpRec(W, Id)) ADPCrew::GiveShift(W, *P);
}

void UStaffPeople::GiveFace(FADPWorld& W, const FString& Id)
{
	// one of the built people of their sex: nobody else's while one is left, the first tried by their id (the same for the
	// same person always; kept in their record). More people than faces: shared with someone of another shift if possible
	FADPPerson* P = SpRec(W, Id);
	const TArray<FSpFace>& All = SpFaces();
	if (!P || (!P->Face.IsEmpty() && (All.ContainsByPredicate([&](const FSpFace& F) { return F.Name == P->Face; }) || SpBuilt(P->Face)))) return;
	const FSpFace* Best = nullptr; int32 Bs = MAX_int32;
	for (int32 Group = 0; Group < 2 && Bs > 0; ++Group)
	{
		TArray<const FSpFace*> L; for (const FSpFace& F : All) if (F.bWoman == P->bWoman && F.Group == Group) L.Add(&F);
		const int32 First = L.Num() ? int32(FCrc::StrCrc32(*Id) % uint32(L.Num())) : 0;
		for (int32 k = 0; k < L.Num(); ++k)
		{
			const FSpFace* F = L[(First + k) % L.Num()];
			int32 Used = 0, Same = 0;
			for (const FADPPerson& X : W.People) if (X.Id != Id && X.AirlineId == P->AirlineId && X.Face == F->Name) { ++Used; if (X.Shift == P->Shift) ++Same; }
			// (a stand-in of another airline's handling team wears it: one nobody wears first)
			bool bStandIn = false; if (Tugs.IsValid()) for (const ATugs::FCrew& C : Tugs->Crew) if (!C.bHired && C.Name == F->Name) bStandIn = true;
			const int32 Sc = Used * 10 + Same * 100 + (bStandIn ? 5 : 0);
			if (Sc < Bs) { Bs = Sc; Best = F; }
		}
	}
	if (!Best) return;
	P->Face = Best->Name;
	if (Bs >= 10 && !bSaidReuse) { bSaidReuse = true; ADP_LOG(TEXT("StaffPeople: more hired people than built faces - some share a face (first: %s, %s)"), *P->Name, *P->Face); }
}

void UStaffPeople::Roster(FADPWorld& W, const FDateTime& Now)
{
	const FString Here = ADPBoards::HereIcao();
	const FDateTime Lt = ADPBoards::Local(Now); const float H = Lt.GetHour() + Lt.GetMinute() / 60.f;
	TMap<FString, bool> Want;   // (id -> called in)
	if (On()) for (const TCHAR* Role : SpRoles())   // (switched off again: everyone goes home)
	{
		struct FRow { FString Id; FDateTime Hired; };
		TArray<FRow> Ids;
		for (const FADPPerson& P : W.People)
			if (P.AirlineId == UADPSim::Me() && P.Role == Role && P.Base == Here && P.Status == TEXT("on") && !(P.OffUntil > Now)) Ids.Add({P.Id, P.HiredAt});
		// (the first hired first, whatever order the career lists them in)
		Ids.Sort([](const FRow& A, const FRow& B) { return A.Hired != B.Hired ? A.Hired < B.Hired : A.Id < B.Id; });
		for (const FRow& R : Ids) { GiveShift(W, R.Id); GiveFace(W, R.Id); }
		TArray<TPair<float, FString>> Others;
		int32 N = 0;
		for (const FRow& R : Ids)
		{
			const FADPPerson* P = SpRec(W, R.Id); const FSpShift* S = P ? SpShift(P->Shift) : nullptr; if (!S) continue;
			if (SpInShift(*S, H)) { Want.Add(R.Id, false); ++N; } else Others.Add(TPair<float, FString>(SpHoursToStart(*S, H), R.Id));
		}
		// (the simulation counts 40% of a team on duty at any hour, at least one - the people due next are called in up to that)
		Others.Sort([](const TPair<float, FString>& A, const TPair<float, FString>& B) { return A.Key != B.Key ? A.Key < B.Key : A.Value < B.Value; });
		// (a job that waits for more people than are on duty at all: the next ones are called in for it - a small team still flies)
		int32 Count = ADPSchedule::OnShift(W, Role, Now);
		for (const FAsk& A : Asks) if (A.Role == Role) Count = FMath::Max(Count, A.Need);
		for (int32 k = 0; k < Others.Num() && N < Count; ++k, ++N) Want.Add(Others[k].Value, true);
	}
	for (FStaff& S : Staff)
	{
		const bool* C = Want.Find(S.Id);
		S.bWanted = C != nullptr;
		if (C) { S.bCalledIn = *C; if (const FADPPerson* P = SpRec(W, S.Id)) S.Shift = P->Shift; }
		else if (S.State == EState::Busy && !S.bOvertime) { S.bOvertime = true; ADP_LOG(TEXT("StaffPeople: %s's shift is over - finishes %s first"), *S.Name, *S.Job); }
	}
	for (const TPair<FString, bool>& X : Want)
	{
		if (Find(X.Key)) continue;
		const FADPPerson* P = SpRec(W, X.Key);
		if (!P) continue;
		if (P->Face.IsEmpty()) { NeverCame(*P, TEXT("no built person of the ground or fire staff is left for them")); continue; }
		FStaff S; S.Id = P->Id; S.Name = P->Name; S.Role = P->Role; S.Face = P->Face; S.Shift = P->Shift; S.bCalledIn = X.Value;
		if (S.Role == TEXT("taxi"))
		{
			// (landside: no ramp room - from the car park; the first free place on the pavement)
			S.bLand = true;
			for (S.Slot = 0; Staff.ContainsByPredicate([&](const FStaff& O) { return O.bLand && O.Slot == S.Slot; }); ++S.Slot) {}
			Staff.Add(MoveTemp(S)); continue;
		}
		// (the ramp room with the fewest of them, the first free place beside its door)
		int32 Bn = MAX_int32;
		for (int32 r = 0; r < Rooms.Num(); ++r) { if (Rooms[r].bNoDoor) continue; int32 N = 0; for (const FStaff& O : Staff) if (O.Room == r) ++N; if (N < Bn) { Bn = N; S.Room = r; } }
		if (S.Room == INDEX_NONE) { NeverCame(*P, TEXT("no ramp room has a staff door onto the apron")); continue; }
		for (S.Slot = 0; Staff.ContainsByPredicate([&](const FStaff& O) { return O.Room == S.Room && O.Slot == S.Slot; }); ++S.Slot) {}
		Staff.Add(MoveTemp(S));
	}
}

// ------------------------------------------------------------------ the day

void UStaffPeople::Tick(float Dt)
{
	ADP_PERF_SCOPE("staff people (each 2 s)", 2.f);
	Super::Tick(Dt);
	if (!On() && !Staff.Num()) return;
	if ((Poll -= Dt) <= 0.f)
	{
		Poll = 2.f + (bRooms ? 0.f : 0.55f);   // (not in the frame of the flights' own two-second look)
		if (!bRooms) FindRooms();
		if (!ADPSchedule::Waiting3D) { TWeakObjectPtr<UStaffPeople> Tw(this); ADPSchedule::Waiting3D = [Tw] { return Tw.IsValid() ? Tw->Waiting() : FString(); }; }
		// (the apron's crew and vehicles are in: the airport has loaded)
		if (!Tugs.IsValid()) for (TActorIterator<ATugs> It(GetWorld()); It; ++It) { Tugs = *It; break; }
		UADPSim* Sim = UADPSim::Get(GetWorld());
		if (Sim && Sim->IsRunning() && Tugs.IsValid()) { if (!bRolesChecked && bRooms) { bRolesChecked = true; CheckRoles(); } DropOldAsks(); Roster(Sim->World, Sim->Now()); CheckClaims(); }
	}
	if (!Tugs.IsValid()) return;
	for (FStaff& S : Staff) StepOne(S, Dt);
	Staff.RemoveAll([](const FStaff& S) { return S.Role.IsEmpty(); });   // (gone home, or never came)
}

void UStaffPeople::StepOne(FStaff& S, float Dt)
{
	static FRoom NoRoom;
	FRoom& R = Rooms.IsValidIndex(S.Room) ? Rooms[S.Room] : NoRoom;   // (the landside crew have none)
	const float T = GetWorld()->GetTimeSeconds();
	if (S.State != EState::Load && S.State != EState::Wait && !S.A.IsValid())
	{
		ADP_WARN(TEXT("StaffPeople: %s's body is gone%s - comes again"), *S.Name, S.Job.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" at work (%s)"), *S.Job));
		LeaveDoor(S); S.Role.Reset(); return;
	}
	if (S.bLand && S.State != EState::Load)
	{
		switch (S.State)
		{
		case EState::Wait:
		{
			if (!S.bWanted) { S.Role.Reset(); return; }
			// out of their car in the car park, unseen (you far, or looking elsewhere), nobody there
			const FVector At = SpLand(SpLandCarX, SpLandCarZ);
			if (T < NextBody || !LandUnseen(At)) return;
			if (AADPPeople* P = AADPPeople::Get(GetWorld())) if (P->AnyoneNear(At, 120.f)) return;
			Body(S);
			return;
		}
		case EState::Out: case EState::Back:
			if (S.State == EState::Back && !S.bWanted) { S.State = EState::Leave; LandWayIn(S); return; }
			if (Walk(S, Dt)) { S.State = EState::Ready; S.Speed = 0.f; }
			return;
		case EState::Ready:
			if (!S.bWanted) { S.State = EState::Leave; LandWayIn(S); ADP_LOG(TEXT("StaffPeople: %s goes off shift (to the car park)"), *S.Name); return; }
			Turn(S, 90.f, Dt);   // (looking out at the kerb)
			return;
		case EState::Leave:
			// (back at the car park: into their car, gone - unseen)
			if (!Walk(S, Dt) || !LandUnseen(S.A->GetActorLocation())) return;
			Gone(S);
			return;
		default: return;   // (Busy: whoever claimed them moves them)
		}
	}
	switch (S.State)
	{
	case EState::Load:
		if (!S.bWanted) { S.Role.Reset(); return; }
		// the body loaded in the background, two people at a time
		if (!S.Handle.IsValid())
		{
			int32 Busy = 0; for (const FStaff& O : Staff) if (O.State == EState::Load && O.Handle.IsValid()) ++Busy;
			if (Busy < 2) { TArray<FSoftObjectPath> Want = ADPLayers::Paths(S.Face); Want.Add(FSoftObjectPath(SpMove)); S.Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Want); }
		}
		else if (S.Handle->HasLoadCompleted()) S.State = EState::Wait;
		return;
	case EState::Wait:
	{
		if (!S.bWanted) { S.Role.Reset(); return; }
		// made in the ramp room behind its shut door, unseen, one at a time, never two in one frame
		if (T < NextBody || T < R.NextSpawn || R.Dir >= 2 || PlayerInRoom(R)) return;
		const float K = ABabylonModel::StaffDoor(DoorOf(R), false);
		if (K < 0.f)
		{
			R.bNoDoor = true;
			if (!bSaidNoDoor) { bSaidNoDoor = true; ADP_WARN(TEXT("StaffPeople: ramp room %d has no staff door in this terminal - nobody comes through it"), R.Gate); }
			S.Role.Reset(); return;
		}
		if (K > 0.05f) return;
		if (AADPPeople* P = AADPPeople::Get(GetWorld())) if (P->AnyoneNear(SpPlan(R.X0 + 2.2f, 71.f), 90.f)) return;
		Body(S);
		return;
	}
	case EState::Out: case EState::Back:
		if (S.State == EState::Back && !S.bWanted) { S.State = EState::Leave; WayIn(S); return; }
		if (Walk(S, Dt)) { S.State = EState::Ready; S.Speed = 0.f; }
		return;
	case EState::Ready:
		if (!S.bWanted && SpPlanZ(S.A->GetActorLocation()) > 83.f) { S.State = EState::Leave; WayIn(S); ADP_LOG(TEXT("StaffPeople: %s goes off shift"), *S.Name); return; }
		Turn(S, FVector(0.3f, -1.f, 0.f).Rotation().Yaw, Dt);   // (looking out at the stand)
		return;
	case EState::Busy: return;   // (whoever claimed them moves them)
	case EState::Leave:
		if (!Walk(S, Dt)) return;
		// (in the room, unseen: the door shut behind them and you not in there)
		if (PlayerInRoom(R) || ABabylonModel::StaffDoor(DoorOf(R), false) > 0.05f) return;
		Gone(S);
		return;
	}
}

void UStaffPeople::Body(FStaff& S)
{
	static FRoom NoRoom;
	FRoom& R = Rooms.IsValidIndex(S.Room) ? Rooms[S.Room] : NoRoom;
	AADPPeople* P = AADPPeople::Get(GetWorld()); if (!P) return;
	NextBody = GetWorld()->GetTimeSeconds() + 0.75f; R.NextSpawn = GetWorld()->GetTimeSeconds() + 4.f;
	const FVector At = S.bLand ? SpLand(SpLandCarX, SpLandCarZ) : SpPlan(R.X0 + 2.2f, 71.f);
	AActor* A = P->Spawn(S.Face, At, FVector(0.f, -1.f, 0.f).Rotation().Yaw);   // (its class is in memory already: the load above)
	if (!A) { S.Role.Reset(); return; }
	A->SetActorLocation(At);
	if (AADPPeople::FWho* W = P->Find(A)) { FString First, Rest; W->First = S.Name.Split(TEXT(" "), &First, &Rest) ? First : S.Name; }
	// (far away their animation updates only while seen, and less often - as the passengers)
	TArray<USkeletalMeshComponent*> Sk; A->GetComponents<USkeletalMeshComponent>(Sk);
	for (USkeletalMeshComponent* M : Sk) { M->bEnableUpdateRateOptimizations = true; M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered; }
	PaxNav::NoNav(A);
	S.A = A; S.State = EState::Out; S.Speed = 0.f;
	if (S.bLand) { LandWayOut(S); ADP_LOG(TEXT("StaffPeople: %s (%s, %s shift%s) comes on duty at the car park - %s"), *S.Name, *S.Role, *S.Shift, S.bCalledIn ? TEXT(", called in") : TEXT(""), *S.Face); return; }
	EnterDoor(S, 1);
	WayOut(S);
	ADP_LOG(TEXT("StaffPeople: %s (%s, %s shift%s) comes on duty at gate %d - %s"), *S.Name, *S.Role, *S.Shift, S.bCalledIn ? TEXT(", called in") : TEXT(""), R.Gate, *S.Face);
}

void UStaffPeople::Gone(FStaff& S)
{
	LeaveDoor(S);
	ADP_LOG(TEXT("StaffPeople: %s has gone off shift"), *S.Name);
	if (AActor* A = S.A.Get()) A->Destroy();
	S.A = nullptr; S.Handle.Reset(); S.Role.Reset();
}

// ------------------------------------------------------------------ the ways

// the door's middle; its one leaf (2.96 m) hangs at its west side (x0 + 3.5) and swings through 3 m one way
FVector UStaffPeople::DoorOf(const FRoom& R) const { return SpPlan(R.X0 + 5.f, 82.f); }

// where they stand by: a row along the terminal west of the stand line, behind the tug's box and off the red lines
FVector UStaffPeople::SpotOf(const FStaff& S) const { return S.bLand ? LandSpot(S) : SpPlan(Rooms[S.Room].X0 - 3.4f - 1.3f * S.Slot, 84.2f); }

// ---- landside (to-do 239b): the taxi bus driver. All outdoors: the car park, the zebra crossing at x 48 (its parts' ends
// from the roads, as the passengers'), the forecourt pavement at z -19.5 (the kerb -21.5, the halls' doors -4), their place
// east of the taxi bus's rear, a row eastwards, facing the kerb.
FVector UStaffPeople::LandSpot(const FStaff& S) const { return SpLand(SpLandX + 1.3f * FMath::Max(0, S.Slot), SpLandZ); }

namespace
{
	// the crossing's ends from the car park's side to the terminal's (world Y is minus plan z: the car park has the larger Y)
	TArray<FVector> SpCrossing()
	{
		TArray<FVector> E;
		for (const TPair<FVector, FVector>& Z : ARoadNetwork::ZebraCrossings)
			if (FMath::Abs((Z.Key.X + Z.Value.X) / 200.f - SpLandCrossX) < 4.f && FMath::Abs(-Z.Key.Y / 100.f + 46.75f) < 40.f) { E.Add(Z.Key); E.Add(Z.Value); }
		if (!E.Num()) for (const float Z : {-51.25f, -42.25f, -38.75f, -21.5f}) E.Add(SpLand(SpLandCrossX, Z));
		for (FVector& P : E) P = SpLand(P.X / 100.f, -P.Y / 100.f);
		E.Sort([](const FVector& A, const FVector& B) { return A.Y > B.Y; });
		return E;
	}
}

void UStaffPeople::LandWayOut(FStaff& S) const
{
	S.Way.Reset();
	auto Add = [&S](const FVector& P) { FLeg L; L.At = P; S.Way.Add(L); };
	const TArray<FVector> E = SpCrossing();
	Add(E[0] + FVector(0.f, 150.f, 0.f));          // (square onto the crossing from the car park's pavement)
	for (const FVector& P : E) Add(P);
	Add(SpLand(E.Last().X / 100.f, -19.8f));       // (up onto the forecourt pavement)
	Add(LandSpot(S));                              // (west along it, clear of the kerb and the halls' doors)
}

void UStaffPeople::LandWayIn(FStaff& S) const
{
	S.Way.Reset();
	auto Add = [&S](const FVector& P) { FLeg L; L.At = P; S.Way.Add(L); };
	const TArray<FVector> E = SpCrossing();
	Add(SpLand(E.Last().X / 100.f, -19.8f));
	for (int32 i = E.Num() - 1; i >= 0; --i) Add(E[i]);
	Add(E[0] + FVector(0.f, 150.f, 0.f));
	Add(SpLand(SpLandCarX, SpLandCarZ));
}

// (made, or gone, at the car park only where no camera of yours can see it: to-do 294; asked a few times a second)
bool UStaffPeople::LandUnseen(const FVector& At) const
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextLook) return false;
	NextLook = Now + 0.3 + FMath::FRand() * 0.1;
	return ADPUnseen::May(GetWorld(), At);
}

void UStaffPeople::WayOut(FStaff& S) const
{
	const float X = Rooms[S.Room].X0;
	const FVector Sp = SpotOf(S);
	auto Add = [&S](float Px, float Pz, bool bDoor = false) { FLeg L; L.At = SpPlan(Px, Pz); L.bDoor = bDoor; S.Way.Add(L); };
	S.Way.Reset();
	Add(X + 2.8f, 78.2f);                  // (past the carts on the west side of the room, clear of the leaf's swing)
	Add(X + 5.2f, 79.f, true);
	Add(X + 5.2f, 85.9f, true);
	Add(X + 2.6f, 86.f, true);             // (round the open leaf)
	Add(Sp.X / 100.f, 85.6f);
	Add(Sp.X / 100.f, 84.2f);
}

void UStaffPeople::WayIn(FStaff& S) const
{
	const float X = Rooms[S.Room].X0;
	auto Add = [&S](float Px, float Pz, bool bDoor = false) { FLeg L; L.At = SpPlan(Px, Pz); L.bDoor = bDoor; S.Way.Add(L); };
	S.Way.Reset();
	if (const AActor* A = S.A.Get())
	{
		const FVector Me = A->GetActorLocation();
		if (FMath::Abs(Me.X / 100.f - (X + 8.4f)) > 30.f) AlongWall(S, SpPlan(X + 8.4f, 87.2f));   // (from a far stand)
		else Add(Me.X / 100.f, FMath::Max(87.2f, SpPlanZ(Me)));   // (out of the row, along in front of it)
	}
	Add(X + 8.4f, 87.2f);                  // (waits here while others come out)
	Add(X + 5.2f, 86.f, true);
	Add(X + 5.2f, 78.7f, true);
	Add(X + 2.8f, 78.2f, true);
	Add(X + 2.2f, 71.f);
}

bool UStaffPeople::EnterDoor(FStaff& S, int8 Dir)
{
	// (one door: people going out or coming in, never both at once - the room is too narrow to pass each other)
	FRoom& R = Rooms[S.Room];
	if (S.bInRoom) return true;
	if (R.Dir && R.Dir != Dir) return false;
	R.Dir = Dir; ++R.Inside; S.bInRoom = true;
	return true;
}

void UStaffPeople::LeaveDoor(FStaff& S)
{
	if (!S.bInRoom || !Rooms.IsValidIndex(S.Room)) return;
	FRoom& R = Rooms[S.Room];
	S.bInRoom = false; R.Inside = FMath::Max(0, R.Inside - 1); if (!R.Inside) R.Dir = 0;
}

// the ramp rooms' door for somebody else (the ambulift's wheelchairs): the whole room while they have it
int32 UStaffPeople::RampNear(float StandX, float& X0) const
{
	int32 B = INDEX_NONE; float Bd = 1e9f;
	for (int32 r = 0; r < Rooms.Num(); ++r) { const float Ds = FMath::Abs(Rooms[r].X0 + 5.f - StandX); if (!Rooms[r].bNoDoor && Ds < Bd && ABabylonModel::StaffDoor(DoorOf(Rooms[r]), false) >= 0.f) { Bd = Ds; B = r; X0 = Rooms[r].X0; } }
	return B;
}
bool UStaffPeople::RampTake(int32 R) { if (!Rooms.IsValidIndex(R)) return true; if (Rooms[R].Dir) return false; Rooms[R].Dir = 3; ++Rooms[R].Inside; return true; }
void UStaffPeople::RampFree(int32 R) { if (Rooms.IsValidIndex(R) && Rooms[R].Dir == 3) { Rooms[R].Inside = FMath::Max(0, Rooms[R].Inside - 1); Rooms[R].Dir = 0; } }
bool UStaffPeople::RampSeen(int32 R) const { return Rooms.IsValidIndex(R) && PlayerInRoom(Rooms[R]); }

bool UStaffPeople::PlayerInRoom(const FRoom& R) const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return false;
	auto In = [&](const FVector& L) { const float X = L.X / 100.f, Z = -L.Y / 100.f; return X > R.X0 - 1.f && X < R.X0 + 29.f && Z > 67.f && Z < 82.5f && L.Z < 560.f; };
	if (const APawn* P = PC->GetPawn()) if (In(P->GetActorLocation())) return true;
	return PC->PlayerCameraManager && In(PC->PlayerCameraManager->GetCameraLocation());
}

bool UStaffPeople::Blocked(const FVector& At) const
{
	if (AADPPeople* P = AADPPeople::Get(GetWorld())) if (P->AnyoneNear(At, 38.f)) return true;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	return Me && FMath::Abs(Me->GetActorLocation().Z - At.Z) < 200.f && FVector::DistSquared2D(Me->GetActorLocation(), At) < FMath::Square(55.f);
}

void UStaffPeople::Turn(FStaff& S, float Yaw, float Dt) const
{
	if (AActor* A = S.A.Get()) A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, Yaw - 90.f, 0.f), Dt, 4.f));   // (a MetaHuman faces along its actor's +Y)
}

bool UStaffPeople::Walk(FStaff& S, float Dt)
{
	AActor* A = S.A.Get(); if (!A) return true;
	static FRoom NoRoom;
	FRoom& R = Rooms.IsValidIndex(S.Room) ? Rooms[S.Room] : NoRoom;
	const FVector Here = A->GetActorLocation();
	FVector Dir = FVector::ZeroVector; float Dist = 0.f, Want = 0.f;
	while (S.Way.Num())
	{
		Dir = S.Way[0].At - Here; Dir.Z = 0.f; Dist = Dir.Size();
		if (Dist > (S.Way.Num() == 1 ? 12.f : 25.f)) break;
		// (outside and round the leaf: the door is free for the others)
		if (S.Way[0].bDoor && S.State == EState::Out && (S.Way.Num() == 1 || !S.Way[1].bDoor)) LeaveDoor(S);
		S.Way.RemoveAt(0);
	}
	if (S.Way.Num())
	{
		const bool bDoor = S.Way[0].bDoor, bOpenApron = !bDoor && !S.bLand && SpPlanZ(Here) > 85.3f;
		// (landside a step round somebody only on the pavement or the crossing - never into the kerbside lane's or the car park's cars)
		auto LandOk = [](float Z) { return Z < -4.5f && Z > -51.f && (Z > -21.3f || Z < -25.3f); };
		const bool bSide = bOpenApron || (S.bLand && LandOk(SpPlanZ(Here)));
		bool bGo = true;
		if (bDoor)
		{
			// through the ramp room's door: the badge opens it, they wait until it stands open and it stays open for them
			bGo = EnterDoor(S, S.State == EState::Leave ? 2 : 1);
			if (bGo)
			{
				const float K = ABabylonModel::StaffDoor(DoorOf(R), true);
				if (K < 0.97f) bGo = false;
			}
		}
		// (round the vehicles in the way)
		const FVector To = bOpenApron && Tugs.IsValid() ? Tugs->Around(Here, S.Way[0].At) : S.Way[0].At;
		Dir = To - Here; Dir.Z = 0.f;
		if (!Dir.Normalize()) Dir = (S.Way[0].At - Here).GetSafeNormal2D();
		if (bGo)
		{
			if (S.SideT > 0.f && bSide) { S.SideT -= Dt; Dir = S.SideDir; Dist = 1e6f; } else S.SideT = 0.f;
			if (Blocked(Here + Dir * 75.f))
			{
				// someone in the way: wait, then on the open apron a step round them - never through
				bGo = false; S.SideT = 0.f;
				if ((S.BlockedT += Dt) > 1.5f && bSide)
					for (const float Sg : {1.f, -1.f})
					{
						const FVector Sd = FVector(-Dir.Y, Dir.X, 0.f) * Sg;
						if ((S.bLand ? !LandOk(SpPlanZ(Here + Sd * 110.f)) : SpPlanZ(Here + Sd * 110.f) < 85.3f) || Blocked(Here + Sd * 75.f)) continue;
						S.SideDir = Sd; S.SideT = 0.8f; S.BlockedT = 0.f; break;
					}
			}
			else S.BlockedT = 0.f;
		}
		Want = bGo ? FMath::Min(140.f, S.Way.Num() == 1 ? FMath::Max(50.f, Dist * 0.9f) : 140.f) : 0.f;
	}
	S.Speed = S.Way.Num() && Want > 0.f ? FMath::FInterpTo(S.Speed, Want, Dt, 3.f) : FMath::FInterpTo(S.Speed, 0.f, Dt, 10.f);
	if (Want > 0.f)
	{
		FVector P = Here + Dir * FMath::Min(Dist, S.Speed * Dt);
		P.Z = S.bLand ? SpLand(P.X / 100.f, SpPlanZ(P)).Z : SpPlan(P.X / 100.f, SpPlanZ(P)).Z;
		A->SetActorLocation(P);
		A->SetActorRotation(FMath::RInterpTo(A->GetActorRotation(), FRotator(0.f, Dir.Rotation().Yaw - 90.f, 0.f), Dt, 6.f));
	}
	else if (!S.Way.Num()) S.Speed = 0.f;
	if (USkeletalMeshComponent* B = SpBodyOf(GetWorld(), A))
		if (UAnimSingleNodeInstance* One = B->GetSingleNodeInstance())
		{
			One->SetBlendSpacePosition(FVector(0.f, FMath::Min(S.Speed * 1.5f, 300.f), 0.f));
			One->SetPlayRate(S.Speed > 20.f ? FMath::Clamp(S.Speed / FMath::Max(S.Speed * 1.5f, 100.f), 0.5f, 1.4f) : 1.f);
		}
	return !S.Way.Num();
}
