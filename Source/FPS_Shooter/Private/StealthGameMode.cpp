#include "StealthGameMode.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "StealthAIController.h"
#include "StealthCharacter.h"
#include "StealthHUD.h"
#include "StealthPlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthMission, Log, All);

AStealthGameMode::AStealthGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AStealthCharacter::StaticClass();
	HUDClass = AStealthHUD::StaticClass();
	PlayerControllerClass = AStealthPlayerController::StaticClass();
}

AStealthLaboratoryGameMode::AStealthLaboratoryGameMode()
{
	bExtendedObjectives = true;
}

void AStealthGameMode::StartPlay()
{
	// Initialize before Super starts AI and other actors.
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		SafeZoneCenter = Player->GetActorLocation();
		bSafeZoneInitialized = true;
	}
	Super::StartPlay();
	// Replace stale prototype tiles using the current mission bounds so all destinations are reachable.
	if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		Navigation->Build();
	}
}

void AStealthGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (MissionState != EStealthMissionState::Playing)
	{
		return;
	}
	MissionTime += DeltaSeconds;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Player && !bSafeZoneInitialized)
	{
		SafeZoneCenter = Player->GetActorLocation();
		bSafeZoneInitialized = true;
	}
	if (Player && !bInfiltrationStarted && !IsActorProtected(Player))
	{
		bInfiltrationStarted = true;
		UE_LOG(LogStealthMission, Display, TEXT("Player left staging area. Infiltration started."));
	}
}

bool AStealthGameMode::IsMissionPlayer(const AActor* Actor) const
{
	const AStealthCharacter* Player = Cast<AStealthCharacter>(Actor);
	return IsValid(Player) && Player == UGameplayStatics::GetPlayerPawn(this, 0) && Player->GetHealth() > 0.0f;
}

bool AStealthGameMode::IsActorProtected(const AActor* Actor) const
{
	// Protection ends permanently on first departure: returning cannot grant invulnerability.
	return IsValid(Actor) && bSafeZoneInitialized && !bInfiltrationStarted
		&& FVector::DistSquared2D(Actor->GetActorLocation(), SafeZoneCenter) <= FMath::Square(SafeZoneRadius)
		&& FMath::Abs(Actor->GetActorLocation().Z - SafeZoneCenter.Z) < 250.0f;
}

bool AStealthGameMode::IsPlayerPursued() const
{
	for (TActorIterator<AStealthAIController> It(GetWorld()); It; ++It)
	{
		if (IsValid(It->GetPawn()) && It->HasActivePursuit())
		{
			return true;
		}
	}
	return false;
}

float AStealthGameMode::GetDetectionLevel() const
{
	float Detection = 0.0f;
	for (TActorIterator<AStealthAIController> It(GetWorld()); It; ++It)
	{
		if (IsValid(It->GetPawn()))
		{
			Detection = FMath::Max(Detection, It->GetDetectionLevel());
		}
	}
	return Detection;
}

EStealthMissionStage AStealthGameMode::GetMissionStage() const
{
	if (!bInfiltrationStarted && IsActorProtected(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		return EStealthMissionStage::SafeZone;
	}
	if (IsPlayerPursued())
	{
		return EStealthMissionStage::Evade;
	}
	return CanExtract() ? EStealthMissionStage::Extraction : EStealthMissionStage::Infiltration;
}

EStealthMissionStep AStealthGameMode::GetMissionStep() const
{
	if (!bObjectiveCollected) { return EStealthMissionStep::RetrieveData; }
	if (bExtendedObjectives)
	{
		if (!bDataUploaded) { return EStealthMissionStep::UploadData; }
		if (!bKeycardCollected) { return EStealthMissionStep::RecoverKeycard; }
		if (!bArchiveVerified) { return EStealthMissionStep::VerifyArchive; }
	}
	return EStealthMissionStep::Extract;
}

bool AStealthGameMode::CanExtract() const
{
	return GetMissionStep() == EStealthMissionStep::Extract;
}

FString AStealthGameMode::GetObjectiveText() const
{
	switch (GetMissionStage())
	{
	case EStealthMissionStage::SafeZone:
		return TEXT("Покиньте безопасную зону");
	case EStealthMissionStage::Evade:
		return TEXT("Скройтесь из виду и переждите поиск");
	case EStealthMissionStage::Extraction:
		return TEXT("Доберитесь до точки эвакуации");
	default:
		break;
	}
	switch (GetMissionStep())
	{
	case EStealthMissionStep::UploadData:
		return TEXT("Загрузите данные в диспетчерской");
	case EStealthMissionStep::RecoverKeycard:
		return TEXT("Заберите пропуск у охранника архива");
	case EStealthMissionStep::VerifyArchive:
		return TEXT("Сверьте архив в закрытой аналитической");
	default:
		return TEXT("Заберите данные в серверной");
	}
}

FString AStealthGameMode::GetThreatText() const
{
	bool bInvestigating = false;
	bool bSuspicious = false;
	for (TActorIterator<AStealthAIController> It(GetWorld()); It; ++It)
	{
		if (!IsValid(It->GetPawn()))
		{
			continue;
		}
		if (It->GetAIState() == EStealthAIState::Chase)
		{
			return TEXT("Вас заметили. Скройтесь из виду!");
		}
		bInvestigating |= It->GetAIState() != EStealthAIState::Patrol;
		bSuspicious |= It->GetAIState() == EStealthAIState::Suspicious;
	}
	return IsPlayerPursued() ? TEXT("Охрана ведёт поиск. Оставайтесь в укрытии.")
		: bSuspicious ? TEXT("Охранник насторожился. Двигайтесь тихо.")
		: bInvestigating ? TEXT("Охрана проверяет шум. Оставайтесь в укрытии.")
		: TEXT("Вы скрыты. Присед приглушает шаги. [Q] бросает приманку.");
}

bool AStealthGameMode::CollectObjective(AActor* Collector)
{
	if (bObjectiveCollected || !CanAdvanceObjectives(Collector))
	{
		return false;
	}
	bInfiltrationStarted = true;
	bObjectiveCollected = true;
	UE_LOG(LogStealthMission, Display, TEXT("Intel secured."));
	return true;
}

bool AStealthGameMode::CanAdvanceObjectives(const AActor* Actor) const
{
	return MissionState == EStealthMissionState::Playing && IsMissionPlayer(Actor) && !IsActorProtected(Actor);
}

bool AStealthGameMode::UploadData(AActor* Operator)
{
	if (!bExtendedObjectives || GetMissionStep() != EStealthMissionStep::UploadData || !CanAdvanceObjectives(Operator))
	{
		return false;
	}
	bDataUploaded = true;
	return true;
}

bool AStealthGameMode::CollectKeycard(AActor* Collector)
{
	if (!bExtendedObjectives || bKeycardCollected || !CanAdvanceObjectives(Collector))
	{
		return false;
	}
	bKeycardCollected = true;
	return true;
}

bool AStealthGameMode::VerifyArchive(AActor* Operator)
{
	if (!bExtendedObjectives || GetMissionStep() != EStealthMissionStep::VerifyArchive || !CanAdvanceObjectives(Operator))
	{
		return false;
	}
	bArchiveVerified = true;
	return true;
}

bool AStealthGameMode::TryCompleteMission(AActor* ExitingActor)
{
	if (MissionState != EStealthMissionState::Playing || !CanExtract() || !IsMissionPlayer(ExitingActor)
		|| IsPlayerPursued())
	{
		return false;
	}
	FinishMission(EStealthMissionState::Won);
	return true;
}

void AStealthGameMode::DefeatPlayer(AActor* DefeatedActor)
{
	const AStealthCharacter* Player = Cast<AStealthCharacter>(DefeatedActor);
	if (MissionState == EStealthMissionState::Playing && IsValid(Player)
		&& Player == UGameplayStatics::GetPlayerPawn(this, 0) && Player->GetHealth() <= 0.0f)
	{
		FinishMission(EStealthMissionState::Lost);
	}
}

void AStealthGameMode::FinishMission(EStealthMissionState NewState)
{
	if (MissionState != EStealthMissionState::Playing || NewState == EStealthMissionState::Playing)
	{
		return;
	}
	MissionState = NewState;
	UE_LOG(LogStealthMission, Display, TEXT("Mission %s after %.1f seconds."),
		NewState == EStealthMissionState::Won ? TEXT("WON") : TEXT("LOST"), MissionTime);
	if (AStealthCharacter* Player = Cast<AStealthCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		Player && NewState == EStealthMissionState::Won)
	{
		Player->StopMissionActions();
	}
	for (TActorIterator<AStealthAIController> It(GetWorld()); It; ++It)
	{
		It->StopMovement();
		It->ClearFocus(EAIFocusPriority::Gameplay);
	}
	if (NewState == EStealthMissionState::Lost)
	{
		FTimerHandle Presentation;
		GetWorldTimerManager().SetTimer(Presentation, this, &AStealthGameMode::ShowMissionResult,
			AStealthCharacter::DeathPresentationDuration, false);
		return;
	}
	ShowMissionResult();
}

void AStealthGameMode::ShowMissionResult()
{
	if (bRestartRequested) { return; }
	bMissionResultReady = true;
	if (AStealthPlayerController* PC = Cast<AStealthPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->ShowMissionResult();
	}
}

void AStealthGameMode::RestartMission()
{
	if (MissionState == EStealthMissionState::Playing || bRestartRequested)
	{
		return;
	}
	bRestartRequested = true;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetPause(false);
		PC->ResetIgnoreMoveInput();
		PC->ResetIgnoreLookInput();
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
