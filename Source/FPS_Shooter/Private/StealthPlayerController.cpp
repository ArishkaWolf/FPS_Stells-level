#include "StealthPlayerController.h"

#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"

void AStealthPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
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
