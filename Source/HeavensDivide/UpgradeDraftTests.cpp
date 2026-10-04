#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerUpgradeComponent.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "SurvivorPlayerController.h"
#include "HeavensDivideMetaSaveGame.h"
#include "MetaSkillTree.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "LevelUpWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDraftToolsTest, "HeavensDivide.Upgrades.DraftUnlocks",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDraftToolsTest::RunTest(const FString&)
{
 auto* GI = NewObject<UGameInstance>(GEngine);
 GI->InitializeStandalone();
 auto* World = GI->GetWorld();
 auto* Meta = GI->GetSubsystem<USynergyMetaProgressionSubsystem>();
 if (!TestNotNull(TEXT("Progression"), Meta) || !World) return false;
 const FString Slot = TEXT("Automation_Draft_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
 Meta->TestSaveSlot = Slot;
 Meta->CreateFreshSave();
 auto* PC = World->SpawnActor<ASurvivorPlayerController>();
 auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
 LocalPlayer->SetControllerId(0);
 PC->SetPlayer(LocalPlayer);
 auto* Upgrades = PC->GetPlayerUpgrades();
 for (int32 Index = 0; Index < 9; ++Index)
 {
  auto* Upgrade = NewObject<UUpgradeDefinition>(Upgrades);
  Upgrade->UpgradeId = FName(*FString::Printf(TEXT("DraftTest%d"), Index));
  Upgrade->Category = EUpgradeCategory::Global;
  Upgrade->DisplayName = FText::FromString(FString::Printf(TEXT("Upgrade %d"), Index + 1));
  Upgrade->Description = FText::FromString(TEXT("Draft tool test: select, reroll, or banish this upgrade."));
  Upgrade->MaxLevel = 5;
  Upgrades->UpgradePool.Add(Upgrade);
 }
 auto BeginDraft = [&]()
 {
  TestTrue(TEXT("Category selection starts"), Upgrades->BeginUpgradeSelection());
  TestTrue(TEXT("Global draft starts"), Upgrades->SelectCategory(EUpgradeCategory::Global));
 };
 BeginDraft();
 TestEqual(TEXT("First run has no rerolls"), Upgrades->GetRerollsRemaining(), 0);
 TestEqual(TEXT("First run has no banishes"), Upgrades->GetBanishesRemaining(), 0);
 TestFalse(TEXT("Cannot bypass locked reroll through API"), Upgrades->RerollUpgrades());
 TestFalse(TEXT("Cannot bypass locked banish through API"), Upgrades->BanishUpgrade(0));
 auto* WidgetClass = LoadClass<ULevelUpWidget>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/UpgradeUI/WBP_LevelUp.WBP_LevelUp_C"));
 auto* Widget = CreateWidget<ULevelUpWidget>(GI, WidgetClass);
 if (!TestNotNull(TEXT("Authored level-up widget"), Widget)) return false;
 // Cache Slate before initialization, matching the controller's real viewport lifecycle.
 Widget->TakeWidget();
 Widget->InitializeDirectUpgradeWidget(PC);
 auto* Reroll = Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("DraftReroll")));
 auto* Banish = Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("DraftBanish")));
 TestTrue(TEXT("Locked buttons visible but disabled"), Reroll && Banish && !Reroll->GetIsEnabled() && !Banish->GetIsEnabled());
 auto Render = [&](const TCHAR* File)
 {
  FWidgetRenderer Renderer(false);
  auto* Target = Renderer.DrawWidget(Widget->TakeWidget(), FVector2D(1920,1080));
  TArray<FColor> Pixels;
  FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
  if (TestTrue(TEXT("UI render readable"), Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
  {
   TArray64<uint8> PNG;
   FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, Pixels, PNG);
   FFileHelper::SaveArrayToFile(PNG, *(FPaths::ProjectSavedDir()/File));
  }
 };
 TestEqual(TEXT("Locked reroll label is name and count only"), Cast<UTextBlock>(Reroll->GetContent())->GetText().ToString(), FString(TEXT("Reroll (0)")));
 TestEqual(TEXT("Locked banish label is name and count only"), Cast<UTextBlock>(Banish->GetContent())->GetText().ToString(), FString(TEXT("Banish (0)")));
 TestTrue(TEXT("No resting ink brush"), Reroll->GetStyle().Normal.DrawAs == ESlateBrushDrawType::NoDrawType && Banish->GetStyle().Normal.DrawAs == ESlateBrushDrawType::NoDrawType);
 Render(TEXT("DraftToolsLocked.png"));
 for (int32 Run = 0; Run < 3; ++Run) Meta->AwardSkillRun(60, false);
 TestEqual(TEXT("Completing runs does not unlock rerolls"), Upgrades->GetRerollsRemaining(), 0);
 TestEqual(TEXT("Completing runs does not unlock banishes"), Upgrades->GetBanishesRemaining(), 0);
 Meta->CurrentSave->SoulEmbers = 200;
 TestFalse(TEXT("Draft skills require their branch prerequisites"), MetaSkillTree::Purchase(*Meta->CurrentSave, TEXT("Bond.Reroll")));
 for (const TCHAR* Id : {TEXT("Root.Vitality"), TEXT("Root.Strength"), TEXT("Bond.Flow"), TEXT("Root.Gather"), TEXT("Root.Step"), TEXT("Bond.Relay")})
  TestTrue(TEXT("Purchase prerequisite"), MetaSkillTree::Purchase(*Meta->CurrentSave, Id));
 TestTrue(TEXT("Purchase reroll skill"), MetaSkillTree::Purchase(*Meta->CurrentSave, TEXT("Bond.Reroll")));
 TestTrue(TEXT("Save purchases"), Meta->SaveMetaProgression());
 Meta->LoadMetaProgression();
 TestEqual(TEXT("Reroll purchase survives reload"), Upgrades->GetRerollsRemaining(), 3);
 TestEqual(TEXT("Unpurchased banish remains locked"), Upgrades->GetBanishesRemaining(), 0);
 const auto BeforeReroll = Upgrades->GetCurrentUpgradeChoices();
 TestTrue(TEXT("Reroll succeeds"), Upgrades->RerollUpgrades());
 for (auto* Choice : Upgrades->GetCurrentUpgradeChoices())
  TestFalse(TEXT("Reroll replaces every card when enough alternatives exist"), BeforeReroll.Contains(Choice));
 TestEqual(TEXT("One reroll spent"), Upgrades->GetRerollsRemaining(), 2);
 TestTrue(TEXT("Purchase banish skill"), MetaSkillTree::Purchase(*Meta->CurrentSave, TEXT("Bond.Banish")));
 Meta->SaveMetaProgression(); Meta->LoadMetaProgression();
 TestEqual(TEXT("Banish purchase grants two charges"), Upgrades->GetBanishesRemaining(), 2);
 Widget->InitializeDirectUpgradeWidget(PC);
 TestTrue(TEXT("Unlocked buttons enabled"), Reroll && Banish && Reroll->GetIsEnabled() && Banish->GetIsEnabled());
 Render(TEXT("DraftToolsUnlocked.png"));
 const FName Banned = Upgrades->CurrentUpgradeChoices[0]->UpgradeId;
 const auto Untouched = Upgrades->CurrentUpgradeOffers[1];
 TestTrue(TEXT("Banish replaces chosen card"), Upgrades->BanishUpgrade(0));
 TestEqual(TEXT("Banish preserves another card"), Upgrades->CurrentUpgradeOffers[1].UpgradeDefinition.Get(), Untouched.UpgradeDefinition.Get());
 TestEqual(TEXT("Banish preserves another rarity"), Upgrades->CurrentUpgradeOffers[1].RolledRarity, Untouched.RolledRarity);
 TestFalse(TEXT("Invalid choice rejected"), Upgrades->BanishUpgrade(20));
 TestEqual(TEXT("Only successful banish charged"), Upgrades->GetBanishesRemaining(), 1);
 for (int32 Index = 0; Index < 12; ++Index)
 {
  BeginDraft();
  for (auto* Choice : Upgrades->GetCurrentUpgradeChoices()) TestNotEqual(TEXT("Banned upgrade never returns"), Choice->UpgradeId, Banned);
 }
 FPlayerUpgradeRunState State;
 Upgrades->CaptureRunState(State);
 auto* NextPC = World->SpawnActor<ASurvivorPlayerController>();
 auto* Next = NextPC->GetPlayerUpgrades();
 Next->RestoreRunState(State);
 TestEqual(TEXT("Reroll budget persists through stage travel"), Next->GetRerollsRemaining(), 2);
 TestEqual(TEXT("Banish budget persists through stage travel"), Next->GetBanishesRemaining(), 1);
 TestTrue(TEXT("Banish list persists through stage travel"), Next->BanishedUpgrades.Contains(Banned));
 TestTrue(TEXT("Second reroll"), Upgrades->RerollUpgrades());
 TestTrue(TEXT("Third reroll"), Upgrades->RerollUpgrades());
 TestFalse(TEXT("Fourth reroll blocked"), Upgrades->RerollUpgrades());
 TestTrue(TEXT("Second banish"), Upgrades->BanishUpgrade(0));
 TestFalse(TEXT("Third banish blocked"), Upgrades->BanishUpgrade(0));
 Upgrades->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::Global);
 TestFalse(TEXT("Reward drafts do not allow reroll"), Upgrades->IsDraftToolOffer());
 Next->UpgradePool = Upgrades->UpgradePool;
 Next->RerollsUsed = Next->BanishesUsed = 0;
 Next->BeginUpgradeSelection(); Next->SelectCategory(EUpgradeCategory::Global, 100);
 TestFalse(TEXT("Exhausted pool cannot reroll"), Next->RerollUpgrades());
 TestFalse(TEXT("Exhausted pool cannot banish"), Next->BanishUpgrade(0));
 TestEqual(TEXT("Failed reroll free"), Next->GetRerollsRemaining(), 3);
 TestEqual(TEXT("Failed banish free"), Next->GetBanishesRemaining(), 2);
 BeginDraft();
 Widget->InitializeDirectUpgradeWidget(PC);
 Upgrades->RerollsUsed = Upgrades->BanishesUsed = 0;
 Widget->InitializeDirectUpgradeWidget(PC);
 if (Banish) Banish->OnClicked.Broadcast();
 Render(TEXT("DraftToolsBanish.png"));
 const FName UIBanned = Upgrades->CurrentUpgradeChoices[0]->UpgradeId;
 TestTrue(TEXT("Banish mode card click succeeds"), Widget->SelectUpgradeChoice(0));
 TestTrue(TEXT("UI banishes chosen card"), Upgrades->BanishedUpgrades.Contains(UIBanned));
 TestEqual(TEXT("UI banish does not grant upgrade"), Upgrades->GetUpgradeLevelById(UIBanned), 0);
 TestTrue(TEXT("Level-up remains open after banish"), Upgrades->IsDraftToolOffer());
 TestTrue(TEXT("Replacement can still be selected"), Widget->SelectUpgradeChoice(0));
 MetaSkillTree::Refund(*Meta->CurrentSave);
 Meta->SaveMetaProgression(); Meta->LoadMetaProgression();
 TestEqual(TEXT("Skill refund removes reroll charges"), Next->GetRerollsRemaining(), 0);
 TestEqual(TEXT("Skill refund removes banish charges"), Next->GetBanishesRemaining(), 0);
 TestTrue(TEXT("Reset succeeds"), Meta->ResetMetaProgression());
 TestEqual(TEXT("Reset locks reroll again"), Next->GetRerollsRemaining(), 0);
 TestEqual(TEXT("Reset locks banish again"), Next->GetBanishesRemaining(), 0);
 Widget->ReleaseSlateResources(true);
 World->DestroyWorld(false);
 GEngine->DestroyWorldContext(World);
 GI->Shutdown();
 UGameplayStatics::DeleteGameInSlot(Slot, 0);
 return true;
}
#endif
