#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CombatAudio.h"
#include "MetasoundSource.h"
#include "Sound/SoundGenerator.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Crc.h"
#include "Engine/Engine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAudioTest, "HeavensDivide.Audio.CombatPalette",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatAudioTest::RunTest(const FString&)
{
    auto* Palette=LoadObject<UCombatAudioPalette>(nullptr,TEXT("/Game/HeavensDivide/Audio/DA_CombatAudio"));
    if(!TestNotNull(TEXT("Combat palette is packaged and loadable"),Palette))return false;
    TestEqual(TEXT("All designed events have sounds"),Palette->Sounds.Num(),56);
    uint64 Instance=100000;
    auto Sounds = Palette->Sounds;
    Sounds.Add(TEXT("MenuHover"), LoadObject<USoundBase>(nullptr,TEXT("/Game/HeavensDivide/Audio/Menu/MS_MenuHover")));
    Sounds.Add(TEXT("MenuPress"), LoadObject<USoundBase>(nullptr,TEXT("/Game/HeavensDivide/Audio/Menu/MS_MenuPress")));
    for(const auto& Pair:Sounds)
    {
        auto* Source=Cast<UMetaSoundSource>(Pair.Value);
        if(!TestNotNull(Pair.Key.ToString()+TEXT(" is a MetaSound"),Source))continue;
        TestTrue(TEXT("One-shot source releases its voice"),Source->IsOneShot());
        TestFalse(TEXT("Voice limits assigned"),Source->ConcurrencySet.IsEmpty());
        Source->InitResources();
        uint32 Hashes[2]={0,0};
        for(int32 Variation=0;Variation<2;++Variation)
        {
            TArray<FAudioParameter> Defaults;
            Source->InitParameters(Defaults,TEXT("CombatAudioTest"));
            FSoundGeneratorInitParams Params;
            Params.AudioDeviceID=0;Params.InstanceID=++Instance;Params.AudioComponentId=Instance;
            Params.SampleRate=48000;Params.NumChannels=1;Params.NumFramesPerCallback=512;Params.AudioMixerNumOutputFrames=512;
            auto Generator=Source->CreateSoundGenerator(Params,MoveTemp(Defaults));
            if(!TestTrue(TEXT("MetaSound graph builds"),Generator.IsValid()))return false;
            Generator->OnBeginGenerate();
            TArray<float> Rendered,Buffer;Buffer.SetNumZeroed(512);
            double Energy=0;float Peak=0;
            const double Deadline=FPlatformTime::Seconds()+5;
            bool Started=false;
            while(FPlatformTime::Seconds()<Deadline && Rendered.Num()<48000*3 && !Generator->IsFinished())
            {
                FMemory::Memzero(Buffer.GetData(),Buffer.Num()*sizeof(float));
                Generator->GetNextBuffer(Buffer.GetData(),Buffer.Num());
                bool Nonzero=false;
                for(float Sample:Buffer)
                {
                    if(!FMath::IsFinite(Sample)){AddError(TEXT("Nonfinite audio sample"));return false;}
                    Energy+=Sample*Sample;Peak=FMath::Max(Peak,FMath::Abs(Sample));Nonzero|=FMath::Abs(Sample)>1.e-6f;
                }
                Started|=Nonzero;
                if(Started)Rendered.Append(Buffer);
                else FPlatformProcess::Sleep(.002f); // Allow async graph construction/decode to finish.
            }
            TestTrue(Pair.Key.ToString()+TEXT(" produces audible PCM"),Energy>.01);
            TestTrue(Pair.Key.ToString()+TEXT(" finishes within three seconds"),Generator->IsFinished());
            TestTrue(Pair.Key.ToString()+TEXT(" has unclipped output"),Peak<.99f);
            Hashes[Variation]=FCrc::MemCrc32(Rendered.GetData(),Rendered.Num()*sizeof(float));
            Generator->OnEndGenerate();Source->OnEndGenerate(Generator);
            if(HasAnyErrors())return false;
        }
        TestNotEqual(Pair.Key.ToString()+TEXT(" varies across plays"),Hashes[0],Hashes[1]);
    }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChestJackpotAudioTest,"HeavensDivide.Audio.ChestJackpot",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FChestJackpotAudioTest::RunTest(const FString&)
{
    auto* Source=LoadObject<UMetaSoundSource>(nullptr,TEXT("/Game/HeavensDivide/Audio/MetaSounds/MS_ChestJackpot"));
    if(!TestNotNull(TEXT("Jackpot MetaSound loads"),Source))return false;
    TestTrue(TEXT("Jackpot is a one-shot"),Source->IsOneShot());
    TestFalse(TEXT("Jackpot has bounded UI voices"),Source->ConcurrencySet.IsEmpty());
    Source->InitResources();
    uint32 Hashes[2]={};
    for(int32 Play=0;Play<2;++Play)
    {
        TArray<FAudioParameter> Defaults;Source->InitParameters(Defaults,TEXT("ChestJackpotTest"));
        FSoundGeneratorInitParams Params;
        Params.AudioDeviceID=GEngine->GetMainAudioDeviceID();Params.InstanceID=900000+Play;Params.AudioComponentId=Params.InstanceID;
        Params.SampleRate=48000;Params.NumChannels=1;Params.NumFramesPerCallback=512;Params.AudioMixerNumOutputFrames=512;
        auto Generator=Source->CreateSoundGenerator(Params,MoveTemp(Defaults));
        if(!TestTrue(TEXT("Jackpot graph builds"),Generator.IsValid()))return false;
        Generator->OnBeginGenerate();
        TArray<float> PCM,Buffer;Buffer.SetNumZeroed(512);
        double Energy=0;float Peak=0;bool Started=false;
        const double Deadline=FPlatformTime::Seconds()+5;
        while(FPlatformTime::Seconds()<Deadline&&PCM.Num()<48000*4&&!Generator->IsFinished())
        {
            FMemory::Memzero(Buffer.GetData(),Buffer.Num()*sizeof(float));
            Generator->GetNextBuffer(Buffer.GetData(),Buffer.Num());
            bool Nonzero=false;
            for(float Sample:Buffer)
            {
                if(!FMath::IsFinite(Sample)){AddError(TEXT("Nonfinite jackpot sample"));return false;}
                Energy+=Sample*Sample;Peak=FMath::Max(Peak,FMath::Abs(Sample));Nonzero|=FMath::Abs(Sample)>1.e-6f;
            }
            Started|=Nonzero;
            if(Started)PCM.Append(Buffer);else FPlatformProcess::Sleep(.002f);
        }
        TestTrue(TEXT("Jackpot produces audible PCM"),Energy>.01);
        TestTrue(TEXT("Jackpot output is unclipped"),Peak<.99f);
        TestTrue(TEXT("Jackpot releases its voice within four seconds"),Generator->IsFinished());
        Hashes[Play]=FCrc::MemCrc32(PCM.GetData(),PCM.Num()*sizeof(float));
        Generator->OnEndGenerate();Source->OnEndGenerate(Generator);
        if(HasAnyErrors())return false;
    }
    TestNotEqual(TEXT("Jackpot pitch varies between plays"),Hashes[0],Hashes[1]);
    return !HasAnyErrors();
}
#endif
