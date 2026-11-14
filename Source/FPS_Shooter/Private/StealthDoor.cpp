#include "StealthDoor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "SoundRayInteractionComponent.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "UObject/ConstructorHelpers.h"

AStealthDoor::AStealthDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Hinge")));
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(GetRootComponent());
	DoorMesh->SetMobility(EComponentMobility::Movable);
	DoorMesh->SetRelativeLocation(FVector(0, 50, 100));
	DoorMesh->SetRelativeScale3D(FVector(0.16f, 1.0f, 2.0f));
	DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
	DoorMesh->SetStaticMesh(Cube.Object);
	SoundEmitter = CreateDefaultSubobject<USoundRayInteractionComponent>(TEXT("SoundEmitter"));
	SoundEmitter->SoundOffset = FVector(0, 50, 100);
}

void AStealthDoor::BeginPlay()
{
	Super::BeginPlay();
	ClosedLeaf = DoorMesh->GetRelativeTransform();
}

void AStealthDoor::Interact_Implementation(AStealthCharacter* InteractingCharacter)
{
	if (IsValid(InteractingCharacter) && InteractingCharacter->CanPerformMissionAction())
	{
		SetOpen(!bIsOpen, InteractingCharacter);
	}
}

FText AStealthDoor::GetInteractionPrompt_Implementation() const
{
	if (IsAccessDenied()) { return AccessDeniedText(); }
	if (bRequiresKeycard && !bIsOpen)
	{
		return NSLOCTEXT("StealthUI", "UseKeycard", "Открыть дверь пропуском");
	}
	return bIsOpen ? NSLOCTEXT("StealthUI", "CloseDoor", "Закрыть дверь")
		: NSLOCTEXT("StealthUI", "OpenDoor", "Открыть дверь");
}

bool AStealthDoor::IsAccessDenied() const
{
	const AStealthGameMode* Mission = GetWorld()->GetAuthGameMode<AStealthGameMode>();
	return bRequiresKeycard && !bIsOpen && (!Mission || !Mission->HasKeycard());
}

FText AStealthDoor::AccessDeniedText()
{
	return NSLOCTEXT("StealthUI", "AccessDenied", "Доступ запрещён. Требуется пропуск");
}

bool AStealthDoor::ShowsAccessWarningTo(const AStealthCharacter& Character) const
{
	if (!IsAccessDenied()) { return false; }
	const FVector Player = Character.GetActorLocation();
	const FVector Leaf = DoorMesh->GetComponentLocation();
	if (FVector::DistSquared2D(Player, Leaf) > FMath::Square(210.0) || FMath::Abs(Player.Z - Leaf.Z) > 110.0)
	{
		return false;
	}
	FHitResult Hit;
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(KeycardDoorNotice), false, &Character);
	return GetWorld()->LineTraceSingleByChannel(Hit, Player, Leaf, ECC_Visibility, Query) && Hit.GetActor() == this;
}

bool AStealthDoor::SetOpen(bool bOpen, AActor* Initiator)
{
	if (bIsOpen == bOpen || (bOpen && IsAccessDenied())) { return false; }
	if (const AStealthGameMode* Mission = GetWorld() ? GetWorld()->GetAuthGameMode<AStealthGameMode>() : nullptr;
		Mission && Mission->GetMissionState() != EStealthMissionState::Playing) { return false; }
	bIsOpen = bOpen;
	SoundEmitter->NotifyDoor(bIsOpen, Initiator);
	return true;
}

void AStealthDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Previous = OpenAlpha;
	OpenAlpha = FMath::FInterpConstantTo(OpenAlpha, bIsOpen ? 1.0f : 0.0f, DeltaSeconds, 1.0f / FMath::Max(0.1f, OpenDuration));
	if (!FMath::IsNearlyEqual(Previous, OpenAlpha))
	{
		// Rotate the leaf around the hinge, not around its mesh center.
		const FQuat Swing(FRotator(0, OpenAngle * FMath::SmoothStep(0.0f, 1.0f, OpenAlpha), 0));
		DoorMesh->SetRelativeLocationAndRotation(Swing.RotateVector(ClosedLeaf.GetLocation()), Swing * ClosedLeaf.GetRotation());
	}
}
