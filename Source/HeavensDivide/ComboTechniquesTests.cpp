#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "JointComboActor.h"
#include "ComboAbilityComponent.h"
#include "AutoAttackComponent.h"
#include "NinjaBuildComponent.h"
#include "NinjaBuildProjectile.h"
#include "SamuraiBladeWave.h"
#include "SamuraiIaijutsu.h"
#include "CharacterManagerComponent.h"
#include "CharacterStatsComponent.h"
#include "SamuraiCharacter.h"
#include "SurvivorAbilityComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NinjaCharacter.h"
#include "PlayerCameraRig.h"
#include "SwapAfterimage.h"
#include "SwapPresentationComponent.h"
#include "CharacterSmokeBurst.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimMontage.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "HeavensDivideMetaSaveGame.h"
#include "MetaSkillTree.h"
#include "RunStanceCatalog.h"
#include "RunLoadoutSubsystem.h"
#include "RunReportSubsystem.h"
#include "AttackProjectileBase.h"
#include "HealthComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "StanceSelectionWidget.h"
#include "MetaSkillTreeWidget.h"
#include "Kismet/GameplayStatics.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJointComboTest,"HeavensDivide.Combat.ComboTechniques",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FJointComboTest::RunTest(const FString&)
{
    const FString Slot=TEXT("Automation_JointCombos_")+FGuid::NewGuid().ToString(EGuidFormats::Digits),OldCmd=FCommandLine::Get();
    FCommandLine::Set(*(TEXT("-MetaSaveSlot=")+Slot+TEXT(" ")+OldCmd));
    auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();FCommandLine::Set(*OldCmd);
    auto* W=GI->GetWorld();W->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT {GI->Shutdown();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);UGameplayStatics::DeleteGameInSlot(Slot,0);};
    auto* Meta=GI->GetSubsystem<USynergyMetaProgressionSubsystem>();auto* L=GI->GetSubsystem<URunLoadoutSubsystem>();
    TestEqual(TEXT("Exactly three joint recipes"),ComboCatalog::Entries().Num(),3);
    TestFalse(TEXT("Locked combo cannot equip"),L->SetSelectedComboAbility(TEXT("BloodMoon")));
    Meta->AwardCurrencies(1000,1000);Meta->RetrySkillReward();TestTrue(TEXT("Learn shared root"),Meta->PurchaseSkill(TEXT("Ability.ComboUnlock")));
    TestEqual(TEXT("Root includes Crimson Tempest"),L->GetSelectedComboAbility(),FName(TEXT("BloodMoon")));
    for(const auto& Entry:ComboCatalog::Entries()) {
        auto* D=ComboCatalog::Definition(Entry.Id);if(!TestNotNull(Entry.Name,D))return false;
        TestTrue(TEXT("Definition is a joint recipe"),D->IsJoint());TestEqual(TEXT("Recipe uses both heroes"),D->Character,EUpgradeStatTarget::SharedPlayer);
        TestNotNull(TEXT("Samurai montage is saved"),D->SamuraiMontage.Get());TestNotNull(TEXT("Ninja montage is saved"),D->NinjaMontage.Get());
        TestNotNull(TEXT("Hit effect saved"),D->HitVFX.Get());
        TestNotNull(TEXT("Charge effect saved"),D->ChargeVFX.Get());// Secondary effects are optional artist assignments.
        TestTrue(TEXT("Project-owned hit VFX"),D->HitVFX&&D->HitVFX->GetPathName().StartsWith(TEXT("/Game/HeavensDivide/VFX/Combos/Joint/")));
        if(Entry.Id!=TEXT("BloodMoon"))TestTrue(TEXT("Learn shared recipe"),Meta->PurchaseSkill(ComboCatalog::UnlockId(Entry.Id)));
        TestTrue(TEXT("Equip shared recipe"),L->SetSelectedComboAbility(Entry.Id));
        TestEqual(TEXT("Both heroes share the equipped recipe"),L->GetComboAbilityFor(EUpgradeStatTarget::Samurai),L->GetComboAbilityFor(EUpgradeStatTarget::Ninja));
    }
    TestFalse(TEXT("Retired solo recipe rejected"),L->SetSelectedComboAbility(TEXT("VoidStep")));
    TestTrue(TEXT("Equip Crimson"),L->SetSelectedComboAbility(TEXT("BloodMoon")));
    // Old solo purchases migrate without losing the currency invested in removed choices.
    auto* OldSave=NewObject<UHeavensDivideMetaSaveGame>();OldSave->SaveVersion=6;OldSave->SoulEmbers=100;OldSave->SkillEmbersSpent=138;
    for(const TCHAR* Id:{TEXT("Ability.ComboUnlock"),TEXT("Ability.Combo.BloodMoon"),TEXT("Ability.Combo.SerpentSeal"),TEXT("Ability.Combo.ReturningStorm"),TEXT("Ability.Combo.Moonwell"),TEXT("Ability.Combo.Starfall"),TEXT("Ability.Combo.IronLotus")})OldSave->SkillRanks.Add(Id,1);
    TestTrue(TEXT("Write isolated old profile"),UGameplayStatics::SaveGameToSlot(OldSave,Slot,0));Meta->LoadMetaProgression();
    TestEqual(TEXT("Retired and duplicate purchases refunded"),Meta->GetSoulEmbers(),172);
    TestTrue(TEXT("Fang purchase migrates to Eclipse"),L->IsComboAbilityUnlocked(TEXT("HeavensDivide")));
    TestTrue(TEXT("Moonwell purchase retains Lunar"),L->IsComboAbilityUnlocked(TEXT("Moonwell")));
    Meta->LoadMetaProgression();TestEqual(TEXT("Migration refunds only once"),Meta->GetSoulEmbers(),172);

    Meta->AwardCurrencies(0,1000);Meta->RetrySkillReward();
    for (const auto& Stance:RunStances::All) if (!Meta->IsStanceUnlocked(Stance.Id)) TestTrue(TEXT("Unlock stance for isolated route coverage"),Meta->PurchaseStance(Stance.Id));
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("EclipseDeathCloth/JointPreviews");IFileManager::Get().MakeDirectory(*Dir,true);
    const auto RenderUI=[&](const TSharedRef<SWidget>& Slate,FVector2D Size,const FString& Name){
        FAssetCompilingManager::Get().FinishAllCompilation();FlushRenderingCommands();FWidgetRenderer R(false);auto* T=R.DrawWidget(Slate,Size);
        for(int I=0;I<4;++I)R.DrawWidget(T,Slate,Size,1.f/60);FlushRenderingCommands();TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
        if(T&&T->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)){TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(T->SizeX,T->SizeY,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(Dir/Name+TEXT(".png")));}
    };
    auto* Menu=CreateWidget<UStanceSelectionWidget>(GI);Menu->SelectSetupPage(ERunSetupPage::SynergiesAndCombo);
    RenderUI(Menu->TakeWidget(),{1280,720},TEXT("Loadout720"));RenderUI(Menu->TakeWidget(),{1920,1080},TEXT("Loadout1080"));Menu->ReleaseSlateResources(true);
    auto* Tree=CreateWidget<UMetaSkillTreeWidget>(GI);Tree->SelectPage(EMetaTreePage::Abilities);RenderUI(Tree->TakeWidget(),{1280,720},TEXT("Abilities720"));Tree->ReleaseSlateResources(true);

    auto* PCClass=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    auto* SamClass=LoadClass<ASamuraiCharacter>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai.BP_Samurai_C"));
    auto* NinClass=LoadClass<ANinjaCharacter>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.BP_Ninja_C"));
    auto* PC=W->SpawnActor<ASurvivorPlayerController>(PCClass);auto* U=PC->GetPlayerUpgrades();auto* Party=PC->GetCharacterManager();
    // Bind presentation as BeginPlay does in real gameplay; this isolated world
    // deliberately does not start the full controller/run scheduler.
    auto* Abilities=PC->FindComponentByClass<USurvivorAbilityComponent>();
    FindFProperty<FObjectProperty>(USurvivorAbilityComponent::StaticClass(),TEXT("Controller"))->SetObjectPropertyValue_InContainer(Abilities,PC);
    FindFProperty<FObjectProperty>(USurvivorAbilityComponent::StaticClass(),TEXT("Upgrades"))->SetObjectPropertyValue_InContainer(Abilities,U);
    FActorSpawnParameters P;P.Owner=PC;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Sam=W->SpawnActor<ASamuraiCharacter>(SamClass,{0,0,88},FRotator::ZeroRotator,P);
    auto* Nin=W->SpawnActor<ANinjaCharacter>(NinClass,{0,0,88},FRotator::ZeroRotator,P);
    const auto SamScale=Sam->GetMesh()->GetComponentScale();
    TestEqual(TEXT("Samurai presentation enlarged in normal gameplay"),SamScale.Z,1.22,.001);
    TestEqual(TEXT("Samurai scale preserves planted mesh root"),Sam->GetMesh()->GetComponentLocation().Z-Sam->GetActorLocation().Z,-90.,.01);
    auto* ScaleCopy=W->SpawnActor<ASwapAfterimage>();
    if(TestTrue(TEXT("Scaled Samurai can become a combo performer"),ScaleCopy->InitializeJointCast(Sam,ComboCatalog::Definition(TEXT("BloodMoon"))->SamuraiMontage,FVector::ForwardVector,2)))
        TestTrue(TEXT("Combo copy inherits full Samurai size"),ScaleCopy->FindComponentByClass<USkeletalMeshComponent>()->GetComponentScale().Equals(SamScale,.001));
    ScaleCopy->Destroy();

    auto SetMember=[&](const TCHAR* Field,AActor* Value){FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),Field)->SetObjectPropertyValue_InContainer(Party,Value);};
    SetMember(TEXT("SamuraiCharacter"),Sam);SetMember(TEXT("NinjaCharacter"),Nin);SetMember(TEXT("ActiveCharacter"),Sam);
    Sam->SetCharacterMode(ECharacterMode::Active);Nin->SetCharacterMode(ECharacterMode::Inactive);Sam->SetVisualFacingRotation(FRotator::ZeroRotator);Nin->SetVisualFacingRotation(FRotator::ZeroRotator);
    auto* SA=Sam->FindComponentByClass<UAutoAttackComponent>();auto* NA=Nin->FindComponentByClass<UAutoAttackComponent>();SA->OwnerCharacter=Sam;NA->OwnerCharacter=Nin;SA->StopAutoAttack();NA->StopAutoAttack();
    auto* B=Nin->FindComponentByClass<UNinjaBuildComponent>();auto* C=PC->FindComponentByClass<UComboAbilityComponent>();
    FPlayerUpgradeRunState Empty;U->CaptureRunState(Empty);
    auto Grant=[&](FName Id){auto* D=U->FindUpgradeDefinition(Id);return TestNotNull(*Id.ToString(),D)&&TestTrue(*Id.ToString(),U->AcquireUpgrade(D));};
    auto Enemy=[&](FVector Point,EPlayerAttackSource Restriction=EPlayerAttackSource::Other){auto* E=W->SpawnActor<AEnemyBase>(Point,FRotator::ZeroRotator,P);E->ConfigureObjectiveEnemy(100000,Restriction,nullptr,FLinearColor::White);E->GetHealthComponent()->RestoreCurrentHealth(100000);return E;};
    auto Health=[](AEnemyBase* E){return E->GetHealthComponent()->GetCurrentHealth();};
    auto Cleanup=[&](){
        for(TActorIterator<AJointComboActor> I(W);I;++I)I->Destroy();for(TActorIterator<AEnemyBase> I(W);I;++I)I->Destroy();for(TActorIterator<ASamuraiBladeWave> I(W);I;++I)I->Destroy();for(TActorIterator<ASamuraiIaijutsu> I(W);I;++I)I->Destroy();
        for(TActorIterator<ASwapAfterimage> I(W);I;++I)I->Destroy();for(TActorIterator<AAbilityAccent> I(W);I;++I)I->Destroy();
        for(TObjectIterator<UNiagaraComponent> I;I;++I)if(I->GetWorld()==W)I->DestroyComponent();
        B->ClearProjectiles();for(TActorIterator<AAttackProjectileBase> I(W);I;++I)I->Destroy();U->RestoreRunState(Empty);Sam->bComboAbilityActive=false;Nin->bComboAbilityActive=false;Sam->GetVisualRoot()->SetHiddenInGame(false,true);SA->StopAutoAttack();NA->StopAutoAttack();
        Sam->SetActorLocation({0,0,88});Nin->SetActorLocation({0,0,88});
    };
    auto Spawn=[&](FName Id){Sam->bComboAbilityActive=true;return AJointComboActor::Spawn(Sam,ComboCatalog::Definition(Id),1.f);};
    auto* Rig=W->SpawnActor<APlayerCameraRig>();Rig->SetFollowTarget(Sam);
    const float SavedArm=Rig->CameraBoom->TargetArmLength,SavedWidth=Rig->Camera->OrthoWidth;const FRotator SavedRotation=Rig->CameraBoom->GetRelativeRotation();
    for(bool Match:{false,true}) {
        Cleanup();if(Match){Grant(TEXT("BattleStance"));Grant(TEXT("Bloodletting"));Grant(TEXT("BarrageStance"));Grant(TEXT("BarragePoisonLoad"));}
        auto* E=Enemy({160,-95,88});auto* Immune=Enemy({160,-130,88},EPlayerAttackSource::Ninja);
        auto* J=Spawn(TEXT("BloodMoon"));if(!TestNotNull(TEXT("Crimson spawns"),J))return false;
        // The current artist-authored slash is much wider than the old radius.
        TestTrue(TEXT("Blood damage uses authored VFX scale"),FMath::IsNearlyEqual(J->GetBloodSlashRadius(),J->Data->BloodSlashVFXRadius*J->Data->VFXScale*J->Data->AreaMultiplier*Sam->GetCharacterStats()->GetFinalAttackAreaMultiplier()));
        const FVector FootprintCenter=J->GetBloodSlashCenter();
        auto* Edge=Enemy(FootprintCenter+FVector(J->GetBloodSlashRadius()*.88f,0,80));
        auto* Outside=Enemy(FootprintCenter+FVector(J->GetBloodSlashRadius()+180,0,80));
        TestTrue(TEXT("Visible outer slash includes victims beyond old hitbox"),J->GetBloodSlashTargets().Contains(Edge));
        TestFalse(TEXT("Outside visible slash is excluded"),J->GetBloodSlashTargets().Contains(Outside));
        auto* AuthoredData=J->Data.Get();const float AuthoredRadius=J->GetBloodSlashRadius();
        J->Data=DuplicateObject<UComboAbilityDefinition>(AuthoredData,J);
        J->Data->VFXOffset+=FVector(0,2000,0);J->Data->VFXScale*=.5f;
        TestTrue(TEXT("Artist scale changes resize damage with VFX"),FMath::IsNearlyEqual(J->GetBloodSlashRadius(),AuthoredRadius*.5f));
        TestTrue(TEXT("Artist offset moves damage with VFX"),J->GetBloodSlashCenter().Equals(FootprintCenter+J->Direction.Rotation().RotateVector(FVector(0,2000,0)),.1));
        TestFalse(TEXT("Moved effect no longer hits old footprint"),J->GetBloodSlashTargets().Contains(E));
        J->Data=AuthoredData;
        TestNotNull(TEXT("Both performers present: Samurai"),J->SamuraiVisual.Get());TestNotNull(TEXT("Both performers present: Ninja"),J->NinjaVisual.Get());
        TestTrue(TEXT("World including enemy attacks slowed to ten percent"),FMath::IsNearlyEqual(W->GetWorldSettings()->CinematicTimeDilation,.1f));
        J->UpdateSlowMotion(W,LEVELTICK_All,.01f);
        TestEqual(TEXT("New frame compensates native cast clock"),J->CustomTimeDilation,10.f);
        TestEqual(TEXT("Enemies retain slowed world clock"),E->CustomTimeDilation,1.f);
        TestTrue(TEXT("No idle tail after animated release"),J->ControlDuration-J->Windup<=.43f);
        TestTrue(TEXT("Leader remains possessed in place"),Party->GetActiveCharacter()==Sam&&Sam->GetActorLocation().Equals({0,0,88}));
        J->Tick(J->Windup-.01f);TestEqual(TEXT("No slash before release"),Health(E),100000.f);Rig->Tick(.3f);TestTrue(TEXT("Camera dollies inward"),Rig->CameraBoom->TargetArmLength<SavedArm);
        J->Tick(.02f);TestTrue(TEXT("Slash lands after windup"),Health(E)<100000);TestEqual(TEXT("Samurai immunity respected"),Health(Immune),100000.f);
        TestTrue(TEXT("Outer visible slash actually deals damage"),Health(Edge)<100000);
        TestEqual(TEXT("Outside visual footprint stays unharmed"),Health(Outside),100000.f);
        const int32 Stacks=E->GetStatusEffectComponent()->GetStatusStacks(EEnemyStatusEffect::Bleed);TestTrue(TEXT("Off-stance slash still applies intrinsic Bleed"),Stacks>0);if(Match)TestTrue(TEXT("Bloodletting applies to joint slash"),Stacks>1);
        TestTrue(TEXT("Intrinsic Bleed has actual damage budget"),E->GetStatusEffectComponent()->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed)>0);
        TestTrue(TEXT("Accepted Blood slash creates enemy impact"),J->LastHitEffects.Contains(E));
        TestFalse(TEXT("Immune enemy receives no false impact"),J->LastHitEffects.Contains(Immune));
        const int32 EffectCount=J->Effects.Num();J->PlayHitImpact(E);
        TestEqual(TEXT("Same-beat repeats do not flood one enemy with bursts"),J->Effects.Num(),EffectCount);
        bool ImpactAtEnemy=false;
        for(const auto& Effect:J->Effects) if(Effect.Effect.IsValid() && Effect.Effect->GetAsset()==J->Data->HitVFX)
            ImpactAtEnemy|=Effect.Effect->GetComponentLocation().Equals(E->GetActorLocation()+J->Data->HitVFXOffset,.01);
        TestTrue(TEXT("Impact follows enemy centre, not ground plane"),ImpactAtEnemy);

        auto* Lethal=Enemy({200,0,88});Lethal->DispatchBeginPlay();Lethal->GetHealthComponent()->RestoreCurrentHealth(1);
        J->Age+=.09f;J->BloodSlash(true);
        TestTrue(TEXT("Killing slash still produces its impact burst"),J->LastHitEffects.Contains(Lethal));
        const auto LethalFX=J->Effects[J->Effects.Num()-2].Effect;
        Lethal->Destroy();TestTrue(TEXT("Enemy deletion cannot destroy combo-owned impact"),LethalFX.IsValid());
        J->Tick(.12f);int32 Shots=0;AAttackProjectileBase* Shot=nullptr;
        for(TActorIterator<AAttackProjectileBase> I(W);I;++I)if(I->AssistDamageSource==TEXT("BloodMoon")){++Shots;Shot=*I;}
        TestTrue(TEXT("Arc volley emitted"),Shots>=J->Data->Count);if(Match)TestTrue(TEXT("Barrage bonus projectiles inherited"),Shots>J->Data->Count);
        if(Shot)TestEqual(TEXT("Native projectiles retain normal speed during slow motion"),Shot->CustomTimeDilation,10.f);
        if(Shot){auto* Target=Enemy({700,250,88});Shot->HandleProjectileOverlap(nullptr,Target,nullptr,0,false,FHitResult());if(Match)TestTrue(TEXT("Barrage poison load inherited"),Target->GetStatusEffectComponent()->GetStatusStacks(EEnemyStatusEffect::Poison)>1);}
        const FVector DepartAt=J->Ground(J->GetHelper()->GetActorLocation());
        J->ReleasePerformers();
        bool Departure=false;for(const auto& FX:J->Effects) if(FX.Effect.IsValid() && FX.Effect->GetAsset()==J->HelperSmoke(true) && FX.Stop>J->Age)
            Departure|=FX.Effect->GetComponentLocation().Equals(DepartAt,.01);
        TestTrue(TEXT("Departure smoke survives performer removal"),Departure);
        Rig->Tick(1);TestTrue(TEXT("Camera rotation restored"),Rig->CameraBoom->GetRelativeRotation().Equals(SavedRotation,.001));TestEqual(TEXT("Camera distance restored"),Rig->CameraBoom->TargetArmLength,SavedArm);TestEqual(TEXT("Ortho width restored"),Rig->Camera->OrthoWidth,SavedWidth);
        TestTrue(TEXT("Visual and damage guard restored"),!Sam->GetVisualRoot()->bHiddenInGame&&PC->GetPlayerHealthComponent()->IsDamageEnabled());
        TestEqual(TEXT("World speed restored after recovery"),W->GetWorldSettings()->CinematicTimeDilation,1.f);
        TestEqual(TEXT("Camera compensation restored"),Rig->CustomTimeDilation,1.f);
    }
    for(bool Match:{false,true}) {
        Cleanup();if(Match){Grant(TEXT("Iaijutsu"));Grant(TEXT("IaijutsuWidth"));Grant(TEXT("IaijutsuChargeSpeed"));Grant(TEXT("ReturningFang"));Grant(TEXT("FangDamage"));}
        auto* In=Enemy({500,-95,88});auto* In2=Enemy({850,-95,88});auto* Out=Enemy({600,1600,88});auto* NinjaOnly=Enemy({650,0,88},EPlayerAttackSource::Ninja);
        const FVector CastStart=Sam->GetActorLocation();
        auto* J=Spawn(TEXT("HeavensDivide"));
        const FVector PerformerStart=J->SamuraiVisual->GetActorLocation();
        TestEqual(TEXT("Lane is not an immediate hit"),Health(In),100000.f);
        int32 Performers=0;for(TActorIterator<ASwapAfterimage> I(W);I;++I)++Performers;
        TestEqual(TEXT("Only the two joint performers; no duplicate Iaijutsu ghost"),Performers,2);
        if(Match)TestTrue(TEXT("Charge speed shortens cinematic windup"),J->Windup<J->Data->Windup);
        const auto FangCount=[&](){int32 Count=0;for(TActorIterator<ANinjaBuildProjectile> I(W);I;++I)if(I->JointOwner.Get()==J)++Count;return Count;};
        J->Tick(J->HelperArrivalTime-.01f);TestEqual(TEXT("No invisible helper throws before arrival"),FangCount(),0);
        TestTrue(TEXT("Helper waits for camera push"),J->GetHelper()->IsHidden());
        TestEqual(TEXT("Samurai-led combo uses Ninja purple smoke"),J->HelperSmoke(false),Nin->SwapPresentation->CharacterSmokeVFX.Get());
        J->Tick(.08f);TestEqual(TEXT("Only one Fang in first charge beat"),FangCount(),1);
        TestFalse(TEXT("Helper appears after camera push"),J->GetHelper()->IsHidden());
        TestEqual(TEXT("First throw uses primary montage"),J->NinjaVisual->GetJointMontage(),J->Data->NinjaMontage.Get());
        TestNotNull(TEXT("Saved alternate throw montage"),J->Data->NinjaAlternateMontage.Get());
        TestTrue(TEXT("Throw montages are distinct"),J->Data->NinjaMontage!=J->Data->NinjaAlternateMontage);
        J->Tick(.04f);TestEqual(TEXT("No simultaneous opening fan"),FangCount(),1);
        J->Tick(.11f);TestEqual(TEXT("Second throw switches the actual animation asset"),J->NinjaVisual->GetJointMontage(),J->Data->NinjaAlternateMontage.Get());
        auto* ThrowAnim=J->NinjaVisual->FindComponentByClass<USkeletalMeshComponent>()->GetSingleNodeInstance();
        const auto* ThrowInstance=ThrowAnim->GetActiveInstanceForMontage(J->Data->NinjaAlternateMontage);
        TestTrue(TEXT("Alternate throw is fully posed without reference-pose blend"),ThrowInstance&&ThrowInstance->GetWeight()>.99f);
        J->Tick(.15f);TestEqual(TEXT("Third throw returns to primary montage"),J->NinjaVisual->GetJointMontage(),J->Data->NinjaMontage.Get());
        while(J->Age<J->Windup-.02f)J->Tick(FMath::Min(.035f,J->Windup-.02f-J->Age));
        TestTrue(TEXT("Ninja repeats individual throws during charge"),FangCount()>=3);
        TestEqual(TEXT("Samurai still charging while Ninja throws"),Health(In),100000.f);
        TestTrue(TEXT("Samurai stays at his starting point throughout charge"),Sam->GetActorLocation().Equals(CastStart,.01)&&J->SamuraiVisual->GetActorLocation().Equals(PerformerStart,.01));
        J->Tick(.03f);for(TActorIterator<ASamuraiIaijutsu> I(W);I;++I)I->Tick(J->Windup+.01f);
        TestTrue(TEXT("Charged lane damages its actual victims"),Health(In)<100000&&Health(In2)<100000);TestEqual(TEXT("Outside lane excluded"),Health(Out),100000.f);
        TestEqual(TEXT("Lane victims include Samurai-immune enemies for Ninja"),J->Victims.Num(),3);J->Tick(.1f);
        int32 Fangs=0;for(TActorIterator<ANinjaBuildProjectile> I(W);I;++I)if(I->bJointProjectile){++Fangs;TestTrue(TEXT("Lightning-fast native Fangs"),I->Speed>=4500);I->Tick(.2f);}
        TestTrue(TEXT("Serial throws visit all three lane targets"),J->FiredVictims.Num()==3&&Fangs>=3);
        if(Match)TestTrue(TEXT("Native Iaijutsu mark survives"),In->GetIaijutsuDamageMultiplier()>1);
        auto* Fallen=Enemy({500,700,88});
        // This isolated world does not begin gameplay globally. Start the victim
        // so its native health-death delegate runs, as it does in a real level.
        if(!Fallen->HasActorBegunPlay())Fallen->DispatchBeginPlay();
        TestTrue(TEXT("Follow-through fixture starts alive"),IsValid(Fallen)&&!Fallen->IsDead());
        J->FangQueue.Reset();J->RecordVictim(Fallen);Fallen->GetHealthComponent()->SetDamageEnabled(true);
        Fallen->GetHealthComponent()->ApplyDamage(Fallen->GetHealthComponent()->GetCurrentHealth()+1);
        TestTrue(TEXT("Follow-through fixture actually dies"),!IsValid(Fallen)||Fallen->IsDead());
        Fallen->Destroy(); // Exercise the immediate-removal case with the captured position.
        J->LaunchFangs();int32 VisualFangs=0;
        for(TActorIterator<ANinjaBuildProjectile> I(W);I;++I)if(I->JointOwner.Get()==J&&!I->IsActorTickEnabled()&&I->Damage==0&&I->FindComponentByClass<UProjectileMovementComponent>())++VisualFangs;
        TestEqual(TEXT("Killed slash victim still receives cosmetic Fang without gameplay damage"),VisualFangs,1);
        // Exhaust the queue, then verify the actual montage keeps progressing
        // beyond a throw beat instead of holding its last frame until unlock.
        J->FangQueue.Reset();J->NextFang=J->ControlDuration+1;
        J->Tick(.18f);
        const float PoseBefore=ThrowAnim->GetCurrentTime();
        const FVector WristBefore=J->NinjaVisual->FindComponentByClass<USkeletalMeshComponent>()->GetSocketLocation(TEXT("RightHand"));
        J->Tick(.04f);
        TestTrue(TEXT("Empty queue preserves animated Ninja recovery"),ThrowAnim->GetCurrentTime()>PoseBefore);
        const FVector WristAfter=J->NinjaVisual->FindComponentByClass<USkeletalMeshComponent>()->GetSocketLocation(TEXT("RightHand"));
        TestTrue(TEXT("Ninja recovery changes the visible hand pose"),!WristAfter.Equals(WristBefore,.001));
        TestTrue(TEXT("Eclipse leaves the actual Samurai at his casting origin"),Sam->GetActorLocation().Equals(CastStart,.01));
        TestTrue(TEXT("Eclipse performer stays at its casting origin during release"),J->SamuraiVisual.IsValid()&&J->SamuraiVisual->GetActorLocation().Equals(PerformerStart,.01));
        J->ReleasePerformers();TestTrue(TEXT("Samurai resumes control at his casting origin"),Sam->GetActorLocation().Equals(CastStart,.01));
    }
    Cleanup();
    {
        SetMember(TEXT("ActiveCharacter"),Nin);Sam->SetCharacterMode(ECharacterMode::Inactive);Nin->SetCharacterMode(ECharacterMode::Active);Rig->SetFollowTarget(Nin);
        Nin->bComboAbilityActive=true;const FVector Start=Nin->GetActorLocation();
        auto* J=AJointComboActor::Spawn(Nin,ComboCatalog::Definition(TEXT("HeavensDivide")),1);
        TestEqual(TEXT("Ninja-led combo uses Samurai red smoke"),J->HelperSmoke(false),Sam->SwapPresentation->CharacterSmokeVFX.Get());
        const FVector HelperStart=J->SamuraiVisual->GetActorLocation();
        J->Tick(J->Windup+.24f);
        TestTrue(TEXT("Samurai helper also stays at his casting origin"),J->SamuraiVisual->GetActorLocation().Equals(HelperStart,.01));
        TestTrue(TEXT("Ninja-led cast keeps the controlled hero at the throw point"),Nin->GetActorLocation().Equals(Start));
        TestTrue(TEXT("Ninja performer matches controlled capsule"),J->NinjaVisual->GetActorLocation().Equals(Start));
        J->ReleasePerformers();TestTrue(TEXT("Ninja-led recovery has no location snap"),Nin->GetActorLocation().Equals(Start));
        SetMember(TEXT("ActiveCharacter"),Sam);Nin->SetCharacterMode(ECharacterMode::Inactive);Sam->SetCharacterMode(ECharacterMode::Active);Rig->SetFollowTarget(Sam);
    }
    for(bool Match:{false,true}) {
        Cleanup();if(Match){Grant(TEXT("BladeWave"));Grant(TEXT("CrescentDamage"));Grant(TEXT("GreatShuriken"));Grant(TEXT("ShurikenSize"));}
        auto* J=Spawn(TEXT("Moonwell"));J->Tick(J->Windup+.01f);if(!TestNotNull(TEXT("Great shuriken exists off and on stance"),J->Wheel.Get()))return false;
        J->Wheel->Tick(.15f);J->Tick(.42f);TestTrue(TEXT("Crescent injection occurs"),J->bInjected);
        int32 Waves=0;for(TActorIterator<ASamuraiBladeWave> I(W);I;++I){++Waves;TestEqual(TEXT("Waves attributed to Lunar Tempest"),I->DamageSource,FName(TEXT("Moonwell")));TestTrue(TEXT("Wave belongs to joint cast"),I->JointOwner.Get()==J);}
        TestTrue(TEXT("Infused shuriken emits native Crescent waves"),Waves>=2);
        if(!Match) TestEqual(TEXT("Off-stance Lunar wheel is fifty percent larger"),J->Wheel->Radius,225.f);
        const float UntilNext=J->NextWave-J->Age;
        J->Tick(UntilNext*.5f);
        int32 Quiet=0;for(TActorIterator<ASamuraiBladeWave> I(W);I;++I)++Quiet;
        TestEqual(TEXT("No extra Crescent pulse between emission beats"),Quiet,Waves);
        const FVector First=J->Wheel->GetActorLocation();J->Wheel->Tick(UntilNext);J->Tick(UntilNext*.5f+.01f);
        TestTrue(TEXT("Shuriken advances while shedding waves"),J->Wheel.IsValid()&&J->Wheel->GetActorLocation().X>First.X);
        int32 More=0;for(TActorIterator<ASamuraiBladeWave> I(W);I;++I)++More;TestTrue(TEXT("Waves continue along path"),More>Waves);
        if(!Match) {
            const FVector Probe(1800,1800,88);
            J->EmitWaves(Probe,37.f);TArray<FVector> Aims;
            for(TActorIterator<ASamuraiBladeWave> I(W);I;++I)if(FVector::Dist2D(I->GetActorLocation(),Probe)<1)
                if(auto* Move=I->FindComponentByClass<UProjectileMovementComponent>())Aims.Add(Move->Velocity.GetSafeNormal2D());
            TestEqual(TEXT("Exactly four base arms"),Aims.Num(),4);
            for(int32 Arm=0;Arm<4;++Arm)TestTrue(TEXT("Plus rotates with wheel yaw"),Aims.ContainsByPredicate([Arm](const FVector& Aim){return Aim.Equals(FRotator(0,37+90*Arm,0).Vector(),.001);}));
        }
    }
    Cleanup();
    // Verify the complete camera trajectory, not only its final restored values.
    {
        auto* J=Spawn(TEXT("BloodMoon"));const float FOV=Rig->Camera->FieldOfView;
        float LastArm=SavedArm,LastYaw=SavedRotation.Yaw;
        for(int Frame=0;Frame<60;++Frame) {
            Rig->Tick(J->Windup/60.f);
            const float Arm=Rig->CameraBoom->TargetArmLength,Yaw=Rig->CameraBoom->GetRelativeRotation().Yaw;
            TestTrue(TEXT("Charge dolly has no reversal"),Arm<=LastArm+.001f);
            TestTrue(TEXT("Charge orbit has no reversal"),FMath::FindDeltaAngleDegrees(LastYaw,Yaw)>=-.001f);
            LastArm=Arm;LastYaw=Yaw;
        }
        const FVector HoldPosition=Rig->GetActorLocation();const FRotator HoldRotation=Rig->CameraBoom->GetRelativeRotation();
        Rig->Tick(.2f);TestTrue(TEXT("Released attack holds camera position"),Rig->GetActorLocation().Equals(HoldPosition,.01));
        TestTrue(TEXT("No extra post-attack orbit"),Rig->CameraBoom->GetRelativeRotation().Equals(HoldRotation,.001));
        TestEqual(TEXT("Perspective FOV never pumps"),Rig->Camera->FieldOfView,FOV);
        J->ReleasePerformers();
        for(int Frame=0;Frame<35;++Frame) {
            Rig->Tick(1.f/60);
            const float Arm=Rig->CameraBoom->TargetArmLength;
            TestTrue(TEXT("Single monotonic return without zoom overshoot"),Arm>=LastArm-.001f&&Arm<=SavedArm+.001f);LastArm=Arm;
        }
        TestFalse(TEXT("Return completes without a second movement"),Rig->bJointCameraActive);
    }
    Cleanup();
    {
        auto* Settings=W->GetWorldSettings();Settings->SetTimeDilation(.7f);Settings->SetCinematicTimeDilation(.8f);
        auto* J=Spawn(TEXT("BloodMoon"));TestTrue(TEXT("Existing slow motion composes"),FMath::IsNearlyEqual(Settings->GetEffectiveTimeDilation(),.056f));
        J->UpdateSlowMotion(W,LEVELTICK_All,.001f);
        TestEqual(TEXT("Native combo clock compensated independently"),J->CustomTimeDilation*Settings->GetEffectiveTimeDilation(),.56f,.0001f);
        int32 Deferred=0;J->ScheduleAttack(.2f,[&Deferred]{++Deferred;});
        J->Tick(.1f);TestEqual(TEXT("Delayed route attacks wait for their beat"),Deferred,0);
        J->Tick(.11f);TestEqual(TEXT("Delayed route attacks use the combo clock"),Deferred,1);
        // An upgrade/death freeze must remain in force when the cinematic ends.
        Settings->SetTimeDilation(.0001f);J->ReleasePerformers(true);
        TestEqual(TEXT("Prior cinematic layer restored"),Settings->CinematicTimeDilation,.8f);
        TestEqual(TEXT("External freeze remains owned by its caller"),Settings->TimeDilation,.0001f);
        TestEqual(TEXT("Attack clock restored on cancellation"),J->CustomTimeDilation,1.f);
        Settings->SetTimeDilation(1);Settings->SetCinematicTimeDilation(1);
    }
    Cleanup();
    {
        auto* J=Spawn(TEXT("BloodMoon"));auto* Settings=W->GetWorldSettings();
        float LastWorld=.1f,LastCast=.35f;bool SawRamp=false;float Elapsed=0;
        // Mirror the engine order: compute world delta first, pre-actor callback
        // second, actor tick last. This catches compensating with next-frame time.
        while (!J->bPerformersReleased && Elapsed<3.f) {
            const float FrameSpeed=Settings->CinematicTimeDilation;
            const float FrameDelta=FrameSpeed/120.f;
            J->UpdateSlowMotion(W,LEVELTICK_All,FrameDelta);
            if (J->Age>=J->Windup) {
                const float CastRate=J->CustomTimeDilation*FrameSpeed;
                TestTrue(TEXT("Release speed increases monotonically"),CastRate>=LastCast-.001f && CastRate<=1.001f);
                TestTrue(TEXT("World recovers smoothly from slow motion"),Settings->CinematicTimeDilation>=LastWorld-.001f && Settings->CinematicTimeDilation<=1.001f);
                SawRamp|=CastRate>.36f&&CastRate<.99f;
                LastCast=CastRate;LastWorld=Settings->CinematicTimeDilation;
            }
            Rig->Tick(FrameDelta*Rig->CustomTimeDilation);
            J->Tick(FrameDelta*J->CustomTimeDilation);Elapsed+=1.f/120.f;
        }
        TestTrue(TEXT("Release crosses intermediate playback rates"),SawRamp);
        TestTrue(TEXT("Acceleration adds less than a quarter second to control lock"),Elapsed<J->ControlDuration+.25f);
        TestTrue(TEXT("Full speed reached before recovery ends"),LastCast>.99f&&LastWorld>.99f);
        TestEqual(TEXT("Ramp cleanup restores original actor rate"),J->CustomTimeDilation,1.f);
        TestEqual(TEXT("Ramp cleanup restores world rate"),Settings->CinematicTimeDilation,1.f);
    }
    Cleanup();Sam->bComboAbilityActive=false;C->RestoreCombo(100);TestTrue(TEXT("Shared meter activates joint cast"),C->TryActivateAbility());TestEqual(TEXT("Meter consumed once"),C->GetCombo(),0.f);TestNotNull(TEXT("Component owns joint performer"),C->ExecutingJoint.Get());
    auto* Running=C->ExecutingJoint.Get();TestFalse(TEXT("Cannot reactivate during performance"),C->TryActivateAbility());
    FindFProperty<FBoolProperty>(ASurvivorPlayerController::StaticClass(),TEXT("bIsPlayerDead"))->SetPropertyValue_InContainer(PC,true);C->TickComponent(.1f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Death cancels cast"),Running->IsActorBeingDestroyed());TestFalse(TEXT("Death releases combo lock"),Sam->bComboAbilityActive);
    TestEqual(TEXT("Death restores cinematic time"),W->GetWorldSettings()->CinematicTimeDilation,1.f);
    FindFProperty<FBoolProperty>(ASurvivorPlayerController::StaticClass(),TEXT("bIsPlayerDead"))->SetPropertyValue_InContainer(PC,false);PC->GetPlayerHealthComponent()->SetDamageEnabled(true);Cleanup();
    // Inspect real characters and real spawned effects through each three-part performance.
    Sam->GetCharacterMovement()->SetComponentTickEnabled(false);Nin->GetCharacterMovement()->SetComponentTickEnabled(false);
    auto* CaptureActor=W->SpawnActor<AActor>();auto* Capture=NewObject<USceneCaptureComponent2D>(CaptureActor);CaptureActor->AddInstanceComponent(Capture);Capture->RegisterComponent();
    Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->ShowFlags.SetLighting(false);Capture->ProjectionType=ECameraProjectionMode::Orthographic;Capture->OrthoWidth=1900;
    auto* RT=NewObject<UTextureRenderTarget2D>(CaptureActor);RT->RenderTargetFormat=RTF_RGBA8;RT->InitAutoFormat(1280,720);RT->UpdateResourceImmediate();Capture->TextureTarget=RT;
    auto* Floor=W->SpawnActor<AActor>();auto* Surface=NewObject<UStaticMeshComponent>(Floor);Floor->SetRootComponent(Surface);Floor->AddInstanceComponent(Surface);
    Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));Surface->SetWorldScale3D(FVector(50,50,1));Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Surface->SetCollisionResponseToAllChannels(ECR_Block);Surface->RegisterComponent();
    if(auto* Mat=Surface->CreateDynamicMaterialInstance(0)) {Mat->SetVectorParameterValue(TEXT("Color"),FLinearColor(.08f,.1f,.13f));Mat->SetVectorParameterValue(TEXT("BaseColor"),FLinearColor(.08f,.1f,.13f));}
    for(int Pass=0;Pass<3;++Pass)for(const auto& Entry:ComboCatalog::Entries()) {
        Cleanup();
        auto* EnemyClass=LoadClass<AEnemyBase>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGrunt.BP_EnemyGrunt_C"));
        for(int I=0;I<10;++I){auto* E=W->SpawnActor<AEnemyBase>(EnemyClass,{250.f+I*75,float(I%3-1)*145,88},FRotator::ZeroRotator,P);E->GetHealthComponent()->RestoreCurrentHealth(1);E->ConfigureObjectiveEnemy(100000,EPlayerAttackSource::Other,nullptr,FLinearColor::White);E->GetHealthComponent()->RestoreCurrentHealth(100000);}
        ACharacterBase* PreviewLeader=Pass==2?static_cast<ACharacterBase*>(Nin):static_cast<ACharacterBase*>(Sam);
        SetMember(TEXT("ActiveCharacter"),PreviewLeader);
        Sam->SetCharacterMode(Pass==2?ECharacterMode::Inactive:ECharacterMode::Active);
        Nin->SetCharacterMode(Pass==2?ECharacterMode::Active:ECharacterMode::Inactive);
        Rig->SetFollowTarget(PreviewLeader);PreviewLeader->bComboAbilityActive=true;
        auto* J=AJointComboActor::Spawn(PreviewLeader,ComboCatalog::Definition(Entry.Id),1);float Previous=0;TMap<TWeakObjectPtr<UNiagaraComponent>,float> LastSimulation;
        for(int Stage=0;Stage<4;++Stage){const float T=Stage==0?J->HelperArrivalTime+.08f:Stage==1?J->Windup+.06f:Stage==2?J->Windup+.18f:J->ControlDuration+.12f;
            // Advance choreography, native movement and particles at the same cadence.
            // Large preview jumps previously aged fresh bursts past their lifetime.
            while(Previous<T-UE_SMALL_NUMBER) {
                const float Dt=FMath::Min(1.f/60,T-Previous);J->Tick(Dt);
                for(TActorIterator<ASamuraiIaijutsu> I(W);I;++I)I->Tick(Dt);
                for(TActorIterator<ANinjaBuildProjectile> I(W);I;++I)I->Tick(Dt);
                for(TActorIterator<AAttackProjectileBase> I(W);I;++I)if(auto* Move=I->FindComponentByClass<UProjectileMovementComponent>())Move->TickComponent(Dt,LEVELTICK_All,nullptr);
                for(TActorIterator<ASamuraiBladeWave> I(W);I;++I){if(auto* Move=I->FindComponentByClass<UProjectileMovementComponent>())Move->TickComponent(Dt,LEVELTICK_All,nullptr);I->Tick(Dt);}
                for(TActorIterator<ASwapAfterimage> I(W);I;++I)I->Tick(Dt);
                for(TActorIterator<AAbilityAccent> I(W);I;++I)I->Tick(Dt);
                Rig->Tick(Dt);Previous+=Dt;
                for(TObjectIterator<UNiagaraComponent> I;I;++I)if(I->GetWorld()==W&&I->GetAsset()&&I->IsActive()&&!LastSimulation.Contains(*I)) {
                    I->GetAsset()->WaitForCompilationComplete(true,false);I->SetForceSolo(true);LastSimulation.Add(*I,Previous);
                }
            }
            TArray<UNiagaraComponent*> Effects;
            for(auto& Pair:LastSimulation)if(Pair.Key.IsValid()) {
                auto* FX=Pair.Key.Get();const float Elapsed=Previous-Pair.Value;
                FX->AdvanceSimulation(FMath::Max(1,FMath::RoundToInt(Elapsed/.016f)),.016f);Pair.Value=Previous;
                FX->UpdateBounds();FX->MarkRenderStateDirty();Effects.Add(FX);
            }
            FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
            Capture->SetWorldLocation(Rig->GetActorLocation()-Rig->CameraBoom->GetRelativeRotation().Vector()*1400);Capture->SetWorldRotation(Rig->CameraBoom->GetRelativeRotation());Capture->OrthoWidth=1900*Rig->CameraBoom->TargetArmLength/SavedArm;
            for(int Frame=0;Frame<3;++Frame){++GFrameCounter;W->Tick(LEVELTICK_All,.001f);for(auto* FX:Effects)if(IsValid(FX))FX->MarkRenderDynamicDataDirty();W->SendAllEndOfFrameUpdates();FlushRenderingCommands();Capture->CaptureScene();FlushRenderingCommands();}
            if(Pass>=1){TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1280,720,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(Dir/Entry.Id.ToString()+(Pass==2?TEXT("_NinjaLead"):TEXT(""))+FString::Printf(TEXT("_%d.png"),Stage)));}
        }
    }
    Cleanup();return true;
}
#endif
