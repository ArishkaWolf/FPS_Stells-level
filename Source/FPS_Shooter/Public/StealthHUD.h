#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "StealthHUD.generated.h"

class AStealthGameMode;
class AStealthCharacter;

UCLASS()
class FPS_SHOOTER_API AStealthHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

private:
	void DrawGuardIndicators();
	void DrawMissionResult(const AStealthGameMode& Mission);
	void DrawTacticalMap(const AStealthGameMode& Mission, const AStealthCharacter& Character);

	TArray<FBox2D> MapObstacles;
	FBox2D MapBounds{ForceInit};
};
