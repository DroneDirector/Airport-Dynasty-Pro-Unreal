// Nanite copies of Unreal's basic shapes (owner, 2026-11-20, to-do 149): the hangars and the control tower are built from
// thousands of cubes and cylinders; the engine's own have no Nanite data, so their shadows went the slow non-Nanite way.
// /Game/Shapes/SM_Cube and SM_Cylinder (Tools/make_nanite_shapes.py) - the same shapes, Nanite on; the engine's if missing.
#pragma once
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace ADPShapes
{
	inline UStaticMesh* Load(const TCHAR* Ours, const TCHAR* Engine)
	{
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Ours, nullptr, LOAD_NoWarn | LOAD_Quiet)) return M;
		return LoadObject<UStaticMesh>(nullptr, Engine);
	}
	inline UStaticMesh* Cube() { return Load(TEXT("/Game/Shapes/SM_Cube.SM_Cube"), TEXT("/Engine/BasicShapes/Cube.Cube")); }
	inline UStaticMesh* Cylinder() { return Load(TEXT("/Game/Shapes/SM_Cylinder.SM_Cylinder"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); }
	/** Nanite draws opaque and masked materials only: a see-through part keeps the ordinary way (no warning, drawn right). */
	inline void NoNaniteIfSeeThrough(UStaticMeshComponent* C)
	{
		if (!C) return;
		for (int32 i = 0; i < C->GetNumMaterials(); ++i)
			if (UMaterialInterface* M = C->GetMaterial(i)) if (IsTranslucentBlendMode(M->GetBlendMode())) { C->bDisallowNanite = true; C->MarkRenderStateDirty(); return; }
	}
}
