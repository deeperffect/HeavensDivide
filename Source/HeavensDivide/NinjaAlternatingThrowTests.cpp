#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AutoAttackComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SwapPresentationComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjaAlternatingThrowTest, "HeavensDivide.Combat.NinjaAlternatingThrow",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNinjaAlternatingThrowTest::RunTest(const FString&)
{
 auto* World = UWorld::CreateWorld(EWorldType::Game, false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 auto* Class = LoadClass<ANinjaCharacter>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.BP_Ninja_C"));
 auto* Ninja = World->SpawnActor<ANinjaCharacter>(Class);
 if (!TestNotNull(TEXT("Ninja Blueprint"), Ninja)) return false;
 auto* Attack = Ninja->FindComponentByClass<UAutoAttackComponent>();
 Attack->OwnerCharacter = Ninja;
 Ninja->SwapPresentation->bEnabled = false;
 Ninja->GetMesh()->InitAnim(true);
 auto* Anim = Ninja->GetMesh()->GetAnimInstance();
 if (!TestNotNull(TEXT("Ninja animation instance"), Anim)) return false;
 TestNotNull(TEXT("Original right throw"), Attack->AttackMontage.Get());
 if (!TestNotNull(TEXT("Mirrored left throw assigned"), Attack->AlternateAttackMontage.Get())) return false;
 TestTrue(TEXT("Distinct montages"), Attack->AttackMontage != Attack->AlternateAttackMontage);
 // Distinct asset names alone did not catch an alternate montage still pointing
 // to the original clip. Verify both its saved track and evaluated hand poses.
 const auto& LeftTracks=Attack->AlternateAttackMontage->SlotAnimTracks;
 if(TestTrue(TEXT("Alternate contains an animation segment"),!LeftTracks.IsEmpty() && !LeftTracks[0].AnimTrack.AnimSegments.IsEmpty()))
 {
  auto* Clip=LeftTracks[0].AnimTrack.AnimSegments[0].GetAnimReference().Get();
  if(TestNotNull(TEXT("Alternate animation clip"),Clip))
   TestEqual(TEXT("Alternate references the baked mirrored sequence"),Clip->GetFName(),FName(TEXT("AS_NinjaThrow_Left")));
 }
 auto* PoseMesh=NewObject<USkeletalMeshComponent>(Ninja);
 PoseMesh->SetSkeletalMeshAsset(Ninja->GetMesh()->GetSkeletalMeshAsset());
 PoseMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
 PoseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 PoseMesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 PoseMesh->RegisterComponent();
 auto Sample=[&](UAnimMontage* Montage,float Time)
 {
  auto* PoseAnim=PoseMesh->GetSingleNodeInstance();
  PoseAnim->SetAnimationAsset(Montage,false,1.f);
  PoseAnim->SetPlaying(false);PoseAnim->SetPosition(Time,false);
  PoseAnim->UpdateMontageWeightForTimeSkip(1.f);
  PoseMesh->TickAnimation(0.f,false);PoseMesh->RefreshBoneTransforms();
  return TPair<FVector,FVector>(PoseMesh->GetSocketTransform(TEXT("RightHand"),RTS_Component).GetLocation(),
      PoseMesh->GetSocketTransform(TEXT("LeftHand"),RTS_Component).GetLocation());
 };
 float MaximumSameHandDifference=0.f;
 for(float Time:{.1f,.3f,.5f,.7f})
 {
  const auto Right=Sample(Attack->AttackMontage,Time),Left=Sample(Attack->AlternateAttackMontage,Time);
  TestTrue(TEXT("Alternate left hand mirrors primary right hand"),Left.Value.Equals(FVector(-Right.Key.X,Right.Key.Y,Right.Key.Z),1.f));
  TestTrue(TEXT("Alternate right hand mirrors primary left hand"),Left.Key.Equals(FVector(-Right.Value.X,Right.Value.Y,Right.Value.Z),1.f));
  MaximumSameHandDifference=FMath::Max(MaximumSameHandDifference,FVector::Distance(Right.Key,Left.Key));
 }
 TestTrue(TEXT("Alternating montage poses are visibly different"),MaximumSameHandDifference>5.f);
 PoseMesh->DestroyComponent();
 TestEqual(TEXT("Mirroring preserves duration"), Attack->AttackMontage->GetPlayLength(), Attack->AlternateAttackMontage->GetPlayLength());
 TestEqual(TEXT("Mirroring preserves notify count"), Attack->AttackMontage->Notifies.Num(), Attack->AlternateAttackMontage->Notifies.Num());
 for (int32 I = 0; I < FMath::Min(Attack->AttackMontage->Notifies.Num(), Attack->AlternateAttackMontage->Notifies.Num()); ++I)
 {
  const auto& Right = Attack->AttackMontage->Notifies[I];
  const auto& Left = Attack->AlternateAttackMontage->Notifies[I];
  TestEqual(TEXT("Release notify time unchanged"), Right.GetTriggerTime(), Left.GetTriggerTime());
  TestEqual(TEXT("Notify type unchanged"), Right.Notify ? Right.Notify->GetClass() : nullptr, Left.Notify ? Left.Notify->GetClass() : nullptr);
 }
 TestTrue(TEXT("Left release bone exists"), Ninja->GetMesh()->DoesSocketExist(Attack->AlternateProjectileSpawnSocket));
 for (int32 I = 0; I < 6; ++I)
 {
  auto* Expected = I % 2 == 0 ? Attack->AttackMontage.Get() : Attack->AlternateAttackMontage.Get();
  TestEqual(TEXT("Right-left-right order"), Attack->GetMontageForNextAttack(), Expected);
  TestTrue(TEXT("Attack montage starts"), Attack->PlayAttackMontage());
  TestEqual(TEXT("Correct montage actually plays"), Attack->ActiveAttackMontage.Get(), Expected);
  const FName Socket = I % 2 == 0 ? Attack->ProjectileSpawnSocket : Attack->AlternateProjectileSpawnSocket;
  TestTrue(TEXT("Kunai release follows selected hand"), Attack->GetProjectileSpawnLocation().Equals(Ninja->GetMesh()->GetSocketLocation(Socket), .01));
  const bool Next = Attack->bNextAttackUsesAlternate;
  TestFalse(TEXT("Overlapping attack rejected"), Attack->PlayAttackMontage());
  TestEqual(TEXT("Rejected attempt does not skip a hand"), Attack->bNextAttackUsesAlternate, Next);
  Attack->StopAutoAttack();
  Anim->Montage_Stop(0);
 }
 const bool BeforeAssist = Attack->bNextAttackUsesAlternate;
 TestTrue(TEXT("Assist can use a throw"), Attack->PlayAttackMontage(false));
 TestEqual(TEXT("Assist does not advance normal attack alternation"), Attack->bNextAttackUsesAlternate, BeforeAssist);
 Attack->StopAutoAttack(); Anim->Montage_Stop(0);
 auto* Alternate = Attack->AlternateAttackMontage.Get();
 Attack->AlternateAttackMontage = nullptr;
 Attack->bNextAttackUsesAlternate = true;
 TestEqual(TEXT("Empty alternate falls back to original"), Attack->GetMontageForNextAttack(), Attack->AttackMontage.Get());
 FActorSpawnParameters SamuraiParams;
 SamuraiParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Samurai = World->SpawnActor<ASamuraiCharacter>(SamuraiParams);
 if (!TestNotNull(TEXT("Samurai for montage isolation check"), Samurai))
 {
  World->DestroyWorld(false);
  GEngine->DestroyWorldContext(World);
  return false;
 }
 auto* SamuraiAttack = Samurai->FindComponentByClass<UAutoAttackComponent>();
 SamuraiAttack->AlternateAttackMontage = Alternate;
 SamuraiAttack->bNextAttackUsesAlternate = true;
 TestEqual(TEXT("Samurai montage selection unaffected"), SamuraiAttack->GetMontageForNextAttack(), SamuraiAttack->AttackMontage.Get());
 World->DestroyWorld(false);
 GEngine->DestroyWorldContext(World);
 return true;
}
#endif
