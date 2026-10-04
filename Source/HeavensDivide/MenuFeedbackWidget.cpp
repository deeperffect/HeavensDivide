#include "MenuFeedbackWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "Sound/SoundBase.h"
#include "Widgets/Input/SButton.h"

void UMenuFeedbackWidget::NativeConstruct()
{
    Super::NativeConstruct();
    LoadedHoverSound = HoverSound.LoadSynchronous();
    LoadedPressSound = PressSound.LoadSynchronous();
    DiscoverButtons();
}

void UMenuFeedbackWidget::DiscoverButtons()
{
    Buttons.RemoveAll([](const FButtonFeedback& State) { return !State.Button.IsValid(); });
    for (FButtonFeedback& State : Buttons) State.bSeenThisFrame = false;
    // Stop at nested user widgets: their own feedback owner handles their tree.
    TFunction<void(TSharedPtr<SWidget>, bool)> Visit = [&](TSharedPtr<SWidget> Widget, bool bRoot)
    {
        if (!Widget || (!bRoot && Widget->GetType() == FName(TEXT("SObjectWidget")))) return;
        if (Widget->GetType() == FName(TEXT("SButton")))
        {
            TSharedPtr<SButton> Button = StaticCastSharedPtr<SButton>(Widget);
            FButtonFeedback* Existing = Buttons.FindByPredicate([&](const FButtonFeedback& S) { return S.Button.Pin() == Button; });
            if (!Existing)
            {
                FButtonFeedback& State = Buttons.AddDefaulted_GetRef();
                State.Button = Button;
                State.OriginalTransform = Button->GetRenderTransform();
                State.OriginalPivot = Button->GetRenderTransformPivot();
                Button->SetRenderTransformPivot(FVector2D(.5f));
                Existing = &State;
            }
            Existing->bSeenThisFrame = true;
            FSlateSound Hover, Press;
            Hover.SetResourceObject(LoadedHoverSound); Press.SetResourceObject(LoadedPressSound);
            Button->SetHoveredSound(Hover); Button->SetPressedSound(Press);
        }
        if (FChildren* Children = Widget->GetChildren())
            for (int32 I = 0; I < Children->Num(); ++I) Visit(Children->GetChildAt(I), false);
    };
    Visit(GetCachedWidget(), true);
    if (WidgetTree) WidgetTree->ForEachWidget([this](UWidget* Widget)
    {
        if (UButton* Button = Cast<UButton>(Widget))
            Button->OnPressed.AddUniqueDynamic(this, &UMenuFeedbackWidget::HandleMenuPressed);
    });
}

void UMenuFeedbackWidget::HandleMenuPressed()
{
    for (FButtonFeedback& State : Buttons)
        if (TSharedPtr<SButton> Button = State.Button.Pin(); Button && Button->IsPressed() && Button->IsEnabled())
            State.PressRemaining = .09f;
    UpdateFeedback(0.f);
}

void UMenuFeedbackWidget::SetMenuFocusedButton(UButton* Button)
{
    ManualFocus = Button ? Button->GetCachedWidget() : TSharedPtr<SWidget>();
}

void UMenuFeedbackWidget::PressMenuFocusedButton()
{
    for (FButtonFeedback& State : Buttons)
        if (TSharedPtr<SButton> Button = State.Button.Pin(); Button && Button == ManualFocus.Pin() && Button->IsEnabled())
        {
            State.PressRemaining = .09f;
            FSlateSound Sound; Sound.SetResourceObject(LoadedPressSound);
            FSlateApplication::Get().PlaySound(Sound);
            break;
        }
    UpdateFeedback(0.f);
}

void UMenuFeedbackWidget::UpdateFeedback(float DeltaTime)
{
    for (FButtonFeedback& State : Buttons)
    {
        TSharedPtr<SButton> Button = State.Button.Pin();
        if (!Button) continue;
        // Disabled ancestors also disable the action. Hidden switcher pages must reset silently.
        bool bActive = State.bSeenThisFrame;
        for (TSharedPtr<SWidget> Ancestor = Button; Ancestor; Ancestor = Ancestor->GetParentWidget())
            if (!Ancestor->IsEnabled() || !Ancestor->GetVisibility().IsVisible()) { bActive = false; break; }
        const bool bHot = bActive && (Button->IsHovered() || (bShowNavigationFocus && Button->HasAnyUserFocus()) || Button == ManualFocus.Pin());
        if (bHot && !State.bWasHot && State.bInitialized && !Button->IsHovered())
        {
            FSlateSound Sound; Sound.SetResourceObject(LoadedHoverSound);
            FSlateApplication::Get().PlaySound(Sound);
        }
        State.bWasHot = bHot; State.bInitialized = true;
        const bool bPressed = bActive && (Button->IsPressed() || State.PressRemaining > 0.f);
        State.PressRemaining = bActive ? FMath::Max(0.f, State.PressRemaining - DeltaTime) : 0.f;
        const float Target = bPressed ? PressScale : bHot ? HoverScale : 1.f;
        // Immediate press feedback; exponential easing on hover/release, independent of framerate.
        State.Scale = !bActive ? 1.f : bPressed ? Target : FMath::Lerp(State.Scale, Target,
            1.f - FMath::Exp(-3.f * DeltaTime / FMath::Max(.01f, ResponseSeconds)));
        Button->SetRenderTransform(Concatenate(FSlateRenderTransform(FScale2D(State.Scale)),
            State.OriginalTransform.Get(FSlateRenderTransform())));
    }
}

void UMenuFeedbackWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    DiscoverButtons(); // Includes collection rebuilds, dynamically populated choices and keybind selectors.
    UpdateFeedback(DeltaTime);
}

FReply UMenuFeedbackWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (!Event.GetCursorDelta().IsNearlyZero()) bShowNavigationFocus = false;
    return Super::NativeOnMouseMove(Geometry, Event);
}

FReply UMenuFeedbackWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    bShowNavigationFocus = true;
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UMenuFeedbackWidget::NativeDestruct()
{
    if (WidgetTree) WidgetTree->ForEachWidget([this](UWidget* Widget)
    {
        if (UButton* Button = Cast<UButton>(Widget)) Button->OnPressed.RemoveDynamic(this, &UMenuFeedbackWidget::HandleMenuPressed);
    });
    for (const FButtonFeedback& State : Buttons)
        if (TSharedPtr<SButton> Button = State.Button.Pin())
        {
            Button->SetRenderTransform(State.OriginalTransform);
            Button->SetRenderTransformPivot(State.OriginalPivot);
            Button->SetHoveredSound(TOptional<FSlateSound>());
            Button->SetPressedSound(TOptional<FSlateSound>());
        }
    Buttons.Reset(); ManualFocus.Reset();
    Super::NativeDestruct();
}
