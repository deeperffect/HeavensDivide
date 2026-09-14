#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "AnimNotify_NinjaThrowSound.h"
#include "Animation/AnimMontage.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjaThrowSoundAssetTest,"HeavensDivide.Combat.NinjaThrowSoundAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNinjaThrowSoundAssetTest::RunTest(const FString&)
{
    auto* Montage = LoadObject<UAnimMontage>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Ninja/AM_AutoAttackNinja.AM_AutoAttackNinja"));
    if (!TestNotNull(TEXT("Saved Ninja attack montage"),Montage)) return false;
    const bool bConfigure = FParse::Param(FCommandLine::Get(),TEXT("ConfigureNinjaThrowSound"));
    int32 SoundCount = 0;
    bool bChanged = false;
    const FString Filename = FPackageName::LongPackageNameToFilename(Montage->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    for (auto& Event : Montage->Notifies)
    {
        auto* Old = Cast<UAnimNotify_PlaySound>(Event.Notify);
        if (!Old) continue;
        ++SoundCount;
        if (bConfigure && !Old->IsA<UAnimNotify_NinjaThrowSound>())
        {
            const FString Backup = FPaths::ProjectSavedDir()/TEXT("Backups/NinjaThrowSound/AM_AutoAttackNinja.uasset");
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
            if (!IFileManager::Get().FileExists(*Backup)
                && !TestEqual(TEXT("Original montage backed up"),IFileManager::Get().Copy(*Backup,*Filename),COPY_OK)) return false;
            auto* Notify = NewObject<UAnimNotify_NinjaThrowSound>(Montage);
            Notify->Sound = Old->Sound;
            Notify->VolumeMultiplier = Old->VolumeMultiplier;
            Notify->PitchMultiplier = Old->PitchMultiplier;
            Notify->bFollow = Old->bFollow;
            Notify->AttachName = Old->AttachName;
            Notify->bPreviewIgnoreAttenuation = Old->bPreviewIgnoreAttenuation;
            Event.Notify = Notify; // Keep the original event's timing, track and trigger settings.
            bChanged = true;
        }
        auto* Notify = Cast<UAnimNotify_NinjaThrowSound>(Event.Notify);
        if (TestNotNull(TEXT("Throw notify selects the Ninja stance sound"),Notify))
            TestTrue(TEXT("Preview retains authored sound"),Notify->ResolveSound(nullptr)==Notify->Sound);
    }
    TestEqual(TEXT("Exactly one throw sound event remains"),SoundCount,1);
    if (bChanged)
    {
        Montage->MarkPackageDirty();
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        TestTrue(TEXT("Stance-aware montage saved"),UPackage::SavePackage(Montage->GetOutermost(),Montage,*Filename,Args));
    }
    return true;
}
#endif
