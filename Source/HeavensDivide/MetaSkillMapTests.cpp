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
 for(const auto& N:MetaSkillTree::Nodes())
  Pages.FindOrAdd(N.RequiredStance.IsNone()?FString::FromInt(static_cast<int32>(N.Page)):N.RequiredStance.ToString()).Add(N.Id);
 for(const auto& Page:Pages)
 {
  Map->SetVisibleNodes(Page.Value,Page.Key);
  for(int32 I=0;I<Page.Value.Num();++I)
  {
   const FVector2D P=Map->DisplayPosition(*MetaSkillTree::Find(Page.Value[I]));
   TestTrue(TEXT("Every page node fits the canvas"),P.X>=30 && P.X<=990 && P.Y>=65 && P.Y<=815);
   TestEqual(TEXT("Every circle hit-tests to its own skill"),Map->HitNode(G,Map->ToScreen(G,P)),Page.Value[I]);
   for(int32 J=I+1;J<Page.Value.Num();++J)
    TestTrue(TEXT("Node rings and hit targets do not overlap"),FVector2D::Distance(P,Map->DisplayPosition(*MetaSkillTree::Find(Page.Value[J])))>=56.f);
  }
  Map->Zoom=1.5f;Map->Pan={-120,60};
  for(FName Id:Page.Value)
   TestEqual(TEXT("Zoom and pan share the paint/hit-test transform"),Map->HitNode(G,Map->ToScreen(G,Map->DisplayPosition(*MetaSkillTree::Find(Id)))),Id);
 }
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
  if(Node)TestEqual(TEXT("Filtered positions share rendering and hit tests"),Map->HitNode(G,Map->ToScreen(G,Map->DisplayPosition(*Node))),Id);
 }
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
