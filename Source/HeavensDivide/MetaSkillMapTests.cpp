#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SMetaSkillMap.h"
#include "InputCoreTypes.h"
#include "MetaSkillTree.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaSkillMapHighlightTest,"HeavensDivide.Meta.NodeMapHighlight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMetaSkillMapHighlightTest::RunTest(const FString&)
{
	FName Selection(TEXT("Steel.Power"));
	const auto Map=SNew(SMetaSkillMap)
		.Selected_Lambda([&Selection](){return Selection;})
		.OnSelected_Lambda([&Selection](FName Id){Selection=Id;});
	Map->SetHoveredNode(TEXT("Fang.Flight"));
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
   TestTrue(TEXT("Every node and label fit the canvas"),P.X>=40 && P.X<=Map->CanvasSize.X-40 && P.Y>=100 && P.Y+92<=Map->CanvasSize.Y-45);
   TestEqual(TEXT("Every circle hit-tests to its own skill"),Map->HitNode(G,Map->ToScreen(G,P)),Ids[I]);
   for(int32 J=I+1;J<Ids.Num();++J)
   {
    const FVector2D Other=Map->DisplayPosition(*MetaSkillTree::Find(Ids[J]));
    TestTrue(TEXT("Node rings and hit targets do not overlap"),FVector2D::Distance(P,Other)>=56.f);
    if(FMath::Abs(P.Y-Other.Y)<1.)
     TestTrue(TEXT("Sibling label columns remain separated"),FMath::Abs(P.X-Other.X)>=(Map->NodeLabelWidth(Ids[I])+Map->NodeLabelWidth(Ids[J]))*.5+12.);
   }
   for(FName ParentId:Node.Prerequisites)if(Ids.Contains(ParentId))
   {
    const auto& Parent=*MetaSkillTree::Find(ParentId);
    const FVector2D A=Map->DisplayPosition(Parent);
    TestTrue(TEXT("Every prerequisite is above its child"),P.Y>A.Y+120.);
    const TArray<FVector2D> Points=Map->ConnectorPath(Parent,Node);
    TestEqual(TEXT("Connector begins on the parent ring"),Points[0],A+FVector2D(0,Parent.MaxRank==1?38:32));
    TestEqual(TEXT("Connector ends on the child ring"),Points.Last(),P-FVector2D(0,Node.MaxRank==1?38:32));
    int32 FontSize;double LineHeight;
    const auto Lines=Map->NodeLabelLines(Parent,FontSize,LineHeight);
    TestTrue(TEXT("Connected stem passes outside the parent label"),Points[2].X>A.X+Map->NodeLabelWidth(Parent.Id)*.5);
    TestTrue(TEXT("Connected stem returns below every label line"),Points[3].Y>A.Y+54+Lines.Num()*LineHeight);
    for(int32 Segment=1;Segment<Points.Num();++Segment)
    {
     const FVector2D Start=Points[Segment-1],End=Points[Segment],Delta=End-Start;
     TestTrue(TEXT("All connector segments are horizontal or vertical"),FMath::Abs(Delta.X)<.01||FMath::Abs(Delta.Y)<.01);
     TestTrue(TEXT("Connections never travel upward"),Delta.Y>=-.01);
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
     TestEqual(TEXT("Children share their parent's horizontal branch bus"),A.Points[5].Y,B.Points[5].Y);
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
 // The unfiltered fallback must use the same downward tree layout as every displayed page.
 VerifyLayout(AllNodes);
 for(const auto& Page:Pages){Map->SetVisibleNodes(Page.Value,Page.Key);VerifyLayout(Page.Value);}

 TArray<FName> Stats;
 for(const auto& Node:MetaSkillTree::Nodes())if(Node.Page==EMetaSkillPage::Stats)Stats.Add(Node.Id);
 Map->SetVisibleNodes(Stats,TEXT("Stats"));
 const double SharedX=Map->DisplayPosition(*MetaSkillTree::Find(TEXT("Root.Vitality"))).X;
 TestEqual(TEXT("Shared root is aligned with the canvas center"),SharedX,Map->CanvasSize.X*.5);
 double SamuraiRight=0.,NinjaLeft=Map->CanvasSize.X,SharedLeft=Map->CanvasSize.X,SharedRight=0.;
 for(FName Id:Stats)
 {
  const auto* Node=MetaSkillTree::Find(Id);const double X=Map->DisplayPosition(*Node).X;
  if(Node->Target==EUpgradeStatTarget::Samurai)SamuraiRight=FMath::Max(SamuraiRight,X);
  else if(Node->Target==EUpgradeStatTarget::Ninja)NinjaLeft=FMath::Min(NinjaLeft,X);
  else {SharedLeft=FMath::Min(SharedLeft,X);SharedRight=FMath::Max(SharedRight,X);}
 }
 TestTrue(TEXT("Samurai stats occupy the left lane"),SamuraiRight<SharedLeft);
 TestTrue(TEXT("Ninja stats occupy the right lane"),NinjaLeft>SharedRight);

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
   TestEqual(TEXT("A stance grows straight down one column"),Map->DisplayPosition(*Node).X,Map->DisplayPosition(*MetaSkillTree::Find(BloodNodes[0])).X);
  }
 }
 TestFalse(TEXT("Navigation above the root can reach external controls"),Map->OnKeyDown(G,FKeyEvent(EKeys::Up,FModifierKeysState(),0,false,0,0)).IsEventHandled());
 Map->Select(TEXT("Fang.Edge"));
 TestEqual(TEXT("Hidden stance cannot be selected"),Selection,FName(TEXT("Blood.Edge")));
 TestTrue(TEXT("Navigation inside the tree is handled by the map"),Map->OnKeyDown(G,FKeyEvent(EKeys::Down,FModifierKeysState(),0,false,0,0)).IsEventHandled());
 TestEqual(TEXT("Directional selection remains within the stance"),Selection,FName(TEXT("Blood.Reach")));
 TestFalse(TEXT("A different page never appears in the highlight"),Map->GetHighlightedAncestors().Contains(TEXT("Steel.Power")));
 Map->Select(TEXT("Blood.Vein"));
 TestFalse(TEXT("Navigation beyond the tree can reach external controls"),Map->OnKeyDown(G,FKeyEvent(EKeys::Gamepad_DPad_Down,FModifierKeysState(),0,false,0,0)).IsEventHandled());
 Map->SetVisibleNodes({},TEXT("Empty"));
 TestTrue(TEXT("Empty pages have no active hit targets"),Map->HitNode(G,{510,425}).IsNone());
 return true;
}
#endif
