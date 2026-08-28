#include "TrialArenaSubsystem.h"
#include "EngineUtils.h"
#include "TrialArenaAnchor.h"

ATrialArenaAnchor* UTrialArenaSubsystem::ReserveAnchor(AActor* TrialOwner)
{
	if (!IsValid(TrialOwner) || !GetWorld()) return nullptr;
	Reservations.RemoveAll([](const FTrialArenaReservation& Entry){ return !Entry.Anchor.IsValid() || !Entry.Owner.IsValid(); });
	for (const FTrialArenaReservation& Entry : Reservations) if (Entry.Owner.Get() == TrialOwner) return Entry.Anchor.Get();
	TArray<ATrialArenaAnchor*> Anchors;
	for (TActorIterator<ATrialArenaAnchor> It(GetWorld()); It; ++It) Anchors.Add(*It);
	Anchors.Sort([](const ATrialArenaAnchor& A, const ATrialArenaAnchor& B){ return A.GetName() < B.GetName(); });
	for (ATrialArenaAnchor* Anchor : Anchors)
	{
		const bool bUsed = Reservations.ContainsByPredicate([Anchor](const FTrialArenaReservation& Entry){ return Entry.Anchor.Get() == Anchor; });
		if (!bUsed)
		{
			FTrialArenaReservation& Reservation = Reservations.AddDefaulted_GetRef();
			Reservation.Anchor = Anchor;
			Reservation.Owner = TrialOwner;
			UE_LOG(LogTemp, Log, TEXT("[TrialRuntime] Reserved Anchor=%s Trial=%s"), *GetNameSafe(Anchor), *GetNameSafe(TrialOwner));
			return Anchor;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("[TrialRuntime] ERROR No unreserved TrialArenaAnchor for %s"), *GetNameSafe(TrialOwner));
	return nullptr;
}

void UTrialArenaSubsystem::ReleaseAnchor(AActor* TrialOwner)
{
	Reservations.RemoveAll([TrialOwner](const FTrialArenaReservation& Entry){ return !Entry.Owner.IsValid() || Entry.Owner.Get() == TrialOwner; });
}

