#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SMetaSkillMap.h"
#include "InputCoreTypes.h"
#include "MetaSkillTree.h"
#include "Widgets/IToolTip.h"
#include "Widgets/Text/STextBlock.h"
#if WITH_EDITOR
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Widgets/Layout/SDPIScaler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaSkillMapHighlightTest,"HeavensDivide.Meta.NodeMapHighlight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMetaSkillMapHighlightTest::RunTest(const FString&)
{
	FName Selection(TEXT("Steel.Power"));
	const auto Map=SNew(SMetaSkillMap)
		.Selected_Lambda([&Selection](){return Selection;})
		.OnSelected_Lambda([&Selection](FName Id){Selection=Id;});
	Map->SetHoveredNode(TEXT("Fang.Flight"));
    TestTrue(TEXT("Hovered node has a custom tooltip"),Map->GetToolTip().IsValid());
    TArray<FString> Texts;
    TFunction<void(TSharedRef<SWidget>)> ReadText=[&](TSharedRef<SWidget> Widget)
    {
        if(Widget->GetType()==TEXT("STextBlock"))Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
        auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)ReadText(Children->GetChildAt(I));
    };
    ReadText(Map->GetToolTip()->GetContentWidget());
    TestEqual(TEXT("Tooltip contains only one description block"),Texts.Num(),1);
    if(Texts.Num()==1)TestEqual(TEXT("Tooltip omits upgrade name and scope heading"),Texts[0],MetaSkillTree::Description(*MetaSkillTree::Find(TEXT("Fang.Flight"))));
 #if WITH_EDITOR
    if(FParse::Param(FCommandLine::Get(),TEXT("MetaSkillScreenshot"))&&!FParse::Param(FCommandLine::Get(),TEXT("NullRHI")))
    {
        FWidgetRenderer Renderer(false);auto Content=Map->GetToolTip()->GetContentWidget();
        for(float Scale:{1.f,1.25f})
        {
            auto Frame=SNew(SDPIScaler).DPIScale(Scale)[Content];Frame->SlatePrepass();
            const FVector2D Desired=Frame->GetDesiredSize();
            const FIntPoint Size(FMath::CeilToInt(Desired.X),FMath::CeilToInt(Desired.Y));
            auto* Target=Renderer.DrawWidget(Frame,FVector2D(Size.X,Size.Y));
            TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
            if(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags))
            {
                const auto GoldAt=[&](int32 X,int32 Y){const auto C=Pixels[Y*Size.X+X];return C.R>C.B+20&&C.G>40;};
                TestTrue(TEXT("Tooltip has gold border on all four sides, including fractional DPI"),
                    GoldAt(1,Size.Y/2)&&GoldAt(Size.X-2,Size.Y/2)&&GoldAt(Size.X/2,1)&&GoldAt(Size.X/2,Size.Y-2));
                TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
                FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/FString::Printf(TEXT("CollectionCombat/Previews/SkillTooltip%d.png"),FMath::RoundToInt(Scale*100))));
            }
        }
    }

 #endif
	auto Path=Map->GetHighlightedAncestors();
	TestTrue(TEXT("Hover cannot replace the selected Steel path"),Path.Contains(TEXT("Steel.Power")));
	TestTrue(TEXT("Selected node included"),Path.Contains(TEXT("Steel.Power")));
	TestTrue(TEXT("Selected parent included"),Path.Contains(TEXT("Steel.Edge")));
	TestFalse(TEXT("Hovered Shadow branch excluded"),Path.Contains(TEXT("Fang.Flight")));
	TestFalse(TEXT("Stance branch excluded"),Path.Contains(TEXT("Blood.Edge")));

	const FGeometry Geometry=FGeometry::MakeRoot(FVector2f(1020,850),FSlateLayoutTransform());
	Map->OnKeyDown(Geometry,FKeyEvent(EKeys::Right,FModifierKeysState(),0,false,0,0));
	TestTrue(TEXT("Directional navigation changes selection"),Selection!=FName(TEXT("Steel.Power")));
	TestTrue(TEXT("Directional navigation clears stale hover"),Map->Hovered.IsNone());
	TestTrue(TEXT("Highlight follows navigated selection"),Map->GetHighlightedAncestors().Contains(Selection));

	Map->Select(TEXT("Bond.Unity"));
	Map->SetHoveredNode(TEXT("Bond.Rhythm"));
	Path=Map->GetHighlightedAncestors();
	TestFalse(TEXT("Unity does not demand an unrelated character branch"),Path.Contains(TEXT("Shadow.Power")));
	TestFalse(TEXT("Other Bond capstone is not highlighted"),Path.Contains(TEXT("Bond.Rhythm")));
	TestFalse(TEXT("Other Bond lane is not highlighted"),Path.Contains(TEXT("Bond.Relay")));
    const float ScaleBefore=Map->ViewScale(Geometry);
    const FPointerEvent Wheel(0,FVector2D(20,20),FVector2D(20,20),TSet<FKey>(),EKeys::Invalid,1,FModifierKeysState());
    TestFalse(TEXT("Wheel bubbles to vertical scrolling"),Map->OnMouseWheel(Geometry,Wheel).IsEventHandled());
    TestEqual(TEXT("Wheel cannot zoom the skill map"),Map->ViewScale(Geometry),ScaleBefore);
    const FPointerEvent Drag(0,FVector2D(20,20),FVector2D(20,20),TSet<FKey>(),EKeys::RightMouseButton,0,FModifierKeysState());
    TestFalse(TEXT("Right click cannot capture the tree for panning"),Map->OnMouseButtonDown(Geometry,Drag).IsEventHandled());
	Map->ResetView();
	TestTrue(TEXT("Reset clears hover at the old map position"),Map->Hovered.IsNone());
	TestTrue(TEXT("Reset preserves the selected path"),Map->GetHighlightedAncestors().Contains(TEXT("Bond.Unity")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaSkillMapLayoutTest,"HeavensDivide.Meta.NodeMapLayout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMetaSkillMapLayoutTest::RunTest(const FString&)
{
 const auto Map=SNew(SMetaSkillMap);
 const FGeometry G=FGeometry::MakeRoot(FVector2f(1020,850),FSlateLayoutTransform());
 TMap<FString,TArray<FName>> Pages;
 TArray<FName> AllNodes;
 for(const auto& N:MetaSkillTree::Nodes())
 {
  Pages.FindOrAdd(N.RequiredStance.IsNone()?FString::FromInt(static_cast<int32>(N.Page)):N.RequiredStance.ToString()).Add(N.Id);
  AllNodes.Add(N.Id);
 }
 auto VerifyLayout=[&](const TArray<FName>& Ids)
 {
  struct FConnection {FName Parent;FName Child;TArray<FVector2D> Points;};
  TArray<FConnection> Connections;
  for(int32 I=0;I<Ids.Num();++I)
  {
   const auto& Node=*MetaSkillTree::Find(Ids[I]);
   const FVector2D P=Map->DisplayPosition(Node);
   int32 LabelSize;double LabelHeight;
   const auto LabelLines=Map->NodeLabelLines(Node,LabelSize,LabelHeight);
   TestTrue(TEXT("Every node, rank row and complete label fit the canvas"),P.X-Map->NodeLabelWidth(Node.Id)*.5>=40 &&
       P.X+Map->NodeLabelWidth(Node.Id)*.5<=Map->CanvasSize.X-40 && P.Y>=150 &&
       P.Y+76+LabelLines.Num()*LabelHeight<=Map->CanvasSize.Y-45);
   TestEqual(TEXT("Every circle hit-tests to its own skill"),Map->HitNode(G,Map->ToScreen(G,P)),Ids[I]);
   TestEqual(TEXT("Rank markers also select their skill"),Map->HitNode(G,Map->ToScreen(G,P+FVector2D(0,56))),Ids[I]);
   for(int32 J=I+1;J<Ids.Num();++J)
   {
    const FVector2D Other=Map->DisplayPosition(*MetaSkillTree::Find(Ids[J]));
    TestTrue(TEXT("Node rings and hit targets do not overlap"),FVector2D::Distance(P,Other)>=56.f);
    if(FMath::Abs(P.Y-Other.Y)<1.)
     TestTrue(TEXT("Sibling label columns remain separated"),FMath::Abs(P.X-Other.X)>=(Map->NodeLabelWidth(Ids[I])+Map->NodeLabelWidth(Ids[J]))*.5+12.);
    if(FMath::Abs(P.X-Other.X)<1.)
     TestTrue(TEXT("Vertical branch lanes leave room for labels and rank dots"),FMath::Abs(P.Y-Other.Y)>=170.);
   }
   for(FName ParentId:Node.Prerequisites)if(Ids.Contains(ParentId))
   {
    const auto& Parent=*MetaSkillTree::Find(ParentId);
    const FVector2D A=Map->DisplayPosition(Parent);
    TestTrue(TEXT("Every prerequisite is to the left of its child"),P.X>A.X+240.);
    const TArray<FVector2D> Points=Map->ConnectorPath(Parent,Node);
    TestEqual(TEXT("Connector begins on the right of the parent ring"),Points[0],A+FVector2D(Parent.MaxRank==1?38:32,0));
    TestEqual(TEXT("Connector ends on the left of the child ring"),Points.Last(),P-FVector2D(Node.MaxRank==1?38:32,0));
    TestTrue(TEXT("Branch bus passes outside parent and child labels"),Points[1].X>A.X+Map->NodeLabelWidth(Parent.Id)*.5 && Points[1].X<P.X-Map->NodeLabelWidth(Node.Id)*.5);
    for(int32 Segment=1;Segment<Points.Num();++Segment)
    {
     const FVector2D Start=Points[Segment-1],End=Points[Segment],Delta=End-Start;
     TestTrue(TEXT("All connector segments are horizontal or vertical"),FMath::Abs(Delta.X)<.01||FMath::Abs(Delta.Y)<.01);
     TestTrue(TEXT("Connections never travel backward"),Delta.X>=-.01);
     for(FName OtherId:Ids)if(OtherId!=Node.Id&&OtherId!=ParentId)
     {
      const FVector2D Center=Map->DisplayPosition(*MetaSkillTree::Find(OtherId));
      const double T=Delta.SizeSquared()>0.?FMath::Clamp(FVector2D::DotProduct(Center-Start,Delta)/Delta.SizeSquared(),0.,1.):0.;
      const double RingRadius=MetaSkillTree::Find(OtherId)->MaxRank==1?38.:32.;
      TestTrue(TEXT("Connectors stay clear of unrelated rings and selection outlines"),FVector2D::Distance(Center,Start+T*Delta)>RingRadius+8.);
     }
    }
    Connections.Add({ParentId,Node.Id,Points});
   }
  }
  for(int32 I=0;I<Connections.Num();++I)
   for(int32 J=I+1;J<Connections.Num();++J)
   {
    const auto& A=Connections[I];const auto& B=Connections[J];
    if(A.Parent==B.Parent)
     TestEqual(TEXT("Children share their parent's vertical branch bus"),A.Points[1].X,B.Points[1].X);
    for(int32 AS=1;AS<A.Points.Num();++AS)
     for(int32 BS=1;BS<B.Points.Num();++BS)
     {
      const FVector2D A0=A.Points[AS-1],A1=A.Points[AS],B0=B.Points[BS-1],B1=B.Points[BS];
      auto Crosses=[](FVector2D V0,FVector2D V1,FVector2D H0,FVector2D H1)
      {
       return FMath::Abs(V0.X-V1.X)<.01&&FMath::Abs(H0.Y-H1.Y)<.01&&
        V0.X>FMath::Min(H0.X,H1.X)+.01&&V0.X<FMath::Max(H0.X,H1.X)-.01&&
        H0.Y>FMath::Min(V0.Y,V1.Y)+.01&&H0.Y<FMath::Max(V0.Y,V1.Y)-.01;
      };
      TestFalse(TEXT("Separate prerequisite routes never cross"),Crosses(A0,A1,B0,B1)||Crosses(B0,B1,A0,A1));
     }
   }

  for(FName Id:Ids)
   TestEqual(TEXT("Width fitting shares the paint/hit-test transform"),Map->HitNode(G,Map->ToScreen(G,Map->DisplayPosition(*MetaSkillTree::Find(Id)))),Id);
  Map->ResetView();
 };
 // The unfiltered fallback must use the same horizontal tree layout as every displayed page.
 VerifyLayout(AllNodes);
 for(const auto& Page:Pages){Map->SetVisibleNodes(Page.Value,Page.Key);VerifyLayout(Page.Value);}

 TArray<FName> Stats;
 for(const auto& Node:MetaSkillTree::Nodes())if(Node.Page==EMetaSkillPage::Stats)Stats.Add(Node.Id);
 Map->SetVisibleNodes(Stats,TEXT("Stats"));
 const auto SharedRoot=Map->DisplayPosition(*MetaSkillTree::Find(TEXT("Root.Vitality")));
 TestTrue(TEXT("The shared entry skill is visible at the top left on opening"),SharedRoot.X<200. && SharedRoot.Y<200.);
 double SamuraiBottom=0.,NinjaTop=Map->CanvasSize.Y,SharedTop=Map->CanvasSize.Y,SharedBottom=0.;
 for(FName Id:Stats)
 {
  if(Id==TEXT("Root.Vitality"))continue;
  const auto* Node=MetaSkillTree::Find(Id);const double Y=Map->DisplayPosition(*Node).Y;
  if(Node->Target==EUpgradeStatTarget::Samurai)SamuraiBottom=FMath::Max(SamuraiBottom,Y);
  else if(Node->Target==EUpgradeStatTarget::Ninja)NinjaTop=FMath::Min(NinjaTop,Y);
  else {SharedTop=FMath::Min(SharedTop,Y);SharedBottom=FMath::Max(SharedBottom,Y);}
 }
 TestTrue(TEXT("Samurai stats occupy the upper branch lanes"),SamuraiBottom<SharedTop);
 TestTrue(TEXT("Ninja stats occupy the lower branch lanes"),NinjaTop>SharedBottom);

 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaSkillMapFilterTest,"HeavensDivide.Meta.NodeMapPages",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMetaSkillMapFilterTest::RunTest(const FString&)
{
 FName Selection(TEXT("Blood.Edge"));
 const auto Map=SNew(SMetaSkillMap).Selected_Lambda([&Selection](){return Selection;})
  .OnSelected_Lambda([&Selection](FName Id){Selection=Id;});
 const TArray<FName> BloodNodes{TEXT("Blood.Edge"),TEXT("Blood.Reach"),TEXT("Blood.Vein")};
 Map->SetVisibleNodes(BloodNodes,TEXT("Blood"));
 const FGeometry G=FGeometry::MakeRoot(FVector2f(1020,850),FSlateLayoutTransform());
 for(FName Id:BloodNodes)
 {
  const auto* Node=MetaSkillTree::Find(Id);
  TestNotNull(TEXT("Stance page has its authored skill"),Node);
  if(Node)
  {
   TestEqual(TEXT("Filtered positions share rendering and hit tests"),Map->HitNode(G,Map->ToScreen(G,Map->DisplayPosition(*Node))),Id);
   TestEqual(TEXT("A stance grows left to right on one row"),Map->DisplayPosition(*Node).Y,Map->DisplayPosition(*MetaSkillTree::Find(BloodNodes[0])).Y);
  }
 }
 TestFalse(TEXT("Navigation left of the root can reach external controls"),Map->OnKeyDown(G,FKeyEvent(EKeys::Left,FModifierKeysState(),0,false,0,0)).IsEventHandled());
 Map->Select(TEXT("Fang.Edge"));
 TestEqual(TEXT("Hidden stance cannot be selected"),Selection,FName(TEXT("Blood.Edge")));
 TestTrue(TEXT("Navigation inside the tree is handled by the map"),Map->OnKeyDown(G,FKeyEvent(EKeys::Right,FModifierKeysState(),0,false,0,0)).IsEventHandled());
 TestEqual(TEXT("Directional selection remains within the stance"),Selection,FName(TEXT("Blood.Reach")));
 TestFalse(TEXT("A different page never appears in the highlight"),Map->GetHighlightedAncestors().Contains(TEXT("Steel.Power")));
 Map->Select(TEXT("Blood.Vein"));
 TestFalse(TEXT("Navigation beyond the tree can reach external controls"),Map->OnKeyDown(G,FKeyEvent(EKeys::Gamepad_DPad_Right,FModifierKeysState(),0,false,0,0)).IsEventHandled());
 Map->SetVisibleNodes({},TEXT("Empty"));
 TestTrue(TEXT("Empty pages have no active hit targets"),Map->HitNode(G,{510,425}).IsNone());
 return true;
}
#endif
