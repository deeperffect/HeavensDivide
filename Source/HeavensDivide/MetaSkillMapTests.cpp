#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SMetaSkillMap.h"
#include "InputCoreTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaSkillMapHighlightTest,"HeavensDivide.Meta.NodeMapHighlight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMetaSkillMapHighlightTest::RunTest(const FString&)
{
	FName Selection(TEXT("Steel.Reach"));
	const auto Map=SNew(SMetaSkillMap)
		.Selected_Lambda([&Selection](){return Selection;})
		.OnSelected_Lambda([&Selection](FName Id){Selection=Id;});
	Map->SetHoveredNode(TEXT("Shadow.Flight"));
	auto Path=Map->GetHighlightedAncestors();
	TestEqual(TEXT("Hover cannot replace the selected Steel path"),Path.Num(),3);
	TestTrue(TEXT("Selected node included"),Path.Contains(TEXT("Steel.Reach")));
	TestTrue(TEXT("Selected parent included"),Path.Contains(TEXT("Steel.Edge")));
	TestTrue(TEXT("Actual cross-path prerequisite included"),Path.Contains(TEXT("Root.Vitality")));
	TestFalse(TEXT("Hovered Shadow branch excluded"),Path.Contains(TEXT("Shadow.Flight")));
	TestFalse(TEXT("Sibling Steel branch excluded"),Path.Contains(TEXT("Steel.Blood")));

	const FGeometry Geometry=FGeometry::MakeRoot(FVector2f(1000,780),FSlateLayoutTransform());
	Map->OnKeyDown(Geometry,FKeyEvent(EKeys::Right,FModifierKeysState(),0,false,0,0));
	TestTrue(TEXT("Directional navigation changes selection"),Selection!=FName(TEXT("Steel.Reach")));
	TestTrue(TEXT("Directional navigation clears stale hover"),Map->Hovered.IsNone());
	TestTrue(TEXT("Highlight follows navigated selection"),Map->GetHighlightedAncestors().Contains(Selection));

	Map->Select(TEXT("Bond.Unity"));
	Map->SetHoveredNode(TEXT("Bond.Rhythm"));
	Path=Map->GetHighlightedAncestors();
	TestTrue(TEXT("Unity includes its Steel prerequisite"),Path.Contains(TEXT("Steel.Power")));
	TestTrue(TEXT("Unity includes its Shadow prerequisite"),Path.Contains(TEXT("Shadow.Power")));
	TestTrue(TEXT("Unity includes its Bond prerequisite"),Path.Contains(TEXT("Bond.Echo")));
	TestFalse(TEXT("Other Bond capstone is not highlighted"),Path.Contains(TEXT("Bond.Rhythm")));
	TestFalse(TEXT("Other Bond lane is not highlighted"),Path.Contains(TEXT("Bond.Reaction")));
	Map->ResetView();
	TestTrue(TEXT("Reset clears hover at the old map position"),Map->Hovered.IsNone());
	TestTrue(TEXT("Reset preserves the selected path"),Map->GetHighlightedAncestors().Contains(TEXT("Bond.Unity")));
	return true;
}
#endif
