// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "ExperienceComponent.h"
#include "FinalBossBase.h"
#include "HealthComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"

void UPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeRunTimerDisplay();
}

void UPlayerHUDWidget::InitializeFromCharacterManager(UCharacterManagerComponent* InCharacterManager)
{
	InitializeFromPlayerController(InCharacterManager ? Cast<ASurvivorPlayerController>(InCharacterManager->GetOwner()) : nullptr);
}

void UPlayerHUDWidget::InitializeFromPlayerController(ASurvivorPlayerController* InPlayerController)
{
	if (!InPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget initialization skipped: PlayerController invalid."));
		return;
	}

	if (SurvivorPlayerController == InPlayerController)
	{
		BroadcastInitialState();
		return;
	}

	UnbindCharacterHealth();
	UnbindPlayerExperience();
	UnbindDashState();
	UnbindSwapCooldownState();

	if (CharacterManager)
	{
		CharacterManager->OnCharacterSwapped.RemoveDynamic(this, &UPlayerHUDWidget::HandleCharacterSwapped);
	}

	SurvivorPlayerController = InPlayerController;
	CharacterManager = SurvivorPlayerController->GetCharacterManager();
	Samurai = CharacterManager ? CharacterManager->GetSamurai() : nullptr;
	Ninja = CharacterManager ? CharacterManager->GetNinja() : nullptr;
	PlayerHealth = SurvivorPlayerController->GetPlayerHealthComponent();
	PlayerExperience = SurvivorPlayerController->GetExperienceComponent();

	if (CharacterManager)
	{
		CharacterManager->OnCharacterSwapped.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleCharacterSwapped);
	}

	BindCharacterHealth();
	BindPlayerExperience();
	BindDashState();
	BindSwapCooldownState();
	BroadcastInitialState();
}

void UPlayerHUDWidget::NativeDestruct()
{
	HideBossHealthBar();
	StopHealthChipChase();
	UnbindCharacterHealth();
	UnbindPlayerExperience();
	UnbindDashState();
	UnbindSwapCooldownState();

	if (CharacterManager)
	{
		CharacterManager->OnCharacterSwapped.RemoveDynamic(this, &UPlayerHUDWidget::HandleCharacterSwapped);
	}

	Super::NativeDestruct();
}

void UPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateRunTimerDisplay();

	if (bDashRechargeProgressActive)
	{
		BroadcastDashRechargeProgress();

		if (!IsDashRecharging())
		{
			bDashRechargeProgressActive = false;
		}
	}

	if (bSwapCooldownProgressActive)
	{
		BroadcastSwapCooldownProgress();

		if (!IsSwapCoolingDown())
		{
			bSwapCooldownProgressActive = false;
		}
	}

	if (!bHealthChipChasing)
	{
		return;
	}

	DelayedHealthPercent = FMath::FInterpTo(
		DelayedHealthPercent,
		TargetHealthPercent,
		InDeltaTime,
		HealthChipChaseSpeed);

	if (FMath::Abs(DelayedHealthPercent - TargetHealthPercent) <= HealthChipSnapTolerance)
	{
		DelayedHealthPercent = TargetHealthPercent;
		bHealthChipChasing = false;
	}
}

ASamuraiCharacter* UPlayerHUDWidget::GetSamurai() const
{
	return Samurai;
}

ANinjaCharacter* UPlayerHUDWidget::GetNinja() const
{
	return Ninja;
}

ACharacterBase* UPlayerHUDWidget::GetActiveCharacter() const
{
	return CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
}

float UPlayerHUDWidget::GetCurrentHealth() const
{
	return PlayerHealth ? PlayerHealth->GetCurrentHealth() : 0.0f;
}

float UPlayerHUDWidget::GetMaxHealth() const
{
	return PlayerHealth ? PlayerHealth->GetMaxHealth() : 0.0f;
}

float UPlayerHUDWidget::GetHealthPercent() const
{
	return PlayerHealth ? PlayerHealth->GetHealthPercent() : 0.0f;
}

float UPlayerHUDWidget::GetDisplayedHealthPercent() const
{
	return DisplayedHealthPercent;
}

float UPlayerHUDWidget::GetDelayedHealthPercent() const
{
	return DelayedHealthPercent;
}

int32 UPlayerHUDWidget::GetCurrentXP() const
{
	return PlayerExperience ? PlayerExperience->GetCurrentXP() : 0;
}

int32 UPlayerHUDWidget::GetCurrentLevel() const
{
	return PlayerExperience ? PlayerExperience->GetCurrentLevel() : 1;
}

int32 UPlayerHUDWidget::GetXPToNextLevel() const
{
	return PlayerExperience ? PlayerExperience->GetXPToNextLevel() : 0;
}

float UPlayerHUDWidget::GetXPPercent() const
{
	return PlayerExperience ? PlayerExperience->GetXPPercent() : 0.0f;
}

int32 UPlayerHUDWidget::GetCurrentDashCharges() const
{
	return SurvivorPlayerController ? SurvivorPlayerController->GetCurrentDashCharges() : 0;
}

int32 UPlayerHUDWidget::GetMaxDashCharges() const
{
	return SurvivorPlayerController ? SurvivorPlayerController->GetMaxDashCharges() : 0;
}

float UPlayerHUDWidget::GetDashRechargePercent() const
{
	return SurvivorPlayerController ? SurvivorPlayerController->GetDashRechargeNormalized() : 0.0f;
}

bool UPlayerHUDWidget::IsDashRecharging() const
{
	return SurvivorPlayerController
		&& SurvivorPlayerController->GetCurrentDashCharges() < SurvivorPlayerController->GetMaxDashCharges()
		&& SurvivorPlayerController->GetDashRechargeRemaining() > 0.0f;
}

int32 UPlayerHUDWidget::GetRechargingDashSlotIndex() const
{
	return IsDashRecharging() ? GetCurrentDashCharges() : INDEX_NONE;
}

TArray<FDashChargeSlotState> UPlayerHUDWidget::GetDashChargeSlotStates() const
{
	TArray<FDashChargeSlotState> ChargeSlots;

	const int32 CurrentCharges = GetCurrentDashCharges();
	const int32 MaxCharges = GetMaxDashCharges();
	const float RechargePercent = GetDashRechargePercent();
	const bool bIsRecharging = IsDashRecharging();
	ChargeSlots.Reserve(MaxCharges);

	for (int32 SlotIndex = 0; SlotIndex < MaxCharges; ++SlotIndex)
	{
		FDashChargeSlotState SlotState;
		SlotState.SlotIndex = SlotIndex;

		if (SlotIndex < CurrentCharges)
		{
			SlotState.State = EDashChargeSlotState::Full;
			SlotState.RechargePercent = 1.0f;
		}
		else if (SlotIndex == CurrentCharges && bIsRecharging)
		{
			SlotState.State = EDashChargeSlotState::Recharging;
			SlotState.RechargePercent = RechargePercent;
		}
		else
		{
			SlotState.State = EDashChargeSlotState::Empty;
			SlotState.RechargePercent = 0.0f;
		}

		ChargeSlots.Add(SlotState);
	}

	return ChargeSlots;
}

float UPlayerHUDWidget::GetSwapCooldownRemaining() const
{
	return SurvivorPlayerController ? SurvivorPlayerController->GetSwapCooldownRemaining() : 0.0f;
}

float UPlayerHUDWidget::GetSwapCooldownDuration() const
{
	return SurvivorPlayerController ? SurvivorPlayerController->GetSwapCooldownDuration() : 0.0f;
}

float UPlayerHUDWidget::GetSwapCooldownProgress() const
{
	return SurvivorPlayerController ? SurvivorPlayerController->GetSwapCooldownProgress() : 0.0f;
}

bool UPlayerHUDWidget::IsSwapCoolingDown() const
{
	return SurvivorPlayerController
		&& SurvivorPlayerController->GetSwapCooldownRemaining() > 0.0f
		&& !SurvivorPlayerController->CanSwap();
}

bool UPlayerHUDWidget::IsSamuraiActive() const
{
	return Samurai && GetActiveCharacter() == Samurai;
}

bool UPlayerHUDWidget::IsNinjaActive() const
{
	return Ninja && GetActiveCharacter() == Ninja;
}

void UPlayerHUDWidget::ShowBossHealthBar(AFinalBossBase* Boss)
{
	if (!Boss || !Boss->GetHealthComponent()) return;
	HideBossHealthBar();
	EnsureBossHealthPresentation();
	DisplayedBoss = Boss;
	Boss->GetHealthComponent()->OnHealthChanged.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleBossHealthChanged);
	if (BossNameText) BossNameText->SetText(Boss->GetBossDisplayName());
	if (BossHealthContainer) BossHealthContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
	HandleBossHealthChanged(
		Boss->GetHealthComponent()->GetCurrentHealth(),
		Boss->GetHealthComponent()->GetMaxHealth(),
		Boss->GetHealthComponent()->GetHealthPercent());
}

void UPlayerHUDWidget::HideBossHealthBar(AFinalBossBase* Boss)
{
	AFinalBossBase* CurrentBoss = DisplayedBoss.Get();
	if (Boss && CurrentBoss && Boss != CurrentBoss) return;
	if (CurrentBoss && CurrentBoss->GetHealthComponent())
	{
		CurrentBoss->GetHealthComponent()->OnHealthChanged.RemoveDynamic(this, &UPlayerHUDWidget::HandleBossHealthChanged);
	}
	DisplayedBoss.Reset();
	if (BossHealthContainer) BossHealthContainer->SetVisibility(ESlateVisibility::Collapsed);
}

void UPlayerHUDWidget::HandleBossHealthChanged(float CurrentHealth, float MaxHealth, float HealthPercent)
{
	if (BossHealthProgressBar) BossHealthProgressBar->SetPercent(FMath::Clamp(HealthPercent, 0.0f, 1.0f));
	if (BossHealthValueText)
	{
		BossHealthValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), FMath::Max(0.0f, CurrentHealth), FMath::Max(0.0f, MaxHealth))));
	}
	if (CurrentHealth <= 0.0f) HideBossHealthBar();
}

void UPlayerHUDWidget::EnsureBossHealthPresentation()
{
	if (BossHealthContainer || !WidgetTree || !WidgetTree->RootWidget) return;
	UWidget* ExistingRoot = WidgetTree->RootWidget;
	UCanvasPanel* CanvasRoot = Cast<UCanvasPanel>(ExistingRoot);
	if (!CanvasRoot)
	{
		CanvasRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerHUDPresentationRoot"));
		WidgetTree->RootWidget = CanvasRoot;
		UCanvasPanelSlot* ExistingSlot = CanvasRoot->AddChildToCanvas(ExistingRoot);
		ExistingSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		ExistingSlot->SetOffsets(FMargin(0.0f));
	}

	BossHealthContainer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BossHealthContainer"));
	BossHealthContainer->SetBrushColor(FLinearColor(0.015f, 0.01f, 0.015f, 0.92f));
	BossHealthContainer->SetPadding(FMargin(18.0f, 10.0f));
	BossHealthContainer->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BossHealthStack"));
	BossHealthContainer->SetContent(Stack);

	BossNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BossNameText"));
	BossNameText->SetJustification(ETextJustify::Center);
	BossNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.80f, 0.62f)));
	FSlateFontInfo NameFont = BossNameText->GetFont();
	NameFont.Size = 22;
	NameFont.TypefaceFontName = TEXT("Bold");
	BossNameText->SetFont(NameFont);
	Stack->AddChildToVerticalBox(BossNameText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));

	BossHealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BossHealthProgressBar"));
	BossHealthProgressBar->SetFillColorAndOpacity(FLinearColor(0.68f, 0.055f, 0.045f, 1.0f));
	Stack->AddChildToVerticalBox(BossHealthProgressBar)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	BossHealthValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BossHealthValueText"));
	BossHealthValueText->SetJustification(ETextJustify::Center);
	BossHealthValueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.88f)));
	Stack->AddChildToVerticalBox(BossHealthValueText);

	UCanvasPanelSlot* BossSlot = CanvasRoot->AddChildToCanvas(BossHealthContainer);
	BossSlot->SetAnchors(FAnchors(0.5f, 0.12f));
	BossSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BossSlot->SetSize(FVector2D(720.0f, 92.0f));
	BossSlot->SetZOrder(200);
}

float UPlayerHUDWidget::GetRunTimeSeconds() const
{
	return SurvivorPlayerController ? SurvivorPlayerController->GetRunTimeSeconds() : 0.0f;
}

FText UPlayerHUDWidget::FormatRunTime(float RunTimeSeconds) const
{
	return FormatRunTimeText(RunTimeSeconds);
}

FText UPlayerHUDWidget::FormatRunTimeText(float RunTimeSeconds)
{
	const float SafeRunTime = FMath::IsFinite(RunTimeSeconds) ? FMath::Max(0.0f, RunTimeSeconds) : 0.0f;
	const int64 TotalSeconds = FMath::FloorToInt64(SafeRunTime);
	const int64 Seconds = TotalSeconds % 60;
	const int64 TotalMinutes = TotalSeconds / 60;
	if (TotalMinutes < 60)
	{
		return FText::FromString(FString::Printf(TEXT("%02lld:%02lld"), TotalMinutes, Seconds));
	}

	const int64 Hours = TotalMinutes / 60;
	const int64 Minutes = TotalMinutes % 60;
	return FText::FromString(FString::Printf(TEXT("%lld:%02lld:%02lld"), Hours, Minutes, Seconds));
}

void UPlayerHUDWidget::InitializeRunTimerDisplay()
{
	if (!RunTimerText && WidgetTree)
	{
		UCanvasPanel* TargetCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
		if (!TargetCanvas)
		{
			TArray<UWidget*> AllWidgets;
			WidgetTree->GetAllWidgets(AllWidgets);
			for (UWidget* Widget : AllWidgets)
			{
				if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Widget))
				{
					TargetCanvas = Canvas;
					break;
				}
			}
		}

		if (TargetCanvas)
		{
			RunTimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RunTimerText"));
			if (UCanvasPanelSlot* TimerSlot = TargetCanvas->AddChildToCanvas(RunTimerText))
			{
				TimerSlot->SetAnchors(FAnchors(0.5f, 0.0f));
				TimerSlot->SetAlignment(FVector2D(0.5f, 0.0f));
				TimerSlot->SetPosition(FVector2D(0.0f, 24.0f));
				TimerSlot->SetAutoSize(true);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget could not create the run timer: no CanvasPanel exists in the HUD widget tree."));
		}
	}

	if (RunTimerText)
	{
		FSlateFontInfo TimerFont = RunTimerText->GetFont();
		TimerFont.Size = 32;
		RunTimerText->SetFont(TimerFont);
		RunTimerText->SetJustification(ETextJustify::Center);
		RunTimerText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		RunTimerText->SetShadowOffset(FVector2D(1.5f, 1.5f));
		RunTimerText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		RunTimerText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	LastDisplayedRunTimeSecond = INDEX_NONE;
	UpdateRunTimerDisplay();
}

void UPlayerHUDWidget::UpdateRunTimerDisplay()
{
	if (!RunTimerText)
	{
		return;
	}

	const float RunTimeSeconds = GetRunTimeSeconds();
	const float SafeRunTime = FMath::IsFinite(RunTimeSeconds) ? FMath::Max(0.0f, RunTimeSeconds) : 0.0f;
	const int64 WholeSecond = FMath::FloorToInt64(SafeRunTime);
	if (WholeSecond == LastDisplayedRunTimeSecond)
	{
		return;
	}

	LastDisplayedRunTimeSecond = WholeSecond;
	RunTimerText->SetText(FormatRunTime(SafeRunTime));
}

void UPlayerHUDWidget::HandlePlayerHealthChanged(float CurrentHealth, float MaxHealth, float HealthPercent)
{
	UpdateHealthAnimation(HealthPercent);
	PlayerHealthUpdated.Broadcast(CurrentHealth, MaxHealth, HealthPercent);
	OnPlayerHealthUpdated(CurrentHealth, MaxHealth, HealthPercent);
}

void UPlayerHUDWidget::HandleCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
	ActiveCharacterChanged.Broadcast(NewCharacter);
	OnActiveCharacterChanged(NewCharacter);
}

void UPlayerHUDWidget::HandlePlayerXPChanged(int32 CurrentXP, int32 XPToNextLevel, float XPPercent)
{
	PlayerXPUpdated.Broadcast(CurrentXP, XPToNextLevel, XPPercent);
	OnPlayerXPUpdated(CurrentXP, XPToNextLevel, XPPercent);
}

void UPlayerHUDWidget::HandlePlayerLevelUp(int32 NewLevel)
{
	PlayerLevelUp.Broadcast(NewLevel);
	OnPlayerLevelUp(NewLevel);
	PlayerLevelUpdated.Broadcast(NewLevel);
	OnPlayerLevelUpdated(NewLevel);
}

void UPlayerHUDWidget::HandleDashChargesChanged(int32 CurrentCharges, int32 MaxCharges)
{
	bDashRechargeProgressActive = IsDashRecharging();
	BroadcastDashState();
}

void UPlayerHUDWidget::HandleDashRechargeStarted()
{
	bDashRechargeProgressActive = true;
	BroadcastDashState();
	BroadcastDashRechargeProgress();
}

void UPlayerHUDWidget::HandleDashRechargeCompleted()
{
	BroadcastDashState();
	BroadcastDashRechargeProgress();
	bDashRechargeProgressActive = IsDashRecharging();
}

void UPlayerHUDWidget::HandleSwapCooldownStarted(float CooldownDuration)
{
	bSwapCooldownProgressActive = CooldownDuration > 0.0f;
	SwapCooldownStarted.Broadcast(CooldownDuration);
	OnSwapCooldownStarted(CooldownDuration);
	BroadcastSwapCooldownProgress();
}

void UPlayerHUDWidget::HandleSwapCooldownFinished()
{
	bSwapCooldownProgressActive = false;
	BroadcastSwapCooldownProgress();
	BroadcastSwapCooldownFinished();
}

void UPlayerHUDWidget::BindCharacterHealth()
{
	if (PlayerHealth)
	{
		PlayerHealth->OnHealthChanged.AddUniqueDynamic(this, &UPlayerHUDWidget::HandlePlayerHealthChanged);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget: shared player HealthComponent invalid."));
	}
}

void UPlayerHUDWidget::UnbindCharacterHealth()
{
	StopHealthChipChase();

	if (PlayerHealth)
	{
		PlayerHealth->OnHealthChanged.RemoveDynamic(this, &UPlayerHUDWidget::HandlePlayerHealthChanged);
	}

	PlayerHealth = nullptr;
}

void UPlayerHUDWidget::BindPlayerExperience()
{
	if (PlayerExperience)
	{
		PlayerExperience->OnXPChanged.AddUniqueDynamic(this, &UPlayerHUDWidget::HandlePlayerXPChanged);
		PlayerExperience->OnLevelUp.AddUniqueDynamic(this, &UPlayerHUDWidget::HandlePlayerLevelUp);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget: shared ExperienceComponent invalid."));
	}
}

void UPlayerHUDWidget::UnbindPlayerExperience()
{
	if (PlayerExperience)
	{
		PlayerExperience->OnXPChanged.RemoveDynamic(this, &UPlayerHUDWidget::HandlePlayerXPChanged);
		PlayerExperience->OnLevelUp.RemoveDynamic(this, &UPlayerHUDWidget::HandlePlayerLevelUp);
	}

	PlayerExperience = nullptr;
}

void UPlayerHUDWidget::BindDashState()
{
	if (!SurvivorPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget: SurvivorPlayerController invalid for dash state."));
		return;
	}

	SurvivorPlayerController->OnDashChargesChanged.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleDashChargesChanged);
	SurvivorPlayerController->OnDashRechargeStarted.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleDashRechargeStarted);
	SurvivorPlayerController->OnDashRechargeCompleted.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleDashRechargeCompleted);
}

void UPlayerHUDWidget::UnbindDashState()
{
	if (SurvivorPlayerController)
	{
		SurvivorPlayerController->OnDashChargesChanged.RemoveDynamic(this, &UPlayerHUDWidget::HandleDashChargesChanged);
		SurvivorPlayerController->OnDashRechargeStarted.RemoveDynamic(this, &UPlayerHUDWidget::HandleDashRechargeStarted);
		SurvivorPlayerController->OnDashRechargeCompleted.RemoveDynamic(this, &UPlayerHUDWidget::HandleDashRechargeCompleted);
	}

	bDashRechargeProgressActive = false;
}

void UPlayerHUDWidget::BindSwapCooldownState()
{
	if (!SurvivorPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget: SurvivorPlayerController invalid for swap cooldown state."));
		return;
	}

	SurvivorPlayerController->OnSwapCooldownStarted.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleSwapCooldownStarted);
	SurvivorPlayerController->OnSwapCooldownFinished.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleSwapCooldownFinished);
}

void UPlayerHUDWidget::UnbindSwapCooldownState()
{
	if (SurvivorPlayerController)
	{
		SurvivorPlayerController->OnSwapCooldownStarted.RemoveDynamic(this, &UPlayerHUDWidget::HandleSwapCooldownStarted);
		SurvivorPlayerController->OnSwapCooldownFinished.RemoveDynamic(this, &UPlayerHUDWidget::HandleSwapCooldownFinished);
	}

	bSwapCooldownProgressActive = false;
}

void UPlayerHUDWidget::BroadcastInitialState()
{
	if (PlayerHealth)
	{
		const float HealthPercent = PlayerHealth->GetHealthPercent();
		DisplayedHealthPercent = HealthPercent;
		DelayedHealthPercent = HealthPercent;
		TargetHealthPercent = HealthPercent;
		bHealthChipChasing = false;

		PlayerHealthUpdated.Broadcast(
			PlayerHealth->GetCurrentHealth(),
			PlayerHealth->GetMaxHealth(),
			HealthPercent);
		OnPlayerHealthUpdated(
			PlayerHealth->GetCurrentHealth(),
			PlayerHealth->GetMaxHealth(),
			HealthPercent);
	}

	if (PlayerExperience)
	{
		PlayerXPUpdated.Broadcast(
			PlayerExperience->GetCurrentXP(),
			PlayerExperience->GetXPToNextLevel(),
			PlayerExperience->GetXPPercent());
		OnPlayerXPUpdated(
			PlayerExperience->GetCurrentXP(),
			PlayerExperience->GetXPToNextLevel(),
			PlayerExperience->GetXPPercent());
		PlayerLevelUpdated.Broadcast(PlayerExperience->GetCurrentLevel());
		OnPlayerLevelUpdated(PlayerExperience->GetCurrentLevel());
	}

	ActiveCharacterChanged.Broadcast(GetActiveCharacter());
	OnActiveCharacterChanged(GetActiveCharacter());
	BroadcastDashState();
	bDashRechargeProgressActive = IsDashRecharging();
	if (bDashRechargeProgressActive)
	{
		BroadcastDashRechargeProgress();
	}

	if (IsSwapCoolingDown())
	{
		bSwapCooldownProgressActive = true;
		SwapCooldownStarted.Broadcast(GetSwapCooldownDuration());
		OnSwapCooldownStarted(GetSwapCooldownDuration());
		BroadcastSwapCooldownProgress();
	}
	else
	{
		BroadcastSwapCooldownFinished();
	}
}

void UPlayerHUDWidget::UpdateHealthAnimation(float NewHealthPercent)
{
	NewHealthPercent = FMath::Clamp(NewHealthPercent, 0.0f, 1.0f);

	const float PreviousDisplayedPercent = DisplayedHealthPercent;
	DisplayedHealthPercent = NewHealthPercent;
	TargetHealthPercent = NewHealthPercent;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealthChipDelayTimerHandle);
	}

	if (NewHealthPercent >= PreviousDisplayedPercent)
	{
		DelayedHealthPercent = NewHealthPercent;
		bHealthChipChasing = false;
		return;
	}

	if (DelayedHealthPercent < PreviousDisplayedPercent)
	{
		DelayedHealthPercent = PreviousDisplayedPercent;
	}

	bHealthChipChasing = false;

	if (HealthChipDelay <= 0.0f)
	{
		bHealthChipChasing = true;
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HealthChipDelayTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				bHealthChipChasing = true;
			}),
			HealthChipDelay,
			false);
	}
}

void UPlayerHUDWidget::StopHealthChipChase()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealthChipDelayTimerHandle);
	}

	bHealthChipChasing = false;
}

void UPlayerHUDWidget::BroadcastDashState()
{
	const int32 CurrentCharges = GetCurrentDashCharges();
	const int32 MaxCharges = GetMaxDashCharges();
	const float RechargePercent = GetDashRechargePercent();
	DashChargesUpdated.Broadcast(CurrentCharges, MaxCharges, RechargePercent);
	OnDashChargesUpdated(CurrentCharges, MaxCharges, RechargePercent);
	BroadcastDashChargeSlots();
}

void UPlayerHUDWidget::BroadcastDashRechargeProgress()
{
	const float RechargePercent = GetDashRechargePercent();
	const int32 RechargingSlotIndex = GetRechargingDashSlotIndex();
	DashRechargeUpdated.Broadcast(RechargePercent);
	OnDashRechargeUpdated(RechargePercent);
	DashRechargeSlotUpdated.Broadcast(RechargingSlotIndex, RechargePercent);
	OnDashRechargeSlotUpdated(RechargingSlotIndex, RechargePercent);
}

void UPlayerHUDWidget::BroadcastDashChargeSlots()
{
	const TArray<FDashChargeSlotState> ChargeSlots = GetDashChargeSlotStates();
	DashChargeSlotsUpdated.Broadcast(ChargeSlots);
	OnDashChargeSlotsUpdated(ChargeSlots);
}

void UPlayerHUDWidget::BroadcastSwapCooldownProgress()
{
	const float RemainingCooldown = GetSwapCooldownRemaining();
	const float CooldownDuration = GetSwapCooldownDuration();
	const float CooldownProgress = GetSwapCooldownProgress();
	SwapCooldownUpdated.Broadcast(RemainingCooldown, CooldownDuration, CooldownProgress);
	OnSwapCooldownUpdated(RemainingCooldown, CooldownDuration, CooldownProgress);
}

void UPlayerHUDWidget::BroadcastSwapCooldownFinished()
{
	SwapCooldownFinished.Broadcast();
	OnSwapCooldownFinished();
}
