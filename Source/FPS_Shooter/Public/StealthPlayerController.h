#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StealthPlayerController.generated.h"

UCLASS()
class FPS_SHOOTER_API AStealthPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void ShowMissionResult();
	void RestartMission();
	bool IsMapOpen() const { return bMapOpen; }
	bool IsSensitivityPanelOpen() const { return bSensitivityPanelOpen; }
	float GetMouseSensitivity() const { return MouseSensitivity; }
	float GetAimSensitivityMultiplier() const { return AimSensitivityMultiplier; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void ToggleMap();
	void ToggleSensitivityPanel();
	void IncreaseMouseSensitivity();
	void DecreaseMouseSensitivity();
	void IncreaseAimSensitivity();
	void DecreaseAimSensitivity();
	void AdjustMouseSensitivity(float Amount);
	void AdjustAimSensitivity(float Amount);
	void SaveSensitivitySettings() const;
	bool bMapOpen = false;
	bool bSensitivityPanelOpen = false;
	float MouseSensitivity = 1.0f;
	float AimSensitivityMultiplier = 0.7f;
};
