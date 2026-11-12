#include "SoundHearingNPC.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "NPCHearingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "StealthAIController.h"
#include "StealthGameMode.h"
#include "StealthNPCAnimInstance.h"
#include "SoundRayEmitterComponent.h"
#include "UObject/ConstructorHelpers.h"

ASoundHearingNPC::ASoundHearingNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AStealthAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	// CharacterMovement applies the configured turn rate, including stationary search sweeps.
	bUseControllerRotationYaw = false;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"));
	if (MannyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannyMesh.Object);
		StealthBallistics::ConfigureMannequinHitMesh(*GetMesh());
	}

	GetMesh()->SetAnimInstanceClass(UStealthNPCAnimInstance::StaticClass());
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleClip(TEXT("/Game/AnimStarterPack/UE5/UE5_Idle_Rifle_Hip"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkClip(TEXT("/Game/AnimStarterPack/UE5/UE5_Walk_Fwd_Rifle_Ironsights"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunClip(TEXT("/Game/AnimStarterPack/UE5/UE5_Jog_Fwd_Rifle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> FireClip(TEXT("/Game/AnimStarterPack/UE5/UE5_Fire_Rifle_Hip"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> HitClip(TEXT("/Game/AnimStarterPack/UE5/UE5_Hit_React_1"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DeathClip(TEXT("/Game/AnimStarterPack/UE5/UE5_Death_1"));
	IdleAnimation = IdleClip.Object; WalkAnimation = WalkClip.Object; RunAnimation = RunClip.Object;
	FireAnimation = FireClip.Object; HitAnimation = HitClip.Object; DeathAnimation = DeathClip.Object;

	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 420.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetRelativeLocation(FVector(2.0f, 10.0f, 0.0f));
	WeaponMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RifleMesh(TEXT("/Game/MilitaryWeapSilver/Weapons/Assault_Rifle_A.Assault_Rifle_A"));
	if (RifleMesh.Succeeded())
	{
		WeaponMesh->SetSkeletalMesh(RifleMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<USoundBase> RifleFireSound(TEXT("/Game/MilitaryWeapSilver/Sound/Rifle/Cues/RifleA_Fire_Cue.RifleA_Fire_Cue"));
	FireSound = RifleFireSound.Object;
	static ConstructorHelpers::FObjectFinder<UParticleSystem> RifleMuzzleFlash(TEXT("/Game/MilitaryWeapSilver/FX/P_AssaultRifle_MuzzleFlash.P_AssaultRifle_MuzzleFlash"));
	MuzzleFlash = RifleMuzzleFlash.Object;
	ImpactEffects.LoadDefaults();

	HearingComponent = CreateDefaultSubobject<UNPCHearingComponent>(TEXT("HearingComponent"));
	HearingComponent->HearingRadius = 220.0f;
	HearingComponent->ForgetDurationSeconds = 12.0f;
	HearingComponent->bDrawDebug = false;
	SoundEmitter = CreateDefaultSubobject<USoundRayEmitterComponent>(TEXT("SoundEmitter"));
	SoundEmitter->bFootstepsEmitSoundRays = false;
	SoundEmitter->MinSpeedForFootsteps = 35.0f;
	SoundEmitter->FootstepPlaybackVolume = 0.75f;
	static ConstructorHelpers::FObjectFinder<USoundBase> ConcreteStep(TEXT("/Game/Audio/Stealth/ConcreteStep.ConcreteStep"));
	SoundEmitter->FootstepSound = ConcreteStep.Object;
	auto* StepAttenuation = CreateDefaultSubobject<USoundAttenuation>(TEXT("FootstepAttenuation"));
	StepAttenuation->Attenuation.AttenuationShapeExtents = FVector(175, 0, 0);
	StepAttenuation->Attenuation.FalloffDistance = 1350.0f;
	StepAttenuation->Attenuation.bEnableOcclusion = true;
	StepAttenuation->Attenuation.OcclusionVolumeAttenuation = 0.50f;
	StepAttenuation->Attenuation.OcclusionLowPassFilterFrequency = 1400.0f;
	SoundEmitter->FootstepAttenuation = StepAttenuation;
	SoundEmitter->RayCount = 16;
	SoundEmitter->RayPitchDegrees = 0.0f;
	SoundEmitter->Volume = 1.6f;
	SoundEmitter->MaxDistance = 3000.0f;
	NPCLabel = FText::FromString(TEXT("NPC"));

	SoundLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SoundLabel"));
	SoundLabel->SetupAttachment(GetRootComponent());
	SoundLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 135.0f));
	SoundLabel->SetHorizontalAlignment(EHTA_Center);
	SoundLabel->SetVerticalAlignment(EVRTA_TextCenter);
	SoundLabel->SetTextRenderColor(FColor::Cyan);
	SoundLabel->SetWorldSize(28.0f);
	static ConstructorHelpers::FObjectFinder<UFont> RussianSigns(TEXT("/Game/Interface/RussianSigns.RussianSigns"));
	if (RussianSigns.Succeeded()) { SoundLabel->SetFont(RussianSigns.Object); }
	SoundLabel->SetText(FText::FromString(TEXT("Шум 0%")));
	SoundLabel->SetVisibility(false);
	CurrentHealth = MaxHealth;
}

void ASoundHearingNPC::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = FMath::Max(1.0f, MaxHealth);
	// Calibrate in the actual rifle idle pose. Model +Y is the barrel direction.
	GetMesh()->TickAnimation(0.0f, false);
	GetMesh()->RefreshBoneTransforms();
	const FTransform Hand = GetMesh()->GetSocketTransform(TEXT("hand_r"));
	const FQuat Rotation = Hand.GetRotation().Inverse()
		* FRotationMatrix::MakeFromYZ(GetActorForwardVector(), FVector::UpVector).ToQuat();
	const FVector Grip = WeaponMesh->GetSocketTransform(TEXT("Grip_Bone"), RTS_Component).GetLocation();
	WeaponMesh->SetRelativeTransform(FTransform(Rotation, -Rotation.RotateVector(Grip)));
}

bool ASoundHearingNPC::FireWeaponAt(AActor* TargetActor)
{
	if (bDead || !IsValid(TargetActor) || !GetWorld())
	{
		return false;
	}
	if (const AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>();
		GameMode && (GameMode->GetMissionState() != EStealthMissionState::Playing || GameMode->IsActorProtected(TargetActor)))
	{
		return false;
	}

	if (FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation()) > FMath::Square(WeaponRange)) { return false; }
	const FVector Chest = GetMesh()->GetSocketLocation(TEXT("spine_04"));
	const FVector Muzzle = WeaponMesh->DoesSocketExist(TEXT("MuzzleFlash"))
		? WeaponMesh->GetSocketLocation(TEXT("MuzzleFlash")) : Chest;
	const ACharacter* Target = Cast<ACharacter>(TargetActor);
	const FVector TargetLocation = Target && Target->GetMesh()->DoesSocketExist(TEXT("spine_04"))
		? Target->GetMesh()->GetSocketLocation(TEXT("spine_04")) : TargetActor->GetActorLocation();
	LastShotAimDirection = (TargetLocation - Muzzle).GetSafeNormal();
	LastShotDirection = FMath::VRandCone(LastShotAimDirection, FMath::DegreesToRadians(FMath::Clamp(FireSpreadDegrees, 0.0f, 12.0f)));
	FireAnimationAge = 0.0f;
	++ShotsFired;
	SoundEmitter->EmitSoundEventAtLocation(ESoundEventType::Gunshot, Muzzle, -1.0f, -1.0f, this);
	StealthBallistics::Fire(*this, Chest, Muzzle, Muzzle, LastShotDirection, WeaponRange, FireDamage, ImpactEffects);

	if (MuzzleFlash)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleFlash, WeaponMesh, TEXT("MuzzleFlash"));
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Muzzle);
	}
	return true;
}

float ASoundHearingNPC::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f || bDead)
	{
		return 0.0f;
	}
	if (const AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>();
		GameMode && GameMode->GetMissionState() != EStealthMissionState::Playing)
	{
		return 0.0f;
	}
	const float AppliedDamage = FMath::Clamp(Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser), 0.0f, CurrentHealth);
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - AppliedDamage);
	if (AppliedDamage <= 0.0f) { return 0.0f; }
	HitAnimationAge = 0.0f;
	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
	else if (AStealthAIController* AI = Cast<AStealthAIController>(GetController()))
	{
		AI->ReactToDamage(EventInstigator && EventInstigator->GetPawn() ? EventInstigator->GetPawn() : DamageCauser);
	}
	return AppliedDamage;
}

void ASoundHearingNPC::Die()
{
	bDead = true;
	DeathAnimationAge = 0.0f;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HearingComponent->ClearSoundMemory();
	HearingComponent->Deactivate();
	HearingComponent->SetComponentTickEnabled(false);
	SoundEmitter->SetComponentTickEnabled(false);
	SoundLabel->SetText(NSLOCTEXT("StealthAI", "Dead", "НЕЙТРАЛИЗОВАН"));
	SoundLabel->SetTextRenderColor(FColor::Silver);
	if (AStealthAIController* AI = Cast<AStealthAIController>(GetController()))
	{
		AI->StopForDeath();
		AI->UnPossess();
		AI->SetLifeSpan(2.0f);
	}
	// Leave the final death pose visible; the corpse never blocks navigation or extraction.
	SetActorEnableCollision(false);
	SetLifeSpan(35.0f);
}

void ASoundHearingNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SoundLabel->SetVisibility(IsSoundRayDebugEnabled());
	FireAnimationAge += DeltaSeconds;
	HitAnimationAge += DeltaSeconds;
	if (bDead)
	{
		DeathAnimationAge += DeltaSeconds;
		return;
	}
	SoundEmitter->FootstepInterval = FMath::GetMappedRangeValueClamped(
		FVector2D(180, 430), FVector2D(0.51, 0.33), GetVelocity().Size2D());

	if (!HearingComponent || !SoundLabel)
	{
		return;
	}

	const int32 SoundPercent = FMath::RoundToInt(FMath::Clamp(HearingComponent->CurrentSoundStrength, 0.0f, 1.0f) * 100.0f);
	const AStealthAIController* StealthController = Cast<AStealthAIController>(GetController());
	const int32 DetectionPercent = StealthController
		? FMath::RoundToInt(StealthController->GetDetectionLevel() * 100.0f)
		: 0;
	const FText StateName = StealthController
		? StealthController->GetStateDisplayName()
		: NSLOCTEXT("StealthAI", "NoController", "БЕЗ УПРАВЛЕНИЯ");
	SoundLabel->SetText(FText::Format(
		NSLOCTEXT("SoundNPC", "NPCStatus", "{0}\n{1}  Зрение {2}%\nШум {3}%"),
		NPCLabel,
		StateName,
		FText::AsNumber(DetectionPercent),
		FText::AsNumber(SoundPercent)));

	const APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager : nullptr;
	if (CameraManager)
	{
		const FVector ToCamera = CameraManager->GetCameraLocation() - SoundLabel->GetComponentLocation();
		SoundLabel->SetWorldRotation(ToCamera.Rotation());
	}

}
