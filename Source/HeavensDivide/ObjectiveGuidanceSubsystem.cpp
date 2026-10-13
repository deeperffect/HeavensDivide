#include "ObjectiveGuidanceSubsystem.h"
#include "ObjectiveGuidanceWidget.h"
#include "MinimapMarkerComponent.h"
#include "SurvivalFieldObjective.h"
#include "SurvivorPlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
bool IsAvailable(AActor* Objective)
{
    if(!IsValid(Objective) || Objective->IsActorBeingDestroyed() || Objective->IsHidden()) return false;
    const auto* Marker=Objective->FindComponentByClass<UMinimapMarkerComponent>();
    return Marker && Marker->IsMarkerVisible() && Marker->GetMarkerState()!=EMinimapMarkerState::Locked;
}
}

void UObjectiveGuidanceSubsystem::Announce(AActor* Objective)
{
    if(!IsAvailable(Objective) || Objectives.Contains(Objective)) return;
    Objectives.Add(Objective);PendingPopups.Add(Objective);
}

bool UObjectiveGuidanceSubsystem::FindGuidanceTarget(AActor* Objective,const FVector& Player,FVector& Target)
{
    if(!IsAvailable(Objective)) return false;
    const auto* Marker=Objective->FindComponentByClass<UMinimapMarkerComponent>();
    Target=Marker->GetMarkerWorldLocation();
    if(const auto* Field=Cast<ASurvivalFieldObjective>(Objective))
    {
        if(Field->IsWallTrap())
        {
            if(FVector::DistSquared2D(Player,Target)>FMath::Square(Field->Radius+120.f)) return false;
            FVector Direction=(Player-Target).GetSafeNormal2D();
            if(Direction.IsNearlyZero()) Direction=FVector::ForwardVector;
            Target+=Direction*Field->Radius;
            return true;
        }
        if(Field->IsHoldActive() && Field->ContainsPlayer(Player)) return false;
    }
    else if(Marker->GetMarkerState()==EMinimapMarkerState::Active) return false;
    return FVector::DistSquared2D(Player,Target)>FMath::Square(300.f);
}

void UObjectiveGuidanceSubsystem::Tick(float Delta)
{
    Objectives.RemoveAll([](const auto& Entry){return !IsAvailable(Entry.Get());});
    PendingPopups.RemoveAll([](const auto& Entry){return !IsAvailable(Entry.Get());});
    auto* PC=Cast<ASurvivorPlayerController>(GetWorld()->GetFirstPlayerController());
    if(!PC || !PC->GetLocalPlayer() || !PC->IsLocalController()) return;
    if(!PC->IsRunInProgress() || PC->IsPlayerDead()) { ClearPresentation();return; }
    if(PC->IsObjectiveIntroPlaying() || PC->IsSelectingUpgrade() || UGameplayStatics::IsGamePaused(PC)
        || UGameplayStatics::GetGlobalTimeDilation(PC)<.01f) return;
    if(!PC->GetPawn()) return;
    if(!Widget && !Objectives.IsEmpty())
    {
        Widget=CreateWidget<UObjectiveGuidanceWidget>(PC);
        if(Widget) Widget->AddToViewport(65);
    }
    if(!Widget) return;
    PopupAge+=FMath::Max(0.f,Delta);
    if((PopupAge>=UObjectiveGuidanceWidget::PopupDuration || !IsAvailable(PopupOwner.Get())) && !PendingPopups.IsEmpty())
    {
        PopupOwner=PendingPopups[0];PendingPopups.RemoveAt(0);PopupAge=0;
    }
    FText Popup;
    if(IsAvailable(PopupOwner.Get())) Popup=PopupOwner->FindComponentByClass<UMinimapMarkerComponent>()->DisplayName;
    const FVector Player=PC->GetPawn()->GetActorLocation();
    FVector2D Origin;
    TArray<FObjectiveArrow> Arrows;
    if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Player,Origin,true))
    {
        for(const auto& Entry:Objectives)
        {
            FVector Target;FVector2D Screen;
            if(!FindGuidanceTarget(Entry.Get(),Player,Target)) continue;
            FVector2D Direction;
            if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Target,Screen,true)) Direction=(Screen-Origin).GetSafeNormal();
            else if(PC->PlayerCameraManager)
            {
                const FVector Offset=Target-Player;
                const FRotationMatrix Camera(PC->PlayerCameraManager->GetCameraRotation());
                Direction=FVector2D(FVector::DotProduct(Offset,Camera.GetUnitAxis(EAxis::Y)),-FVector::DotProduct(Offset,Camera.GetUnitAxis(EAxis::Z))).GetSafeNormal();
            }
            if(Direction.IsNearlyZero()) continue;
            const auto* Field=Cast<ASurvivalFieldObjective>(Entry.Get());
            const FLinearColor Color=Field?(Field->IsWallTrap()?FLinearColor(1,.46f,.18f):FLinearColor(.35f,1,.7f)):FLinearColor(1,.45f,.38f);
            Arrows.Add({Origin,Direction,Color});
        }
    }
    Widget->Present(Arrows,Popup,PopupAge);
}

void UObjectiveGuidanceSubsystem::Deinitialize()
{
    ClearPresentation();
    Super::Deinitialize();
}
void UObjectiveGuidanceSubsystem::ClearPresentation()
{
    if(Widget) { Widget->RemoveFromParent();Widget=nullptr; }
    Objectives.Reset();PendingPopups.Reset();PopupOwner.Reset();
}
