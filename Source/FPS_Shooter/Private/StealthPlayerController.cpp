#include "StealthPlayerController.h"

#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"

void AStealthPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	GConfig->GetFloat(TEXT("Mouse"), TEXT("Sensitivity"), MouseSensitivity, GGameUserSettingsIni);
	GConfig->GetFloat(TEXT("Mouse"), TEXT("AimMultiplier"), AimSensitivityMultiplier, GGameUserSettingsIni);
	MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.1f, 3.0f);
	AimSensitivityMultiplier = FMath::Clamp(AimSensitivityMultiplier, 0.2f, 1.5f);
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
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AStealthPlayerController::ToggleSensitivityPanel);
	InputComponent->BindKey(EKeys::Add, IE_Pressed, this, &AStealthPlayerController::IncreaseMouseSensitivity);
	InputComponent->BindKey(EKeys::Equals, IE_Pressed, this, &AStealthPlayerController::IncreaseMouseSensitivity);
	InputComponent->BindKey(EKeys::Subtract, IE_Pressed, this, &AStealthPlayerController::DecreaseMouseSensitivity);
	InputComponent->BindKey(EKeys::Hyphen, IE_Pressed, this, &AStealthPlayerController::DecreaseMouseSensitivity);
	InputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &AStealthPlayerController::IncreaseAimSensitivity);
	InputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &AStealthPlayerController::DecreaseAimSensitivity);
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
