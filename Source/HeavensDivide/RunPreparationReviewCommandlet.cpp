#include "RunPreparationReviewCommandlet.h"
#if WITH_EDITOR
#include "FileHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "MinimapBounds.h"
#include "ObjectiveSpawnPoint.h"
#include "AssetCompilingManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "UpgradeDefinition.h"
#include "RunStanceCatalog.h"
#endif

URunPreparationReviewCommandlet::URunPreparationReviewCommandlet()
{ IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }

int32 URunPreparationReviewCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    if(FParse::Param(*Params,TEXT("ControllerAudit")) || FParse::Param(*Params,TEXT("FixStartupStances")))
    {
        auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController"));
        if(!BP)return 5;
        TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
        FString Report;TArray<UEdGraphNode*> Grants;
        for(auto* Graph:Graphs)for(auto NodePtr:Graph->Nodes)
        {
            auto* Node=NodePtr.Get();if(!Node)continue;
            Report+=Graph->GetName()+TEXT(" | ")+Node->GetName()+TEXT(" | ")+Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString()+TEXT("\n");
            FString Function;
            if(auto* Property=FindFProperty<FProperty>(Node->GetClass(),TEXT("FunctionReference")))Property->ExportText_InContainer(0,Function,Node,Node,Node,PPF_None);
            Report+=Function+TEXT("\n");
            bool bStance=false;
            for(auto* Pin:Node->Pins)if(Pin)
            {
                Report+=FString::Printf(TEXT("  %s %s default=%s object=%s -> "),*Pin->PinName.ToString(),Pin->Direction==EGPD_Input?TEXT("IN"):TEXT("OUT"),*Pin->DefaultValue,*GetNameSafe(Pin->DefaultObject));
                for(auto* Link:Pin->LinkedTo)Report+=Link->GetOwningNode()->GetName()+TEXT(".")+Link->PinName.ToString()+TEXT(" ");
                Report+=TEXT("\n");
                if(auto* Card=Cast<UUpgradeDefinition>(Pin->DefaultObject))bStance|=RunStances::Find(Card->UpgradeId)!=nullptr;
            }
            const auto* Exec=Node->FindPin(TEXT("execute"));
            const bool bStartup=Exec&&Exec->LinkedTo.ContainsByPredicate([](const UEdGraphPin* Link)
            {
                auto* Event=Link->GetOwningNode();FString Reference;
                if(auto* P=FindFProperty<FProperty>(Event->GetClass(),TEXT("EventReference")))P->ExportText_InContainer(0,Reference,Event,Event,Event,PPF_None);
                return Reference.Contains(TEXT("MemberName=\"ReceiveBeginPlay\""));
            });
            if(Function.Contains(TEXT("MemberName=\"DebugForceAcquireUpgrade\""))&&bStance&&bStartup)Grants.Add(Node);
        }
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("RunPreparation");IFileManager::Get().MakeDirectory(*Dir,true);
        FFileHelper::SaveStringToFile(Report,*(Dir/TEXT("ControllerBlueprint.txt")));
        if(!FParse::Param(*Params,TEXT("FixStartupStances")))return 0;
        const FString Filename=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        const FString Backup=Dir/TEXT("Backup/BP_SurvivorPlayerController.uasset");
        if(!IFileManager::Get().FileExists(*Backup)&&IFileManager::Get().Copy(*Backup,*Filename)!=COPY_OK)return 6;
        for(auto* Node:Grants)
        {
            auto* In=Node->FindPin(TEXT("execute"));auto* Out=Node->FindPin(TEXT("then"));
            if(!In||!Out)return 7;
            const auto Sources=In->LinkedTo,Targets=Out->LinkedTo;
            Node->BreakAllNodeLinks();
            for(auto* Source:Sources)for(auto* Target:Targets)Source->MakeLinkTo(Target);
            FBlueprintEditorUtils::RemoveNode(BP,Node,true);
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
        if(BP->Status==BS_Error)return 8;
        FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;
        if(!UPackage::SavePackage(BP->GetOutermost(),BP,*Filename,Save))return 9;
        UE_LOG(LogTemp,Display,TEXT("RUN_STARTUP_STANCES_REMOVED count=%d"),Grants.Num());
        return 0;
    }
    auto* W=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/Maps/Lvl_B1_Lvl1"));
    if(!W)return 1;
    W->FlushLevelStreaming(EFlushLevelStreamingType::Full);
    W->WorldType=EWorldType::Game;
    if(!W->Scene || !W->GetPhysicsScene())
    {
        W->CleanupWorld(false,true);
        W->InitWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).EnableTraceCollision(true));
        W->UpdateWorldComponents(true,false);
        W->FlushLevelStreaming(EFlushLevelStreamingType::Full);
    }
    W->InitializeActorsForPlay(FURL());
    FAssetCompilingManager::Get().FinishAllCompilation();
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
    FVector Start=FVector::ZeroVector;
    for(TActorIterator<APlayerStart> It(W);It;++It){Start=It->GetActorLocation();break;}
    auto Ground=[&](FVector P,FHitResult& H)
    { return W->LineTraceSingleByChannel(H,P+FVector(0,0,5000),P-FVector(0,0,5000),ECC_GameTraceChannel2) && H.ImpactNormal.Z>.85f; };
    struct FSample { FVector Position; FString Actor; bool bReachable=false; };
    TMap<FIntPoint,FSample> Grid;
    auto Clear=[&](const FVector& P)
    { return !W->OverlapBlockingTestByChannel(P+FVector(0,0,115),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(90,100)); };
    FString CSV=TEXT("x,y,z,reachable,ground\n");
    int32 Samples=0;
    for(int32 X=-7000;X<=7000;X+=500)for(int32 Y=-7000;Y<=7000;Y+=500)
    {
        FHitResult H;
        if(!Ground(FVector(X,Y,Start.Z),H)||!Clear(H.ImpactPoint))continue;
        Grid.Add(FIntPoint(X,Y),{H.ImpactPoint,GetNameSafe(H.GetActor()),false});
        ++Samples;
    }
    // This map uses direct movement, with no NavMesh. Flood clear capsule sweeps from
    // the player start so the survey excludes roofs, isolated terrain and blocked courtyards.
    FIntPoint Seed=FIntPoint::ZeroValue;float Nearest=MAX_flt;
    for(const auto& Entry:Grid)if(FVector::DistSquared2D(Entry.Value.Position,Start)<Nearest)
    { Seed=Entry.Key;Nearest=FVector::DistSquared2D(Entry.Value.Position,Start); }
    TArray<FIntPoint> Queue;
    if(Grid.Contains(Seed)){Grid[Seed].bReachable=true;Queue.Add(Seed);}
    for(int32 I=0;I<Queue.Num();++I)for(const FIntPoint Offset:{FIntPoint(500,0),FIntPoint(-500,0),FIntPoint(0,500),FIntPoint(0,-500)})
    {
        auto* A=Grid.Find(Queue[I]);auto* B=Grid.Find(Queue[I]+Offset);
        if(!B||B->bReachable||FMath::Abs(A->Position.Z-B->Position.Z)>70)continue;
        FHitResult Hit;
        if(W->SweepSingleByChannel(Hit,A->Position+FVector(0,0,115),B->Position+FVector(0,0,115),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(60,95)))continue;
        B->bReachable=true;Queue.Add(Queue[I]+Offset);
    }
    for(const auto& Entry:Grid)
    {const auto& P=Entry.Value;CSV+=FString::Printf(TEXT("%.1f,%.1f,%.1f,%d,%s\n"),P.Position.X,P.Position.Y,P.Position.Z,P.bReachable,*P.Actor);}
    for(TActorIterator<ANavMeshBoundsVolume> It(W);It;++It)
    { FVector C,E;It->GetActorBounds(false,C,E);UE_LOG(LogTemp,Display,TEXT("RUN_NAV_BOUNDS center=%s extent=%s"),*C.ToString(),*E.ToString()); }
    int32 Bounds=0,Points=0,Valid=0;
    for(TActorIterator<AMinimapBounds> It(W);It;++It)++Bounds;
    for(TActorIterator<AObjectiveSpawnPoint> It(W);It;++It)
    {
        ++Points;FHitResult H;FNavLocation N;const FVector P=It->GetActorLocation();
        const auto* Sample=Grid.Find(FIntPoint(FMath::RoundToInt(P.X/500)*500,FMath::RoundToInt(P.Y/500)*500));
        const bool Good=Ground(P,H)&&FMath::Abs(P.Z-H.ImpactPoint.Z)<120 && Clear(H.ImpactPoint) && Sample && Sample->bReachable;
        if(Good)++Valid;
        UE_LOG(LogTemp,Display,TEXT("RUN_OBJECTIVE_POINT %s position=%s ground=%s valid=%d"),*It->GetName(),*P.ToString(),*H.ImpactPoint.ToString(),Good);
    }
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("RunPreparation");IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveStringToFile(CSV,*(Dir/TEXT("LevelGround.csv")));
    UE_LOG(LogTemp,Display,TEXT("RUN_LEVEL_PROBE bounds=%d points=%d valid=%d samples=%d"),Bounds,Points,Valid,Samples);
    return FParse::Param(*Params,TEXT("Validate")) && (Bounds!=1 || Points<5 || Valid!=Points) ? 2 : 0;
#else
    return 1;
#endif
}
