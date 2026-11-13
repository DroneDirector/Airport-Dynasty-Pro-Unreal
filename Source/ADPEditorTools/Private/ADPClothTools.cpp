#include "ADPClothTools.h"
#include "Dataflow/DataflowInstance.h"
#include "Dataflow/DataflowObject.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ADPEditorTools);

// Marking a variable as overridden is private to Unreal's own details panel (its friend class, which lives in the
// DataflowEditor module we don't link); the same friend here reaches the exported setter
namespace UE::Dataflow::Private
{
	class FVariablesOverridesDetails
	{
	public:
		static void MarkOverridden(FDataflowVariableOverrides& O, const FGuid& Id) { O.SetVariableOverridden(Id, true); }
	};
}

static FDataflowInstance* InstanceOf(UObject* Asset)
{
	IDataflowInstanceInterface* I = Cast<IDataflowInstanceInterface>(Asset);
	return I ? &I->GetDataflowInstance() : nullptr;
}

bool UADPClothTools::SetDataflow(UObject* Asset, UObject* Dataflow, FName Terminal)
{
	FDataflowInstance* I = InstanceOf(Asset); UDataflow* D = Cast<UDataflow>(Dataflow);
	if (!I || !D) return false;
	Asset->Modify();
	I->SetDataflowAsset(D);
	I->SetDataflowTerminal(Terminal);
	I->SyncVariables();
	Asset->MarkPackageDirty();
	return true;
}

bool UADPClothTools::SetSizedOutfitSources(UObject* Asset, FName Variable, const TArray<FChaosSizedOutfitSource>& Sources)
{
	FDataflowInstance* I = InstanceOf(Asset);
	if (!I) return false;
	Asset->Modify();
	I->SyncVariables();
	FDataflowVariableOverrides& O = I->GetVariableOverrides();
	FInstancedPropertyBag& Bag = O.GetVariables();
	const FPropertyBagPropertyDesc* Desc = Bag.FindPropertyDescByName(Variable);
	if (!Desc) return false;
	TValueOrError<FPropertyBagArrayRef, EPropertyBagResult> Arr = Bag.GetMutableArrayRef(Variable);
	if (Arr.HasError()) return false;
	FPropertyBagArrayRef& A = Arr.GetValue();
	A.EmptyValues();
	A.AddValues(Sources.Num());
	for (int32 i = 0; i < Sources.Num(); ++i)
		if (A.SetValueStruct(i, Sources[i]) != EPropertyBagResult::Success) return false;
	UE::Dataflow::Private::FVariablesOverridesDetails::MarkOverridden(O, Desc->ID);
	Asset->MarkPackageDirty();
	return true;
}

bool UADPClothTools::Regenerate(UObject* Asset)
{
	FDataflowInstance* I = InstanceOf(Asset);
	return I && I->UpdateOwnerAsset(true);
}

FString UADPClothTools::DescribeDataflow(UObject* Asset)
{
	FDataflowInstance* I = InstanceOf(Asset);
	if (!I) return TEXT("(no dataflow instance)");
	FString S = FString::Printf(TEXT("dataflow=%s terminal=%s vars:"), *GetPathNameSafe(I->GetDataflowAsset()), *I->GetDataflowTerminal().ToString());
	const FInstancedPropertyBag& V = I->GetVariables();
	if (const UPropertyBag* B = V.GetPropertyBagStruct())
		for (const FPropertyBagPropertyDesc& Desc : B->GetPropertyDescs())
			S += FString::Printf(TEXT(" %s(%s)"), *Desc.Name.ToString(), Desc.CachedProperty ? *Desc.CachedProperty->GetCPPType() : TEXT("?"));
	return S;
}
