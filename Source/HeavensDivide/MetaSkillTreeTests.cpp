#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif
#include "MetaSkillTree.h"
#include "MetaSkillTreeWidget.h"
#include "SMetaSkillMap.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
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
	FCommandLine::Set(*(TEXT("-MetaSaveSlot=") + Slot + TEXT(" ") + PreviousCommandLine));
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
	constexpr int32 StartingEmbers=10000;
	Meta->CurrentSave->SoulEmbers = StartingEmbers;
 Meta->CurrentSave->SoulSigils = 100;
 TestEqual(TEXT("Combo unlock now costs three times its original price"),Meta->GetSkillCost(TEXT("Ability.ComboUnlock")),30);
 TestEqual(TEXT("Dash unlock now costs three times its original price"),Meta->GetSkillCost(TEXT("Root.Dash")),60);
 TestEqual(TEXT("Reroll unlock now costs three times its original price"),Meta->GetSkillCost(TEXT("Bond.Reroll")),45);
 TestEqual(TEXT("Banish unlock now costs three times its original price"),Meta->GetSkillCost(TEXT("Bond.Banish")),45);
 TestEqual(TEXT("Repeatable stat price is tripled too"),Meta->GetSkillCost(TEXT("Root.Vitality")),15);
 TestEqual(TEXT("Stance-specific rank price is tripled too"),Meta->GetSkillCost(TEXT("Barrage.Edge")),30);
 for (const TCHAR* Id : {TEXT("Iaijutsu"), TEXT("BladeWave"), TEXT("ReturningFang"), TEXT("GreatShuriken")})
  TestTrue(TEXT("Purchase stance before its skill ranks"), Meta->PurchaseStance(Id));
	Meta->CurrentSave->UnlockedSynergyUpgradeIds.Add(TEXT("Synergy.TestLegacy"));
	TestFalse(TEXT("Unknown node cannot be purchased"),Meta->PurchaseSkill(TEXT("Missing")));
	TestFalse(TEXT("Prerequisites enforced"),Meta->PurchaseSkill(TEXT("Steel.Edge")));
	TestEqual(TEXT("45 existing skills plus three joint combo unlocks"),MetaSkillTree::Nodes().Num(),48);
 int32 TotalSkillCost = 0;
	TSet<FName> Seen;
	for (const FMetaSkillNode& N : MetaSkillTree::Nodes())
	{
		TestFalse(TEXT("Stable node IDs unique"),Seen.Contains(N.Id));
		for (FName P : N.Prerequisites) TestTrue(TEXT("Catalog topological and all prerequisites exist"),Seen.Contains(P));
		Seen.Add(N.Id);
  for (FName P : N.Prerequisites) TestEqual(TEXT("Prerequisites remain within the same page"), MetaSkillTree::Find(P)->Page, N.Page);
		TestTrue(TEXT("Node has concrete effect and text"),N.PerRank > 0 && !N.Description.IsEmpty());
		for (int32 Rank=0;Rank<N.MaxRank;++Rank)
		{
   TestEqual(TEXT("Every permanent rank costs triple its catalog price"),Meta->GetSkillCost(N.Id),3*N.BaseCost*(Rank+1));
   TotalSkillCost += Meta->GetSkillCost(N.Id);
   TestTrue(TEXT("Legal rank purchase"),Meta->PurchaseSkill(N.Id));
  }
		TestFalse(TEXT("Rank cap enforced"),Meta->PurchaseSkill(N.Id));
	}
	const int32 WalletAfterAll = StartingEmbers - TotalSkillCost;
 TestEqual(TEXT("Complete tree deducts exact catalog cost"),Meta->GetSoulEmbers(),WalletAfterAll);
	TestTrue(TEXT("Swap speed totals 30 percent"),FMath::IsNearlyEqual(Meta->GetSkillBonus(TEXT("Swap")),.3f));
	Meta->LoadMetaProgression();
	TestEqual(TEXT("Wallet survives disk reload"),Meta->GetSoulEmbers(),WalletAfterAll);
	TestEqual(TEXT("Capstone survives disk reload"),Meta->GetSkillRank(TEXT("Bond.Unity")),1);
	TestTrue(TEXT("Legacy discoveries preserved"),Meta->IsSynergyUpgradeUnlocked(TEXT("Synergy.TestLegacy")));
	Meta->bSimulateSaveFailure = true;
	TestFalse(TEXT("Failed refund reports failure"),Meta->RefundSkills());
	TestEqual(TEXT("Failed refund restores ranks"),Meta->GetSkillRank(TEXT("Bond.Unity")),1);
	TestEqual(TEXT("Failed refund restores wallet"),Meta->GetSoulEmbers(),WalletAfterAll);
	Meta->bSimulateSaveFailure = false;
	TestTrue(TEXT("Full refund"),Meta->RefundSkills());
	TestEqual(TEXT("Exact paid cost returned"),Meta->GetSoulEmbers(),StartingEmbers);
	TestEqual(TEXT("Refund removes cached effects"),Meta->GetSkillBonus(TEXT("Swap")),0.f);
	TestTrue(TEXT("Repeated refund safe"),Meta->RefundSkills());
	TestEqual(TEXT("No duplicate refund"),Meta->GetSoulEmbers(),StartingEmbers);
 auto* OldPriceSave=NewObject<UHeavensDivideMetaSaveGame>();
 OldPriceSave->SkillRanks.Add(TEXT("Ability.ComboUnlock"),1);
 OldPriceSave->SkillEmbersSpent=10;
 MetaSkillTree::Sanitize(*OldPriceSave);
 TestEqual(TEXT("A bought combo remains owned after the price increase"),MetaSkillTree::Rank(*OldPriceSave,TEXT("Ability.ComboUnlock")),1);
 TestEqual(TEXT("Refund returns the original price, not today's higher price"),MetaSkillTree::Refund(*OldPriceSave),10);
 TestEqual(TEXT("Old-price refund cannot repeat"),MetaSkillTree::Refund(*OldPriceSave),0);
	Meta->bSimulateSaveFailure = true;
	TestFalse(TEXT("Failed purchase reports failure"),Meta->PurchaseSkill(TEXT("Root.Vitality")));
	TestEqual(TEXT("Failed purchase restores wallet"),Meta->GetSoulEmbers(),StartingEmbers);
	TestEqual(TEXT("Failed purchase restores rank"),Meta->GetSkillRank(TEXT("Root.Vitality")),0);
	const int32 SigilsBeforeRun=Meta->GetSoulSigils();
 TestFalse(TEXT("Failed run save retains pending award"),Meta->AwardSkillRun(125.f,true));
 TestEqual(TEXT("Failed save keeps both wallet currencies unchanged"),Meta->GetSoulSigils(),SigilsBeforeRun);
 TestEqual(TEXT("Boss sigils queue atomically with embers"),Meta->GetPendingSigils(),5);
	TestEqual(TEXT("Pending reward not prematurely credited"),Meta->GetSoulEmbers(),StartingEmbers);
	Meta->bSimulateSaveFailure = false;
	Meta->RetrySkillReward(); Meta->RetrySkillReward();
	TestEqual(TEXT("Retry credits once"),Meta->GetSoulEmbers(),StartingEmbers+24);
 TestEqual(TEXT("Retry also credits Sigils exactly once"),Meta->GetSoulSigils(),SigilsBeforeRun+5);
 TestFalse(TEXT("Successful atomic retry clears queue"),Meta->HasPendingSkillReward());
    Meta->bSimulateSaveFailure=true;
    Meta->AwardObjectiveEmbers(20);
    TestEqual(TEXT("Failed objective save does not credit wallet early"),Meta->GetSoulEmbers(),StartingEmbers+24);
    Meta->bSimulateSaveFailure=false;
    Meta->RetrySkillReward();Meta->RetrySkillReward();
    TestEqual(TEXT("Objective reward retries exactly once"),Meta->GetSoulEmbers(),StartingEmbers+44);
    TestEqual(TEXT("Objective reward preserves run summary"),Meta->GetLastSkillRunReward(),24);
	TestEqual(TEXT("Short defeat gives zero"),MetaSkillTree::RunReward(29,false),0);
	TestEqual(TEXT("30 seconds grants one"),MetaSkillTree::RunReward(30,false),1);
	TestEqual(TEXT("Negative time rejected"),MetaSkillTree::RunReward(-1,true),0);
	TestEqual(TEXT("Reward bounded"),MetaSkillTree::RunReward(100000,true),140);
	// Return the recorded price, including purchases made under older balance values.
 Meta->CurrentSave->SaveVersion=4;
 Meta->CurrentSave->SkillRanks.Add(TEXT("Shadow.Twin"),1);
 Meta->CurrentSave->SkillRanks.Add(TEXT("Root.Vitality"),1);
 Meta->CurrentSave->SkillEmbersSpent=137;
 Meta->CurrentSave->TwinSoulCompletionsTowardDiscovery=2;
 Meta->SaveMetaProgression();Meta->LoadMetaProgression();
 TestEqual(TEXT("Migration upgrades schema"),Meta->CurrentSave->SaveVersion,7);
 TestEqual(TEXT("Redesign returns exact historical spend"),Meta->GetSoulEmbers(),StartingEmbers+181);
 TestEqual(TEXT("Redesign clears old purchases"),Meta->CurrentSave->SkillRanks.Num(),0);
 TestEqual(TEXT("Redesign clears refunded spend"),Meta->CurrentSave->SkillEmbersSpent,0);
 TestEqual(TEXT("Discovery progress survives redesign"),Meta->GetTwinSoulDiscoveryProgress(),2);
 Meta->LoadMetaProgression();
 TestEqual(TEXT("Redesign refund cannot repeat"),Meta->GetSoulEmbers(),StartingEmbers+181);
	auto* RetiredSave = NewObject<UHeavensDivideMetaSaveGame>();
	RetiredSave->SoulEmbers = 100; RetiredSave->SkillEmbersSpent = 35;
	RetiredSave->SkillRanks.Add(TEXT("Bond.Memory"), 2);
	RetiredSave->SkillRanks.Add(TEXT("Root.Vitality"), 1);
	RetiredSave->SkillRanks.Add(TEXT("Root.Gather"), 1);
	RetiredSave->SkillRanks.Add(TEXT("Root.Step"), 1);
	MetaSkillTree::Sanitize(*RetiredSave);
	TestEqual(TEXT("Retired Prepare ranks are refunded"), RetiredSave->SoulEmbers, 115);
	TestEqual(TEXT("Refund removes only retired spending"), RetiredSave->SkillEmbersSpent, 20);
	TestEqual(TEXT("Existing valid skills survive"), RetiredSave->SkillRanks.Num(), 3);
	MetaSkillTree::Sanitize(*RetiredSave);
	TestEqual(TEXT("Retired rank refund is idempotent"), RetiredSave->SoulEmbers, 115);
	for (const auto* Id : {TEXT("Bond.Memory"), TEXT("Bond.Strike"), TEXT("Bond.Echo"), TEXT("Bond.Reaction")})
		TestNull(TEXT("Prepare passives removed from catalog"), MetaSkillTree::Find(Id));
	TestFalse(TEXT("Shop migration removes obsolete free discoveries"),Meta->IsSynergyUpgradeUnlocked(TEXT("Synergy.TestLegacy")));
 TestTrue(TEXT("Shop migration keeps Tag Team default"),Meta->IsSynergyUpgradeUnlocked(TEXT("Synergy.TagTeam")));
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
	TestTrue(TEXT("Pickup passives double each rank's bonus"),FMath::IsNearlyEqual(PC->GetSharedPlayerStats()->GetFinalPickupRadiusMultiplier(),1.88f));
	TestTrue(TEXT("Samurai direct damage applies"),FMath::IsNearlyEqual(Samurai->GetCharacterStats()->GetFinalDamageMultiplier(),1.24f));
	TestTrue(TEXT("Ninja direct damage applies"),FMath::IsNearlyEqual(Ninja->GetCharacterStats()->GetFinalDamageMultiplier(),1.24f));
	TestEqual(TEXT("No generic projectile bonus before choosing a stance"),Ninja->GetCharacterStats()->GetFinalProjectileCount(),1);
	TestEqual(TEXT("Obsolete generic pierce removed"),Ninja->GetCharacterStats()->GetFinalProjectilePierceBonus(),0);
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
	auto* Blood=NewObject<UUpgradeDefinition>();Blood->UpgradeId=TEXT("BattleStance");
	Upgrades->AcquireUpgrade(Blood);
	Status->ApplyStatus(EEnemyStatusEffect::Bleed,Upgrades,EPlayerAttackSource::Samurai,false,80.f);
	const float WithBleed=Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed);
	Meta->CachedStanceSkillBonuses.FindChecked(TEXT("BattleStance")).Add(TEXT("Bleed"),0);
	const float WithoutBleed=Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed);
	TestTrue(TEXT("Bleed passive reaches status damage"),WithoutBleed>0 && FMath::IsNearlyEqual(WithBleed/WithoutBleed,1.2f));
	Meta->RefreshSkillBonuses();
	Status->ApplyStatus(EEnemyStatusEffect::Poison,Upgrades,EPlayerAttackSource::Ninja,true);
	const float WithPoison=Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison);
	Meta->CachedSkillBonuses.Add(TEXT("Poison"),0);
	const float WithoutPoison=Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison);
	TestTrue(TEXT("Poison no longer has a misleading universal passive"),WithoutPoison>0 && FMath::IsNearlyEqual(WithPoison,WithoutPoison));
	Meta->RefreshSkillBonuses();
	// Construct Slate graph without needing a local player or modifying project assets.
	auto* Tree=CreateWidget<UMetaSkillTreeWidget>(GI);
	// A viewport normally retains this shared Slate root. Keep it alive while testing page switches.
	const auto TreeSlate=Tree->TakeWidget();
	TestTrue(TEXT("Skill tree Slate graph builds"),TreeSlate->GetVisibility().IsVisible());
	Tree->SelectPage(EMetaTreePage::Stances);
	TestFalse(TEXT("Stance purchase page has no skill map until a stance is opened"),Tree->SkillMap.IsValid());
    const auto OldUnlocked=Meta->CurrentSave->UnlockedStanceIds;
    const int32 OldSigils=Meta->CurrentSave->SoulSigils;
    Meta->CurrentSave->UnlockedStanceIds.Remove(TEXT("Iaijutsu"));Meta->CurrentSave->SoulSigils=40;
    PC->Destroy();
    Tree->OpenStanceTree(TEXT("Iaijutsu"));
    TestEqual(TEXT("Locked stance opens a confirmation"),Tree->PendingStance,FName(TEXT("Iaijutsu")));
    TestEqual(TEXT("Opening confirmation spends nothing"),Meta->GetSoulSigils(),40);
    if(FParse::Param(FCommandLine::Get(),TEXT("MetaSkillScreenshot")))
    {
        FWidgetRenderer PurchaseRenderer(false);
        auto* Target=PurchaseRenderer.DrawWidget(TreeSlate,FVector2D(1280,720));
        FlushRenderingCommands();TArray<FColor> Pixels;
        if(Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels))
        {TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1280,720,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("LoadoutMenus/StanceConfirmation1280.png")));}
    }
    Tree->ConfirmStanceUnlock(false);
    TestFalse(TEXT("No leaves the stance locked"),Meta->IsStanceUnlocked(TEXT("Iaijutsu")));
    Tree->OpenStanceTree(TEXT("Iaijutsu"));Tree->ConfirmStanceUnlock(true);
    TestTrue(TEXT("Yes purchases the stance"),Meta->IsStanceUnlocked(TEXT("Iaijutsu")));
    TestEqual(TEXT("Confirmed purchase charges once"),Meta->GetSoulSigils(),22);
    Tree->ConfirmStanceUnlock(true);TestEqual(TEXT("Repeated confirmation cannot spend twice"),Meta->GetSoulSigils(),22);
    Meta->CurrentSave->UnlockedStanceIds=OldUnlocked;Meta->CurrentSave->SoulSigils=OldSigils;
    Tree->SelectPage(EMetaTreePage::Stances);
	const FReply OpenStanceReply=Tree->OpenStanceTree(TEXT("BattleStance"));
	TestTrue(TEXT("Owned stance opens a dedicated tree"),Tree->SkillMap.IsValid());
	TestTrue(TEXT("Opening a stance transfers focus to its new map"),OpenStanceReply.ShouldSetUserFocus()&&OpenStanceReply.GetUserFocusRecepient().Get()==Tree->SkillMap.Get());
	for(FName Id:Tree->VisibleSkills())TestEqual(TEXT("Only the opened stance's skills are shown"),MetaSkillTree::Find(Id)->RequiredStance,FName(TEXT("BattleStance")));
	const FReply CloseStanceReply=Tree->CloseTree();
	TestTrue(TEXT("Returning to stances focuses the surviving tab"),CloseStanceReply.ShouldSetUserFocus()&&CloseStanceReply.GetUserFocusRecepient().Get()==Tree->StancesTab.Get());
	Tree->SelectPage(EMetaTreePage::Abilities);
	for(FName Id:Tree->VisibleSkills())TestTrue(TEXT("Abilities page filters by catalog category"),MetaSkillTree::Find(Id)->Page==EMetaSkillPage::Abilities);
	Tree->SelectPage(EMetaTreePage::Synergies);
	TestFalse(TEXT("Synergy purchase page does not show passive skills"),Tree->SkillMap.IsValid());
	Tree->SelectPage(EMetaTreePage::Stats);
	for(FName Id:Tree->VisibleSkills())TestTrue(TEXT("Stats page has generic skills only"),MetaSkillTree::Find(Id)->Page==EMetaSkillPage::Stats&&MetaSkillTree::Find(Id)->RequiredStance.IsNone());
	if (FParse::Param(FCommandLine::Get(),TEXT("MetaSkillScreenshot")))
	{
		World->GetTimerManager().ClearAllTimersForObject(PC);
		PC->Destroy();
		Meta->CurrentSave->SkillRanks.Reset();Meta->CurrentSave->SoulEmbers=270;
		for(const TCHAR* Id : {TEXT("Root.Vitality"),TEXT("Root.Gather"),TEXT("Root.Strength"),TEXT("Root.Step"),TEXT("Steel.Edge"),TEXT("Shadow.Edge"),TEXT("Bond.Flow")}) MetaSkillTree::Purchase(*Meta->CurrentSave,Id);
		Meta->RefreshSkillBonuses();
        IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("MetaTreeRedesign")),true);
        FWidgetRenderer Renderer(false);
        auto Capture = [&](FName Selection, FVector2D Size, const TCHAR* Filename, EMetaTreePage Page=EMetaTreePage::Stats,float ScrollOffset=0.f)
        {
            auto* Preview=CreateWidget<UMetaSkillTreeWidget>(GI);
            Preview->Selected=Selection;
            Preview->CurrentPage=Page;
            if(const auto* Node=MetaSkillTree::Find(Selection);Node&&!Node->RequiredStance.IsNone())
            {Preview->CurrentPage=EMetaTreePage::Stances;Preview->OpenStance=Node->RequiredStance;}
            else if(Node&&Node->Page==EMetaSkillPage::Abilities)Preview->CurrentPage=EMetaTreePage::Abilities;
            const auto PreviewSlate=Preview->TakeWidget();
#if WITH_EDITOR
            // Cold editor loads may still be compiling the artwork requested by RebuildWidget.
            FAssetCompilingManager::Get().FinishAllCompilation();
#endif
            FlushRenderingCommands();
            UTextureRenderTarget2D* Target=Renderer.DrawWidget(PreviewSlate,Size);
            if(Preview->SkillScroll)Preview->SkillScroll->SetScrollOffset(ScrollOffset);
            for(int32 I=0;I<4;++I){PreviewSlate->Invalidate(EInvalidateWidgetReason::Prepass);Preview->ForceLayoutPrepass();Renderer.DrawWidget(Target,PreviewSlate,Size,1.f/60.f);}
            FlushRenderingCommands();
            TArray<FColor> Pixels;
            FReadSurfaceDataFlags ReadFlags;ReadFlags.SetLinearToGamma(false);
            TestTrue(TEXT("Read rendered skill tree"),Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,ReadFlags));
            if(Target && Pixels.Num()>0)
            {
                TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,PNG);
                TestTrue(TEXT("Export skill tree preview"),FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/Filename)));
            }
            Preview->ReleaseSlateResources(true);
        };
        Capture(TEXT("Root.Vitality"),{1920,1080},TEXT("MetaSkillTree.png"));
        Capture(TEXT("Root.Vitality"),{1280,720},TEXT("MetaTreeRedesign/StatsSmall.png"));
        IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("MenuTreePolish")),true);
        Capture(TEXT("Root.Gather"),{1280,720},TEXT("MenuTreePolish/StatsHighlighted1280.png"));
        Capture(TEXT("Root.Area"),{1280,720},TEXT("MenuTreePolish/StatsScrolled1280.png"),EMetaTreePage::Stats,210.f);
        Capture(TEXT("Bond.Relay"),{1280,720},TEXT("MenuTreePolish/AbilitiesHighlighted1280.png"));

        Capture(TEXT("Barrage.Twin"),{1920,1080},TEXT("MetaTreeRedesign/BarragePreview.png"));
        Capture(TEXT("Iaijutsu.Focus"),{1444,976},TEXT("MetaTreeRedesign/MenuBoundsPreview.png"));
        Capture(TEXT("Shuriken.Grind"),{1280,720},TEXT("MetaTreeRedesign/SmallPreview.png"));
        Capture(TEXT("Bond.Banish"),{1280,720},TEXT("MetaTreeRedesign/LongDescriptionPreview.png"));
        Capture(NAME_None,{1920,1080},TEXT("MetaTreeRedesign/StanceUnlocks.png"),EMetaTreePage::Stances);
        Capture(NAME_None,{1280,720},TEXT("MetaTreeRedesign/StanceUnlocksSmall.png"),EMetaTreePage::Stances);
        Capture(NAME_None,{1920,1080},TEXT("MetaTreeRedesign/SynergyUnlocks.png"),EMetaTreePage::Synergies);
		Capture(NAME_None,{1280,720},TEXT("MetaTreeRedesign/SynergyUnlocksSmall.png"),EMetaTreePage::Synergies);
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
