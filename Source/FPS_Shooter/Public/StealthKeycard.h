#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthInteractable.h"
#include "StealthKeycard.generated.h"

UCLASS()
class FPS_SHOOTER_API AStealthKeycard : public AActor, public IStealthInteractable
{
	GENERATED_BODY()

public:
	AStealthKeycard();
	virtual void Interact_Implementation(AStealthCharacter* InteractingCharacter) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
};
