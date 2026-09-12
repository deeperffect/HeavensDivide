#include "SMetaSkillMap.h"
#include "MetaSkillTree.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

namespace
{
	const FVector2D MapCenter(500, 390);
	const FLinearColor Ivory(.93f, .93f, .87f);
	const FLinearColor Gold(.74f, .70f, .59f);
	FLinearColor PathColor(int32 Branch)
	{
		const FLinearColor Colors[] = {Gold, FLinearColor(.7f,.32f,.23f), FLinearColor(.42f,.47f,.74f), FLinearColor(.27f,.61f,.56f)};
		return Colors[FMath::Clamp(Branch,0,3)];
	}
	float NodeAngle(const FMetaSkillNode& N)
	{
		const float Sectors[] = {135,225,315,45};
		return Sectors[N.Branch] + (N.Lane == 0 ? -1 : 1) * (13.f + N.Tier * 1.5f);
	}
	FVector2D Polar(float Radius, float Degrees)
	{
		const float A = FMath::DegreesToRadians(Degrees);
		return MapCenter + FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius;
	}
	FVector2D Position(const FMetaSkillNode& N) { return Polar(135+N.Tier*70,NodeAngle(N)); }
	bool OpenPath(const FMetaSkillNode& N, const USynergyMetaProgressionSubsystem* Meta)
	{
		if (!Meta) return false;
		for(FName Parent:N.Prerequisites) if(Meta->GetSkillRank(Parent)==0) return false;
		return true;
	}
	FLinearColor Alpha(FLinearColor C,float A) { C.A=A;return C; }
}

void SMetaSkillMap::Construct(const FArguments& Args)
{
	Progression=Args._Progression;Selected=Args._Selected;OnSelected=Args._OnSelected;
	SetClipping(EWidgetClipping::ClipToBounds);
}
FVector2D SMetaSkillMap::ComputeDesiredSize(float) const { return FVector2D(1000,780); }
float SMetaSkillMap::ViewScale(const FGeometry& G) const
{
	return FMath::Max(.01f,static_cast<float>(FMath::Min(G.GetLocalSize().X/1000.,G.GetLocalSize().Y/780.)*Zoom));
}
FVector2D SMetaSkillMap::ToScreen(const FGeometry& G,FVector2D P) const { return G.GetLocalSize()*.5+(P-MapCenter+Pan)*ViewScale(G); }
FVector2D SMetaSkillMap::ToMap(const FGeometry& G,FVector2D P) const { return (P-G.GetLocalSize()*.5)/ViewScale(G)+MapCenter-Pan; }
void SMetaSkillMap::ResetView() { Zoom=1;Pan=FVector2D::ZeroVector;SetHoveredNode(NAME_None);Refresh(); }
void SMetaSkillMap::Select(FName Id)
{
	if(Id.IsNone())return;
	SetHoveredNode(NAME_None);OnSelected.ExecuteIfBound(Id);Refresh();
}
void SMetaSkillMap::SetHoveredNode(FName Id)
{
	if(Hovered==Id)return;
	Hovered=Id;
	const auto* Node=MetaSkillTree::Find(Id);
	SetToolTipText(Node?FText::FromString(Node->Name+TEXT("\n")+Node->Description):FText::GetEmpty());
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
		if(const auto* Node=MetaSkillTree::Find(Id))
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
	for(const auto& N:MetaSkillTree::Nodes()) if(FVector2D::Distance(MapPoint,Position(N))<=(N.MaxRank==1?32:29))return N.Id;
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
		const FVector2D Delta=Position(N)-Position(*Current);
		const double Forward=FVector2D::DotProduct(Delta,Direction);
		if(Forward<1)continue;
		const double Side=FMath::Abs(Delta.X*Direction.Y-Delta.Y*Direction.X);
		const double Score=Delta.Size()+Side*2.5;
		if(Score<BestScore){BestScore=Score;Best=N.Id;}
	}
	if(!Best.IsNone())
	{
		Select(Best);
		const FVector2D P=ToScreen(G,Position(*MetaSkillTree::Find(Best)));
		if(P.X<45 || P.Y<45 || P.X>G.GetLocalSize().X-45 || P.Y>G.GetLocalSize().Y-45)Pan=MapCenter-Position(*MetaSkillTree::Find(Best));
	}
	return FReply::Handled();
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
	// Quiet orbital construction lines and deterministic flecks evoke a celestial chart.
	for(float R:{84.f,165.f,245.f,325.f,382.f})Arc(MapCenter,R,0,360,Alpha(Ivory,.045f));
	for(int32 I=0;I<72;++I)
	{
		const float A=I*137.508f;const float R=90+(I*47)%285;
		Disc(Polar(R,A),I%7==0?1.4f:.7f,Alpha(Ivory,I%7==0?.16f:.055f),0);
	}
	for(int32 I=0;I<4;++I)Stroke({Polar(92,I*90),Polar(375,I*90)},Alpha(Ivory,.05f));
	// The inspector and strong node ring both describe Selected. Hover only previews
	// a node's tooltip/ring; it must never redirect the selected prerequisite path.
	const TSet<FName> Ancestors=GetHighlightedAncestors();
	for(const auto& N:MetaSkillTree::Nodes())
	{
		const FVector2D B=Position(N);
		for(FName ParentId:N.Prerequisites)
		{
			const auto* Parent=MetaSkillTree::Find(ParentId);if(!Parent)continue;
			const bool bActive=Ancestors.Contains(N.Id)&&Ancestors.Contains(ParentId);
			const bool bLearned=Meta&&Meta->GetSkillRank(N.Id)>0&&Meta->GetSkillRank(ParentId)>0;
			FLinearColor Color=bActive?Alpha(Ivory,.85f):bLearned?Alpha(Ivory,.8f):FLinearColor(.29f,.29f,.26f);
			const FVector2D A=Position(*Parent);
			if(Parent->Branch==N.Branch)
			{
				Stroke({A,B},Color,bActive?3.f:2.2f,1);
			}
			else
			{
				// Cross-path requirements follow curved bridges. Distant capstone bridges
				// are revealed only for the focused ancestry to keep the map legible.
				if(N.Tier==3&&!bActive)continue;
				TArray<FVector2D> Points;
				const FVector2D ControlA=MapCenter+(A-MapCenter)*.64;
				const FVector2D ControlB=MapCenter+(B-MapCenter)*.64;
				for(int32 I=0;I<=36;++I)
				{
					const double T=I/36.,U=1-T;
					Points.Add(A*U*U*U+ControlA*3*U*U*T+ControlB*3*U*T*T+B*T*T*T);
				}
				Stroke(Points,Alpha(Color,bActive?.8f:.32f),bActive?2.2f:1.5f,1);
			}
		}
		if(N.Prerequisites.IsEmpty())Stroke({Polar(55,NodeAngle(N)),B},Alpha(Gold,.4f),1.5f,1);
	}
	// Central twin-soul seal, drawn from two opposing crescents.
	Disc(MapCenter,57,FLinearColor(.023f,.023f,.022f),2);
	Arc(MapCenter,57,0,360,Alpha(Gold,.45f),1.f,3);
	Arc(MapCenter,49,28,165,Alpha(Ivory,.65f),1.8f,3);
	Arc(MapCenter,49,208,345,Alpha(Ivory,.65f),1.8f,3);
	Arc(MapCenter+FVector2D(0,-15),15,-90,90,Ivory,2.f,3);
	Arc(MapCenter+FVector2D(0,15),15,90,270,Ivory,2.f,3);
	Arc(MapCenter,30,-90,90,Gold,2.f,3);Arc(MapCenter,30,90,270,Ivory,2.f,3);
	Disc(MapCenter+FVector2D(0,-15),3,Gold,4);Disc(MapCenter+FVector2D(0,15),3,Ivory,4);
	Text(MapCenter+FVector2D(-45,66),TEXT("T W I N  S O U L"),9,Alpha(Ivory,.55f));

	for(const auto& N:MetaSkillTree::Nodes())
	{
		const FVector2D P=Position(N);const int32 Rank=Meta?Meta->GetSkillRank(N.Id):0;
		const bool bSelected=N.Id==Selected.Get(),bHovered=N.Id==Hovered,bOpen=OpenPath(N,Meta);
		const float Radius=N.MaxRank==1?29.f:25.f;
		const FLinearColor Accent=PathColor(N.Branch);
		if(bSelected||bHovered)
		{
			Arc(P,Radius+10,0,360,Alpha(Accent,.12f),7.f,2);
			Arc(P,Radius+8,0,360,Alpha(Ivory,bSelected?.95f:.5f),bSelected?2.f:1.f,3);
		}
		if(N.MaxRank==1)Arc(P,Radius+5,0,360,Alpha(Accent,.23f),1.f,2);
		Disc(P,Radius,Rank>0?Ivory:FLinearColor(.06f,.06f,.057f),3);
		Arc(P,Radius,0,360,Rank>0?Ivory:bOpen?Alpha(Accent,.9f):FLinearColor(.22f,.22f,.20f),bOpen?1.8f:1.2f,4);
		const FLinearColor Ink=Rank>0?FLinearColor(.04f,.04f,.036f):bOpen?Ivory:FLinearColor(.38f,.38f,.34f);
		auto Glyph=[&](std::initializer_list<FVector2D> Points)
		{
			TArray<FVector2D> Path;for(auto V:Points)Path.Add(P+V*14.);Stroke(Path,Ink,1.65f,5);
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
		else if(Effect.Contains(TEXT("Damage"))||Effect==TEXT("Reaction"))
		{
			Glyph({{.8,-1},{.65,-.45},{-.45,.7},{-.7,.45},{.45,-.65},{.8,-1}});
			Glyph({{-.8,.2},{-.2,.8}});Glyph({{-.55,.55},{-.95,.95}});
			if(Effect==TEXT("Damage")||Effect==TEXT("Reaction"))Glyph({{-.9,-.95},{-.55,-.85},{.8,.6},{.55,.85},{-.85,-.55},{-.9,-.95}});
		}
		else if(Effect==TEXT("Swap"))
		{
			Arc(P,12,-145,10,Ink,1.7f,5);Arc(P,12,35,190,Ink,1.7f,5);
			Glyph({{.6,-.3},{.87,.12},{1.15,-.25}});Glyph({{-.6,.3},{-.87,-.12},{-1.15,.25}});
		}
		else if(Effect==TEXT("Preparation"))
		{
			Arc(P,13,0,360,Ink,1.7f,5);Glyph({{0,-.7},{0,0},{.5,.3}});Glyph({{-.3,-1.15},{.3,-1.15}});
		}
		else if(Effect==TEXT("Pickup"))
		{
			Glyph({{0,-.85},{.45,0},{0,.85},{-.45,0},{0,-.85}});
			Glyph({{-.75,-.65},{-1,-.4},{-1,.4},{-.75,.65}});Glyph({{.75,-.65},{1,-.4},{1,.4},{.75,.65}});
		}
		else if(Effect==TEXT("SteelArea"))
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
			Disc(P+FVector2D(FMath::Cos(A),FMath::Sin(A))*(Radius+5),2.6f,I<Rank?Accent:FLinearColor(.18f,.18f,.16f),6);
		}
	}
	Text(FVector2D(124,22),TEXT("W A Y  O F  S T E E L"),12,PathColor(1));
	Text(FVector2D(662,22),TEXT("W A Y  O F  S H A D O W"),12,PathColor(2));
	Text(FVector2D(124,738),TEXT("S H A R E D  R O O T S"),12,PathColor(0));
	Text(FVector2D(662,738),TEXT("T W I N  S O U L  B O N D"),12,PathColor(3));

	return Layer+7;
}
