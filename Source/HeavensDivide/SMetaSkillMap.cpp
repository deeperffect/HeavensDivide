#include "SMetaSkillMap.h"
#include "MetaSkillTree.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

namespace
{
 const FVector2D MapCenter(510,425);
 const FLinearColor Ivory(.90f,.88f,.80f);
 const FLinearColor Gold(.68f,.46f,.20f);
 FLinearColor PathColor(int32 Branch)
 {
  if(Branch==1)return FLinearColor(.85f,.24f,.18f);
  if(Branch==2)return FLinearColor(.37f,.76f,.35f);
  return FLinearColor(.30f,.52f,.92f);
 }
 float NodeRadius(const FMetaSkillNode& N){return N.MaxRank==1?22.f:18.f;}
 FVector2D Polar(float Radius,float Degrees)
 {
  const float A=FMath::DegreesToRadians(Degrees);
  return MapCenter+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius;
 }
 bool OpenPath(const FMetaSkillNode& N,const USynergyMetaProgressionSubsystem* Meta)
 {
  if(!Meta)return false;
  for(FName Parent:N.Prerequisites)if(Meta->GetSkillRank(Parent)==0)return false;
  return true;
 }
 FLinearColor Alpha(FLinearColor C,float A){C.A=A;return C;}
}
FVector2D SMetaSkillMap::NodePosition(const FMetaSkillNode& N)
{
 if(N.MapPosition.X>=0 && N.MapPosition.Y>=0)return N.MapPosition;
 // Keep custom pre-redesign catalogs inspectable until their positions are authored.
 const float Sectors[]={270,35,145,270};
 return Polar(135+N.Tier*70,Sectors[FMath::Clamp(N.Branch,0,3)]+(N.Lane==0?-1:1)*18.f);
}

void SMetaSkillMap::Construct(const FArguments& Args)
{
	Progression=Args._Progression;Selected=Args._Selected;OnSelected=Args._OnSelected;
	SetClipping(EWidgetClipping::ClipToBounds);
}
void SMetaSkillMap::SetVisibleNodes(const TArray<FName>& Ids, const FString& Title)
{
	bFiltered=true; VisibleNodes.Reset(); FilteredPositions.Reset(); FilterTitle=Title;
	for(FName Id:Ids)if(MetaSkillTree::Find(Id))VisibleNodes.Add(Id);
	// Lay out only this page's dependencies. Other pages never leave orphan links or hit targets.
	TMap<FName,int32> Depth;
	for(FName Id:Ids)Depth.Add(Id,0);
	for(int32 Pass=0;Pass<Ids.Num();++Pass)
		for(FName Id:Ids)if(const auto* N=MetaSkillTree::Find(Id))
			for(FName Parent:N->Prerequisites)if(VisibleNodes.Contains(Parent))
				Depth[Id]=FMath::Max(Depth[Id],FMath::Min(Ids.Num()-1,Depth.FindRef(Parent)+1));
	int32 MaxDepth=0;
	for(const auto& Pair:Depth)MaxDepth=FMath::Max(MaxDepth,Pair.Value);
	for(int32 Level=0;Level<=MaxDepth;++Level)
	{
		TArray<FName> Row;
		for(FName Id:Ids)if(VisibleNodes.Contains(Id)&&Depth.FindRef(Id)==Level)Row.Add(Id);
		for(int32 I=0;I<Row.Num();++I)
			FilteredPositions.Add(Row[I],FVector2D(510+(I-(Row.Num()-1)*.5)*FMath::Min(195.,720./FMath::Max(1,Row.Num()-1)),
				MaxDepth?210.+Level*430./MaxDepth:425.));
	}
	ResetView();
}
FVector2D SMetaSkillMap::DisplayPosition(const FMetaSkillNode& Node) const
{
	if(const auto* Position=FilteredPositions.Find(Node.Id))return *Position;
	return NodePosition(Node);
}
FVector2D SMetaSkillMap::ComputeDesiredSize(float) const { return FVector2D(1020,850); }
float SMetaSkillMap::ViewScale(const FGeometry& G) const
{
	return FMath::Max(.01f,static_cast<float>(FMath::Min(G.GetLocalSize().X/1020.,G.GetLocalSize().Y/850.)*Zoom));
}
FVector2D SMetaSkillMap::ToScreen(const FGeometry& G,FVector2D P) const { return G.GetLocalSize()*.5+(P-MapCenter+Pan)*ViewScale(G); }
FVector2D SMetaSkillMap::ToMap(const FGeometry& G,FVector2D P) const { return (P-G.GetLocalSize()*.5)/ViewScale(G)+MapCenter-Pan; }
void SMetaSkillMap::ResetView() { Zoom=1;Pan=FVector2D::ZeroVector;SetHoveredNode(NAME_None);Refresh(); }
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
	SetToolTipText(Node?FText::FromString(Node->Name+TEXT("\n")+MetaSkillTree::Scope(*Node)+TEXT("\n")+MetaSkillTree::Description(*Node)):FText::GetEmpty());
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
	for(const auto& N:MetaSkillTree::Nodes()) if(IsNodeVisible(N.Id)&&FVector2D::Distance(MapPoint,DisplayPosition(N))<=(NodeRadius(N)+6.f))return N.Id;
	return NAME_None;
}
FReply SMetaSkillMap::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
	const FVector2D P=G.AbsoluteToLocal(E.GetScreenSpacePosition());
	if(bDragging && HasMouseCapture())
	{
		Pan+=(P-LastDrag)/ViewScale(G);Pan.X=FMath::Clamp(Pan.X,-450.,450.);Pan.Y=FMath::Clamp(Pan.Y,-350.,350.);LastDrag=P;Refresh();
		return FReply::Handled();
	}
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
	if(E.GetEffectingButton()==EKeys::LeftMouseButton || E.GetEffectingButton()==EKeys::RightMouseButton)
	{
		SetHoveredNode(NAME_None);bDragging=true;LastDrag=P;return FReply::Handled().CaptureMouse(AsShared()).SetUserFocus(AsShared(),EFocusCause::Mouse);
	}
	return FReply::Unhandled();
}
FReply SMetaSkillMap::OnMouseButtonUp(const FGeometry&,const FPointerEvent& E)
{
	if(bDragging && (E.GetEffectingButton()==EKeys::LeftMouseButton || E.GetEffectingButton()==EKeys::RightMouseButton))
	{
		bDragging=false;return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}
FReply SMetaSkillMap::OnMouseWheel(const FGeometry& G,const FPointerEvent& E)
{
	const FVector2D P=G.AbsoluteToLocal(E.GetScreenSpacePosition());
	const FVector2D Before=ToMap(G,P);
	Zoom=FMath::Clamp(Zoom+E.GetWheelDelta()*.12f,.75f,1.8f);
	Pan+=ToMap(G,P)-Before;
	Pan.X=FMath::Clamp(Pan.X,-450.,450.);Pan.Y=FMath::Clamp(Pan.Y,-350.,350.);SetHoveredNode(HitNode(G,P));Refresh();
	return FReply::Handled();
}
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
		const FVector2D P=ToScreen(G,DisplayPosition(*MetaSkillTree::Find(Best)));
		if(P.X<45 || P.Y<45 || P.X>G.GetLocalSize().X-45 || P.Y>G.GetLocalSize().Y-45)Pan=MapCenter-DisplayPosition(*MetaSkillTree::Find(Best));
		return FReply::Handled();
	}
	// At the edge of the constellation, let Slate navigate to page tabs or actions.
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
 // Engraved circles and a restrained frame echo the reference's three-way atlas.
 for(float R:{406.f,414.f,422.f})Arc(MapCenter,R,0,360,Alpha(Ivory,.055f));
 for(FVector2D C:{FVector2D(510,205),FVector2D(280,595),FVector2D(740,595)})
  Arc(C,263,0,360,Alpha(Ivory,.045f));
 for(int32 I=0;I<70;++I)Disc(Polar(105+(I*47)%300,I*137.508f),I%9==0?1.2f:.65f,Alpha(Ivory,.1f),0);
 for(int32 Corner=0;Corner<4;++Corner)
 {
  auto At=[&](float X,float Y){return FVector2D(Corner&1?1020-X:X,Corner&2?850-Y:Y);};
  Stroke({At(12,110),At(12,28),At(28,12),At(110,12)},Alpha(Ivory,.15f));
  Stroke({At(21,84),At(21,34),At(34,21),At(84,21)},Alpha(Gold,.2f));
  for(float D:{42.f,63.f,84.f})Stroke({At(D,9),At(D+7,16),At(D,23),At(D-7,16),At(D,9)},Alpha(Ivory,.12f));
 }
 const FVector2D Seals[]={FVector2D(510,35),FVector2D(930,710),FVector2D(90,710)};
 for(int32 Branch=0;!bFiltered&&Branch<3;++Branch)
 {
  const FVector2D Axis=Seals[Branch]-MapCenter;
  const FVector2D Normal=FVector2D(-Axis.Y,Axis.X).GetSafeNormal();
  const auto C=PathColor(Branch);
  Stroke({MapCenter,Seals[Branch]},Alpha(C,.035f),15.f,0);
  for(float Offset:{-8.f,0.f,8.f})Stroke({MapCenter+Normal*Offset*.2,Seals[Branch]+Normal*Offset},Alpha(C,.25f),Offset==0?2.f:1.f,0);
 }
 const TSet<FName> Ancestors=GetHighlightedAncestors();
 for(const auto& N:MetaSkillTree::Nodes())
 {
  if(!IsNodeVisible(N.Id))continue;
  const FVector2D B=DisplayPosition(N);
  for(FName ParentId:N.Prerequisites)
  {
   const auto* Parent=MetaSkillTree::Find(ParentId);if(!Parent||!IsNodeVisible(ParentId))continue;
   const bool bActive=Ancestors.Contains(N.Id)&&Ancestors.Contains(ParentId);
   const bool bLearned=Meta&&Meta->GetSkillRank(N.Id)>0&&Meta->GetSkillRank(ParentId)>0;
   const auto Color=bActive?Alpha(Ivory,.95f):Alpha(PathColor(N.Branch),bLearned?.88f:.3f);
   const FVector2D A=DisplayPosition(*Parent);
   // Both character trunks meet the shared starting skill through the central seal.
   if(!bFiltered&&Parent->Branch==0 && (N.Branch==1 || N.Branch==2))
    Stroke({A,MapCenter,B},Color,bActive?2.6f:1.5f,1);
   else Stroke({A,B},Color,bActive?2.6f:1.5f,1);
  }
  if(!bFiltered&&N.Prerequisites.IsEmpty())Stroke({MapCenter,B},Alpha(PathColor(N.Branch),.7f),2.f,1);
 }
 // Large end crests are landmarks, not purchase nodes.
 for(int32 Branch=0;!bFiltered&&Branch<3;++Branch)
 {
  const auto P=Seals[Branch];const auto C=PathColor(Branch);
  for(int32 I=0;I<4;++I)Arc(P,40+I*5,0,360,Alpha(C,.065f-I*.012f),7.f,1);
  Disc(P,37,FLinearColor(.012f,.018f,.019f),2);
  Arc(P,38,0,360,Alpha(C,.9f),2.f,3);Arc(P,32,0,360,Alpha(C,.3f),1.f,3);
  if(Branch==1)
  {
   for(float Sign:{-1.f,1.f})
   {
    Stroke({P+FVector2D(-19*Sign,20),P+FVector2D(16*Sign,-18),P+FVector2D(21*Sign,-22),P+FVector2D(20*Sign,-14),P+FVector2D(-15*Sign,21)},Ivory,1.6f,4);
    Stroke({P+FVector2D(-22*Sign,6),P+FVector2D(-6*Sign,22)},C,2.f,4);
   }
  }
  else if(Branch==2)
  {
   for(int32 I=0;I<4;++I)
   {
    const double A=I*PI*.5;
    auto V=[&](double X,double Y){return P+FVector2D(X*FMath::Cos(A)-Y*FMath::Sin(A),X*FMath::Sin(A)+Y*FMath::Cos(A));};
    Stroke({V(0,-24),V(5,-7),V(14,0),V(0,-2),V(0,-24)},Ivory,1.4f,4);
   }
   Arc(P,5,0,360,C,1.5f,4);
  }
  else
  {
   Arc(P,21,-90,90,Ivory,2.f,4);Arc(P,21,90,270,C,2.f,4);
   Arc(P+FVector2D(0,-10),10,-90,90,Ivory,1.5f,4);Arc(P+FVector2D(0,10),10,90,270,C,1.5f,4);
   Disc(P+FVector2D(0,-10),2.5f,C,4);Disc(P+FVector2D(0,10),2.5f,Ivory,4);
  }
 }
 // Compact central knot leaves space for the branches to read as a single tree.
 if(!bFiltered)
 {
 Disc(MapCenter,29,FLinearColor(.014f,.022f,.025f),2);
 Arc(MapCenter,29,0,360,Alpha(Ivory,.6f),1.5f,3);
 for(float R:{18.f,23.f})Arc(MapCenter,R,0,360,Alpha(Gold,.65f),1.f,3);
 for(int32 I=0;I<3;++I)
 {
  const float A=FMath::DegreesToRadians(I*120.f-90);
  Arc(MapCenter+FVector2D(FMath::Cos(A),FMath::Sin(A))*7.f,12,0,360,Alpha(Ivory,.7f),1.f,4);
 }
 CenterText(MapCenter+FVector2D(0,36),TEXT("TWIN SOUL"),10,Alpha(Ivory,.65f));
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
			Arc(P,Radius+8,0,360,Alpha(Ivory,bSelected?.95f:.5f),bSelected?2.f:1.f,3);
		}
		if(N.MaxRank==1)Arc(P,Radius+5,0,360,Alpha(Accent,.23f),1.f,2);
		Disc(P,Radius,Rank>0?Alpha(Accent*.25f,1.f):FLinearColor(.013f,.022f,.029f),3);
		Arc(P,Radius,0,360,Rank>0?Accent:bOpen?Alpha(Accent,.95f):Alpha(Accent,.43f),bOpen?1.8f:1.2f,4);
		const FLinearColor Ink=Rank>0||bOpen?Ivory:FLinearColor(.43f,.48f,.5f);
		auto Glyph=[&](std::initializer_list<FVector2D> Points)
		{
			TArray<FVector2D> Path;for(auto V:Points)Path.Add(P+V*10.5);Stroke(Path,Ink,1.65f,5);
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
			const float A=FMath::DegreesToRadians(N.MaxRank==1?90.f:65.f+I*25.f);
			Disc(P+FVector2D(FMath::Cos(A),FMath::Sin(A))*(Radius+5),2.1f,I<Rank?Ivory:FLinearColor(.18f,.18f,.16f),6);
		}
		if(bFiltered)
		{
			// Size labels to the actual row spacing, so wide tiers remain readable without overlapping names.
			double LabelWidth=240.;
			for(const auto& Other:MetaSkillTree::Nodes())if(Other.Id!=N.Id&&IsNodeVisible(Other.Id))
			{
				const FVector2D Delta=DisplayPosition(Other)-P;
				if(FMath::Abs(Delta.Y)<1.)LabelWidth=FMath::Min(LabelWidth,FMath::Abs(Delta.X)-14.);
			}
			int32 LabelSize=LabelWidth<145.?14:17;
			FSlateFontInfo Font=LabelFont.FontObject?LabelFont:FCoreStyle::GetDefaultFontStyle("Regular",LabelSize);Font.Size=LabelSize;
			const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			TArray<FString> Words,Lines;N.Name.ParseIntoArray(Words,TEXT(" "),true);FString Line;
			for(const FString& Word:Words)
				while(LabelSize>10&&Measure->Measure(Word,Font).X>LabelWidth)Font.Size=--LabelSize;
			for(const FString& Word:Words)
			{
				const FString Candidate=Line.IsEmpty()?Word:Line+TEXT(" ")+Word;
				if(!Line.IsEmpty()&&Measure->Measure(Candidate,Font).X>LabelWidth){Lines.Add(Line);Line=Word;}
				else Line=Candidate;
			}
			if(!Line.IsEmpty())Lines.Add(Line);
			for(int32 I=0;I<Lines.Num();++I)CenterText(P+FVector2D(0,36+I*(LabelSize+4)),Lines[I],LabelSize,Rank>0||bOpen?Ivory:Alpha(Ivory,.6f));
		}
	}
 if(bFiltered)
 {
  CenterText({510,90},FilterTitle.ToUpper(),21,Gold);
  CenterText({510,744},TEXT("A permanent legacy, one skill at a time"),14,Alpha(Ivory,.5f));
  return Layer+7;
 }
 CenterText({660,54},TEXT("SHARED / BOND"),14,PathColor(0));
 CenterText({90,766},TEXT("WAY OF SHADOW"),13,PathColor(2));
 CenterText({90,788},TEXT("NINJA"),10,Alpha(Ivory,.6f));
 CenterText({930,766},TEXT("WAY OF STEEL"),13,PathColor(1));
 CenterText({930,788},TEXT("SAMURAI"),10,Alpha(Ivory,.6f));
 CenterText({204,315},TEXT("RETURNING FANG"),11,Alpha(PathColor(2),.8f));
 CenterText({126,426},TEXT("BARRAGE"),11,Alpha(PathColor(2),.8f));
 CenterText({319,761},TEXT("GREAT SHURIKEN"),11,Alpha(PathColor(2),.8f));
 CenterText({829,315},TEXT("BLOOD"),11,Alpha(PathColor(1),.8f));
 CenterText({909,426},TEXT("IAIJUTSU"),11,Alpha(PathColor(1),.8f));
 CenterText({694,761},TEXT("CRESCENT"),11,Alpha(PathColor(1),.8f));
 return Layer+7;
}
