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
#include "Materials/MaterialInstanceDynamic.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"

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
 for (int32 Index = 0; Index < 27; ++Index)
 {
  auto* Upgrade = NewObject<UUpgradeDefinition>(Upgrades);
  Upgrade->UpgradeId = FName(*FString::Printf(TEXT("DraftTest%d"), Index));
  Upgrade->Category = Index % 3 == 0 ? EUpgradeCategory::Samurai : Index % 3 == 1 ? EUpgradeCategory::Ninja : EUpgradeCategory::Global;
  Upgrade->DisplayName = FText::FromString(FString::Printf(TEXT("Upgrade %d"), Index + 1));
  Upgrade->Description = FText::FromString(TEXT("Draft tool test: select, reroll, or banish this upgrade."));
  Upgrade->MaxLevel = 5;
  Upgrades->UpgradePool.Add(Upgrade);
 }
 auto BeginDraft = [&]()
 {
  TestTrue(TEXT("Category-first level-up starts"), Upgrades->BeginUpgradeSelection(2));
  TestTrue(TEXT("No upgrade is offered before choosing a category"), Upgrades->GetCurrentUpgradeChoices().IsEmpty());
  const auto Category = Upgrades->GetCurrentCategoryChoices()[0];
  TestTrue(TEXT("Offered category can be selected"), Upgrades->SelectCategory(Category, 3));
  TestEqual(TEXT("Chosen category offers three choices"), Upgrades->GetCurrentUpgradeChoices().Num(), 3);
  for (auto* Choice : Upgrades->GetCurrentUpgradeChoices())
   TestEqual(TEXT("Draft is restricted to its selected category"), Choice->Category, Category);
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
 const auto DraftSlate=Widget->TakeWidget();
 Widget->InitializeDirectUpgradeWidget(PC);
 auto* Reroll = Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("DraftReroll")));
 auto* Banish = Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("DraftBanish")));
 TestTrue(TEXT("Locked buttons visible but disabled"), Reroll && Banish && !Reroll->GetIsEnabled() && !Banish->GetIsEnabled());
 auto Render = [&](const TCHAR* File)
 {
  if (!FApp::CanEverRender()) return;
  FWidgetRenderer Renderer(false);
  auto* Target = Renderer.DrawWidget(DraftSlate, FVector2D(1920,1080));
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
 Meta->CurrentSave->SoulEmbers = 2000;
 TestFalse(TEXT("Banish requires its Abilities-page reroll prerequisite"), MetaSkillTree::Purchase(*Meta->CurrentSave, TEXT("Bond.Banish")));
 TestTrue(TEXT("Reroll has no cross-page stat prerequisite"), MetaSkillTree::Find(TEXT("Bond.Reroll"))->Prerequisites.IsEmpty());
 TestTrue(TEXT("Purchase reroll skill"), MetaSkillTree::Purchase(*Meta->CurrentSave, TEXT("Bond.Reroll")));
 TestTrue(TEXT("Save purchases"), Meta->SaveMetaProgression());
 Meta->LoadMetaProgression();
 TestEqual(TEXT("Reroll purchase survives reload"), Upgrades->GetRerollsRemaining(), 3);
 TestEqual(TEXT("Unpurchased banish remains locked"), Upgrades->GetBanishesRemaining(), 0);
 const auto BeforeReroll = Upgrades->GetCurrentUpgradeChoices();
 TestTrue(TEXT("Reroll succeeds"), Upgrades->RerollUpgrades());
 for (auto* Choice : Upgrades->GetCurrentUpgradeChoices())
 {
  TestFalse(TEXT("Reroll replaces every card when enough alternatives exist"), BeforeReroll.Contains(Choice));
  TestEqual(TEXT("Reroll stays inside the selected category"), Choice->Category, BeforeReroll[0]->Category);
 }
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
 TestEqual(TEXT("Banish replacement stays inside the selected category"), Upgrades->CurrentUpgradeChoices[0]->Category, Untouched.UpgradeDefinition->Category);
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
 Next->BeginUpgradeSelection(2);
 Next->SelectCategory(Next->GetCurrentCategoryChoices()[0], 100);
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
 const FText OldTitle=Widget->UpgradeTitleTexts[0]->GetText();
 TestTrue(TEXT("Banish mode card click succeeds"), Widget->SelectUpgradeChoice(0));
 TestTrue(TEXT("UI banishes chosen card"), Upgrades->BanishedUpgrades.Contains(UIBanned));
 TestEqual(TEXT("UI banish does not grant upgrade"), Upgrades->GetUpgradeLevelById(UIBanned), 0);
 TestTrue(TEXT("Level-up remains open after banish"), Upgrades->IsDraftToolOffer());
 TestTrue(TEXT("Banish starts its burn presentation"),Widget->bChoiceExitActive&&Widget->bChoiceExitBanish);
 TestTrue(TEXT("Old artwork stays until burn finishes"),Widget->UpgradeTitleTexts[0]->GetText().EqualTo(OldTitle));
 TestFalse(TEXT("Double click cannot select the incoming replacement"),Widget->SelectUpgradeChoice(0));
 const int32 RerollsBeforeBurn=Upgrades->GetRerollsRemaining();
 Widget->HandleReroll();Widget->HandleBanishMode();
 TestEqual(TEXT("Reroll input is ignored during burning"),Upgrades->GetRerollsRemaining(),RerollsBeforeBurn);
 TestFalse(TEXT("Banish input is ignored during burning"),Widget->bBanishMode);
 TestTrue(TEXT("Draft tools are disabled during burning"),!Reroll->GetIsEnabled()&&!Banish->GetIsEnabled());
 Widget->UpdateChoiceExit(.54f);
 TestEqual(TEXT("Untouched cards stay visible during banish"),Widget->ChoiceExitVisuals[1]->GetRenderOpacity(),1.f);
 if(TestNotNull(TEXT("Burn effect is available"),Widget->ChoiceExitMaterial.Get()))
 {
  TestEqual(TEXT("Banish uses ember-red burn variant"),Widget->ChoiceExitMaterial->K2_GetScalarParameterValue(TEXT("Burn")),1.f);
  TestTrue(TEXT("Burn reaches halfway in real UI time"),FMath::IsNearlyEqual(Widget->ChoiceExitMaterial->K2_GetScalarParameterValue(TEXT("Progress")),.5f,KINDA_SMALL_NUMBER));
 }
 Widget->UpdateChoiceExit(.31f);
 TestTrue(TEXT("Replacement waits for full burn duration"),Widget->bChoiceExitActive);
 Widget->UpdateChoiceExit(.02f);
 TestFalse(TEXT("Finished banish releases its input guard"),Widget->bChoiceExitActive);
 TestFalse(TEXT("Banish does not commit an upgrade selection"),Widget->bUpgradeChoiceCommitted);
 TestTrue(TEXT("Replacement title appears after burning"),Widget->UpgradeTitleTexts[0]->GetText().EqualTo(Upgrades->CurrentUpgradeChoices[0]->DisplayName));
 TestTrue(TEXT("Draft tools are restored after burning"),Reroll->GetIsEnabled()&&Banish->GetIsEnabled());
 TestEqual(TEXT("Only one banish is consumed"),Upgrades->GetBanishesRemaining(),1);
 TestTrue(TEXT("Replacement can still be selected"), Widget->SelectUpgradeChoice(0));
 if(Widget->ChoiceExitMaterial)TestEqual(TEXT("Selecting after banish resets the effect to gold"),Widget->ChoiceExitMaterial->K2_GetScalarParameterValue(TEXT("Burn")),0.f);
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
