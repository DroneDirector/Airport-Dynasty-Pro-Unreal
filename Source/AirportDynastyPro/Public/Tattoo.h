// Tattoos on the MetaHumans (owner, 2026-11-15). The shared skin master (M_skin_unified_baked) has six tattoo layers
// (Tools/set_tattoos.py): each lays one design picture on one place of the skin - the left/right arm (down over the
// back of the hand), the left/right leg, the fingers of either hand (never the nails), the neck's left side, right
// side, back and throat. The places are maps in the skin's own layout (Art/Tattoos/make_spots.py), the designs one
// picture each in their place's frame (Art/Tattoos/make_designs.py, listed in Content/Data/tattoos.json). Any design
// on any place that fits it, several at once; chosen in the character creator (optional) and at the tattooist, who
// also takes them off again (the owner wants that, even for the creator's). A person wears theirs on their own copy of
// the skin material. Your own are saved (ADP.Player Tattoos); other people's come from who they are - 3 in 10 have
// some, the same ones every time they are seen - and are never added to or taken off.
#pragma once

#include "CoreMinimal.h"

class USkeletalMeshComponent;

namespace ADPTattoo
{
	/** One tattoo worn: a place ("arm_l", "neck_throat", ...) and a design id (tattoos.json). */
	struct FWorn { FString Place; FString Design; };
	/** One design (tattoos.json): Kind = the places it fits (arm, leg, neck, fingers). */
	struct FDesign { FString Id; FString Name; FString Kind; FString Picture; bool bColour = false; };

	/** The designs (Content/Data/tattoos.json). */
	const TArray<FDesign>& Designs();
	/** The places a kind of design fits (arm -> arm_l, arm_r ...). */
	TArray<FString> PlacesFor(const FString& Kind);
	/** Is the place on the head mesh (the neck) rather than the body? */
	bool IsHeadPlace(const FString& Place);

	/** Lays the list on a person (their body and their head mesh; six per mesh at most); anything not in the list comes
	 *  off. Returns how many were laid. */
	int32 Wear(USkeletalMeshComponent* Body, USkeletalMeshComponent* Face, const TArray<FWorn>& List);

	/** Your own tattoos (saved), as "place:design,place:design" in ADP.Player Tattoos. */
	TArray<FWorn> Saved();
	void Save(const TArray<FWorn>& List);
	/** The creator and the tattooist: a design added on a place (a place can hold several), or taken off again - one
	 *  design, or everything on the place when Design is empty. Saved. Six per mesh at most (the skin's layers). */
	TArray<FWorn> Add(const FString& Place, const FString& Design);
	TArray<FWorn> Remove(const FString& Place, const FString& Design = FString());
	/** "place:design,..." <-> list. */
	TArray<FWorn> Parse(const FString& Text);
	FString Format(const TArray<FWorn>& List);

	/** Someone else's tattoos: 3 in 10 people have some (an arm and/or the neck), chosen by who they are (Seed), so
	 *  the same person always has the same ones. Never changed. */
	TArray<FWorn> ForPerson(uint32 Seed);

	/** (The first sample: one picture already laid in the skin's layout - kept for the old test pictures.) */
	int32 Apply(USkeletalMeshComponent* Mesh, const FString& Picture);
}
