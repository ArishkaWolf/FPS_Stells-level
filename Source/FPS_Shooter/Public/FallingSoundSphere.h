#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthInteractable.h"
#include "FallingSoundSphere.generated.h"

class USoundRayEmitterComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class FPS_SHOOTER_API AFallingSoundSphere : public AActor, public IStealthInteractable
{
	GENERATED_BODY()

public:
	AFallingSoundSphere();
	virtual void Interact_Implementation(AStealthCharacter* InteractingCharacter) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USoundRayEmitterComponent> SoundEmitter;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

private:
	UPROPERTY(EditDefaultsOnly, Category="Audio")
	TObjectPtr<USoundBase> ImpactSound;

	bool bEmittedSound = false;
};
