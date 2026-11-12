#include "StealthSupplyPickup.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SoundRayInteractionComponent.h"
#include "StealthCharacter.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "StealthSupplies"

AStealthSupplyPickup::AStealthSupplyPickup()
{
	Target = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	SetRootComponent(Target);
	Target->InitBoxExtent(FVector(29, 22, 15));
	Target->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Target->SetCollisionResponseToAllChannels(ECR_Ignore);
	Target->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Target->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	const auto MakePart = [this](FName Name, FVector Position, FVector Scale)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Target);
		Part->SetStaticMesh(Cube.Object);
		Part->SetRelativeLocation(Position);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		return Part;
	};
	Case = MakePart(TEXT("Case"), FVector(0, 0, -1), FVector(.52, .36, .26));
	Lid = MakePart(TEXT("Lid"), FVector(0, 0, 9), FVector(.55, .39, .065));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Finish(TEXT("/Game/Industrial/Materials/M_SupplyCase.M_SupplyCase"));
	CaseFinish = Finish.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Fittings(TEXT("/Game/Industrial/Materials/M_MatteSteel.M_MatteSteel"));
	for (const int32 Side : {-1, 1})
	{
		UStaticMeshComponent* Latch = MakePart(Side < 0 ? TEXT("LeftLatch") : TEXT("RightLatch"),
			FVector(-26, 11 * Side, 4), FVector(.025, .046, .077));
		Latch->SetMaterial(0, Fittings.Object);
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Markings.Add(MakePart(*FString::Printf(TEXT("Mark%d"), Index), FVector(0, 0, 12.5), FVector(.15, .05, .012)));
	}
	Sound = CreateDefaultSubobject<USoundRayInteractionComponent>(TEXT("SupplySound"));
	Sound->InteractionVolume = 0.32f;
	Sound->InteractionDistance = 350.0f;
}

void AStealthSupplyPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateAppearance();
}

void AStealthSupplyPickup::BeginPlay()
{
	Super::BeginPlay();
	UpdateAppearance();
}

void AStealthSupplyPickup::UpdateAppearance()
{
	const bool bMedkit = Kind == EStealthSupplyKind::Medkit;
	const auto Tint = [this](UStaticMeshComponent* Part, const FLinearColor& Color)
	{
		if (!Part || !CaseFinish) { return; }
		if (UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0, CaseFinish))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
	};
	Tint(Case, bMedkit ? FLinearColor(0.28f, 0.035f, 0.027f) : FLinearColor(0.15f, 0.19f, 0.07f));
	Tint(Lid, bMedkit ? FLinearColor(0.39f, 0.052f, 0.040f) : FLinearColor(0.21f, 0.25f, 0.10f));
	for (int32 Index = 0; Index < Markings.Num(); ++Index)
	{
		UStaticMeshComponent* Mark = Markings[Index];
		Mark->SetVisibility(!bMedkit || Index < 2);
		Mark->SetRelativeLocation(FVector(0, bMedkit ? 0 : 7 * (Index - 1), 13));
		Mark->SetRelativeScale3D(bMedkit && Index == 1 ? FVector(.05, .15, .012) : FVector(.15, .045, .012));
		Tint(Mark, bMedkit ? FLinearColor::White : FLinearColor(0.80f, 0.67f, 0.32f));
	}
}

void AStealthSupplyPickup::Interact_Implementation(AStealthCharacter* Character)
{
	if (!IsValid(Character)) { return; }
	if (Kind == EStealthSupplyKind::Medkit)
	{
		Character->TryStartHealing(*this);
	}
	else if (Character->TryResupplyAmmo(AmmoAmount))
	{
		Consume(*Character);
	}
}

FText AStealthSupplyPickup::GetInteractionPrompt_Implementation() const
{
	return Kind == EStealthSupplyKind::Medkit ? LOCTEXT("UseMedkit", "Использовать аптечку")
		: LOCTEXT("Resupply", "Пополнить боезапас");
}

FText AStealthSupplyPickup::GetPromptFor(const AStealthCharacter& Character) const
{
	if (Kind == EStealthSupplyKind::Medkit && Character.GetHealthFraction() >= 1.0f)
	{
		return LOCTEXT("FullHealth", "Здоровье полное");
	}
	if (Kind == EStealthSupplyKind::Ammo && Character.GetReserveAmmo() >= Character.GetReserveAmmoCapacity())
	{
		return LOCTEXT("FullReserve", "Боезапас полный");
	}
	return GetInteractionPrompt_Implementation();
}

void AStealthSupplyPickup::Consume(AStealthCharacter& Character)
{
	Sound->NotifyInteraction(&Character);
	Destroy();
}

#undef LOCTEXT_NAMESPACE
