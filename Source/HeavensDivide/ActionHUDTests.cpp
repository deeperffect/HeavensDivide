#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerHUDWidget.h"
#include "ComboRadialWidget.h"
#include "ComboAbilityComponent.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "CharacterManagerComponent.h"
#include "Blueprint/WidgetTree.h"
#include "HeavensDivideGameUserSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Styling/CoreStyle.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "AssetCompilingManager.h"
void UpdatePlayerActionHUD(UPlayerHUDWidget& HUD);
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionHUDTest, "HeavensDivide.UI.ActionHUD",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FActionHUDTest::RunTest(const FString&)
{
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 W->InitializeActorsForPlay(FURL());auto* GI=NewObject<UGameInstance>(GEngine);W->SetGameInstance(GI);
 auto* PC=W->SpawnActor<ASurvivorPlayerController>();PC->GetPlayerUpgrades()->BalancePreviewMetaProfile=1;PC->FindComponentByClass<UComboAbilityComponent>()->RestoreCombo(100);auto* Sam=W->SpawnActor<ASamuraiCharacter>();auto* Ninja=W->SpawnActor<ANinjaCharacter>();
 for(auto Pair : {TPair<FName,ACharacterBase*>(TEXT("ActiveCharacter"),Sam),TPair<FName,ACharacterBase*>(TEXT("SamuraiCharacter"),Sam),TPair<FName,ACharacterBase*>(TEXT("NinjaCharacter"),Ninja)})
  FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),Pair.Key)->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Pair.Value);
 auto* Class=LoadClass<UPlayerHUDWidget>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/UI/WBP_PlayerHUD.WBP_PlayerHUD_C"));
 auto* HUD=CreateWidget<UPlayerHUDWidget>(GI,Class);HUD->InitializeFromPlayerController(PC);
 auto SlateHUD=HUD->TakeWidget();
 auto* Dash=Cast<UComboRadialWidget>(HUD->WidgetTree->FindWidget(TEXT("DashRadialBadge")));
 auto* Swap=Cast<UComboRadialWidget>(HUD->WidgetTree->FindWidget(TEXT("SwapRadialBadge")));
 if(!TestNotNull(TEXT("Dash uses a circle"),Dash)||!TestNotNull(TEXT("Swap uses a circle"),Swap))return false;
 TestNotNull(TEXT("Swap keeps its arrow icon"),Swap->GetIcon());
 PC->MaxDashCharges=3;PC->CurrentDashCharges=3;
 const auto Render=[&](int32 Height,const FString& Name){
  FAssetCompilingManager::Get().FinishAllCompilation();
  auto Preview=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.008f,.018f,.022f)).Padding(0)
   [SNew(SDPIScaler).DPIScale(Height/1080.f)[SlateHUD]];
  FWidgetRenderer Renderer(false);auto* Target=Renderer.DrawWidget(Preview,FVector2D(Height*16/9,Height));
  TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
  if(TestTrue(TEXT("Action HUD renders"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags))){
   TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,PNG);
   const FString Out=FPaths::ProjectSavedDir()/TEXT("CombatPresentation/Previews");IFileManager::Get().MakeDirectory(*Out,true);
   FFileHelper::SaveArrayToFile(PNG,*(Out/(Name+FString::Printf(TEXT("_%d.png"),Height))));
  }
 };
 UpdatePlayerActionHUD(*HUD);
 TestTrue(TEXT("Full dash ring available"),Dash->IsReady()&&Dash->GetPercent()==1.f&&Dash->GetAvailableCharges()==3);
 TestTrue(TEXT("Swap available at full ring"),Swap->IsReady()&&Swap->GetPercent()==1.f);
 for(int32 H:{720,1080})Render(H,TEXT("ActionsReady"));
 PC->CurrentDashCharges=1;PC->StartDashRechargeIfNeeded();PC->bCanSwap=false;
 W->GetTimerManager().SetTimer(PC->SwapCooldownTimerHandle,FTimerDelegate::CreateLambda([]{}),PC->GetSwapCooldownDuration()*.5f,false);
 PC->ReduceDashRecharge(PC->DashRechargeTime*.5f);
 UpdatePlayerActionHUD(*HUD);
 TestEqual(TEXT("Dash preserves charge count while next charge fills"),Dash->GetAvailableCharges(),1);
 TestTrue(TEXT("Available dash can recharge simultaneously"),Dash->IsReady()&&FMath::IsNearlyEqual(Dash->GetPercent(),.5f));
 TestTrue(TEXT("Swap ring reflects cooldown elapsed fraction"),FMath::IsNearlyEqual(Swap->GetPercent(),1.f-PC->GetSwapCooldownProgress()));
 Render(1080,TEXT("ActionsRecharging"));
 PC->CurrentDashCharges=0;UpdatePlayerActionHUD(*HUD);TestFalse(TEXT("Empty dash is not ready"),Dash->IsReady());
 Sam->bComboAbilityActive=true;UpdatePlayerActionHUD(*HUD);
 TestEqual(TEXT("Combo locks both action captions"),Dash->GetStatusLabel().ToString(),FString(TEXT("LOCKED")));
 TestFalse(TEXT("Combo suppresses swap ready glow"),Swap->IsReady());Render(1080,TEXT("ActionsLocked"));
 Sam->bComboAbilityActive=false;PC->ResetSwapCooldown();UpdatePlayerActionHUD(*HUD);
 TestTrue(TEXT("Swap recovers after cooldown"),Swap->IsReady());
 auto* Settings=UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
 if(Settings)TestFalse(TEXT("Rebound action key remains visible"),Dash->GetKeyLabel().IsEmpty());
 for(FName Name:{TEXT("DashChargeContainer"),TEXT("SwapCooldownWidget"),TEXT("TXT_DashAction"),TEXT("TXT_SwapAction")})
  if(auto* Old=HUD->WidgetTree->FindWidget(Name))TestEqual(TEXT("No duplicate legacy HUD"),Old->GetVisibility(),ESlateVisibility::Collapsed);
 HUD->ReleaseSlateResources(true);W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
