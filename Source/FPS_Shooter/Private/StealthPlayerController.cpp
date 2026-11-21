#include "StealthPlayerController.h"

#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"

void AStealthPlayerController::BeginPlay()
{
	Super::BeginPlay();
	PrimaryActorTick.bTickEvenWhenPaused = true;
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	GConfig->GetFloat(TEXT("Mouse"), TEXT("Sensitivity"), MouseSensitivity, GGameUserSettingsIni);
	GConfig->GetFloat(TEXT("Mouse"), TEXT("AimMultiplier"), AimSensitivityMultiplier, GGameUserSettingsIni);
	MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.1f, 3.0f);
	AimSensitivityMultiplier = FMath::Clamp(AimSensitivityMultiplier, 0.2f, 1.5f);
	const EWorldType::Type WorldType = GetWorld() ? GetWorld()->WorldType.GetValue() : EWorldType::None;
	if ((WorldType == EWorldType::Game || WorldType == EWorldType::PIE) && !GIsAutomationTesting)
	{
		bMainMenuOpen = true;
		SetPause(true);
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);

		if (WorldType == EWorldType::Game)
		{
			if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
			{
				Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
				Settings->SetScreenResolution(Settings->GetDesktopResolution());
				Settings->ApplySettings(false);
			}
		}
	}
	else
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
	}
}

void AStealthPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	for (const FKey Key : { EKeys::Enter, EKeys::Gamepad_Special_Right })
	{
		FInputKeyBinding& Binding = InputComponent->BindKey(Key, IE_Pressed, this, &AStealthPlayerController::RestartMission);
		Binding.bExecuteWhenPaused = true;
	}
	for (const FKey Key : { EKeys::M, EKeys::Gamepad_Special_Left })
	{
		FInputKeyBinding& Binding = InputComponent->BindKey(Key, IE_Pressed, this, &AStealthPlayerController::ToggleMap);
		Binding.bExecuteWhenPaused = true;
	}
	InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &AStealthPlayerController::ToggleSensitivityPanel);
	FInputKeyBinding& EscapeBinding = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AStealthPlayerController::ToggleMainMenu);
	EscapeBinding.bExecuteWhenPaused = true;
	FInputKeyBinding& SliderPressBinding = InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AStealthPlayerController::BeginSensitivitySliderDrag);
	SliderPressBinding.bExecuteWhenPaused = true;
	FInputKeyBinding& SliderReleaseBinding = InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AStealthPlayerController::EndSensitivitySliderDrag);
	SliderReleaseBinding.bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Add, IE_Pressed, this, &AStealthPlayerController::IncreaseMouseSensitivity);
	InputComponent->BindKey(EKeys::Equals, IE_Pressed, this, &AStealthPlayerController::IncreaseMouseSensitivity);
	InputComponent->BindKey(EKeys::Subtract, IE_Pressed, this, &AStealthPlayerController::DecreaseMouseSensitivity);
	InputComponent->BindKey(EKeys::Hyphen, IE_Pressed, this, &AStealthPlayerController::DecreaseMouseSensitivity);
	InputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &AStealthPlayerController::IncreaseAimSensitivity);
	InputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &AStealthPlayerController::DecreaseAimSensitivity);
}

void AStealthPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDraggingSensitivitySlider || bDraggingAimSensitivitySlider)
	{
		if (IsInputKeyDown(EKeys::LeftMouseButton)) { UpdateSensitivitySliderFromMouse(); }
		else { EndSensitivitySliderDrag(); }
	}
}

bool AStealthPlayerController::IsFullscreenEnabled() const
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return Settings && Settings->GetFullscreenMode() != EWindowMode::Windowed;
}

void AStealthPlayerController::StartMission()
{
	bMainMenuOpen = false;
	bSettingsMenuOpen = false;
	bSensitivityPanelOpen = false;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	SetInputMode(FInputModeGameOnly());
	SetPause(false);
}

void AStealthPlayerController::ToggleMainMenu()
{
	if (bMainMenuOpen)
	{
		StartMission();
		return;
	}
	const AStealthGameMode* Mission = GetWorld()->GetAuthGameMode<AStealthGameMode>();
	if (Mission && Mission->GetMissionState() != EStealthMissionState::Playing) { return; }
	bMainMenuOpen = true;
	bSettingsMenuOpen = false;
	bSensitivityPanelOpen = false;
	if (AStealthCharacter* StealthPawn = Cast<AStealthCharacter>(GetPawn())) { StealthPawn->ReleaseHeldActions(); }
	SetPause(true);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void AStealthPlayerController::HandleMenuButton(FName ButtonName)
{
	if (!bMainMenuOpen) { return; }
	if (ButtonName == TEXT("MenuPlay")) { StartMission(); }
	else if (ButtonName == TEXT("MenuSettings")) { bSettingsMenuOpen = true; }
	else if (ButtonName == TEXT("MenuBack")) { bSettingsMenuOpen = false; }
	else if (ButtonName == TEXT("MenuLookDown")) { MouseSensitivity = FMath::Clamp(MouseSensitivity - 0.1f, 0.1f, 3.0f); SaveSensitivitySettings(); }
	else if (ButtonName == TEXT("MenuLookUp")) { MouseSensitivity = FMath::Clamp(MouseSensitivity + 0.1f, 0.1f, 3.0f); SaveSensitivitySettings(); }
	else if (ButtonName == TEXT("MenuAimDown")) { AimSensitivityMultiplier = FMath::Clamp(AimSensitivityMultiplier - 0.1f, 0.2f, 1.5f); SaveSensitivitySettings(); }
	else if (ButtonName == TEXT("MenuAimUp")) { AimSensitivityMultiplier = FMath::Clamp(AimSensitivityMultiplier + 0.1f, 0.2f, 1.5f); SaveSensitivitySettings(); }
	else if (ButtonName == TEXT("MenuFullscreen"))
	{
		if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			Settings->SetFullscreenMode(IsFullscreenEnabled() ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen);
			Settings->ApplySettings(false);
			Settings->SaveSettings();
		}
	}
}

void AStealthPlayerController::BeginSensitivitySliderDrag()
{
	if (!bMainMenuOpen || !bSettingsMenuOpen) { return; }
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	int32 ViewX = 0;
	int32 ViewY = 0;
	if (!GetMousePosition(MouseX, MouseY)) { return; }
	GetViewportSize(ViewX, ViewY);
	if (ViewX <= 0 || ViewY <= 0) { return; }
	const float Scale = FMath::Min(ViewX / 960.0f, ViewY / 540.0f);
	const FVector2D LogicalSize(ViewX / Scale, ViewY / Scale);
	const float PanelTop = (LogicalSize.Y - 470.0f) * 0.5f;
	const float LogicalY = MouseY / Scale;
	if (LogicalY >= PanelTop + 164.0f && LogicalY <= PanelTop + 204.0f)
	{
		bDraggingSensitivitySlider = true;
		bDraggingAimSensitivitySlider = false;
		UpdateSensitivitySliderFromMouse();
	}
	else if (LogicalY >= PanelTop + 266.0f && LogicalY <= PanelTop + 306.0f)
	{
		bDraggingSensitivitySlider = false;
		bDraggingAimSensitivitySlider = true;
		UpdateSensitivitySliderFromMouse();
	}
}

void AStealthPlayerController::EndSensitivitySliderDrag()
{
	if (bDraggingSensitivitySlider || bDraggingAimSensitivitySlider) { SaveSensitivitySettings(); }
	bDraggingSensitivitySlider = false;
	bDraggingAimSensitivitySlider = false;
}

void AStealthPlayerController::UpdateSensitivitySliderFromMouse()
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	int32 ViewX = 0;
	int32 ViewY = 0;
	if (!GetMousePosition(MouseX, MouseY)) { return; }
	GetViewportSize(ViewX, ViewY);
	if (ViewX <= 0 || ViewY <= 0) { return; }
	const float Scale = FMath::Min(ViewX / 960.0f, ViewY / 540.0f);
	const float LogicalWidth = ViewX / Scale;
	const float PanelWidth = FMath::Min(900.0f, LogicalWidth - 48.0f);
	const float TrackStart = (LogicalWidth - PanelWidth) * 0.5f + 30.0f;
	const float TrackWidth = PanelWidth - 60.0f;
	const float Fraction = FMath::Clamp((MouseX / Scale - TrackStart) / TrackWidth, 0.0f, 1.0f);
	if (bDraggingSensitivitySlider) { MouseSensitivity = FMath::Lerp(0.1f, 3.0f, Fraction); }
	if (bDraggingAimSensitivitySlider) { AimSensitivityMultiplier = FMath::Lerp(0.2f, 1.5f, Fraction); }
}

void AStealthPlayerController::ToggleSensitivityPanel()
{
	bSensitivityPanelOpen = !bSensitivityPanelOpen;
	if (bSensitivityPanelOpen)
	{
		bMapOpen = false;
		SetPause(false);
	}
}

void AStealthPlayerController::AdjustMouseSensitivity(float Amount)
{
	if (!bSensitivityPanelOpen) { return; }
	MouseSensitivity = FMath::Clamp(MouseSensitivity + Amount, 0.1f, 3.0f);
	SaveSensitivitySettings();
}

void AStealthPlayerController::IncreaseMouseSensitivity() { AdjustMouseSensitivity(0.1f); }
void AStealthPlayerController::DecreaseMouseSensitivity() { AdjustMouseSensitivity(-0.1f); }
void AStealthPlayerController::IncreaseAimSensitivity() { AdjustAimSensitivity(0.1f); }
void AStealthPlayerController::DecreaseAimSensitivity() { AdjustAimSensitivity(-0.1f); }

void AStealthPlayerController::AdjustAimSensitivity(float Amount)
{
	if (!bSensitivityPanelOpen) { return; }
	AimSensitivityMultiplier = FMath::Clamp(AimSensitivityMultiplier + Amount, 0.2f, 1.5f);
	SaveSensitivitySettings();
}

void AStealthPlayerController::SaveSensitivitySettings() const
{
	GConfig->SetFloat(TEXT("Mouse"), TEXT("Sensitivity"), MouseSensitivity, GGameUserSettingsIni);
	GConfig->SetFloat(TEXT("Mouse"), TEXT("AimMultiplier"), AimSensitivityMultiplier, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void AStealthPlayerController::ToggleMap()
{
	const AStealthGameMode* Mission = GetWorld()->GetAuthGameMode<AStealthGameMode>();
	if (!Mission || Mission->GetMissionState() != EStealthMissionState::Playing) { return; }
	if (SetPause(!bMapOpen))
	{
		bMapOpen = !bMapOpen;
		if (AStealthCharacter* StealthPawn = Cast<AStealthCharacter>(GetPawn()); bMapOpen && StealthPawn)
		{
			StealthPawn->ReleaseHeldActions();
		}
	}
}

void AStealthPlayerController::ShowMissionResult()
{
	bMapOpen = false;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetPause(true);
}

void AStealthPlayerController::RestartMission()
{
	if (AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>())
	{
		GameMode->RestartMission();
	}
}
