#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthInteractable.h"
#include "StealthSupplyPickup.generated.h"

class UBoxComponent;
class UMaterialInterface;
class UStaticMeshComponent;
class USoundRayInteractionComponent;

UENUM()
enum class EStealthSupplyKind : uint8
{
	Medkit,
	Ammo
};

UCLASS()
class FPS_SHOOTER_API AStealthSupplyPickup : public AActor, public IStealthInteractable
{
	GENERATED_BODY()

public:
	AStealthSupplyPickup();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Interact_Implementation(AStealthCharacter* Character) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	FText GetPromptFor(const AStealthCharacter& Character) const;
	EStealthSupplyKind GetKind() const { return Kind; }
	float GetHealingAmount() const { return HealingAmount; }
	void Consume(AStealthCharacter& Character);

protected:
	virtual void BeginPlay() override;

private:
	void UpdateAppearance();

	UPROPERTY(EditAnywhere, Category="Supplies")
	EStealthSupplyKind Kind = EStealthSupplyKind::Medkit;

	UPROPERTY(EditAnywhere, Category="Supplies", meta=(ClampMin="1"))
	float HealingAmount = 50.0f;

	UPROPERTY(EditAnywhere, Category="Supplies", meta=(ClampMin="1"))
	int32 AmmoAmount = 45;

	UPROPERTY(VisibleAnywhere, Category="Supplies")
	TObjectPtr<UBoxComponent> Target;

	UPROPERTY(VisibleAnywhere, Category="Supplies")
	TObjectPtr<UStaticMeshComponent> Case;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Lid;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Markings;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> CaseFinish;

	UPROPERTY()
	TObjectPtr<USoundRayInteractionComponent> Sound;
};
