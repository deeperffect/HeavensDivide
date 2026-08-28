#include "ObjectiveSpawnPoint.h"
#include "Components/BillboardComponent.h"

AObjectiveSpawnPoint::AObjectiveSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	EditorSprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("ObjectiveSpawnPoint"));
	SetRootComponent(EditorSprite);
	EditorSprite->SetHiddenInGame(true);
	EditorSprite->SetRelativeScale3D(FVector(1.5f));
#if WITH_EDITORONLY_DATA
	EditorSprite->bIsScreenSizeScaled = true;
#endif
}

