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

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void ToggleMap();
	bool bMapOpen = false;
};
