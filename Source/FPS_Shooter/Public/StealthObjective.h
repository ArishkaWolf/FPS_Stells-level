#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthInteractable.h"
#include "StealthObjective.generated.h"

class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class USoundRayInteractionComponent;

UCLASS(Abstract)
class FPS_SHOOTER_API AStealthMissionTerminal : public AActor, public IStealthInteractable
{
	GENERATED_BODY()

public:
	AStealthMissionTerminal();

protected:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USoundRayInteractionComponent> SoundEmitter;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> LightComponent;
};

UCLASS()
class FPS_SHOOTER_API AStealthObjective : public AStealthMissionTerminal
{
	GENERATED_BODY()

public:
	virtual void Interact_Implementation(AStealthCharacter* InteractingCharacter) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
};

UCLASS()
class FPS_SHOOTER_API AStealthOperationsTerminal : public AStealthMissionTerminal
{
	GENERATED_BODY()

public:
	virtual void Interact_Implementation(AStealthCharacter* InteractingCharacter) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mission")
	bool bVerifiesArchive = false;
};
