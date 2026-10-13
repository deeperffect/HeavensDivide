#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "CharacterBase.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "HeavensDivideGameUserSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "EnhancedPlayerInput.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Kismet/GameplayStatics.h"

class FMenuMovementInput : public IInputProcessor
{
public:
    TMap<FKey, float> Values;
    bool bRestorePending=false;
    void Tick(float, FSlateApplication& App, TSharedRef<ICursor>) override
    { if (!App.IsActive()) for (auto& Pair:Values) Pair.Value=0.f; }
    void Remember(FKey Key, float Value) { if (float* Existing=Values.Find(Key)) *Existing=Value; }
    bool HandleKeyDownEvent(FSlateApplication&,const FKeyEvent& E) override { Remember(E.GetKey(),1); return false; }
    bool HandleKeyUpEvent(FSlateApplication&,const FKeyEvent& E) override { Remember(E.GetKey(),0); return false; }
    bool HandleAnalogInputEvent(FSlateApplication&,const FAnalogInputEvent& E) override { Remember(E.GetKey(),E.GetAnalogValue()); return false; }
    bool HandleMouseButtonDownEvent(FSlateApplication&,const FPointerEvent& E) override { Remember(E.GetEffectingButton(),1); return false; }
    bool HandleMouseButtonUpEvent(FSlateApplication&,const FPointerEvent& E) override { Remember(E.GetEffectingButton(),0); return false; }
};

void ASurvivorPlayerController::BeginMenuMovement()
{
    if (bMenuMovementBlocked) return;
    if (!MenuMovementInput && FSlateApplication::IsInitialized())
    {
        MenuMovementInput=MakeShared<FMenuMovementInput>();
        FSlateApplication::Get().RegisterInputPreProcessor(MenuMovementInput,0);
    }
    if (MenuMovementInput)
    {
        TSet<FKey> Keys;
        const auto* Settings=UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
        for (FName Action:{FName(TEXT("MoveForward")),FName(TEXT("MoveBackward")),FName(TEXT("MoveLeft")),FName(TEXT("MoveRight"))})
            Keys.Add(Settings?Settings->GetKeyBinding(Action):UHeavensDivideGameUserSettings::GetDefaultBinding(Action));
        if (RuntimeMappingContext) for (const auto& Mapping:RuntimeMappingContext->GetMappings())
            if (Mapping.Action==MoveAction && !Mapping.Key.IsAnalog()) Keys.Add(Mapping.Key);
        Keys.Add(EKeys::Gamepad_LeftX); Keys.Add(EKeys::Gamepad_LeftY);
        for (FKey Key:Keys) if (!MenuMovementInput->Values.Contains(Key))
            MenuMovementInput->Values.Add(Key,PlayerInput?PlayerInput->GetKeyValue(Key):0.f);
        for (auto It=MenuMovementInput->Values.CreateIterator();It;++It) if (!Keys.Contains(It.Key())) It.RemoveCurrent();
    }
    bMenuMovementBlocked=true; bMenuMovementReconcile=false;
    LastMovementInput=FVector2D::ZeroVector;
    ACharacterBase* Characters[] = {CharacterManager?CharacterManager->GetSamurai():nullptr,CharacterManager?CharacterManager->GetNinja():nullptr};
    for (auto* MenuCharacter:Characters)
        if (MenuCharacter) { MenuCharacter->ConsumeMovementInputVector(); MenuCharacter->GetCharacterMovement()->StopMovementImmediately(); }
}
void ASurvivorPlayerController::EndMenuMovement()
{
    if (!bMenuMovementBlocked) return;
    bMenuMovementBlocked=false; bMenuMovementReconcile=true;
    if (MenuMovementInput) MenuMovementInput->bRestorePending=false;
    if (FSlateApplication::IsInitialized() && GetLocalPlayer()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
}
void ASurvivorPlayerController::ReconcileMenuMovement()
{
    if (!bMenuMovementReconcile || bMenuMovementBlocked || bLevelUpSelectionActive || UGameplayStatics::IsGamePaused(this)) return;
    LastMovementInput=FVector2D::ZeroVector;
    if (!PlayerInput || !MenuMovementInput) { bMenuMovementReconcile=false; return; }
    if (MenuMovementInput->bRestorePending)
    {
        bMenuMovementReconcile=false; MenuMovementInput->bRestorePending=false;
        for (const auto& Pair:MenuMovementInput->Values)
            PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(Pair.Key,
                Pair.Key.IsAnalog()?IE_Axis:FMath::IsNearlyZero(Pair.Value)?IE_Released:IE_Pressed,Pair.Value));
        return;
    }
    // Let Enhanced Input evaluate one released movement frame, clearing UE 5.8's
    // ignore-until-release state. Restore physical movement on the next frame.
    // Held ability buttons retain their normal flush protection.
    PlayerInput->FlushPressedKeys();
    TSet<FKey> Keys;
    for (const auto& Pair:MenuMovementInput->Values)
    { Keys.Add(Pair.Key); if (Pair.Key.GetPairedAxisKey().IsValid()) Keys.Add(Pair.Key.GetPairedAxisKey()); }
    for (FKey Key:Keys) if (auto* State=PlayerInput->GetKeyState(Key))
    {
        State->RawValueAccumulator=FVector::ZeroVector; State->SampleCountAccumulator=0; State->PairSampledAxes=0;
        for (auto& Events:State->EventAccumulator) Events.Reset();
    }
    for (const auto& Pair:MenuMovementInput->Values)
        PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(Pair.Key,Pair.Key.IsAnalog()?IE_Axis:IE_Released,0.f));
    MenuMovementInput->bRestorePending=true;
}
void ASurvivorPlayerController::ReleaseMenuMovement()
{
    if (MenuMovementInput && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(MenuMovementInput);
    MenuMovementInput.Reset();
}
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuMovementRegressionTest,"HeavensDivide.Input.MenuMovement",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMenuMovementRegressionTest::RunTest(const FString&)
{
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();
 auto* World=GI->GetWorld();World->InitializeActorsForPlay(FURL());
 auto* PC=World->SpawnActor<ASurvivorPlayerController>();
 auto* Local=NewObject<ULocalPlayer>(GEngine);Local->PlayerAdded(nullptr,0);PC->SetPlayer(Local);
 PC->PlayerInput=NewObject<UEnhancedPlayerInput>(PC);
 const auto* Settings=UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
 const FKey Key=Settings?Settings->GetKeyBinding(TEXT("MoveForward")):EKeys::W;
 PC->MoveAction=NewObject<UInputAction>(PC);PC->MoveAction->ValueType=EInputActionValueType::Axis2D;
 PC->DashAction=NewObject<UInputAction>(PC);
 PC->RuntimeMappingContext=NewObject<UInputMappingContext>(PC);
 PC->RuntimeMappingContext->MapKey(PC->MoveAction,Key);PC->RuntimeMappingContext->MapKey(PC->DashAction,EKeys::SpaceBar);
 FModifyContextOptions Options;Options.bForceImmediately=true;Options.bIgnoreAllPressedKeysUntilRelease=false;
 auto* Subsystem=Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
 if(TestNotNull(TEXT("Real Enhanced Input subsystem"),Subsystem)) Subsystem->AddMappingContext(PC->RuntimeMappingContext,0,Options);
 PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Pressed,1));
 PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar,IE_Pressed,1));
 {TArray<UInputComponent*> Stack;PC->PlayerInput->ProcessInputStack(Stack,.016f,false);}
 PC->BeginMenuMovement();
 if(TestTrue(TEXT("Menu tracks current physical movement"),PC->MenuMovementInput.IsValid()))
 {
  auto& Tracker=*PC->MenuMovementInput;
  Tracker.HandleKeyDownEvent(FSlateApplication::Get(),FKeyEvent(Key,FModifierKeysState(),uint32(0),false,0,0));
  PC->EndMenuMovement();PC->ReconcileMenuMovement();
  { TArray<UInputComponent*> Stack; PC->PlayerInput->ProcessInputStack(Stack,.016f,false); PC->ReconcileMenuMovement(); PC->PlayerInput->ProcessInputStack(Stack,.016f,false); }
  TestEqual(TEXT("Held movement is re-injected after choice without another press"),PC->PlayerInput->GetKeyState(Key)->RawValue.X,1.);
  auto* Enhanced=Cast<UEnhancedPlayerInput>(PC->PlayerInput);
  TestTrue(TEXT("Held movement action remains valid after UE 5.8 ignores held keys on flush"),!Enhanced->GetActionValue(PC->MoveAction).Get<FVector2D>().IsNearlyZero());
  TestFalse(TEXT("Held dash does not re-trigger during movement restoration"),Enhanced->GetActionValue(PC->DashAction).Get<bool>());
  PC->BeginMenuMovement();Tracker.HandleKeyUpEvent(FSlateApplication::Get(),FKeyEvent(Key,FModifierKeysState(),uint32(0),false,0,0));PC->LastMovementInput=FVector2D(0,1);
  PC->EndMenuMovement();PC->ReconcileMenuMovement();
  { TArray<UInputComponent*> Stack; PC->PlayerInput->ProcessInputStack(Stack,.016f,false); PC->ReconcileMenuMovement(); PC->PlayerInput->ProcessInputStack(Stack,.016f,false); }
  TestTrue(TEXT("Released movement cannot stay latched after chest"),PC->LastMovementInput.IsNearlyZero());
  TestEqual(TEXT("Consumed key release clears stale Enhanced Input"),PC->PlayerInput->GetKeyState(Key)->RawValue.X,0.);
  PC->BeginMenuMovement();Tracker.Remember(EKeys::Gamepad_LeftX,.75f);PC->EndMenuMovement();PC->ReconcileMenuMovement();
  { TArray<UInputComponent*> Stack; PC->PlayerInput->ProcessInputStack(Stack,.016f,false); PC->ReconcileMenuMovement(); PC->PlayerInput->ProcessInputStack(Stack,.016f,false); }
  TestTrue(TEXT("Held stick survives a modal"),FMath::IsNearlyEqual(PC->PlayerInput->GetKeyState(EKeys::Gamepad_LeftX)->RawValue.X,.75,.001));
  PC->BeginMenuMovement();Tracker.Remember(EKeys::Gamepad_LeftX,0.f);PC->EndMenuMovement();PC->ReconcileMenuMovement();
  { TArray<UInputComponent*> Stack; PC->PlayerInput->ProcessInputStack(Stack,.016f,false); PC->ReconcileMenuMovement(); PC->PlayerInput->ProcessInputStack(Stack,.016f,false); }
  TestEqual(TEXT("Released stick stops after modal"),PC->PlayerInput->GetKeyState(EKeys::Gamepad_LeftX)->RawValue.X,0.);
 }
 PC->ReleaseMenuMovement();Local->PlayerRemoved();GI->Shutdown();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
