#include "NoiseMakerPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "StealthCharacter.h"
#include "SoundRayInteractionComponent.h"
#include "UObject/ConstructorHelpers.h"

ANoiseMakerPickup::ANoiseMakerPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	SoundEmitter = CreateDefaultSubobject<USoundRayInteractionComponent>(TEXT("SoundEmitter"));

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(35.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	CollisionComponent->SetCanEverAffectNavigation(false);
	SetRootComponent(CollisionComponent);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeScale3D(FVector(0.36f));
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 6.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Steel(TEXT("/Game/Industrial/Materials/M_MatteSteel.M_MatteSteel"));
	if (Steel.Succeeded()) { MeshComponent->SetMaterial(0, Steel.Object); }
}

void ANoiseMakerPickup::Interact_Implementation(AStealthCharacter* InteractingCharacter)
{
	if (!IsValid(InteractingCharacter))
	{
		return;
	}

	InteractingCharacter->AddNoiseMakers(Amount);
	SoundEmitter->NotifyInteraction(InteractingCharacter);
	Destroy();
}

FText ANoiseMakerPickup::GetInteractionPrompt_Implementation() const
{
	return FText::Format(NSLOCTEXT("StealthUI", "TakeNoiseMakers", "Взять приманки (+{0})"), FText::AsNumber(Amount));
}
