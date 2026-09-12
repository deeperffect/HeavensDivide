#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "MetaSkillTree.h"
#include "MetaSkillTreeWidget.h"
#include "HeavensDivideMetaSaveGame.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "SurvivorPlayerController.h"
#include "SurvivorAbilityComponent.h"
#include "CharacterManagerComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SharedPlayerStatsComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaSkillTreeTest,"HeavensDivide.Meta.SkillTree",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMetaSkillTreeTest::RunTest(const FString&)
{
	const FString Slot = TEXT("Automation_SkillTree_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString PreviousCommandLine = FCommandLine::Get();
	FCommandLine::Set(*(PreviousCommandLine + TEXT(" -MetaSaveSlot=") + Slot));
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	GI->InitializeStandalone();
	FCommandLine::Set(*PreviousCommandLine);
	UWorld* World = GI->GetWorld();
	auto* Meta = GI->GetSubsystem<USynergyMetaProgressionSubsystem>();
	if (!TestNotNull(TEXT("Progression subsystem"),Meta) || !World) return false;
	// Explicit override also isolates runs launched with another automation slot argument.
	Meta->TestSaveSlot = Slot;
	Meta->CreateFreshSave();
	TestEqual(TEXT("New profiles have no currency"),Meta->GetSoulEmbers(),0);
	TestFalse(TEXT("Cannot purchase with empty wallet"),Meta->PurchaseSkill(TEXT("Root.Vitality")));
	Meta->CurrentSave->SoulEmbers = 2000;
	Meta->CurrentSave->UnlockedSynergyUpgradeIds.Add(TEXT("Synergy.TestLegacy"));
	TestFalse(TEXT("Unknown node cannot be purchased"),Meta->PurchaseSkill(TEXT("Missing")));
	TestFalse(TEXT("Prerequisites enforced"),Meta->PurchaseSkill(TEXT("Steel.Edge")));
	TestEqual(TEXT("32 authored passives"),MetaSkillTree::Nodes().Num(),32);
	TSet<FName> Seen;
	for (const FMetaSkillNode& N : MetaSkillTree::Nodes())
	{
		TestFalse(TEXT("Stable node IDs unique"),Seen.Contains(N.Id));
		for (FName P : N.Prerequisites) TestTrue(TEXT("Catalog topological and all prerequisites exist"),Seen.Contains(P));
		Seen.Add(N.Id);
		TestTrue(TEXT("Node has concrete effect and text"),N.PerRank > 0 && !N.Description.IsEmpty());
		for (int32 Rank=0;Rank<N.MaxRank;++Rank) TestTrue(TEXT("Legal rank purchase"),Meta->PurchaseSkill(N.Id));
		TestFalse(TEXT("Rank cap enforced"),Meta->PurchaseSkill(N.Id));
	}
	TestEqual(TEXT("Complete tree costs 1600 Embers"),Meta->GetSoulEmbers(),400);
	TestTrue(TEXT("Swap speed totals 30 percent"),FMath::IsNearlyEqual(Meta->GetSkillBonus(TEXT("Swap")),.3f));
	Meta->LoadMetaProgression();
	TestEqual(TEXT("Wallet survives disk reload"),Meta->GetSoulEmbers(),400);
	TestEqual(TEXT("Capstone survives disk reload"),Meta->GetSkillRank(TEXT("Bond.Unity")),1);
	TestTrue(TEXT("Legacy discoveries preserved"),Meta->IsSynergyUpgradeUnlocked(TEXT("Synergy.TestLegacy")));
	Meta->bSimulateSaveFailure = true;
	TestFalse(TEXT("Failed refund reports failure"),Meta->RefundSkills());
	TestEqual(TEXT("Failed refund restores ranks"),Meta->GetSkillRank(TEXT("Bond.Unity")),1);
	TestEqual(TEXT("Failed refund restores wallet"),Meta->GetSoulEmbers(),400);
	Meta->bSimulateSaveFailure = false;
	TestTrue(TEXT("Full refund"),Meta->RefundSkills());
	TestEqual(TEXT("Exact paid cost returned"),Meta->GetSoulEmbers(),2000);
	TestEqual(TEXT("Refund removes cached effects"),Meta->GetSkillBonus(TEXT("Swap")),0.f);
	TestTrue(TEXT("Repeated refund safe"),Meta->RefundSkills());
	TestEqual(TEXT("No duplicate refund"),Meta->GetSoulEmbers(),2000);
	Meta->bSimulateSaveFailure = true;
	TestFalse(TEXT("Failed purchase reports failure"),Meta->PurchaseSkill(TEXT("Root.Vitality")));
	TestEqual(TEXT("Failed purchase restores wallet"),Meta->GetSoulEmbers(),2000);
	TestEqual(TEXT("Failed purchase restores rank"),Meta->GetSkillRank(TEXT("Root.Vitality")),0);
	TestFalse(TEXT("Failed run save retains pending award"),Meta->AwardSkillRun(125.f,true));
	TestEqual(TEXT("Pending reward not prematurely credited"),Meta->GetSoulEmbers(),2000);
	Meta->bSimulateSaveFailure = false;
	Meta->RetrySkillReward(); Meta->RetrySkillReward();
	TestEqual(TEXT("Retry credits once"),Meta->GetSoulEmbers(),2024);
	TestEqual(TEXT("Short defeat gives zero"),MetaSkillTree::RunReward(29,false),0);
	TestEqual(TEXT("30 seconds grants one"),MetaSkillTree::RunReward(30,false),1);
	TestEqual(TEXT("Negative time rejected"),MetaSkillTree::RunReward(-1,true),0);
	TestEqual(TEXT("Reward bounded"),MetaSkillTree::RunReward(100000,true),140);
	// Simulate an old profile; migration must not reset existing discoveries.
	Meta->CurrentSave->SaveVersion=2;Meta->SaveMetaProgression();Meta->LoadMetaProgression();
	TestEqual(TEXT("Migration upgrades schema"),Meta->CurrentSave->SaveVersion,3);
	TestTrue(TEXT("Migration retains discoveries"),Meta->IsSynergyUpgradeUnlocked(TEXT("Synergy.TestLegacy")));
	Meta->CurrentSave->SkillRanks.Add(TEXT("Unknown"),999);
	Meta->CurrentSave->SkillRanks.Add(TEXT("Steel.Edge"),999);
	MetaSkillTree::Sanitize(*Meta->CurrentSave);
	TestEqual(TEXT("Unknown and orphaned ranks removed"),Meta->CurrentSave->SkillRanks.Num(),0);
	for (const FMetaSkillNode& N : MetaSkillTree::Nodes())
		for(int32 R=0;R<N.MaxRank;++R) Meta->PurchaseSkill(N.Id);

	World->InitializeActorsForPlay(FURL());
	auto* PC=World->SpawnActor<ASurvivorPlayerController>();
	FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Samurai=World->SpawnActor<ASamuraiCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
	auto* Ninja=World->SpawnActor<ANinjaCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
	Samurai->SetOwner(PC);Ninja->SetOwner(PC);
	auto* Party=PC->GetCharacterManager();
	FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("SamuraiCharacter"))->SetObjectPropertyValue_InContainer(Party,Samurai);
	FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("NinjaCharacter"))->SetObjectPropertyValue_InContainer(Party,Ninja);
	FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(Party,Samurai);
	auto* Upgrades=PC->GetPlayerUpgrades();
	Upgrades->RebuildAllUpgradeModifiers();
	TestTrue(TEXT("Shared health modifier applies"),FMath::IsNearlyEqual(PC->GetSharedPlayerStats()->GetFinalMaxHealthMultiplier(),1.27f));
	TestTrue(TEXT("Samurai direct damage applies"),FMath::IsNearlyEqual(Samurai->GetCharacterStats()->GetFinalDamageMultiplier(),1.24f));
	TestTrue(TEXT("Ninja direct damage applies"),FMath::IsNearlyEqual(Ninja->GetCharacterStats()->GetFinalDamageMultiplier(),1.24f));
	TestEqual(TEXT("Ninja projectile capstone applies"),Ninja->GetCharacterStats()->GetFinalProjectileCount(),2);
	TestEqual(TEXT("Ninja pierce capstone applies"),Ninja->GetCharacterStats()->GetFinalProjectilePierceBonus(),1);
	TestEqual(TEXT("Meta does not grant run mastery"),Upgrades->GetSamuraiMasteryPoints(),0);
	PC->BasePlayerMaxHealth=100;
	PC->GetSharedPlayerStats()->OnStatsChanged.AddDynamic(PC,&ASurvivorPlayerController::HandleSharedPlayerStatsChanged);
	PC->ApplySharedPlayerStats();
	PC->GetPlayerHealthComponent()->RestoreCurrentHealth(63.5f);
	PC->CurrentDashCharges=0;
	Upgrades->RebuildAllUpgradeModifiers();Upgrades->RebuildAllUpgradeModifiers();
	TestTrue(TEXT("Rebuild preserves injured health"),FMath::IsNearlyEqual(PC->GetPlayerHealthComponent()->GetCurrentHealth(),63.5f));
	TestEqual(TEXT("Rebuild does not refill dash charges"),PC->GetCurrentDashCharges(),0);
	TestTrue(TEXT("Swap duration uses passive bonus"),FMath::IsNearlyEqual(PC->GetSwapCooldownDuration(),3.f/1.3f));
	PC->StartSwapCooldown();
	TestTrue(TEXT("Actual swap timer agrees with UI"),FMath::IsNearlyEqual(PC->GetSwapCooldownRemaining(),PC->GetSwapCooldownDuration(),.01f));
	TestFalse(TEXT("Purchasing blocked during gameplay"),Meta->RefundSkills());
	auto* Enemy=World->SpawnActor<AEnemyBase>(FVector(100,0,0),FRotator::ZeroRotator,Params);
	Enemy->GetHealthComponent()->SetMaxHealthPreservePercent(10000);Enemy->GetHealthComponent()->RestoreCurrentHealth(10000);
	auto* Status=Enemy->GetStatusEffectComponent();
	Status->ApplyStatus(EEnemyStatusEffect::Bleed,Upgrades,EPlayerAttackSource::Samurai,true);
	const float WithBleed=Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed);
	Meta->CachedSkillBonuses.Add(TEXT("Bleed"),0);
	const float WithoutBleed=Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed);
	TestTrue(TEXT("Bleed passive reaches status damage"),WithoutBleed>0 && FMath::IsNearlyEqual(WithBleed/WithoutBleed,1.3f));
	Meta->RefreshSkillBonuses();
	Status->ApplyStatus(EEnemyStatusEffect::Poison,Upgrades,EPlayerAttackSource::Ninja,true);
	const float WithPoison=Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison);
	Meta->CachedSkillBonuses.Add(TEXT("Poison"),0);
	const float WithoutPoison=Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison);
	TestTrue(TEXT("Poison passive reaches status damage"),WithoutPoison>0 && FMath::IsNearlyEqual(WithPoison/WithoutPoison,1.3f));
	Meta->RefreshSkillBonuses();
	auto* Ability=PC->FindComponentByClass<USurvivorAbilityComponent>();
	Ability->Controller=PC;Ability->Upgrades=Upgrades;
	const int32 Family=5; // Crescent Reaper in the authored runtime catalog.
	const auto& Spec=BuildFamilies[Family];
	auto* Starter=LoadObject<UUpgradeDefinition>(nullptr,*FString::Printf(TEXT("/Game/HeavensDivide/Upgrades/%s/DA_Upgrade_%s%s"),Spec.Owner,Spec.Owner,Spec.Id));
	auto* Synergy=LoadObject<UUpgradeDefinition>(nullptr,*FString::Printf(TEXT("/Game/HeavensDivide/Upgrades/Synergy/DA_BuildSynergy_%s"),Spec.Synergy));
	TestTrue(TEXT("Acquire family for reaction test"),Upgrades->AcquireUpgrade(Starter));
	TestTrue(TEXT("Acquire family synergy"),Upgrades->AcquireUpgrade(Synergy));
	Ability->RegisterFamilyHit(Family,Enemy,20);
	TestTrue(TEXT("Preparation gets three bonus seconds"),Ability->BuildMarks.Num()==1 && FMath::IsNearlyEqual(Ability->BuildMarks[0].Remaining,Ability->Tuning(Family,TEXT("PreparationDuration"),6,3)+3));
	const float Before=Enemy->GetHealthComponent()->GetCurrentHealth();
	Ability->NotifyPartnerHit(EPlayerAttackSource::Ninja,Enemy);
	const float Expected=20*Ability->FamilySpec(Family).ReactionFactor*1.3f;
	TestTrue(TEXT("Partner reaction receives damage passive"),FMath::IsNearlyEqual(Before-Enemy->GetHealthComponent()->GetCurrentHealth(),Expected,.01f));
	Ability->NotifyPartnerHit(EPlayerAttackSource::Ninja,Enemy);
	TestTrue(TEXT("Passive does not bypass duplicate reaction guard"),FMath::IsNearlyEqual(Before-Enemy->GetHealthComponent()->GetCurrentHealth(),Expected,.01f));
	// Construct Slate graph without needing a local player or modifying project assets.
	auto* Tree=CreateWidget<UMetaSkillTreeWidget>(GI);
	TestTrue(TEXT("Skill tree Slate graph builds"),Tree->TakeWidget()->GetVisibility().IsVisible());
	if (FParse::Param(FCommandLine::Get(),TEXT("MetaSkillScreenshot")))
	{
		World->GetTimerManager().ClearAllTimersForObject(PC);
		PC->Destroy();
		Meta->CurrentSave->SkillRanks.Reset();Meta->CurrentSave->SoulEmbers=90;
		for(const TCHAR* Id : {TEXT("Root.Vitality"),TEXT("Root.Gather"),TEXT("Root.Strength"),TEXT("Root.Step"),TEXT("Steel.Edge"),TEXT("Shadow.Edge"),TEXT("Bond.Flow")}) MetaSkillTree::Purchase(*Meta->CurrentSave,Id);
		Meta->RefreshSkillBonuses();
		FWidgetRenderer Renderer(false);
		UTextureRenderTarget2D* Target=Renderer.DrawWidget(Tree->TakeWidget(),FVector2D(1600,1100));
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags;ReadFlags.SetLinearToGamma(false);
		TestTrue(TEXT("Read rendered skill tree"),Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,ReadFlags));
		if(Target && Pixels.Num()>0)
		{
			TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,PNG);
			TestTrue(TEXT("Export skill tree preview"),FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("MetaSkillTree.png"))));
		}
	}
	Tree->ReleaseSlateResources(true);
	World->GetTimerManager().ClearAllTimersForObject(PC);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	GI->Shutdown();
	UGameplayStatics::DeleteGameInSlot(Slot,0);
	return true;
}
#endif
