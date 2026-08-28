#include "TrialArenaAnchor.h"
#include "Components/BillboardComponent.h"

ATrialArenaAnchor::ATrialArenaAnchor()
{
	PrimaryActorTick.bCanEverTick = false;
	EditorSprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("TrialArenaAnchor"));
	SetRootComponent(EditorSprite);
	EditorSprite->SetHiddenInGame(true);
	EditorSprite->SetRelativeScale3D(FVector(2.0f));
}

