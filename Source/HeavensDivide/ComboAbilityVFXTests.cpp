#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ComboAbilityDefinition.h"
#include "JointComboActor.h"
#include "SamuraiCharacter.h"
#include "EnemyBase.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboAbilityVFXTest, "HeavensDivide.Combat.ComboAbilityVFX",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FComboAbilityVFXTest::RunTest(const FString&)
{
    auto* W = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    W->InitializeActorsForPlay(FURL());
    auto* Hero = W->SpawnActor<ASamuraiCharacter>();
    auto* J = W->SpawnActor<AJointComboActor>();
    auto* D = DuplicateObject(ComboCatalog::Definition(TEXT("BloodMoon")), GetTransientPackage());
    J->Data = D; J->Active = Hero; J->Origin = Hero->GetActorLocation(); J->Direction = FVector::RightVector;
    D->bTintVFX = false;
    J->Burst(D->VFX, J->Origin, .73f, .41f, FLinearColor::Green, FRotator(0,27,0), FVector(40,20,30));
    TestEqual(TEXT("One independent burst"), J->Effects.Num(), 1);
    if (J->Effects.Num() == 1) {
        auto* FX = J->Effects[0].Effect.Get();
        TestTrue(TEXT("VFX uses requested scale"), FX->GetComponentScale().Equals(FVector(.73f), .001));
        TestTrue(TEXT("VFX rotation follows cast yaw plus authored rotation"), FX->GetComponentRotation().Equals(FRotator(0,117,0), .001));
        TestTrue(TEXT("Local offset is applied after ground anchoring"), FX->GetComponentLocation().Equals(J->Ground(J->Origin)+FVector(-20,40,30), .001));
        TestEqual(TEXT("Emission uses requested visibility duration"), J->Effects[0].Stop, .41f);
        TArray<FNiagaraVariable> Params; D->VFX->GetExposedParameters().GetParameters(Params);
        for (const auto& P : Params) if (P.GetType() == FNiagaraTypeDefinition::GetColorDef()) {
            const int32 Index = FX->GetOverrideParameters().IndexOf(P);
            TestTrue(TEXT("Tint opt-out preserves authored color"), Index == INDEX_NONE || FX->GetOverrideParameters().GetParameterValue<FLinearColor>(P) == D->VFX->GetExposedParameters().GetParameterValue<FLinearColor>(P));
        }
    }
    for (const TCHAR* Name : {TEXT("VFX"),TEXT("VFXOffset"),TEXT("VFXScale"),TEXT("VFXRotation"),TEXT("VFXSeconds"),TEXT("bTintVFX"),TEXT("VFXCompletionSeconds"),TEXT("ChargeVFX"),TEXT("ChargeVFXScale"),TEXT("ChargeVFXOffset"),TEXT("ChargeVFXRotation"),TEXT("ChargeVFXColor"),TEXT("ChargeVFXSeconds"),TEXT("SecondaryVFX"),TEXT("SecondaryVFXScale"),TEXT("SecondaryVFXOffset"),TEXT("SecondaryVFXRotation"),TEXT("SecondaryVFXColor"),TEXT("SecondaryVFXSeconds"),TEXT("FangPresentationScale"),TEXT("NinjaAlternateMontage"),TEXT("HelperArrivalVFX"),TEXT("HelperDepartureVFX"),TEXT("HelperSmokeScale"),TEXT("HelperSmokeOffset"),TEXT("HelperSmokeRotation"),TEXT("HelperSmokeSeconds"),TEXT("HitVFX"),TEXT("HitVFXScale"),TEXT("HitVFXOffset"),TEXT("HitVFXRotation"),TEXT("HitVFXSeconds"),TEXT("HitVFXInterval"),TEXT("ReleaseStartSpeed"),TEXT("ReleaseRampSeconds"),TEXT("HelperArrivalFraction")}) {
        const auto* Property = FindFProperty<FProperty>(UComboAbilityDefinition::StaticClass(), Name);
        TestTrue(Name, Property && Property->HasAnyPropertyFlags(CPF_Edit));
    }
    // Some enemy death Blueprints remove the actor immediately, before damage
    // returns. A captured hit location must still show a detached killing burst.
    auto* Victim=W->SpawnActor<AEnemyBase>(FVector(310,200,120),FRotator::ZeroRotator);
    const FVector Contact=Victim->GetActorLocation();Victim->Destroy();
    const int32 BeforeKill=J->Effects.Num();J->PlayHitImpact(Victim,Contact);
    TestEqual(TEXT("Immediate enemy destruction retains killing impact"),J->Effects.Num(),BeforeKill+1);
    if(J->Effects.Num()>BeforeKill) TestTrue(TEXT("Killing impact uses position captured before death"),J->Effects.Last().Effect->GetComponentLocation().Equals(Contact+J->Direction.Rotation().RotateVector(D->HitVFXOffset),.01));
    TestEqual(TEXT("Only three active enum choices remain"), StaticEnum<EComboTechnique>()->NumEnums()-2, 3);
    TestNull(TEXT("Removed solo ID cannot load a definition"), ComboCatalog::Definition(TEXT("VoidStep")));
    J->Destroy(); W->DestroyWorld(false); GEngine->DestroyWorldContext(W);
    return true;
}
#endif
