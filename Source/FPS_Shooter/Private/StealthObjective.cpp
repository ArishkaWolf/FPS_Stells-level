#include "StealthObjective.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "SoundRayInteractionComponent.h"
#include "UObject/ConstructorHelpers.h"

AStealthObjective::AStealthObjective()
{
	PrimaryActorTick.bCanEverTick = false;
	SoundEmitter = CreateDefaultSubobject<USoundRayInteractionComponent>(TEXT("SoundEmitter"));

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(55.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionComponent->SetCanEverAffectNavigation(false);
	SetRootComponent(CollisionComponent);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ScreenMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Steel(TEXT("/Game/Industrial/Materials/M_MatteSteel.M_MatteSteel"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Shell(TEXT("/Game/Industrial/Materials/M_RibbedShutter.M_RibbedShutter"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Screen(TEXT("/Game/Industrial/Materials/M_IntelDisplay.M_IntelDisplay"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Trim(TEXT("/Game/Industrial/Materials/M_SiteMarking.M_SiteMarking"));

	const auto SetPiece = [](UStaticMeshComponent* Piece, UStaticMesh* Mesh, UMaterialInterface* Material,
		const FVector& Position, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		Piece->SetStaticMesh(Mesh);
		Piece->SetMaterial(0, Material);
		Piece->SetRelativeLocation(Position);
		Piece->SetRelativeScale3D(Scale);
		Piece->SetRelativeRotation(Rotation);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCanEverAffectNavigation(false);
	};
	const auto AddPiece = [this, &SetPiece](FName Name, const FVector& Position, const FVector& Scale,
		UMaterialInterface* Material, const FRotator& Rotation = FRotator::ZeroRotator, UStaticMesh* Mesh = nullptr)
	{
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Piece->SetupAttachment(CollisionComponent);
		SetPiece(Piece, Mesh ? Mesh : CubeMesh.Object.Get(), Material, Position, Scale, Rotation);
		return Piece;
	};

	SetPiece(MeshComponent, CubeMesh.Object, Steel.Object, FVector(0, 0, -4), FVector(.44, .59, .10));
	const FRotator DisplayTilt(27, 0, 0);
	const FVector DisplayCenter(5, 0, 8);
	AddPiece(TEXT("DisplayHousing"), DisplayCenter, FVector(.31, .49, .042), Steel.Object, DisplayTilt);
	UStaticMeshComponent* Surface = AddPiece(TEXT("DisplaySurface"),
		DisplayCenter + DisplayTilt.RotateVector(FVector(0, 0, 2.2)), FVector(.275, .45, 1),
		Screen.Object, DisplayTilt, ScreenMesh.Object);
	Surface->SetCastShadow(false);
	for (const int32 Side : {-1, 1})
	{
		const bool bLeft = Side < 0;
		AddPiece(bLeft ? TEXT("LeftBumper") : TEXT("RightBumper"),
			FVector(0, 27.5 * Side, -1), FVector(.46, .06, .14), Shell.Object);
		AddPiece(bLeft ? TEXT("LeftLatch") : TEXT("RightLatch"),
			FVector(-22, 19 * Side, -2), FVector(.03, .055, .062), Trim.Object);
	}
	AddPiece(TEXT("Keypad"), FVector(-15, 0, 2), FVector(.10, .36, .02), Shell.Object);
	AddPiece(TEXT("StatusLamp"), FVector(-15, -19, 2), FVector(.036, .029, .02), Trim.Object);

	LightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComponent"));
	LightComponent->SetupAttachment(RootComponent);
	LightComponent->SetRelativeLocation(FVector(0, 0, 22));
	LightComponent->SetLightColor(FLinearColor(.40f, .85f, .73f));
	LightComponent->SetIntensity(28.0f);
	LightComponent->SetAttenuationRadius(150.0f);
	LightComponent->SetCastShadows(false);
}

void AStealthObjective::Interact_Implementation(AStealthCharacter* InteractingCharacter)
{
	AStealthGameMode* GameMode = Cast<AStealthGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode && GameMode->CollectObjective(InteractingCharacter))
	{
		SoundEmitter->NotifyInteraction(InteractingCharacter);
		Destroy();
	}
}

FText AStealthObjective::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("StealthUI", "CollectIntel", "Забрать данные");
}
