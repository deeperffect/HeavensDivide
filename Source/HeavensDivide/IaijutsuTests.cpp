#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AutoAttackComponent.h"
#include "SamuraiIaijutsu.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "EnemyBase.h"
#include "EnemyLightweightMovementComponent.h"
#include "NinjaCharacter.h"
#include "HealthComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CharacterManagerComponent.h"
#include "SwapAfterimage.h"
#include "SwapPresentationComponent.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "IaijutsuBuild.h"
#include "HeavensDivideGameUserSettings.h"
#include "Misc/ScopeExit.h"
#include "MouseGroundAim.h"
#include "CharacterStatsComponent.h"
#include "GameFramework/WorldSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIaijutsuTest, "HeavensDivide.Combat.Iaijutsu",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FIaijutsuTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const auto Cleanup = [&] { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
    if (!TestNotNull(TEXT("Targeting settings"), Settings)) { Cleanup(); return false; }
    const bool OriginalAutoTargeting = Settings->IsAutoTargetingEnabled();
    const auto SetAutoTargeting = [&](bool Enabled)
    {
        // Exercise the actual setting without saving changes to the user's profile.
        FindFProperty<FBoolProperty>(UHeavensDivideGameUserSettings::StaticClass(), TEXT("bAutoTargetingEnabled"))
            ->SetPropertyValue_InContainer(Settings, Enabled);
    };
    ON_SCOPE_EXIT { SetAutoTargeting(OriginalAutoTargeting); };
    SetAutoTargeting(true);
    auto* Class = LoadClass<ASurvivorPlayerController>(nullptr,
        TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    auto* PC = World->SpawnActor<ASurvivorPlayerController>(Class);
    if (!TestNotNull(TEXT("Saved controller"), PC)) { Cleanup(); return false; }
    PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* SamuraiClass = Cast<UClass>(FindFProperty<FClassProperty>(UCharacterManagerComponent::StaticClass(),
        TEXT("SamuraiClass"))->GetObjectPropertyValue_InContainer(PC->GetCharacterManager()));
    auto* Samurai = World->SpawnActor<ASamuraiCharacter>(SamuraiClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!TestNotNull(TEXT("Saved Samurai Blueprint"), Samurai)) { Cleanup(); return false; }
    Samurai->SetOwner(PC);
    Samurai->SetCharacterMode(ECharacterMode::Active);
    auto* Attack = Samurai->FindComponentByClass<UAutoAttackComponent>();
    Attack->OwnerCharacter = Samurai;
    Attack->bAutoAttackEnabled = true;
    Attack->ImpactFeedback.bEnableCameraShake = false;
    if (!TestNotNull(TEXT("Saved Iaijutsu montage"), Attack->IaijutsuMontage.Get())) { Cleanup(); return false; }
    TestEqual(TEXT("Uses supplied Iaijutsu montage"), Attack->IaijutsuMontage->GetFName(), FName(TEXT("AM_SamuraiIaijutsu")));
    Samurai->GetMesh()->InitAnim(true);
    auto* SamuraiAnim = Samurai->GetMesh()->GetAnimInstance();
    if (!TestNotNull(TEXT("Real Samurai animation instance"), SamuraiAnim)) { Cleanup(); return false; }
    auto* Upgrades = PC->GetPlayerUpgrades();
    auto* Card = Upgrades->FindUpgradeDefinition(TEXT("Iaijutsu"));
    if (!TestNotNull(TEXT("Saved Iaijutsu card"), Card)) { Cleanup(); return false; }
    TestTrue(TEXT("Acquire Iaijutsu"), Upgrades->AcquireUpgrade(Card));

    auto SpawnEnemy = [&](FVector Position, EPlayerAttackSource Restriction = EPlayerAttackSource::Other)
    {
        auto* Enemy = World->SpawnActor<AEnemyBase>(Position, FRotator::ZeroRotator, Params);
        Enemy->ConfigureObjectiveEnemy(10000, Restriction, nullptr, FLinearColor::White);
        Enemy->GetHealthComponent()->RestoreCurrentHealth(10000);
        // This isolated world does not run BeginPlay; install the normal death hook.
        FScriptDelegate Death;
        Death.BindUFunction(Enemy, TEXT("HandleDeath"));
        Enemy->GetHealthComponent()->OnDeath.AddUnique(Death);
        return Enemy;
    };
    {
        // Use a camera-like oblique ray so an enemy body hit would change XY aim.
        const FVector Anchor(100000,0,0);
        const FVector RayOrigin = Anchor + FVector(-600,-600,1000);
        const FVector RayDirection = FVector(1,1,-1).GetSafeNormal();
        auto MakeSurface = [&](AActor* Actor, FVector Location, FVector Extent)
        {
            auto* Box = NewObject<UBoxComponent>(Actor);
            if (!Actor->GetRootComponent()) Actor->SetRootComponent(Box);
            else Box->SetupAttachment(Actor->GetRootComponent());
            Box->SetBoxExtent(Extent);
            Box->SetCollisionObjectType(ECC_WorldStatic);
            Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->RegisterComponent(); Box->SetWorldLocation(Location);
            return Box;
        };
        auto* Floor = World->SpawnActor<AActor>();
        MakeSurface(Floor, Anchor+FVector(0,0,-20), FVector(2000,2000,20));
        FVector Ground;
        TestTrue(TEXT("Mouse ray resolves ground"), MouseGroundAim::Resolve(World,RayOrigin,RayDirection,0,nullptr,Ground));
        TestTrue(TEXT("Mouse aims at the ground under the ray"), Ground.Equals(Anchor+FVector(400,400,0),.1f));
        auto* Occluder = SpawnEnemy(Anchor+FVector(200,200,200));
        Occluder->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        // Also test a misconfigured body/weapon mesh: its actor must be rejected
        // even when the mesh advertises WorldStatic instead of Enemy/Pawn.
        MakeSurface(Occluder,Occluder->GetActorLocation(),FVector(80,80,80));
        FHitResult VisibilityHit;
        TestTrue(TEXT("Old Visibility aim would hit the mob"), World->LineTraceSingleByChannel(
            VisibilityHit,RayOrigin,RayOrigin+RayDirection*10000.f,ECC_Visibility)
            && VisibilityHit.GetActor()==Occluder);
        for (float Offset : {-150.f,-75.f,0.f,75.f,150.f})
        {
            Occluder->SetActorLocation(Anchor+FVector(200+Offset,200,200));
            FVector WithMob;
            TestTrue(TEXT("Ground resolves as a mob crosses the mouse ray"), MouseGroundAim::Resolve(World,RayOrigin,RayDirection,0,nullptr,WithMob));
            TestTrue(TEXT("Moving mob never changes the cursor ground point"), WithMob.Equals(Ground,.1f));
        }
        // A pawn-owned, separate weapon actor must be ignored too.
        auto* Weapon = World->SpawnActor<AActor>(); Weapon->SetOwner(Occluder);
        MakeSurface(Weapon,Anchor+FVector(100,100,300),FVector(50,50,50));
        FVector WithWeapon;
        TestTrue(TEXT("Mouse resolves through a pawn-owned weapon"), MouseGroundAim::Resolve(World,RayOrigin,RayDirection,0,nullptr,WithWeapon));
        TestTrue(TEXT("Pawn-owned weapon cannot move the cursor ground point"), WithWeapon.Equals(Ground,.1f));
        Occluder->Destroy(); Weapon->Destroy();
        auto* RaisedFloor = World->SpawnActor<AActor>();
        MakeSurface(RaisedFloor,Anchor+FVector(400,400,150),FVector(350,350,50));
        auto* Wall = World->SpawnActor<AActor>();
        MakeSurface(Wall,Anchor+FVector(150,150,500),FVector(10,300,500));
        TestTrue(TEXT("Ground behind a non-walkable wall still resolves"), MouseGroundAim::Resolve(World,RayOrigin,RayDirection,0,nullptr,Ground));
        TestTrue(TEXT("Elevated ground is used instead of the fallback plane"), Ground.Equals(Anchor+FVector(200,200,200),.1f));
        Floor->Destroy(); RaisedFloor->Destroy(); Wall->Destroy();
        TestTrue(TEXT("Empty space falls back to foot-height plane"), MouseGroundAim::Resolve(World,RayOrigin,RayDirection,40,nullptr,Ground));
        TestTrue(TEXT("Fallback plane uses supplied ground height"), Ground.Equals(Anchor+FVector(360,360,40),.1f));
        TestFalse(TEXT("Parallel mouse ray has no false ground hit"), MouseGroundAim::Resolve(World,RayOrigin,FVector::ForwardVector,40,nullptr,Ground));
        TestFalse(TEXT("Ray pointing away from ground is rejected"), MouseGroundAim::Resolve(World,RayOrigin,FVector::UpVector,40,nullptr,Ground));
    }
    {
        auto* ActiveSlot = FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), TEXT("ActiveCharacter"));
        auto* SavedActive = ActiveSlot->GetObjectPropertyValue_InContainer(PC->GetCharacterManager());
        ActiveSlot->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(), Samurai);
        auto* AutoTarget = SpawnEnemy(FVector(200, 0, 0));
        auto* ManualTarget = SpawnEnemy(FVector(0, 600, 0));
        // Without a viewport the shared manual resolver uses the active character's
        // facing. Deliberately aim away from the nearest enemy to expose the bug.
        Samurai->SetVisualFacingRotation(FRotator(0, 90, 0));
        FVector ManualDirection;
        TestTrue(TEXT("Shared manual aim resolves"), PC->GetCursorAttackDirection(Samurai->GetActorLocation(), ManualDirection));
        TestTrue(TEXT("Manual aim differs from nearest enemy"), ManualDirection.Equals(FVector::RightVector, .001f));
        auto LatestSlash = [&]() -> ASamuraiIaijutsu*
        {
            for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It) return *It;
            return nullptr;
        };
        auto ResetAttack = [&]
        {
            for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It) It->Destroy();
            for (TActorIterator<ASwapAfterimage> It(World); It; ++It) It->Destroy();
            Attack->StopAutoAttack();
            Attack->NextAttackReadyTime = 0; Attack->IaijutsuChargeEndTime = 0;
        };
        TestTrue(TEXT("Enabled auto-targeting starts Iaijutsu"), Attack->StartTargetedAttack());
        if (auto* Aimed = LatestSlash())
        {
            TestTrue(TEXT("Enabled setting selects nearest enemy over manual aim"), (Aimed->End-Aimed->Origin).GetSafeNormal2D().Equals(FVector::ForwardVector,.001f));
            Samurai->SetActorLocation(FVector(100,100,0));
            AutoTarget->SetActorLocation(FVector(300,100,0));
            ++GFrameCounter; Aimed->Tick(.2f);
            TestTrue(TEXT("Auto-aim charge follows Samurai movement"), Aimed->Origin.Equals(Samurai->GetActorLocation(),.001f));
            AutoTarget->SetActorLocation(FVector(100,400,0));
            ++GFrameCounter; Aimed->Tick(.2f);
            TestTrue(TEXT("Auto-aim charge follows a moving enemy"), (Aimed->End-Aimed->Origin).GetSafeNormal2D().Equals(FVector::RightVector,.001f));
            SetAutoTargeting(false); Samurai->SetVisualFacingRotation(FRotator(0,180,0));
            ++GFrameCounter; Aimed->Tick(.1f);
            TestTrue(TEXT("Disabling auto-targeting immediately redirects the charge to manual aim"), (Aimed->End-Aimed->Origin).GetSafeNormal2D().Equals(-FVector::ForwardVector,.001f));
            SetAutoTargeting(true);
            ++GFrameCounter; Aimed->Tick(.1f);
            TestTrue(TEXT("Re-enabling auto-targeting redirects the same charge to an enemy"), (Aimed->End-Aimed->Origin).GetSafeNormal2D().Equals(FVector::RightVector,.001f));
        }
        ResetAttack(); SetAutoTargeting(false);
        Samurai->SetActorLocation(FVector::ZeroVector); AutoTarget->SetActorLocation(FVector(200,0,0));
        Samurai->SetVisualFacingRotation(FRotator(0, 90, 0));
        TestTrue(TEXT("Disabled auto-targeting starts a manually aimed Iaijutsu"), Attack->StartTargetedAttack());
        TestFalse(TEXT("Manual Iaijutsu keeps its charge/cooldown gate"), Attack->StartTargetedAttack());
        if (auto* Aimed = LatestSlash(); TestNotNull(TEXT("Manual slash exists"), Aimed))
        {
            TestTrue(TEXT("Manual lane begins at full range"), Aimed->End.Equals(Aimed->Origin + ManualDirection * 800.f,.001f));
            auto* OldLaneTarget = SpawnEnemy(FVector(0,500,0));
            auto* MovedVacuumTarget = SpawnEnemy(FVector(500,1140,0));
            Samurai->SetActorLocation(FVector(1000,1000,0));
            Samurai->SetVisualFacingRotation(FRotator(0,180,0));
            const FRotator GhostRotation = Aimed->Visual.IsValid() ? Aimed->Visual->GetActorRotation() : FRotator::ZeroRotator;
            ++GFrameCounter; Aimed->Tick(.4f);
            TestTrue(TEXT("Manual charge follows the player"), Aimed->Origin.Equals(FVector(1000,1000,0),.001f));
            TestTrue(TEXT("Manual charge turns with current cursor/controller aim"), Aimed->End.Equals(FVector(200,1000,0),.001f));
            const float FloorOffset = Samurai->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-4.f;
            TestTrue(TEXT("Indicator follows the translated and rotated lane"), Aimed->ChargeIndicator->GetComponentLocation().Equals(FVector(600,1000,-FloorOffset),.01f));
            TestTrue(TEXT("Indicator points along current manual aim"), Aimed->ChargeIndicator->GetForwardVector().Equals(-FVector::ForwardVector,.001f));
            TestTrue(TEXT("Moving and turning do not reset charge progress"), FMath::IsNearlyEqual(Aimed->ChargeMaterial->K2_GetScalarParameterValue(TEXT("FillAmount")),.4f,.001f));
            if (TestTrue(TEXT("Following charge retains the spectral visual"), Aimed->Visual.IsValid()))
            {
                TestTrue(TEXT("Spectral visual follows current charge position"), Aimed->Visual->GetActorLocation().Equals(FVector(680,1000,0),.01f));
                TestTrue(TEXT("Spectral visual turns with the lane"), FMath::IsNearlyEqual(FMath::FindDeltaAngleDegrees(GhostRotation.Yaw,Aimed->Visual->GetActorRotation().Yaw),90.f,.01f));
            }
            TestEqual(TEXT("Vacuum stays inactive before the final 0.2 seconds"), MovedVacuumTarget->GetActorLocation().Y, 1140.0);
            TestEqual(TEXT("Vacuum does not deal premature damage"), MovedVacuumTarget->GetHealthComponent()->GetCurrentHealth(),10000.f);
            // The final update must happen before the damage query, even in a frame
            // that advances past the end of the charge.
            Samurai->SetVisualFacingRotation(FRotator(0,90,0));
            ManualTarget->SetActorLocation(FVector(1000,1600,0));
            ++GFrameCounter; Aimed->Tick(.61f);
            TestTrue(TEXT("Release uses the latest aim"), Aimed->End.Equals(FVector(1000,1800,0),.001f));
            TestTrue(TEXT("Released lane damages its new target"), ManualTarget->GetHealthComponent()->GetCurrentHealth()<10000.f);
            TestEqual(TEXT("Original floor position deals no damage"), OldLaneTarget->GetHealthComponent()->GetCurrentHealth(),10000.f);
            TestEqual(TEXT("Intermediate lane position deals no damage"), MovedVacuumTarget->GetHealthComponent()->GetCurrentHealth(),10000.f);
            const FVector ReleasedEnd = Aimed->End;
            const float HealthAfter = ManualTarget->GetHealthComponent()->GetCurrentHealth();
            Samurai->SetActorLocation(FVector::ZeroVector); Samurai->SetVisualFacingRotation(FRotator::ZeroRotator);
            ++GFrameCounter; Aimed->Tick(.2f);
            TestTrue(TEXT("Resolved hitbox stops following"), Aimed->End.Equals(ReleasedEnd,.001f));
            TestEqual(TEXT("Resolved hit cannot repeat"), ManualTarget->GetHealthComponent()->GetCurrentHealth(),HealthAfter);
            OldLaneTarget->Destroy(); MovedVacuumTarget->Destroy();
        }
        ResetAttack(); SetAutoTargeting(true); ManualTarget->SetActorLocation(FVector(0,600,0));
        TestTrue(TEXT("Re-enabling auto-targeting affects the next attack"), Attack->StartTargetedAttack());
        if (auto* Aimed = LatestSlash())
        {
            TestTrue(TEXT("Next attack returns to nearest-enemy aim"), (Aimed->End-Aimed->Origin).GetSafeNormal2D().Equals(FVector::ForwardVector,.001f));
            AutoTarget->Destroy(); ManualTarget->Destroy();
            Samurai->SetActorLocation(FVector(100,50,0));
            ++GFrameCounter; Aimed->Tick(.2f);
            TestTrue(TEXT("Lost target keeps the last aim while still following movement"), Aimed->End.Equals(FVector(900,50,0),.001f));
            ++GFrameCounter; Aimed->Tick(.81f);
            TestTrue(TEXT("Lost target still allows the charge to complete"), Aimed->bResolved);
        }
        ResetAttack(); AutoTarget->Destroy(); ManualTarget->Destroy(); Samurai->SetActorLocation(FVector::ZeroVector);
        TestFalse(TEXT("Auto-targeting waits when no enemy exists"), Attack->StartTargetedAttack());
        SetAutoTargeting(false);
        TestTrue(TEXT("Manual Iaijutsu can attack empty space"), Attack->StartTargetedAttack());
        ResetAttack(); SetAutoTargeting(true);
        Samurai->SetVisualFacingRotation(FRotator::ZeroRotator);
        ActiveSlot->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(), SavedActive);
    }
    auto* Near = SpawnEnemy(FVector(200, 0, 0));
    auto* Far = SpawnEnemy(FVector(600, 0, 0));
    auto* OffPath = SpawnEnemy(FVector(400, 350, 0));
    auto* Beyond = SpawnEnemy(FVector(1200, 0, 0));
    auto* Restricted = SpawnEnemy(FVector(450, 0, 0), EPlayerAttackSource::Ninja);
    auto* Leaving = SpawnEnemy(FVector(350, 0, 0));
    auto* Entering = SpawnEnemy(FVector(650, 350, 0));
    auto* VacuumTarget = SpawnEnemy(FVector(750, 140, 0));
    const FVector Position = Samurai->GetActorLocation();
    const FRotator Rotation = Samurai->GetActorRotation();
    TestTrue(TEXT("Normal attack starts spectral slash"), Attack->StartTargetedAttack());
    TestTrue(TEXT("Dedicated Iaijutsu montage plays on the real Samurai"), SamuraiAnim->Montage_IsPlaying(Attack->IaijutsuMontage));
    if (auto* Instance = SamuraiAnim->GetActiveInstanceForMontage(Attack->IaijutsuMontage))
        TestTrue(TEXT("Iaijutsu montage cannot move the player through root motion"), Instance->IsRootMotionDisabled());
    TestTrue(TEXT("Player montage fits the committed charge"), FMath::IsNearlyEqual(
        Attack->IaijutsuMontage->GetPlayLength() / (SamuraiAnim->Montage_GetPlayRate(Attack->IaijutsuMontage) * Attack->IaijutsuMontage->RateScale),
        Attack->GetIaijutsuChargeDuration(), .001f));
    AddInfo(FString::Printf(TEXT("Saved Iaijutsu montage length=%.3fs, authored RateScale=%.3f, base charge=%.3fs, runtime PlayRate=%.3f"),
        Attack->IaijutsuMontage->GetPlayLength(), Attack->IaijutsuMontage->RateScale,
        Attack->GetIaijutsuChargeDuration(), SamuraiAnim->Montage_GetPlayRate(Attack->IaijutsuMontage)));
    const float OriginalCharge = Attack->GetIaijutsuChargeDuration();
    const float OriginalMontageRate = SamuraiAnim->Montage_GetPlayRate(Attack->IaijutsuMontage);
    const float OriginalInterval = Attack->GetEffectiveAttackInterval();
    FCharacterStatModifier SpeedBuff;
    SpeedBuff.ModifierId = TEXT("Test_Iaijutsu_AttackSpeed");
    SpeedBuff.Stat = ECharacterStatType::AttackSpeedMultiplier;
    SpeedBuff.Operation = EStatModifierOperation::Multiply; SpeedBuff.Value = 2.f;
    Samurai->GetCharacterStats()->AddModifier(SpeedBuff);
    Attack->PlayIaijutsuMontage(Attack->GetIaijutsuChargeDuration());
    TestEqual(TEXT("Attack-speed bonus does not shorten Iaijutsu charge"), Attack->GetIaijutsuChargeDuration(),OriginalCharge);
    TestEqual(TEXT("Attack-speed bonus does not speed up player montage"), SamuraiAnim->Montage_GetPlayRate(Attack->IaijutsuMontage),OriginalMontageRate);
    TestTrue(TEXT("Attack-speed bonus still shortens the separate cooldown"), Attack->GetEffectiveAttackInterval()<OriginalInterval);
    Samurai->GetCharacterStats()->RemoveModifier(SpeedBuff.ModifierId);
    TestFalse(TEXT("No montage movement lock"), Attack->bIsAttacking);
    TestNull(TEXT("Iaijutsu does not enter normal melee montage state"), Attack->ActiveAttackMontage.Get());
    Attack->bIsAttacking = true; // Even a stale melee state/notify cannot add a hit.
    Attack->PerformAttackTrace();
    TestFalse(TEXT("Iaijutsu cannot execute a normal melee trace"), Attack->ExecuteMeleeAttackTrace());
    Attack->bIsAttacking = false;
    TestEqual(TEXT("Montage notify cannot damage the nearby enemy before charge completion"), Near->GetHealthComponent()->GetCurrentHealth(), 10000.f);
    TestEqual(TEXT("Player position unchanged"), Samurai->GetActorLocation(), Position);
    TestEqual(TEXT("Player rotation unchanged"), Samurai->GetActorRotation(), Rotation);
    TestFalse(TEXT("Normal attack cooldown enforced"), Attack->StartTargetedAttack());
    TestTrue(TEXT("One-second charge plus half-second base cooldown"),
        FMath::IsNearlyEqual(Attack->NextAttackReadyTime - Attack->LastAttackStartTime, 1.5, .001));
    ASwapAfterimage* Ghost = nullptr;
    for (TActorIterator<ASwapAfterimage> It(World); It; ++It) Ghost = *It;
    TestNotNull(TEXT("Saved mesh and material produce spectral visual"), Ghost);
    if (Ghost)
    {
        auto* Mesh = Ghost->FindComponentByClass<USkeletalMeshComponent>();
        auto* Anim = Mesh ? Mesh->GetSingleNodeInstance() : nullptr;
        if (TestNotNull(TEXT("Spectral montage animation instance"), Anim))
            TestEqual(TEXT("Spectral attacker retains the original attack animation"), Anim->GetCurrentAsset(), static_cast<UAnimationAsset*>(Attack->AttackMontage.Get()));
        Ghost->Tick(.04f);
    }
    ASamuraiIaijutsu* Slash = nullptr;
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It) Slash = *It;
    if (!TestNotNull(TEXT("Independent attack actor spawned"), Slash)) { Cleanup(); return false; }
    TestEqual(TEXT("Base lane has 100 cm half-width"), Slash->HitRadius, 100.f);
    const float Damage = Attack->GetEffectiveAttackDamage();
    ++GFrameCounter;
    Slash->Tick(.99f);
    TestEqual(TEXT("Nearest enemy takes no damage during charge"), Near->GetHealthComponent()->GetCurrentHealth(), 10000.f);
    TestEqual(TEXT("Far enemy takes no damage during charge"), Far->GetHealthComponent()->GetCurrentHealth(), 10000.f);
    TestTrue(TEXT("Spectral Samurai moves during charge"), Ghost && Ghost->GetActorLocation().X > 700.f);
    TestTrue(TEXT("Ground indicator visible while charging"), Slash->ChargeIndicator->IsVisible());
    TestNotNull(TEXT("Enemy-style indicator material exists"), Slash->ChargeMaterial.Get());
    if (Slash->ChargeMaterial)
    {
        TestTrue(TEXT("Indicator fills with charge progress"), FMath::IsNearlyEqual(Slash->ChargeMaterial->K2_GetScalarParameterValue(TEXT("FillAmount")), .99f, .001f));
        TestEqual(TEXT("Friendly cyan indicator"), Slash->ChargeMaterial->K2_GetVectorParameterValue(TEXT("FillColor")), Attack->IaijutsuIndicatorColor);
    }
    Leaving->SetActorLocation(FVector(350, 350, 0));
    Entering->SetActorLocation(FVector(650, 0, 0));
    ++GFrameCounter;
    Slash->Tick(.02f);
    TestTrue(TEXT("Charge resolves once"), Slash->bResolved);
    TestFalse(TEXT("Indicator removed on hit"), Slash->ChargeIndicator->IsVisible());
    Slash->Tick(.5f);
    TestEqual(TEXT("Each enemy hit at most once"), Near->GetHealthComponent()->GetCurrentHealth(), 10000.f - Damage);
    TestEqual(TEXT("Full lane damages distant enemies"), Far->GetHealthComponent()->GetCurrentHealth(), 10000.f - Damage);
    TestFalse(TEXT("Iaijutsu cannot apply Bleed"), Far->HasStatus(EEnemyStatusEffect::Bleed));
    TestEqual(TEXT("Outside path untouched"), OffPath->GetHealthComponent()->GetCurrentHealth(), 10000.f);
    TestEqual(TEXT("Fixed distance respected"), Beyond->GetHealthComponent()->GetCurrentHealth(), 10000.f);
    TestEqual(TEXT("Source restrictions respected"), Restricted->GetHealthComponent()->GetCurrentHealth(), 10000.f);
    TestEqual(TEXT("Enemy leaving lane before completion is unharmed"), Leaving->GetHealthComponent()->GetCurrentHealth(), 10000.f);
    TestEqual(TEXT("Enemy entering lane before completion is hit"), Entering->GetHealthComponent()->GetCurrentHealth(), 10000.f - Damage);
    TestEqual(TEXT("Attack does not move player during charge"), Samurai->GetActorLocation(), Position);
    TestTrue(TEXT("Nearby enemy pulled into lane"), VacuumTarget->GetActorLocation().Y < 130.f);
    TestTrue(TEXT("Vacuum target takes lane damage"), VacuumTarget->GetHealthComponent()->GetCurrentHealth() < 10000.f);
    TestTrue(TEXT("Lane applies separate Iaijutsu mark"), Far->HasIaijutsuMark());
    TestFalse(TEXT("Iaijutsu mark does not become consumable Marked Blade"), Far->IsMarked());
    const float BeforeMarkedHit = Far->GetHealthComponent()->GetCurrentHealth();
    Far->ApplyPlayerDamage(100.f, EPlayerAttackSource::Ninja);
    TestEqual(TEXT("Mark increases other character damage by 50%"), Far->GetHealthComponent()->GetCurrentHealth(), BeforeMarkedHit - 150.f);
    Far->ApplyIaijutsuMark(Upgrades, .5f, .02f, false);
    ++GFrameCounter; // TimerManager advances only once per engine frame.
    World->Tick(LEVELTICK_All, .03f);
    TestFalse(TEXT("Mark expires"), Far->HasIaijutsuMark());
    TestEqual(TEXT("Expired mark has no damage bonus"), Far->GetIaijutsuDamageMultiplier(), 1.f);

    FPlayerUpgradeRunState Base; Upgrades->CaptureRunState(Base);
    auto Acquire = [&](const TCHAR* Id)
    {
        auto* C = Upgrades->FindUpgradeDefinition(Id);
        TestNotNull(Id, C);
        return C && Upgrades->AcquireUpgrade(C);
    };
    TestFalse(TEXT("Rare proc scaling requires unlock"), Upgrades->CanAcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("IaijutsuInstantChance"))));
    TestTrue(TEXT("Iaijutsu Shrine offers tradeoffs"), Upgrades->BeginBloodShrineSelection(3));
    TestEqual(TEXT("Three Iaijutsu Shrine options"), Upgrades->GetCurrentUpgradeChoices().Num(), 3);
    for (auto* Choice : Upgrades->GetCurrentUpgradeChoices())
        TestTrue(TEXT("Shrine offers only Iaijutsu pacts"), Choice->UpgradeId.ToString().StartsWith(TEXT("Iaijutsu")));
    Upgrades->BeginDirectUpgradeSelection(1000);
    for (auto* Choice : Upgrades->GetCurrentUpgradeChoices())
        TestFalse(TEXT("Ordinary rewards exclude Iaijutsu pacts"), Choice->Category == EUpgradeCategory::Cursed && Choice->UpgradeId.ToString().StartsWith(TEXT("Iaijutsu")));
    Acquire(TEXT("IaijutsuChargeSpeed"));
    TestTrue(TEXT("Charge speed shortens charge"), FMath::IsNearlyEqual(Attack->GetIaijutsuChargeDuration(), 1.f / 1.15f));
    Acquire(TEXT("IaijutsuPowerPact"));
    TestTrue(TEXT("Power pact slows charge multiplicatively"), FMath::IsNearlyEqual(Attack->GetIaijutsuChargeDuration(), 1.f / 1.15f * 1.3f));
    Upgrades->RestoreRunState(Base);

    auto ClearSlashes = [&]
    {
        for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It) It->Destroy();
    };
    auto CountSlashes = [&]
    {
        int32 Count = 0;
        for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It) if (!It->bResolved) ++Count;
        return Count;
    };
    ClearSlashes();
    {
        const FVector SavedPosition = Samurai->GetActorLocation();
        Samurai->SetActorLocation(FVector::ZeroVector);
        auto ResetPresentation = [&]
        {
            ClearSlashes(); Attack->StopAutoAttack();
            Attack->NextAttackReadyTime = 0; Attack->IaijutsuChargeEndTime = 0;
        };
        ResetPresentation();
        TestFalse(TEXT("Stopping attacks stops the player Iaijutsu montage"), SamuraiAnim->Montage_IsPlaying(Attack->IaijutsuMontage));
        Acquire(TEXT("IaijutsuChargeSpeed"));
        TestTrue(TEXT("Faster charge starts an animated attack"), Attack->StartTargetedAttack());
        TestTrue(TEXT("Player montage follows upgraded charge duration"), FMath::IsNearlyEqual(
            Attack->IaijutsuMontage->GetPlayLength() / (SamuraiAnim->Montage_GetPlayRate(Attack->IaijutsuMontage) * Attack->IaijutsuMontage->RateScale),
            Attack->GetIaijutsuChargeDuration(), .001f));
        ResetPresentation(); Upgrades->RestoreRunState(Base);
        Acquire(TEXT("IaijutsuDoubleCut")); Attack->IaijutsuAttackCounter = 3;
        TestTrue(TEXT("Crossing normal attack starts"), Attack->StartTargetedAttack());
        TestEqual(TEXT("Animated Double Cut still creates both lanes"), CountSlashes(), 2);
        TestTrue(TEXT("Crossing attack animates the real Samurai"), SamuraiAnim->Montage_IsPlaying(Attack->IaijutsuMontage));
        SetAutoTargeting(false); PC->bControllerIsActiveTargetingDevice = true;
        Samurai->SetActorLocation(FVector(1000,1000,0));
        PC->LastValidControllerAimDirection = FVector::RightVector;
        ++GFrameCounter;
        for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
        {
            It->Tick(.3f);
            TestTrue(TEXT("Moving X lanes keep their farther shared midpoint"), ((It->Origin+It->End)*.5f).Equals(FVector(1000,1480,0),.01f));
            TestTrue(TEXT("Moving X lanes retain their extended length"), FMath::IsNearlyEqual(FVector::Distance(It->Origin,It->End),1000.f,.01f));
        }
        PC->LastValidControllerAimDirection = -FVector::ForwardVector;
        ++GFrameCounter;
        for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
        {
            It->Tick(.2f);
            TestTrue(TEXT("Both X lanes rotate around the updated aim midpoint"), ((It->Origin+It->End)*.5f).Equals(FVector(520,1000,0),.01f));
        }
        PC->bControllerIsActiveTargetingDevice = false; SetAutoTargeting(true);
        Samurai->SetActorLocation(FVector::ZeroVector);
        ResetPresentation(); Upgrades->RestoreRunState(Base);
        const auto SavedMontage = Attack->IaijutsuMontage;
        Attack->IaijutsuMontage = nullptr;
        TestTrue(TEXT("Missing player montage does not prevent the attack"), Attack->StartTargetedAttack());
        TestEqual(TEXT("Missing player montage still creates its lane"), CountSlashes(), 1);
        TestNull(TEXT("Missing player montage does not animate a fallback on the player"), Attack->ActiveIaijutsuMontage.Get());
        ResetPresentation(); Attack->IaijutsuMontage = SavedMontage;
        Samurai->SetActorLocation(SavedPosition);
    }
    Acquire(TEXT("IaijutsuDamage"));
    Acquire(TEXT("IaijutsuWidth"));
    Acquire(TEXT("IaijutsuMarkDamage"));
    Attack->SpawnIaijutsuSlashes(FVector(5000,0,0), FVector::ForwardVector, 800, .5f, false);
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
    {
        TestTrue(TEXT("Damage card scales committed Iaijutsu damage"), FMath::IsNearlyEqual(It->HitDamage, Attack->GetEffectiveAttackDamage() * 1.2f));
        TestTrue(TEXT("Width card scales lane"), FMath::IsNearlyEqual(It->HitRadius, 125.f));
    }
    ClearSlashes();
    Upgrades->RestoreRunState(Base);
    Acquire(TEXT("IaijutsuDoubleCut"));
    Attack->IaijutsuAttackCounter = 0;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Attack->SpawnIaijutsuSlashes(FVector(5000,0,0), FVector::ForwardVector, 800, .5f, false);
        TestEqual(TEXT("Fourth attack produces two lanes"), CountSlashes(), Index == 3 ? 2 : 1);
        if (Index == 3)
        {
            TArray<FVector> Directions;
            for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
            {
                Directions.Add((It->End - It->Origin).GetSafeNormal());
                TestTrue(TEXT("X lanes intersect farther ahead"), ((It->End + It->Origin) * .5f).Equals(FVector(5480,0,0), .1f));
                TestTrue(TEXT("Fixed X lanes are 25 percent longer"), FMath::IsNearlyEqual(FVector::Distance(It->Origin,It->End),1000.f,.01f));
            }
            if (Directions.Num() == 2) TestTrue(TEXT("Crossing lanes have different directions"), FVector::DotProduct(Directions[0], Directions[1]) < .9f);
        }
        ClearSlashes();
    }
    for (int32 Rank = 0; Rank < 3; ++Rank) TestTrue(TEXT("Useful frequency rank acquired"), Acquire(TEXT("IaijutsuDoubleCutFrequency")));
    TestFalse(TEXT("Redundant fourth Iaijutsu frequency rank rejected"), Acquire(TEXT("IaijutsuDoubleCutFrequency")));
    Attack->SpawnIaijutsuSlashes(FVector(5000,0,0), FVector::ForwardVector, 800, .5f, false);
    TestEqual(TEXT("Max Double Cut frequency produces X every attack"), CountSlashes(), 2);
    ClearSlashes();
    Upgrades->RestoreRunState(Base);
    Acquire(TEXT("IaijutsuDash"));
    for (int32 Rank = 0; Rank < 5; ++Rank) Acquire(TEXT("IaijutsuDashPower"));
    Samurai->StartDashVisual(.2f, FVector::ForwardVector);
    auto* DashMontage = SamuraiAnim->GetCurrentActiveMontage();
    TestNotNull(TEXT("Dash montage starts on the real Samurai"), DashMontage);
    Attack->SpawnIaijutsuDash(FVector(5000,0,0), FVector(5300,0,0));
    TestTrue(TEXT("Dash Draw does not replace the dash montage"), DashMontage && SamuraiAnim->Montage_IsPlaying(DashMontage));
    TestFalse(TEXT("Dash Draw does not play the player's normal Iaijutsu montage"), SamuraiAnim->Montage_IsPlaying(Attack->IaijutsuMontage));
    Attack->StopAutoAttack();
    TestTrue(TEXT("Stopping Iaijutsu leaves the dash montage alone"), DashMontage && SamuraiAnim->Montage_IsPlaying(DashMontage));
    TestFalse(TEXT("Normal attack cannot interrupt the dash"), Attack->StartTargetedAttack());
    Samurai->EndDashVisual(); SamuraiAnim->Montage_Stop(0.f);
    const FVector PositionBeforeDashCheck = Samurai->GetActorLocation();
    Samurai->SetActorLocation(FVector(1000,1000,0));
    ++GFrameCounter;
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
    {
        It->Tick(.2f);
        TestEqual(TEXT("Dash slash begins at previous position"), It->Origin, FVector(5000,0,0));
        TestEqual(TEXT("Dash slash reaches actual dash endpoint"), It->End, FVector(5300,0,0));
        TestTrue(TEXT("Dash Draw Power doubles only dash attack damage"), FMath::IsNearlyEqual(It->HitDamage, Attack->GetEffectiveAttackDamage() * 2.f));
    }
    TestEqual(TEXT("Dash creates a slash"), CountSlashes(), 1);
    Samurai->SetActorLocation(PositionBeforeDashCheck);
    ClearSlashes();
    Attack->SpawnIaijutsuSlashes(FVector(5000,0,0), FVector::ForwardVector, 800, 1.f, true);
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
        TestTrue(TEXT("Dash damage scaling leaves normal attacks unchanged"), FMath::IsNearlyEqual(It->HitDamage, Attack->GetEffectiveAttackDamage()));
    ClearSlashes();
    Upgrades->RestoreRunState(Base);
    Acquire(TEXT("IaijutsuInstant"));
    auto* Instant = Upgrades->FindUpgradeDefinition(TEXT("IaijutsuInstant"));
    const auto OldInstantBalance = Instant->BalanceParameters;
    Instant->BalanceParameters.Add(TEXT("Chance"), 1.f);
    Samurai->SetActorLocation(FVector::ZeroVector);
    Attack->NextAttackReadyTime = 0; Attack->IaijutsuChargeEndTime = 0;
    TestTrue(TEXT("Forced instant normal attack starts"), Attack->StartIaijutsuAttack());
    TestTrue(TEXT("Instant cast still animates the real Samurai"), SamuraiAnim->Montage_IsPlaying(Attack->IaijutsuMontage));
    TestEqual(TEXT("Instant cast animation uses authored speed"), SamuraiAnim->Montage_GetPlayRate(Attack->IaijutsuMontage), 1.f);
    TestEqual(TEXT("Instant cast resolves without a world tick"), CountSlashes(), 0);
    int32 InstantIndicators = 0;
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
    {
        ++InstantIndicators;
        TestTrue(TEXT("Instant proc shows its lane immediately"), It->ChargeIndicator->IsVisible());
        if (TestNotNull(TEXT("Instant proc has an indicator material"), It->ChargeMaterial.Get()))
            TestEqual(TEXT("Instant indicator is fully charged"), It->ChargeMaterial->K2_GetScalarParameterValue(TEXT("FillAmount")), 1.f);
        It->Tick(.29f);
        TestTrue(TEXT("Instant indicator remains visible during post-hit window"), It->ChargeIndicator->IsVisible());
        TestFalse(TEXT("Instant indicator survives until the window ends"), It->IsActorBeingDestroyed());
        It->Tick(.02f);
        TestTrue(TEXT("Instant indicator expires with the post-hit window"), It->IsActorBeingDestroyed());
    }
    TestEqual(TEXT("Instant proc presents one lane indicator"), InstantIndicators, 1);
    TestTrue(TEXT("Instant cast retains only cooldown"), FMath::IsNearlyEqual(Attack->NextAttackReadyTime - Attack->LastAttackStartTime, .5, .001));
    Instant->BalanceParameters = OldInstantBalance;
    Upgrades->RestoreRunState(Base);
    Acquire(TEXT("IaijutsuAOE"));
    auto* AOE = Upgrades->FindUpgradeDefinition(TEXT("IaijutsuAOE"));
    const auto OldAOEBalance = AOE->BalanceParameters;
    AOE->BalanceParameters.Add(TEXT("Chance"), 1.f);
    auto* EndpointEnemy = SpawnEnemy(FVector(5800,200,0));
    Attack->SpawnIaijutsuSlashes(FVector(5000,0,0), FVector::ForwardVector, 800, 0.f, false);
    TestTrue(TEXT("Endpoint burst damages enemies outside lane"), EndpointEnemy->GetHealthComponent()->GetCurrentHealth() < 10000.f);
    // A normal burst must use the final tracked endpoint, not its launch endpoint.
    Attack->NextAttackReadyTime = 0; Attack->IaijutsuChargeEndTime = 0;
    SetAutoTargeting(false); PC->bControllerIsActiveTargetingDevice = true;
    PC->LastValidControllerAimDirection = FVector::ForwardVector;
    TestTrue(TEXT("Normal endpoint-burst charge starts"), Attack->StartTargetedAttack());
    Samurai->SetActorLocation(FVector(4000,4000,0));
    PC->LastValidControllerAimDirection = FVector::RightVector;
    auto* TrackedEndpointEnemy = SpawnEnemy(FVector(4200,4800,0));
    auto* PlayerSideEnemy = SpawnEnemy(FVector(4200,4000,0));
    const auto SavedEndpointMontage = Attack->IaijutsuEndpointMontage;
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
        It->EndpointMontage = SavedEndpointMontage ? SavedEndpointMontage.Get() : Attack->AttackMontage.Get();
    TSet<ASwapAfterimage*> ExistingGhosts;
    for (TActorIterator<ASwapAfterimage> It(World); It; ++It) ExistingGhosts.Add(*It);
    ++GFrameCounter;
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It) It->Tick(1.01f);
    TestTrue(TEXT("Endpoint burst follows the released lane"), TrackedEndpointEnemy->GetHealthComponent()->GetCurrentHealth()<10000.f);
    TestEqual(TEXT("Endpoint burst does not damage an enemy beside the player"), PlayerSideEnemy->GetHealthComponent()->GetCurrentHealth(), 10000.f);
    ASwapAfterimage* EndpointGhost = nullptr;
    for (TActorIterator<ASwapAfterimage> It(World); It; ++It)
        if (!ExistingGhosts.Contains(*It)) EndpointGhost = *It;
    if (TestNotNull(TEXT("Endpoint burst spawns its montage ghost"), EndpointGhost))
    {
        TestTrue(TEXT("Endpoint ghost actor is at the released endpoint"), EndpointGhost->GetActorLocation().Equals(FVector(4000,4800,0), .01f));
        auto* Mesh = EndpointGhost->FindComponentByClass<USkeletalMeshComponent>();
        if (TestNotNull(TEXT("Endpoint montage mesh"), Mesh))
            TestTrue(TEXT("Endpoint mesh is at endpoint, not the player's position"),
                FVector::Dist2D(Mesh->GetComponentLocation(), FVector(4000,4800,0)) < 1.f);
    }
    PlayerSideEnemy->Destroy(); TrackedEndpointEnemy->Destroy(); ClearSlashes();
    Samurai->SetActorLocation(FVector::ZeroVector);
    PC->bControllerIsActiveTargetingDevice = false; SetAutoTargeting(true);
    AOE->BalanceParameters = OldAOEBalance;
    Upgrades->RestoreRunState(Base);
    Acquire(TEXT("IaijutsuMarkPact"));
    auto* PactEnemy = SpawnEnemy(FVector(10000,0,0));
    Attack->SpawnIaijutsuSlashes(FVector(9800,0,0), FVector::ForwardVector, 800, 0.f, false);
    TestEqual(TEXT("Mark pact doubles base vulnerability to +100%"), PactEnemy->GetIaijutsuDamageMultiplier(), 2.f);
    TestFalse(TEXT("Focused Malice blocks Relentless Steps"), Acquire(TEXT("IaijutsuDashPact")));
    Upgrades->RestoreRunState(Base);
    TestTrue(TEXT("Relentless Steps can be chosen without Focused Malice"), Acquire(TEXT("IaijutsuDashPact")));
    PC->CurrentDashCharges = 0; PC->MaxDashCharges = 2;
    PC->StartDashRechargeIfNeeded();
    const float RechargeBefore = PC->GetDashRechargeRemaining();
    auto* RefundEnemy = SpawnEnemy(FVector(12000,0,0));
    RefundEnemy->GetHealthComponent()->RestoreCurrentHealth(1.f);
    Attack->SpawnIaijutsuSlashes(FVector(11800,0,0), FVector::ForwardVector, 800, 0.f, false);
    TestTrue(TEXT("Lethal applying hit receives dash refund"), FMath::IsNearlyEqual(PC->GetDashRechargeRemaining(), RechargeBefore - .3f, .001f));
    PactEnemy->ApplyIaijutsuMark(Upgrades, 2.f, 3.f, true);
    TestEqual(TEXT("Dash pact disables mark damage even with mark scaling"), PactEnemy->GetIaijutsuDamageMultiplier(), 1.f);
    PC->ReduceDashRecharge(100.f);
    TestEqual(TEXT("Refund completes only current recharge"), PC->GetCurrentDashCharges(), 1);
    Upgrades->RestoreRunState(Base);
    Acquire(TEXT("IaijutsuChain"));
    Acquire(TEXT("IaijutsuDash"));
    for (int32 Rank = 0; Rank < 5; ++Rank)
    {
        Acquire(TEXT("IaijutsuCascadePower"));
        Acquire(TEXT("IaijutsuDashPower"));
    }
    auto* ChainVictim = SpawnEnemy(FVector(15200,0,0));
    ChainVictim->GetHealthComponent()->RestoreCurrentHealth(1.f);
    auto* ChainTarget = SpawnEnemy(FVector(15300,400,0));
    ChainTarget->GetHealthComponent()->RestoreCurrentHealth(1.f);
    // In follow-up range but outside both the original and follow-up lanes.
    auto* ChainBystander = SpawnEnemy(FVector(15700,400,0));
    Acquire(TEXT("IaijutsuDoubleCut"));
    Attack->IaijutsuAttackCounter = 2;
    Attack->StopAutoAttack(); SamuraiAnim->Montage_Stop(0.f);
    Attack->SpawnIaijutsuSlashes(FVector(15000,0,0), FVector::ForwardVector, 800, 0.f, false);
    TestEqual(TEXT("Originating kill advances Double Cut to three attacks"), Attack->IaijutsuAttackCounter, 3);
    ++GFrameCounter; // TimerManager advances only once per engine frame.
    World->Tick(LEVELTICK_All, .01f);
    TestEqual(TEXT("Kill schedules one follow-up"), CountSlashes(), 1);
    TestEqual(TEXT("Death Cascade does not advance or consume Double Cut"), Attack->IaijutsuAttackCounter, 3);
    TestFalse(TEXT("Cascade does not restart the real Samurai's montage"), SamuraiAnim->Montage_IsPlaying(Attack->IaijutsuMontage));
    const FVector PositionBeforeCascadeCheck = Samurai->GetActorLocation();
    Samurai->SetActorLocation(FVector(1000,1000,0));
    ++GFrameCounter;
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
    {
        if (It->bResolved) continue;
        It->Tick(.2f);
        TestTrue(TEXT("Follow-up originates at victim"), It->Origin.Equals(FVector(15200,0,0), 1.f));
        TestTrue(TEXT("Cascade Power doubles follow-up damage without adding Dash Draw Power"),
            FMath::IsNearlyEqual(It->HitDamage, Attack->GetEffectiveAttackDamage() * 2.f));
        It->Tick(1.f);
    }
    TestTrue(TEXT("Follow-up targets and kills another enemy"), ChainTarget->IsDead());
    Samurai->SetActorLocation(PositionBeforeCascadeCheck);
    ++GFrameCounter;
    World->Tick(LEVELTICK_All, .01f);
    TestEqual(TEXT("Lethal cascaded attack cannot cascade again"), CountSlashes(), 0);
    TestEqual(TEXT("Enemy available for another cascade stays unharmed"), ChainBystander->GetHealthComponent()->GetCurrentHealth(), 10000.f);
    ClearSlashes();
    Attack->SpawnIaijutsuSlashes(FVector(18000,0,0), FVector::ForwardVector, 800, .5f, false);
    TestEqual(TEXT("Next originating attack still triggers the earned Double Cut"), CountSlashes(), 2);
    TestEqual(TEXT("Originating Double Cut resets its counter"), Attack->IaijutsuAttackCounter, 0);
    ClearSlashes();
    Acquire(TEXT("IaijutsuAssist"));
    for (int32 Rank = 0; Rank < 5; ++Rank) Acquire(TEXT("IaijutsuAssistChance"));
    TestTrue(TEXT("Assist chance reaches 30 percent at maximum ranks"), FMath::IsNearlyEqual(
        IaijutsuBuild::Chance(Upgrades, TEXT("IaijutsuAssist"), TEXT("IaijutsuAssistChance"), .05f, .05f), .3f));
    Attack->StartAutoAttack();
    TestTrue(TEXT("Assist checks use a timer"), World->GetTimerManager().IsTimerActive(Attack->IaijutsuAssistTimer));
    Attack->StopAutoAttack();
    TestFalse(TEXT("Stopping attacks removes assist timer"), World->GetTimerManager().IsTimerActive(Attack->IaijutsuAssistTimer));
    Upgrades->RestoreRunState(Base);
    Samurai->SetActorLocation(FVector(20000,0,0));
    auto* BeyondRange = SpawnEnemy(FVector(20830,0,0));
    Attack->NextAttackReadyTime = 0; Attack->IaijutsuChargeEndTime = 0;
    TestTrue(TEXT("Can acquire a target just beyond lane distance"), Attack->StartIaijutsuAttack());
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It) It->Tick(1.1f);
    TestTrue(TEXT("Vacuum brings beyond-range target into endpoint"), BeyondRange->GetHealthComponent()->GetCurrentHealth() < 10000.f);

    // Run real pursuit movement between vacuum updates, including a grounded capsule.
    // The old stationary-target test missed enemies walking faster than the pull.
    auto SpawnBlocker = [&](FVector Location, FVector Extent)
    {
        auto* Actor = World->SpawnActor<AActor>();
        auto* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent();
        Actor->SetActorLocation(Location);
        return Actor;
    };
    for (const int32 FPS : {30, 60, 120})
    {
        const FVector LaneOrigin(30000 + FPS * 100, 0, 300);
        auto* Moving = SpawnEnemy(LaneOrigin + FVector(400, 140, 0));
        auto* Outside = SpawnEnemy(LaneOrigin + FVector(600, 240, 0));
        auto* Immune = SpawnEnemy(LaneOrigin + FVector(200, 145, 0), EPlayerAttackSource::Ninja);
        const FVector ImmuneStart = Immune->GetActorLocation();
        auto* Ground = SpawnBlocker(LaneOrigin - FVector(0, 0,
            Moving->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10.f), FVector(1000, 1000, 10));
        auto* Movement = Moving->FindComponentByClass<UEnemyLightweightMovementComponent>();
        Movement->SetMovementEnabled(true);
        Movement->SetMoveSpeed(300.f);
        Movement->RefreshSpawnZ();
        auto* PullSlash = World->SpawnActor<ASamuraiIaijutsu>(LaneOrigin, FRotator::ZeroRotator, Params);
        PullSlash->Initialize(nullptr, Upgrades, FVector::ForwardVector, 10, 800, 100, 1,
            nullptr, Attack->ImpactFeedback);
        PullSlash->Tick(.79f);
        TestEqual(TEXT("No early vacuum movement"), Moving->GetActorLocation(), LaneOrigin + FVector(400,140,0));
        const float Step = 1.f / FPS;
        for (int32 Frame = 0; Frame < FMath::CeilToInt(.21f * FPS); ++Frame)
        {
            Movement->RequestMove(FVector::RightVector); // Chase away from the committed lane.
            Movement->TickComponent(Step, LEVELTICK_All, nullptr);
            PullSlash->Tick(Step);
        }
        PullSlash->Tick(.001f);
        TestTrue(FString::Printf(TEXT("Vacuum groups a pursuing enemy at %d FPS"), FPS), Moving->GetActorLocation().Y < 100.f);
        TestTrue(TEXT("Moving vacuum target is hit at resolution"), Moving->GetHealthComponent()->GetCurrentHealth() < 10000.f);
        TestEqual(TEXT("Vacuum preserves grounded height"), Moving->GetActorLocation().Z, LaneOrigin.Z);
        TestEqual(TEXT("Vacuum does not extend beyond its margin"), Outside->GetActorLocation(), LaneOrigin + FVector(600, 240, 0));
        TestEqual(TEXT("Vacuum respects character damage restrictions"), Immune->GetActorLocation(), ImmuneStart);
        const FVector HeldPosition = Moving->GetActorLocation();
        Movement->RequestMove(FVector::ForwardVector);
        Movement->TickComponent(.1f, LEVELTICK_All, nullptr);
        TestEqual(TEXT("Hitbox stops movement along the lane after impact"), Moving->GetActorLocation(), HeldPosition);
        PullSlash->Tick(.29f);
        TestFalse(TEXT("Post-hit vacuum lasts at least 0.29 seconds"), PullSlash->IsActorBeingDestroyed());
        PullSlash->Tick(.011f);
        TestTrue(TEXT("Post-hit vacuum ends after 0.3 seconds"), PullSlash->IsActorBeingDestroyed());
        const float ReleasedY = Moving->GetActorLocation().Y;
        Movement->RequestMove(FVector::RightVector);
        Movement->TickComponent(.1f, LEVELTICK_All, nullptr);
        TestTrue(TEXT("Pursuit continues normally after the slash"), Moving->GetActorLocation().Y > ReleasedY + 29.f);
        Moving->Destroy(); Outside->Destroy(); Immune->Destroy(); Ground->Destroy();
    }
    const FVector BlockedOrigin(50000, 0, 300);
    auto* Wall = SpawnBlocker(BlockedOrigin + FVector(400, 100, 0), FVector(100, 5, 200));
    auto* Blocked = SpawnEnemy(BlockedOrigin + FVector(400, 150, 0));
    auto* BlockedSlash = World->SpawnActor<ASamuraiIaijutsu>(BlockedOrigin, FRotator::ZeroRotator, Params);
    BlockedSlash->Initialize(nullptr, Upgrades, FVector::ForwardVector, 10, 800, 100, 1, nullptr, Attack->ImpactFeedback);
    BlockedSlash->Tick(1.f);
    TestTrue(TEXT("Vacuum cannot drag enemies through walls"), Blocked->GetActorLocation().Y >= 138.f);
    Wall->Destroy(); Blocked->Destroy();

    // Exercise actual dash travel, character scoping, restoration and collision-shortened lanes.
    Upgrades->RestoreRunState(Base);
    auto* Manager = PC->GetCharacterManager();
    auto* ActiveProperty = FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), TEXT("ActiveCharacter"));
    ActiveProperty->SetObjectPropertyValue_InContainer(Manager, Samurai);
    PC->bLevelUpSelectionActive = false;
    PC->LastMovementInput = FVector2D(0, -1);
    const FVector DashStart(60000, 0, 300);
    auto Dash = [&](ACharacterBase* Character, float ExpectedDistance)
    {
        ActiveProperty->SetObjectPropertyValue_InContainer(Manager, Character);
        Character->SetActorLocation(DashStart);
        Character->SetCharacterMode(ECharacterMode::Active);
        PC->CurrentDashCharges = PC->MaxDashCharges;
        TestTrue(TEXT("Dash starts"), PC->TryDash());
        for (int32 Step = 0; Step < 10; ++Step) PC->HandleDashStep(PC->DashDuration / 10.f);
        TestTrue(TEXT("Dash travels the expected stance-specific distance"),
            FMath::IsNearlyEqual(FVector::Dist2D(Character->GetActorLocation(), DashStart), ExpectedDistance, .1f));
        TestFalse(TEXT("Dash completes in the original duration"), PC->bIsDashing);
    };
    Dash(Samurai, PC->DashDistance);
    ClearSlashes();
    Acquire(TEXT("IaijutsuDash"));
    TestEqual(TEXT("Saved Dash Draw grants 100 percent distance"),
        Upgrades->FindUpgradeDefinition(TEXT("IaijutsuDash"))->GetBalanceValue(TEXT("DashDistanceBonus"), 1.f), 1.f);
    Dash(Samurai, PC->DashDistance * 2.f);
    TestEqual(TEXT("Extended dash creates one slash"), CountSlashes(), 1);
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
    {
        TestEqual(TEXT("Extended dash slash starts at old location"), It->Origin, DashStart);
        TestTrue(TEXT("Extended dash slash matches actual doubled path"), It->End.Equals(Samurai->GetActorLocation(), .1f));
    }
    ClearSlashes();
    auto* Ninja = World->SpawnActor<ANinjaCharacter>(DashStart, FRotator::ZeroRotator, Params);
    Ninja->SetOwner(PC);
    Dash(Ninja, PC->DashDistance);
    TestEqual(TEXT("Ninja cannot create Iaijutsu dash slashes"), CountSlashes(), 0);
    Ninja->Destroy();
    ActiveProperty->SetObjectPropertyValue_InContainer(Manager, Samurai);
    Samurai->SetActorLocation(DashStart);
    auto* DashWall = SpawnBlocker(DashStart + FVector(300, 0, 0), FVector(10, 300, 300));
    PC->CurrentDashCharges = PC->MaxDashCharges;
    TestTrue(TEXT("Extended dash toward a wall starts"), PC->TryDash());
    PC->HandleDashStep(PC->DashDuration);
    TestTrue(TEXT("Extended dash still stops at walls"), Samurai->GetActorLocation().X < DashStart.X + 300.f);
    TestEqual(TEXT("Blocked dash still creates its attack"), CountSlashes(), 1);
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
        TestTrue(TEXT("Blocked dash slash ends at collision-shortened position"), It->End.Equals(Samurai->GetActorLocation(), .1f));
    DashWall->Destroy(); ClearSlashes();
    Upgrades->RestoreRunState(Base);
    Dash(Samurai, PC->DashDistance);
    TestEqual(TEXT("Removing Dash Draw removes its attack"), CountSlashes(), 0);
    for (int32 Rank = 0; Rank < 5; ++Rank) Acquire(TEXT("IaijutsuVacuumReach"));
    TestTrue(TEXT("Maximum Vacuum Reach is 105 cm"), FMath::IsNearlyEqual(IaijutsuBuild::VacuumReach(Upgrades), 105.f));
    Samurai->SetActorLocation(FVector(70000,0,0));
    auto* ReachTarget = SpawnEnemy(FVector(70890,0,0));
    Attack->NextAttackReadyTime = 0; Attack->IaijutsuChargeEndTime = 0;
    TestTrue(TEXT("Vacuum Reach extends target acquisition beyond the old 860 cm limit"), Attack->StartIaijutsuAttack());
    for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It)
    {
        TestEqual(TEXT("Vacuum Reach does not widen the damage lane"), It->HitRadius, 100.f);
        It->Tick(1.1f);
    }
    TestTrue(TEXT("Extended vacuum brings the new outer target into the lane"), ReachTarget->GetHealthComponent()->GetCurrentHealth() < 10000.f);
    FPlayerUpgradeRunState Empty;
    Upgrades->RestoreRunState(Empty);
    Acquire(TEXT("BattleStance"));
    TestFalse(TEXT("Blood cannot acquire Iaijutsu support"), Upgrades->CanAcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("IaijutsuDamage"))));
    TestFalse(TEXT("Blood cannot acquire Iaijutsu pact"), Upgrades->CanAcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("IaijutsuMarkPact"))));
    Cleanup();
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIaijutsuPathVFXTest, "HeavensDivide.Combat.IaijutsuPathVFX",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FIaijutsuPathVFXTest::RunTest(const FString&)
{
    auto* Class = LoadClass<ASamuraiCharacter>(nullptr,
        TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai.BP_Samurai_C"));
    auto* Attack = Class ? Class->GetDefaultObject<ASamuraiCharacter>()->FindComponentByClass<UAutoAttackComponent>() : nullptr;
    if (!TestNotNull(TEXT("Saved Samurai attack configuration"), Attack)) return false;
    auto* System = Cast<UNiagaraSystem>(FindFProperty<FObjectProperty>(UAutoAttackComponent::StaticClass(),
        TEXT("IaijutsuPathSlashVFX"))->GetObjectPropertyValue_InContainer(Attack));
    if (!TestNotNull(TEXT("Saved release slash system"), System)) return false;
#if WITH_EDITORONLY_DATA
    System->WaitForCompilationComplete(true, false);
#endif
    const int32 Count = FindFProperty<FIntProperty>(UAutoAttackComponent::StaticClass(),
        TEXT("IaijutsuPathSlashCount"))->GetPropertyValue_InContainer(Attack);
    const float Scale = FindFProperty<FFloatProperty>(UAutoAttackComponent::StaticClass(),
        TEXT("IaijutsuPathSlashScale"))->GetPropertyValue_InContainer(Attack);
    const float Delay = FindFProperty<FFloatProperty>(UAutoAttackComponent::StaticClass(),
        TEXT("IaijutsuPathSlashDelay"))->GetPropertyValue_InContainer(Attack);
    const FRotator Randomness = *FindFProperty<FStructProperty>(UAutoAttackComponent::StaticClass(),
        TEXT("IaijutsuPathSlashRotationRandomness"))->ContainerPtrToValuePtr<FRotator>(Attack);
    TestTrue(TEXT("Saved configuration staggers the release slashes"), Delay > 0.f);
    TestFalse(TEXT("Saved configuration varies slash rotation"), Randomness.IsNearlyZero());
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    const auto Effects = [&]
    {
        TArray<UNiagaraComponent*> Components;
        World->GetWorldSettings()->GetComponents(Components);
        Components.RemoveAll([&](const auto* Component) { return Component->GetAsset() != System || !IsValid(Component); });
        return Components;
    };
    auto* Slash = World->SpawnActor<ASamuraiIaijutsu>();
    Slash->Initialize(nullptr, nullptr, FVector::ForwardVector, 10, 800, 100, 1, nullptr, FImpactFeedbackData());
    Slash->ConfigurePathSlashes(System, Count, Scale, FRotator::ZeroRotator, Delay, Randomness);
    Slash->Tick(.5f);
    TestTrue(TEXT("No release slashes during charging"), Effects().IsEmpty());
    const FVector FinalOrigin(1000,300,100), FinalEnd(1000,1100,100);
    Slash->UpdateAim = FIaijutsuAimUpdate::CreateLambda([&](FVector& Origin, FVector& End)
    { Origin = FinalOrigin; End = FinalEnd; });
    Slash->Tick(.5f);
    TestEqual(TEXT("Only the first burst fires at release"), Effects().Num(), 1);
    TestFalse(TEXT("Gameplay actor retains the post-hit vacuum"), Slash->IsActorBeingDestroyed());
    // Pending timers hold their own final transforms and settings, even after
    // destruction or a later attack changing the source configuration.
    Slash->ConfigurePathSlashes(nullptr, 0, 0.f, FRotator(90,90,90), 0.f, FRotator::ZeroRotator);
    const auto AdvanceTimers = [&](float Seconds)
    {
        ++GFrameCounter;
        World->GetTimerManager().Tick(Seconds);
    };
    AdvanceTimers(0.f); // Activate timers queued before this world's first tick.
    AdvanceTimers(Delay * .9f);
    TestEqual(TEXT("Next burst waits for its delay"), Effects().Num(), 1);
    for (int32 Index = 1; Index < Count; ++Index)
    {
        AdvanceTimers((Index == 1 ? Delay * .1f : Delay) + .0001f);
        auto Current = Effects();
        TestEqual(TEXT("Each interval adds exactly one trailing burst"), Current.Num(), Index + 1);
        const FVector Expected = FMath::Lerp(FinalOrigin, FinalEnd, (Index + .5f) / Count);
        TestTrue(TEXT("Delayed bursts proceed from lane start to end"), Current.ContainsByPredicate(
            [&](const auto* Effect) { return Effect->GetComponentLocation().Equals(Expected, .01f); }));
    }
    auto Burst = Effects();
    Slash->Tick(.5f);
    AdvanceTimers(Delay * 2.f);
    TestEqual(TEXT("Resolved attack cannot duplicate VFX"), Effects().Num(), Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FVector Expected = FMath::Lerp(FinalOrigin, FinalEnd, (Index + .5f) / Count);
        TestTrue(TEXT("Burst is on the final moved and rotated lane"), Burst.ContainsByPredicate(
            [&](const auto* Effect) { return Effect->GetComponentLocation().Equals(Expected, .01f); }));
    }
    const FQuat LaneRotation = (FinalEnd - FinalOrigin).Rotation().Quaternion();
    bool bVariedRotation = false;
    for (auto* Effect : Burst)
    {
        const FRotator Variation = (LaneRotation.Inverse() * Effect->GetComponentQuat()).Rotator();
        TestTrue(TEXT("Random slash pitch stays within tuning"), FMath::Abs(Variation.Pitch) <= Randomness.Pitch + .01);
        TestTrue(TEXT("Random slash yaw stays within tuning"), FMath::Abs(Variation.Yaw) <= Randomness.Yaw + .01);
        TestTrue(TEXT("Random slash roll stays within tuning"), FMath::Abs(Variation.Roll) <= Randomness.Roll + .01);
        if (!Burst.IsEmpty()) bVariedRotation |= !Effect->GetComponentQuat().Equals(Burst[0]->GetComponentQuat(), .001);
    }
    if (Count > 1) TestTrue(TEXT("Each slash receives independent rotation"), bVariedRotation);
    // Zero settings restore simultaneous, consistently oriented bursts.
    auto* Instant = World->SpawnActor<ASamuraiIaijutsu>(FVector(2000,-500,0), FRotator::ZeroRotator);
    Instant->Initialize(nullptr, nullptr, FVector::ForwardVector, 10, 800, 100, 0, nullptr, FImpactFeedbackData());
    Instant->ConfigurePathSlashes(System, 3, Scale, FRotator(0,45,0), 0.f, FRotator::ZeroRotator);
    Instant->Tick(0.f);
    TestEqual(TEXT("Zero delay emits all instant-cast bursts immediately"), Effects().Num(), Count + 3);
    for (auto* Effect : Effects()) if (!Burst.Contains(Effect))
        TestTrue(TEXT("Zero randomness preserves the authored rotation"), Effect->GetComponentQuat().Equals(FRotator(0,45,0).Quaternion(), .001));
    int32 Particles = 0;
    for (auto* Effect : Burst)
    {
        Effect->SetForceSolo(true);
        Effect->AdvanceSimulation(6, .02f);
        Effect->UpdateBounds();
        Effect->MarkRenderDynamicDataDirty();
        if (auto Controller = Effect->GetSystemInstanceController())
            if (auto* Instance = Controller->GetSystemInstance_Unsafe())
                for (const auto& Emitter : Instance->GetEmitters()) Particles += Emitter->GetNumParticles();
    }
    TestTrue(TEXT("Authored release effect actually emits particles"), Particles > 0);
    AddInfo(FString::Printf(TEXT("Iaijutsu path VFX: %d bursts, %d particles at 0.12s"), Burst.Num(), Particles));
    for (auto* Effect : Burst)
    {
        Effect->AdvanceSimulation(150, .02f);
        TestTrue(TEXT("Release bursts finish naturally without a gameplay actor or timer"),
            !IsValid(Effect) || Effect->IsComplete());
    }
    auto* Source = World->SpawnActor<ASamuraiCharacter>(Class, FVector(4000,4000,0), FRotator::ZeroRotator);
    Source->GetMesh()->InitAnim(true);
    auto* EndpointMontage = Cast<UAnimMontage>(FindFProperty<FObjectProperty>(UAutoAttackComponent::StaticClass(),
        TEXT("IaijutsuEndpointMontage"))->GetObjectPropertyValue_InContainer(Attack));
    if (TestNotNull(TEXT("Saved endpoint montage"), EndpointMontage))
    {
        AddInfo(FString::Printf(TEXT("Endpoint montage: %s"), *EndpointMontage->GetPathName()));
        TSet<UNiagaraComponent*> ExistingEffects;
        for (TObjectIterator<UNiagaraComponent> It; It; ++It) ExistingEffects.Add(*It);
        auto* Ghost = World->SpawnActor<ASwapAfterimage>(FVector(4800,4000,0), FRotator::ZeroRotator);
        Ghost->InitializeSwordDash(Source, Source->SwapPresentation->GhostMaterial, EndpointMontage,
            FVector::ForwardVector, EndpointMontage->GetPlayLength());
        Ghost->EnableCosmeticNiagaraNotifies();
        TSet<UNiagaraComponent*> SpawnedEffects;
        int32 EndpointParticles = 0;
        for (float Time = 0.f; Time < EndpointMontage->GetPlayLength(); Time += .02f)
        {
            Ghost->Tick(.02f);
            for (TObjectIterator<UNiagaraComponent> It; It; ++It)
            {
                auto* Effect = *It;
                if (Effect->GetWorld() != World || ExistingEffects.Contains(Effect) || SpawnedEffects.Contains(Effect)) continue;
                SpawnedEffects.Add(Effect);
                TestTrue(TEXT("Endpoint Niagara spawns near endpoint, away from player"),
                    FVector::Dist2D(Effect->GetComponentLocation(), FVector(4800,4000,0)) < 400.f);
                Effect->SetForceSolo(true);
                Effect->AdvanceSimulation(6, .02f);
                if (auto Controller = Effect->GetSystemInstanceController())
                    if (auto* Instance = Controller->GetSystemInstance_Unsafe())
                        for (const auto& Emitter : Instance->GetEmitters()) EndpointParticles += Emitter->GetNumParticles();
            }
        }
        TestTrue(TEXT("Saved endpoint montage plays its Niagara notify"), SpawnedEffects.Num() > 0);
        TestTrue(TEXT("Endpoint Niagara actually emits particles"), EndpointParticles > 0);
        AddInfo(FString::Printf(TEXT("Endpoint VFX: %d effects, %d particles"), SpawnedEffects.Num(), EndpointParticles));
    }
    return true;
}
#endif
