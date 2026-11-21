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
	bool IsMainMenuOpen() const { return bMainMenuOpen; }
	bool IsSettingsMenuOpen() const { return bSettingsMenuOpen; }
	float GetMouseSensitivity() const { return MouseSensitivity; }
	float GetAimSensitivityMultiplier() const { return AimSensitivityMultiplier; }
	bool IsFullscreenEnabled() const;
	void HandleMenuButton(FName ButtonName);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void ToggleMap();
	void ToggleSensitivityPanel();
	void ToggleMainMenu();
	void StartMission();
	void BeginSensitivitySliderDrag();
	void EndSensitivitySliderDrag();
	void UpdateSensitivitySliderFromMouse();
	void IncreaseMouseSensitivity();
	void DecreaseMouseSensitivity();
	void IncreaseAimSensitivity();
	void DecreaseAimSensitivity();
	void AdjustMouseSensitivity(float Amount);
	void AdjustAimSensitivity(float Amount);
	void SaveSensitivitySettings() const;
	bool bMapOpen = false;
	bool bSensitivityPanelOpen = false;
	bool bMainMenuOpen = false;
	bool bSettingsMenuOpen = false;
	bool bDraggingSensitivitySlider = false;
	bool bDraggingAimSensitivitySlider = false;
	float MouseSensitivity = 1.0f;
	float AimSensitivityMultiplier = 0.7f;
};
