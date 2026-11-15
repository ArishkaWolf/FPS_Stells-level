#include "StealthKeycard.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "UObject/ConstructorHelpers.h"

AStealthKeycard::AStealthKeycard()
{
	auto* Target = CreateDefaultSubobject<UBoxComponent>(TEXT("LootTarget"));
	SetRootComponent(Target);
	Target->InitBoxExtent(FVector(24, 24, 18));
	Target->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Target->SetCollisionResponseToAllChannels(ECR_Ignore);
	Target->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Target->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CardFinish(TEXT("/Game/Laboratory/Materials/M_StripeAmber.M_StripeAmber"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StripeFinish(TEXT("/Game/Industrial/Materials/M_MatteSteel.M_MatteSteel"));
	const auto AddPart = [this, Target](FName Name, const FVector& Position, const FVector& Scale, UMaterialInterface* Material)
	{
		auto* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Target);
		Part->SetStaticMesh(Cube.Object);
		Part->SetMaterial(0, Material);
		Part->SetRelativeLocation(Position);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
	};
	AddPart(TEXT("Card"), FVector(0, 0, -8), FVector(.13, .085, .012), CardFinish.Object);
	AddPart(TEXT("Stripe"), FVector(0, -2, -7.3), FVector(.12, .02, .002), StripeFinish.Object);
}

void AStealthKeycard::Interact_Implementation(AStealthCharacter* InteractingCharacter)
{
	if (AStealthGameMode* Mission = GetWorld()->GetAuthGameMode<AStealthGameMode>();
		Mission && Mission->CollectKeycard(InteractingCharacter))
	{
		Destroy();
	}
}

FText AStealthKeycard::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("StealthUI", "LootKeycard", "Забрать пропуск охранника");
}
