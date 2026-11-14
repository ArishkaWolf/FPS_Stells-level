#include "StealthExitZone.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"

AStealthExitZone::AStealthExitZone()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerComponent"));
	TriggerComponent->InitBoxExtent(FVector(140.0f, 140.0f, 110.0f));
	TriggerComponent->SetCollisionProfileName(TEXT("Trigger"));
	SetRootComponent(TriggerComponent);

	LightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComponent"));
	LightComponent->SetupAttachment(RootComponent);
	LightComponent->SetAttenuationRadius(450.0f);
	LightComponent->SetIntensity(1600.0f);
}

void AStealthExitZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AStealthGameMode* GameMode = Cast<AStealthGameMode>(UGameplayStatics::GetGameMode(this));
	const bool bUnlocked = GameMode && GameMode->CanExtract();
	const bool bPursued = GameMode && GameMode->IsPlayerPursued();
	const FColor ExitColor = !bUnlocked ? FColor::Red : bPursued ? FColor::Yellow : FColor::Green;
	LightComponent->SetLightColor(FLinearColor(ExitColor));

	// Recheck occupants when intel or pursuit changes; no need to leave and re-enter the trigger.
	if (bUnlocked && !bPursued)
	{
		AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Player && TriggerComponent->IsOverlappingActor(Player))
		{
			if (AStealthGameMode* MutableGameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>())
			{
				MutableGameMode->TryCompleteMission(Player);
			}
		}
	}
}

void AStealthExitZone::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || !OtherActor->IsA<AStealthCharacter>())
	{
		return;
	}

	if (AStealthGameMode* GameMode = Cast<AStealthGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GameMode->TryCompleteMission(OtherActor);
	}
}

void AStealthExitZone::BeginPlay()
{
	Super::BeginPlay();
	TriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &AStealthExitZone::OnTriggerBeginOverlap);
}
