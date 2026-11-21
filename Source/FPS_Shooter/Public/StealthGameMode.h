#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StealthGameMode.generated.h"

UENUM(BlueprintType)
enum class EStealthMissionState : uint8
{
	Playing,
	Won,
	Lost
};

UENUM(BlueprintType)
enum class EStealthMissionStage : uint8
{
	SafeZone,
	Infiltration,
	Evade,
	Extraction
};

UENUM(BlueprintType)
enum class EStealthMissionStep : uint8
{
	RetrieveData,
	UploadData,
	RecoverKeycard,
	VerifyArchive,
	Extract
};

UENUM(BlueprintType)
enum class EStealthDifficulty : uint8
{
	Recruit,
	Private,
	Veteran
};

UCLASS()
class FPS_SHOOTER_API AStealthGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AStealthGameMode();
	virtual void Tick(float DeltaSeconds) override;
	virtual void StartPlay() override;

	UFUNCTION(BlueprintCallable, Category="Mission")
	bool CollectObjective(AActor* Collector);

	UFUNCTION(BlueprintCallable, Category="Mission")
	bool TryCompleteMission(AActor* ExitingActor);

	UFUNCTION(BlueprintCallable, Category="Mission")
	void DefeatPlayer(AActor* DefeatedActor);

	UFUNCTION(BlueprintCallable, Category="Mission")
	void RestartMission();

	UFUNCTION(BlueprintPure, Category="Mission")
	bool IsActorProtected(const AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category="Mission")
	bool IsPlayerPursued() const;

	UFUNCTION(BlueprintPure, Category="Mission")
	EStealthMissionStage GetMissionStage() const;

	UFUNCTION(BlueprintPure, Category="Mission")
	float GetMissionTime() const { return MissionTime; }

	FString GetObjectiveText() const;
	FString GetThreatText() const;
	float GetDetectionLevel() const;
	FVector GetSafeZoneCenter() const { return SafeZoneCenter; }

	UFUNCTION(BlueprintPure, Category="Mission")
	bool HasObjective() const { return bObjectiveCollected; }

	EStealthMissionStep GetMissionStep() const;
	EStealthDifficulty GetDifficulty() const { return Difficulty; }
	void SetDifficulty(EStealthDifficulty NewDifficulty);
	float GetNPCDamageMultiplier() const;
	bool HasExtendedObjectives() const { return bExtendedObjectives; }
	bool HasUploadedData() const { return bDataUploaded; }
	bool HasKeycard() const { return bKeycardCollected; }
	bool CanExtract() const;
	bool UploadData(AActor* Operator);
	bool CollectKeycard(AActor* Collector);
	bool VerifyArchive(AActor* Operator);

	UFUNCTION(BlueprintPure, Category="Mission")
	EStealthMissionState GetMissionState() const { return MissionState; }
	bool IsMissionResultReady() const { return bMissionResultReady; }

protected:
	bool bExtendedObjectives = false;

private:
	UPROPERTY(EditDefaultsOnly, Category="Mission", meta=(ClampMin="100.0"))
	float SafeZoneRadius = 350.0f;

	FVector SafeZoneCenter = FVector::ZeroVector;
	bool bSafeZoneInitialized = false;
	bool bInfiltrationStarted = false;
	bool bRestartRequested = false;
	bool bMissionResultReady = false;
	float MissionTime = 0.0f;
	EStealthDifficulty Difficulty = EStealthDifficulty::Private;

	UPROPERTY(VisibleAnywhere, Category="Mission")
	EStealthMissionState MissionState = EStealthMissionState::Playing;

	UPROPERTY(VisibleAnywhere, Category="Mission")
	bool bObjectiveCollected = false;
	bool bDataUploaded = false;
	bool bKeycardCollected = false;
	bool bArchiveVerified = false;
	bool CanAdvanceObjectives(const AActor* Actor) const;

	bool IsMissionPlayer(const AActor* Actor) const;
	void FinishMission(EStealthMissionState NewState);
	void ShowMissionResult();
};

UCLASS()
class FPS_SHOOTER_API AStealthLaboratoryGameMode : public AStealthGameMode
{
	GENERATED_BODY()

public:
	AStealthLaboratoryGameMode();
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	TArray<TWeakObjectPtr<class ASoundHearingNPC>> InitialGuards;
	TArray<TWeakObjectPtr<class ATargetPoint>> ReserveSpawns;
	TSet<EStealthMissionStep> DispatchedSteps;
	TOptional<EStealthMissionStep> SpawningWave;
	TArray<TWeakObjectPtr<class ASoundHearingNPC>> ActiveReserve;
	int32 GuardsSpawnedInWave = 0;
	float NextReserveSpawn = 0.0f;
	int32 GetReinforcementsPerWave() const;
	void SpawnReserveGuard();
};
