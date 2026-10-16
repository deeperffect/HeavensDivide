#include "SMetaSkillMap.h"
#include "BloodshiftMenuStyle.h"
#include "Widgets/Layout/SScrollBox.h"
#include "MetaSkillTree.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"

namespace
{
 class SSkillDescriptionFrame : public SBorder
 {
 public:
  SLATE_BEGIN_ARGS(SSkillDescriptionFrame) {} SLATE_ARGUMENT(FString, Description) SLATE_END_ARGS()
  void Construct(const FArguments& Args)
  {
   SBorder::Construct(SBorder::FArguments().BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
    .BorderBackgroundColor(FLinearColor(.025f,.055f,.065f)).Padding(18,14)
    [SNew(STextBlock).Font(BloodshiftMenu::BodyFont(16)).ColorAndOpacity(BloodshiftMenu::Paper).WrapTextAt(320)
     .Text(FText::FromString(Args._Description))]);
  }
  virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
   FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override
  {
   const int32 Last=SBorder::OnPaint(Args,Geometry,Culling,Out,Layer,Style,Enabled);
   const FVector2D Size=Geometry.GetLocalSize();
   // Draw inside the allotted bounds: fractional DPI/window rounding cannot clip the right edge.
   const float Inset=1.5f;
   TArray<FVector2D> Frame{{Inset,Inset},{Size.X-Inset,Inset},{Size.X-Inset,Size.Y-Inset},
    {Inset,Size.Y-Inset},{Inset,Inset}};
   FLinearColor Color=BloodshiftMenu::Gold;Color.A*=Style.GetColorAndOpacityTint().A;
   FSlateDrawElement::MakeLines(Out,Last+1,Geometry.ToPaintGeometry(),Frame,ESlateDrawEffect::None,Color,true,1.5f);
   return Last+1;
  }
 };
 const FLinearColor Ivory(.90f,.88f,.80f);
 const FLinearColor Gold=BloodshiftMenu::Gold;
 constexpr double RankTop=56.;
 constexpr double RankRadius=7.;
 constexpr double RankSpacing=22.;
 constexpr double LabelTop=76.;
 FLinearColor PathColor(int32 Branch)
 {
  if(Branch==1)return FLinearColor(.85f,.24f,.18f);
  if(Branch==2)return FLinearColor(.37f,.76f,.35f);
  return FLinearColor(.30f,.52f,.92f);
 }
 float NodeRadius(const FMetaSkillNode& N){return N.MaxRank==1?38.f:32.f;}
 bool OpenPath(const FMetaSkillNode& N,const USynergyMetaProgressionSubsystem* Meta)
 {
  if(!Meta)return false;
  for(FName Parent:N.Prerequisites)if(Meta->GetSkillRank(Parent)==0)return false;
  return true;
 }
 FLinearColor Alpha(FLinearColor C,float A){C.A=A;return C;}
}
void SMetaSkillMap::Construct(const FArguments& Args)
{
	Progression=Args._Progression;Selected=Args._Selected;OnSelected=Args._OnSelected;
	SetClipping(EWidgetClipping::ClipToBounds);
	BuildLayout();
}
void SMetaSkillMap::SetVisibleNodes(const TArray<FName>& Ids, const FString& Title)
{
 bFiltered=true; VisibleNodes.Reset(); FilterTitle=Title;
 for(FName Id:Ids)if(MetaSkillTree::Find(Id))VisibleNodes.Add(Id);
 BuildLayout(); ResetView();
}
void SMetaSkillMap::BuildLayout()
{
 LayoutPositions.Reset(); LabelWidths.Reset();
 TArray<FName> Ids;
 for(const auto& Node:MetaSkillTree::Nodes())if(IsNodeVisible(Node.Id))Ids.Add(Node.Id);
 bStatsLayout=bFiltered&&!Ids.IsEmpty();
 for(FName Id:Ids)if(MetaSkillTree::Find(Id)->Page!=EMetaSkillPage::Stats)bStatsLayout=false;
 TMap<FName,int32> Depth;
 for(FName Id:Ids)Depth.Add(Id,0);
 // Derive tiers from prerequisites, never from the old radial artwork coordinates.
 for(int32 Pass=0;Pass<Ids.Num();++Pass)
  for(FName Id:Ids)
   for(FName Parent:MetaSkillTree::Find(Id)->Prerequisites)if(Depth.Contains(Parent))
    Depth[Id]=FMath::Max(Depth[Id],FMath::Min(Ids.Num()-1,Depth.FindRef(Parent)+1));
 TMap<FName,TArray<FName>> Children;
 TArray<FName> Roots;
 int32 MaxDepth=0;
 for(FName Id:Ids)
 {
  FName LayoutParent;
  for(FName Parent:MetaSkillTree::Find(Id)->Prerequisites)
   if(Depth.Contains(Parent)&&Depth.FindRef(Parent)<Depth.FindRef(Id)&&
      (LayoutParent.IsNone()||Depth.FindRef(Parent)>Depth.FindRef(LayoutParent)))LayoutParent=Parent;
  if(LayoutParent.IsNone())Roots.Add(Id);else Children.FindOrAdd(LayoutParent).Add(Id);
  MaxDepth=FMath::Max(MaxDepth,Depth.FindRef(Id));
 }
 if(bStatsLayout)
 {
  auto Order=[](FName Id)
  {
   const auto Target=MetaSkillTree::Find(Id)->Target;
   return Target==EUpgradeStatTarget::Samurai?0:Target==EUpgradeStatTarget::Ninja?2:1;
  };
  Roots.StableSort([&](FName A,FName B){return Order(A)<Order(B);});
  for(auto& Group:Children)Group.Value.StableSort([&](FName A,FName B){return Order(A)<Order(B);});
 }
 // Progress left to right; reserve a vertical lane for each complete subtree.
 // Keep full-size nodes and scroll extra branches instead of shrinking dense pages.
 constexpr double LeafSpan=170.,SiblingGap=20.,RootGap=60.,LevelStep=280.;
 TMap<FName,double> Heights;
 TFunction<double(FName)> MeasureSubtree=[&](FName Id)
 {
  double Height=0.;
  for(FName Child:Children.FindRef(Id))Height+=MeasureSubtree(Child)+(Height>0.?SiblingGap:0.);
  Height=FMath::Max(LeafSpan,Height);Heights.Add(Id,Height);return Height;
 };
 double TotalHeight=0.;
 for(FName Root:Roots)TotalHeight+=MeasureSubtree(Root)+(TotalHeight>0.?RootGap:0.);
 CanvasSize=FVector2D(FMath::Max(1020.,320.+MaxDepth*LevelStep),FMath::Max(420.,TotalHeight+230.));
 const double FirstX=(CanvasSize.X-MaxDepth*LevelStep)*.5;
 TFunction<void(FName,double)> PlaceSubtree=[&](FName Id,double Top)
 {
  const auto Branch=Children.FindRef(Id);
  // Align each parent with its first child so the entry skill is visible on opening a page.
  const double Y=Top+45.;
  if(!Branch.IsEmpty())
  {
   double Cursor=Top;
   for(int32 I=0;I<Branch.Num();++I)
   {
    PlaceSubtree(Branch[I],Cursor);
    Cursor+=Heights.FindRef(Branch[I])+SiblingGap;
   }
  }
  LayoutPositions.Add(Id,FVector2D(FirstX+Depth.FindRef(Id)*LevelStep,Y));
  LabelWidths.Add(Id,220.);
 };
 double Cursor=140.;
 for(FName Root:Roots){PlaceSubtree(Root,Cursor);Cursor+=Heights.FindRef(Root)+RootGap;}
 Invalidate(EInvalidateWidgetReason::Layout);
}
FVector2D SMetaSkillMap::DisplayPosition(const FMetaSkillNode& Node) const
{
 if(const auto* Position=LayoutPositions.Find(Node.Id))return *Position;
 return FVector2D(CanvasSize.X*.5,145.);
}
double SMetaSkillMap::NodeLabelWidth(FName Id) const
{
 double Width=LabelWidths.FindRef(Id);
 const FVector2D Position=LayoutPositions.FindRef(Id);
 for(const auto& Other:LayoutPositions)if(Other.Key!=Id&&FMath::Abs(Other.Value.Y-Position.Y)<1.)
  Width=FMath::Min(Width,FMath::Abs(Other.Value.X-Position.X)-16.);
 return FMath::Max(80.,Width);
}
TArray<FVector2D> SMetaSkillMap::ConnectorPath(const FMetaSkillNode& Parent,const FMetaSkillNode& Child) const
{
 const FVector2D A=DisplayPosition(Parent),B=DisplayPosition(Child);
 // Ring-to-ring horizontal routes leave names and rank markers below each skill unobstructed.
 const double BusX=A.X+140.;
 return {{A.X+NodeRadius(Parent),A.Y},{BusX,A.Y},{BusX,B.Y},{B.X-NodeRadius(Child),B.Y}};
}
TArray<FString> SMetaSkillMap::NodeLabelLines(const FMetaSkillNode& Node,int32& FontSize,double& LineHeight) const
{
 const double Width=NodeLabelWidth(Node.Id);
 FontSize=18;
 FSlateFontInfo Font=LabelFont.FontObject?LabelFont:FCoreStyle::GetDefaultFontStyle("Regular",FontSize);Font.Size=FontSize;
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 TArray<FString> Words,Lines;Node.Name.ParseIntoArray(Words,TEXT(" "),true);FString Line;
 for(const FString& Word:Words)while(FontSize>10&&Measure->Measure(Word,Font).X>Width)Font.Size=--FontSize;
 for(const FString& Word:Words)
 {
  const FString Candidate=Line.IsEmpty()?Word:Line+TEXT(" ")+Word;
  if(!Line.IsEmpty()&&Measure->Measure(Candidate,Font).X>Width){Lines.Add(Line);Line=Word;}else Line=Candidate;
 }
 if(!Line.IsEmpty())Lines.Add(Line);
 LineHeight=FMath::CeilToDouble(Measure->Measure(TEXT("Ag"),Font).Y)+2.;
 return Lines;
}
FVector2D SMetaSkillMap::ComputeDesiredSize(float) const { return CanvasSize; }
float SMetaSkillMap::ViewScale(const FGeometry& G) const
{
 return FMath::Max(.01f,float(FMath::Min(1.,G.GetLocalSize().X/CanvasSize.X)));
}
FVector2D SMetaSkillMap::ToScreen(const FGeometry& G,FVector2D P) const { const float Scale=ViewScale(G);return FVector2D((G.GetLocalSize().X-CanvasSize.X*Scale)*.5,0)+P*Scale; }
FVector2D SMetaSkillMap::ToMap(const FGeometry& G,FVector2D P) const { const float Scale=ViewScale(G);return (P-FVector2D((G.GetLocalSize().X-CanvasSize.X*Scale)*.5,0))/Scale; }
void SMetaSkillMap::ResetView() { if(auto Scroll=ScrollTarget.Pin())Scroll->SetScrollOffset(0);SetHoveredNode(NAME_None);Refresh(); }
void SMetaSkillMap::Select(FName Id)
{
	if(Id.IsNone()||!IsNodeVisible(Id))return;
	SetHoveredNode(NAME_None);OnSelected.ExecuteIfBound(Id);Refresh();
}
void SMetaSkillMap::SetHoveredNode(FName Id)
{
	if(Hovered==Id)return;
	Hovered=Id;
	const auto* Node=MetaSkillTree::Find(Id);
	if(Node)
    {
        SetToolTip(SNew(SToolTip).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).TextMargin(2)
            [SNew(SSkillDescriptionFrame).Description(MetaSkillTree::Description(*Node))]);
    }
    else SetToolTip(nullptr);
	Refresh();
}
TSet<FName> SMetaSkillMap::GetHighlightedAncestors() const
{
	TSet<FName> Ancestors;
	TArray<FName> Queue{Selected.Get()};
	while(!Queue.IsEmpty())
	{
		const FName Id=Queue.Pop();
		if(Ancestors.Contains(Id))continue;
		if(const auto* Node=MetaSkillTree::Find(Id);Node&&IsNodeVisible(Id))
		{
			Ancestors.Add(Id);
			Queue.Append(Node->Prerequisites);
		}
	}
	return Ancestors;
}

FName SMetaSkillMap::HitNode(const FGeometry& G,FVector2D P) const
{
	const FVector2D MapPoint=ToMap(G,P);
	for(const auto& N:MetaSkillTree::Nodes()) if(IsNodeVisible(N.Id))
    {
        const FVector2D Offset=MapPoint-DisplayPosition(N);
        const bool bRankRow=FMath::Abs(Offset.Y-RankTop)<=RankRadius+5. &&
            FMath::Abs(Offset.X)<=(N.MaxRank-1)*RankSpacing*.5+RankRadius+5.;
        if(Offset.Size()<=NodeRadius(N)+6.f||bRankRow)return N.Id;
    }
	return NAME_None;
}
FReply SMetaSkillMap::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
	const FVector2D P=G.AbsoluteToLocal(E.GetScreenSpacePosition());
	SetHoveredNode(HitNode(G,P));
	return FReply::Handled();
}
void SMetaSkillMap::OnMouseLeave(const FPointerEvent& E)
{
	SetHoveredNode(NAME_None);SLeafWidget::OnMouseLeave(E);
}
FReply SMetaSkillMap::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
	const FVector2D P=G.AbsoluteToLocal(E.GetScreenSpacePosition());
	if(E.GetEffectingButton()==EKeys::LeftMouseButton)
	{
		const FName Hit=HitNode(G,P);
		if(!Hit.IsNone()){Select(Hit);return FReply::Handled().SetUserFocus(AsShared(),EFocusCause::Mouse);}
	}
    return FReply::Unhandled();
}
FReply SMetaSkillMap::OnMouseButtonUp(const FGeometry&,const FPointerEvent&) { return FReply::Unhandled(); }
FReply SMetaSkillMap::OnMouseWheel(const FGeometry&,const FPointerEvent&) { return FReply::Unhandled(); }
FReply SMetaSkillMap::OnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
	const FKey Key=E.GetKey();FVector2D Direction=FVector2D::ZeroVector;
	if(Key==EKeys::Enter || Key==EKeys::SpaceBar || Key==EKeys::Gamepad_FaceButton_Bottom)
	{
		if(auto Target=InspectTarget.Pin();Target.IsValid()&&Target->IsEnabled())return FReply::Handled().SetUserFocus(Target.ToSharedRef(),EFocusCause::Navigation);
		return FReply::Handled();
	}
	if(Key==EKeys::Left || Key==EKeys::Gamepad_DPad_Left)Direction.X=-1;
	if(Key==EKeys::Right || Key==EKeys::Gamepad_DPad_Right)Direction.X=1;
	if(Key==EKeys::Up || Key==EKeys::Gamepad_DPad_Up)Direction.Y=-1;
	if(Key==EKeys::Down || Key==EKeys::Gamepad_DPad_Down)Direction.Y=1;
	if(Key==EKeys::Home){ResetView();return FReply::Handled();}
	if(Direction.IsNearlyZero())return SLeafWidget::OnKeyDown(G,E);
	const auto* Current=MetaSkillTree::Find(Selected.Get());if(!Current)return FReply::Unhandled();
	FName Best;double BestScore=TNumericLimits<double>::Max();
	for(const auto& N:MetaSkillTree::Nodes())
	{
		if(!IsNodeVisible(N.Id))continue;
		const FVector2D Delta=DisplayPosition(N)-DisplayPosition(*Current);
		const double Forward=FVector2D::DotProduct(Delta,Direction);
		if(Forward<1)continue;
		const double Side=FMath::Abs(Delta.X*Direction.Y-Delta.Y*Direction.X);
		const double Score=Delta.Size()+Side*2.5;
		if(Score<BestScore){BestScore=Score;Best=N.Id;}
	}
	if(!Best.IsNone())
	{
		Select(Best);
        if(auto Scroll=ScrollTarget.Pin())
        {
            const double Y=ToScreen(G,DisplayPosition(*MetaSkillTree::Find(Best))).Y;
            const float Top=Scroll->GetScrollOffset(),Height=Scroll->GetCachedGeometry().GetLocalSize().Y;
            if(Y<Top+65)Scroll->SetScrollOffset(FMath::Max(0.f,float(Y-65)));
            else if(Y>Top+Height-125)Scroll->SetScrollOffset(FMath::Max(0.f,float(Y-Height+125)));
        }
		return FReply::Handled();
	}
	// At the edge of the tree, let Slate navigate to page tabs or actions.
	return FReply::Unhandled();
}

int32 SMetaSkillMap::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
	const auto* Meta=Progression.Get();const float Scale=ViewScale(G);
	auto Stroke=[&](const TArray<FVector2D>& Points,FLinearColor Color,float Width=1.f,int32 Depth=0)
	{
		TArray<FVector2D> Screen;Screen.Reserve(Points.Num());for(auto P:Points)Screen.Add(ToScreen(G,P));
		FSlateDrawElement::MakeLines(Out,Layer+Depth,G.ToPaintGeometry(),Screen,ESlateDrawEffect::None,Color,true,FMath::Max(.5f,Width*Scale));
	};
	auto Arc=[&](FVector2D Center,float R,float Start,float End,FLinearColor Color,float Width=1.f,int32 Depth=0)
	{
		TArray<FVector2D> Points;
		const int32 Segments=FMath::Max(6,FMath::CeilToInt(FMath::Abs(End-Start)/5));
		for(int32 I=0;I<=Segments;++I){const float A=FMath::DegreesToRadians(FMath::Lerp(Start,End,float(I)/Segments));Points.Add(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*R);}
		Stroke(Points,Color,Width,Depth);
	};
	auto Disc=[&](FVector2D P,float R,FLinearColor Color,int32 Depth)
	{
		const FSlateRoundedBoxBrush Brush(FLinearColor::White,R*Scale);
		FSlateDrawElement::MakeBox(Out,Layer+Depth,G.ToPaintGeometry(FVector2f(R*2*Scale,R*2*Scale),FSlateLayoutTransform(FVector2f(ToScreen(G,P)-FVector2D(R*Scale)))),&Brush,ESlateDrawEffect::None,Color);
	};
	auto Text=[&](FVector2D P,const FString& Value,int32 Size,FLinearColor Color,int32 Depth=5)
	{
		FSlateDrawElement::MakeText(Out,Layer+Depth,G.ToPaintGeometry(FVector2f(1,1),FSlateLayoutTransform(Scale,FVector2f(ToScreen(G,P)))),Value,[&](){FSlateFontInfo Font=LabelFont.FontObject?LabelFont:FCoreStyle::GetDefaultFontStyle("Regular",Size);Font.Size=Size;return Font;}(),ESlateDrawEffect::None,Color);
	};

 auto CenterText=[&](FVector2D P,const FString& Value,int32 Size,FLinearColor Color)
 {
  FSlateFontInfo Font=LabelFont.FontObject?LabelFont:FCoreStyle::GetDefaultFontStyle("Regular",Size);Font.Size=Size;
  const FVector2D Extent=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value,Font);
  Text(P-FVector2D(Extent.X*.5,0),Value,Size,Color);
 };
 // Keep the engraved frame around the horizontally progressing branches.
 for(int32 Corner=0;Corner<4;++Corner)
 {
  auto At=[&](float X,float Y){return FVector2D(Corner&1?CanvasSize.X-X:X,Corner&2?CanvasSize.Y-Y:Y);};
  Stroke({At(12,110),At(12,28),At(28,12),At(110,12)},Alpha(Ivory,.15f));
  Stroke({At(21,84),At(21,34),At(34,21),At(84,21)},Alpha(Gold,.2f));
  for(float D:{42.f,63.f,84.f})Stroke({At(D,9),At(D+7,16),At(D,23),At(D-7,16),At(D,9)},Alpha(Ivory,.12f));
 }
 Stroke({{CanvasSize.X*.5-90,101},{CanvasSize.X*.5+90,101}},Alpha(Gold,.25f));
 const TSet<FName> Ancestors=GetHighlightedAncestors();
 // Paint highlighted paths last so a shared branch bus cannot hide the selected prerequisite route.
 for(int32 Pass=0;Pass<2;++Pass)
  for(const auto& N:MetaSkillTree::Nodes())
  {
   if(!IsNodeVisible(N.Id))continue;
   for(FName ParentId:N.Prerequisites)
   {
    const auto* Parent=MetaSkillTree::Find(ParentId);if(!Parent||!IsNodeVisible(ParentId))continue;
    const bool bActive=Ancestors.Contains(N.Id)&&Ancestors.Contains(ParentId);
    if((Pass==1)!=bActive)continue;
    const bool bLearned=Meta&&Meta->GetSkillRank(N.Id)>0&&Meta->GetSkillRank(ParentId)>0;
    const auto Color=bActive?Alpha(Gold,1.f):Alpha(PathColor(N.Branch),bLearned?.88f:.3f);
    Stroke(ConnectorPath(*Parent,N),Color,bActive?2.6f:1.5f,1);
   }
  }

	for(const auto& N:MetaSkillTree::Nodes())
	{
		if(!IsNodeVisible(N.Id))continue;
		const FVector2D P=DisplayPosition(N);const int32 Rank=Meta?Meta->GetSkillRank(N.Id):0;
		const bool bSelected=N.Id==Selected.Get(),bHovered=N.Id==Hovered,bOpen=OpenPath(N,Meta);
		const float Radius=NodeRadius(N);
		const FLinearColor Accent=PathColor(N.Branch);
		if(bSelected||bHovered)
		{
			Arc(P,Radius+10,0,360,Alpha(Accent,.12f),7.f,2);
			Arc(P,Radius+8,0,360,bSelected?Gold:Alpha(Ivory,.5f),bSelected?2.f:1.f,3);
		}
		if(N.MaxRank==1)Arc(P,Radius+5,0,360,Alpha(Accent,.23f),1.f,2);
		Disc(P,Radius,Rank>0?Alpha(Accent*.25f,1.f):FLinearColor(.013f,.022f,.029f),3);
		Arc(P,Radius,0,360,Rank>0?Accent:bOpen?Alpha(Accent,.95f):Alpha(Accent,.43f),bOpen?1.8f:1.2f,4);
		const FLinearColor Ink=Rank>0||bOpen?Ivory:FLinearColor(.43f,.48f,.5f);
		auto Glyph=[&](std::initializer_list<FVector2D> Points)
		{
			TArray<FVector2D> Path;for(auto V:Points)Path.Add(P+V*19.);Stroke(Path,Ink,1.65f,5);
		};
		const FString Effect=N.Effect.ToString();
		if(Effect==TEXT("Health"))
		{
			Glyph({{0,-1},{.8,-.65},{.65,.45},{0,1},{-.65,.45},{-.8,-.65},{0,-1}});
			Glyph({{-.35,-.1},{.35,-.1}});Glyph({{0,-.45},{0,.35}});
		}
		else if(Effect==TEXT("Bleed"))
		{
			Glyph({{0,-1.1},{.65,-.1},{.72,.45},{.4,.85},{0,1},{-.4,.85},{-.72,.45},{-.65,-.1},{0,-1.1}});
			Glyph({{-.35,.2},{-.32,.5},{-.1,.65}});
		}
		else if(Effect==TEXT("Poison"))
		{
			Glyph({{.85,-1},{.75,.4},{.2,.9},{-.5,.75},{-.8,.2},{-.6,-.4},{.85,-1},{-.75,.95}});
			Glyph({{-.3,.4},{-.55,-.05}});Glyph({{.15,-.05},{.5,.05}});
		}
		else if(Effect.Contains(TEXT("Damage")))
		{
			Glyph({{.8,-1},{.65,-.45},{-.45,.7},{-.7,.45},{.45,-.65},{.8,-1}});
			Glyph({{-.8,.2},{-.2,.8}});Glyph({{-.55,.55},{-.95,.95}});
			if(Effect==TEXT("Damage"))Glyph({{-.9,-.95},{-.55,-.85},{.8,.6},{.55,.85},{-.85,-.55},{-.9,-.95}});
		}
		else if(Effect==TEXT("Swap")||Effect==TEXT("Reroll"))
		{
			Arc(P,12,-145,10,Ink,1.7f,5);Arc(P,12,35,190,Ink,1.7f,5);
			Glyph({{.6,-.3},{.87,.12},{1.15,-.25}});Glyph({{-.6,.3},{-.87,-.12},{-1.15,.25}});
		}
		else if(Effect==TEXT("Pickup"))
		{
			Glyph({{0,-.85},{.45,0},{0,.85},{-.45,0},{0,-.85}});
			Glyph({{-.75,-.65},{-1,-.4},{-1,.4},{-.75,.65}});Glyph({{.75,-.65},{1,-.4},{1,.4},{.75,.65}});
		}
		else if(Effect==TEXT("SteelArea")||Effect==TEXT("FangRange"))
		{
			Arc(P,16,205,335,Ink,1.7f,5);Arc(P,10,205,335,Ink,1.4f,5);
			Glyph({{-.6,.8},{.65,-.5},{.5,-.2},{-.35,.8},{-.6,.8}});
		}
		else if(Effect==TEXT("Projectile")||Effect==TEXT("Pierce")||Effect==TEXT("ShadowFlight"))
		{
			Glyph({{.95,-.8},{.5,.1},{-.55,.65},{-.15,-.4},{.95,-.8},{-.8,.8}});
			if(Effect==TEXT("Projectile")){Glyph({{-.9,-.7},{-.55,-1.15},{-.35,-.7},{-.55,-.25},{-.9,-.7}});}
			else {Glyph({{-.9,-.2},{-.5,-.6}});Glyph({{.15,.8},{.55,.4}});}
		}
		else if(Effect==TEXT("Banish"))
  {
   Glyph({{-.7,-.9},{.7,-.9},{.7,.9},{-.7,.9},{-.7,-.9}});
   Glyph({{-.4,-.5},{.4,.5}});Glyph({{.4,-.5},{-.4,.5}});
  }
  else if(Effect==TEXT("ShurikenSize")||Effect==TEXT("ShurikenTempo"))
  {
   Glyph({{0,-1},{.25,-.25},{1,0},{.25,.25},{0,1},{-.25,.25},{-1,0},{-.25,-.25},{0,-1}});
   Arc(P,3,0,360,Ink,1.f,5);
  }
  else if(Effect==TEXT("IaijutsuCharge"))
  {
   Arc(P,11,0,360,Ink,1.3f,5);Glyph({{0,-.6},{0,0},{.5,.3}});
  }
  else if(Effect==TEXT("Dash")||Effect==TEXT("Move"))
		{
			Glyph({{-.7,-.75},{0,-.3},{-.5,.15},{.35,.15},{.95,.75},{-.15,.75},{-.5,.4}});
			Glyph({{-.95,-.15},{-.45,-.15}});Glyph({{-.95,.15},{-.75,.15}});
		}
		else // Basic attack tempo: three cutting strokes.
		{
			Glyph({{-.95,-.7},{.65,-.7},{.95,-.4},{.65,-.1},{-.4,-.1}});
			Glyph({{-.9,.2},{.45,.2},{.7,.45},{.45,.7},{-.6,.7}});
		}
		for(int32 I=0;I<N.MaxRank;++I)
		{
            const FVector2D Dot=P+FVector2D((I-(N.MaxRank-1)*.5)*RankSpacing,RankTop);
            const bool bLearned=I<Rank;
            Disc(Dot,RankRadius,bLearned?Gold:FLinearColor(.012f,.025f,.03f),6);
            Arc(Dot,RankRadius,0,360,bLearned?Ivory:Alpha(Ivory,.6f),bLearned?1.5f:1.8f,6);
		}
		{
            int32 LabelSize;double LineHeight;
            const auto Lines=NodeLabelLines(N,LabelSize,LineHeight);
			for(int32 I=0;I<Lines.Num();++I)CenterText(P+FVector2D(0,LabelTop+I*LineHeight),Lines[I],LabelSize,Rank>0||bOpen?Ivory:Alpha(Ivory,.6f));
		}
	}
 CenterText({CanvasSize.X*.5,65},FilterTitle.IsEmpty()?TEXT("SKILL TREE"):FilterTitle.ToUpper(),21,Gold);
 if(bStatsLayout)
 {
  // Small, fixed legend replaces column headings now that branches run across rows.
  for(int32 Group=0;Group<3;++Group)
  {
   const double X=CanvasSize.X*.5+(Group-1)*145.;
   Disc({X-46.,122.},3.f,PathColor(Group==0?1:Group==2?2:0),5);
   Text({X-36.,111.},Group==0?TEXT("SAMURAI"):Group==1?TEXT("SHARED"):TEXT("NINJA"),12,Alpha(Ivory,.8f));
  }
 }
 CenterText({CanvasSize.X*.5,CanvasSize.Y-35},TEXT("A permanent legacy, one skill at a time"),14,Alpha(Ivory,.5f));
 return Layer+7;
}
