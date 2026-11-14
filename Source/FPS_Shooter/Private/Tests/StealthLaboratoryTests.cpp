#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/GameViewportClient.h"
#include "Engine/RectLight.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Slate/SceneViewport.h"
#include "SoundHearingNPC.h"
#include "StealthAIController.h"
#include "StealthCharacter.h"
#include "StealthDoor.h"
#include "StealthExitZone.h"
#include "StealthGameMode.h"
#include "StealthKeycard.h"
#include "StealthObjective.h"
#include "StealthPatrolPoint.h"
#include "Tests/AutomationCommon.h"

namespace
{
class FLaboratoryWalkthrough final : public IAutomationLatentCommand
{
public:
	explicit FLaboratoryWalkthrough(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 100.0)
		{
			Test.AddError(TEXT("Timed out while checking the research facility"));
			return true;
		}
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!World || World->WorldType != EWorldType::PIE || World->GetTimeSeconds() < ReadyAt ||
			UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) { return false; }
		APlayerController* PC = World->GetFirstPlayerController();
		auto* Player = PC ? Cast<AStealthCharacter>(PC->GetPawn()) : nullptr;
		auto* Mission = World->GetAuthGameMode<AStealthGameMode>();
		if (!Player || !Mission) { Test.AddError(TEXT("Missing the live mission or player")); return true; }

		switch (Stage++)
		{
		case 0:
			if (!CheckLayout(*World, *Player)) { return true; }
			View(*PC, *Player, FVector(980, 1710, 98), FVector(2440, 1825, 169));
			break;
		case 1:
			Capture(*World, TEXT("Corridor"));
			View(*PC, *Player, FVector(1525, 2160, 98), FVector(1280, 2560, 123));
			break;
		case 2:
			Capture(*World, TEXT("Infirmary"));
			View(*PC, *Player, FVector(2515, 2695, 98), FVector(2750, 2935, 146));
			break;
		case 3:
			Capture(*World, TEXT("WetLab"));
			View(*PC, *Player, FVector(3530, 2190, 98), FVector(3740, 1930, 148));
			break;
		case 4:
			Capture(*World, TEXT("WetLab-Opposite-Wall"));
			View(*PC, *Player, FVector(4920, 665, 98), FVector(4490, 375, 145));
			break;
		case 5:
			Capture(*World, TEXT("Plant"));
			View(*PC, *Player, FVector(2590, 1150, 98), FVector(2480, 415, 155));
			break;
		case 6:
			Capture(*World, TEXT("Control"));
			View(*PC, *Player, FVector(3500, 1210, 98), FVector(3500, 510, 151));
			break;
		case 7:
			Capture(*World, TEXT("Containment"));
			View(*PC, *Player, FVector(1355, 1190, 98), FVector(1210, 775, 144));
			break;
		case 8:
			Capture(*World, TEXT("Security"));
			View(*PC, *Player, FVector(4440, 2200, 98), FVector(4440, 2850, 160));
			ServerDoor->SetOpen(true, Player);
			break;
		case 9:
			Capture(*World, TEXT("ServerRoom"));
			Test.TestFalse(TEXT("Open server door clears the main entrance"), DoorBlocks(*World));
			CheckRoute(*World, TEXT("Main route with the server door open"), FVector(4450, 1770, 5), FVector(4450, 2030, 5));
			View(*PC, *Player, Objective->GetActorLocation() - Objective->GetActorForwardVector() * 132 +
				FVector(0, 0, -2), Objective->GetActorLocation());
			break;
		case 10:
			Capture(*World, TEXT("DataTerminal"));
			Test.TestTrue(TEXT("The terminal is visible to the interaction trace"), Player->GetInteractionPrompt().ToString() == TEXT("Забрать данные"));
			Tap(*PC, EKeys::E);
			Test.TestTrue(TEXT("E downloads the data at the terminal"), Mission->HasObjective());
			Test.TestEqual(TEXT("Stage 2 is the dispatcher upload"), Mission->GetMissionStep(), EStealthMissionStep::UploadData);
			Test.TestFalse(TEXT("The exit requires the remaining stages"), Mission->TryCompleteMission(Player));
			View(*PC, *Player, FVector(5050, 1698, 98), FVector(5194, 1698, 117));
			break;
		case 11:
			Capture(*World, TEXT("Keycard-Denied"));
			Test.TestEqual(TEXT("A nearby player sees the access warning"), Player->GetAccessWarning().ToString(),
				AStealthDoor::AccessDeniedText().ToString());
			PC->SetControlRotation(FRotator(0, 180, 0));
			Test.TestFalse(TEXT("Proximity warning does not require aiming"), Player->GetAccessWarning().IsEmpty());
			PC->SetControlRotation(FRotator::ZeroRotator);
			Tap(*PC, EKeys::E);
			Test.TestFalse(TEXT("E cannot open the locked door without a keycard"), KeycardDoor->IsOpen());
			Test.TestTrue(TEXT("The locked door physically blocks entry"), AnalysisDoorBlocks(*World));
			Test.TestFalse(TEXT("The archive cannot be verified before upload"), Mission->VerifyArchive(Player));
			View(*PC, *Player, FVector(2275, 826, 98), UploadTerminal->GetActorLocation());
			break;
		case 12:
			Capture(*World, TEXT("Dispatcher-Upload"));
			Test.TestEqual(TEXT("The upload computer can be targeted"), Player->GetInteractionPrompt().ToString(), FString(TEXT("Загрузить данные")));
			Tap(*PC, EKeys::E);
			Test.TestEqual(TEXT("Stage 3 asks for the guard's keycard"), Mission->GetMissionStep(), EStealthMissionStep::RecoverKeycard);
			Carrier->SetActorLocation(FVector(4810, 1700, 98));
			Carrier->TakeDamage(1000, FDamageEvent(), PC, Player);
			for (TActorIterator<AStealthKeycard> It(World); It; ++It) { Keycard = *It; break; }
			if (!Test.TestTrue(TEXT("The designated guard drops the keycard"), Keycard.IsValid())) { return true; }
			Carrier->Destroy();
			Test.TestTrue(TEXT("The keycard survives removal of the corpse"), Keycard.IsValid());
			View(*PC, *Player, Keycard->GetActorLocation() + FVector(-112, 0, 88), Keycard->GetActorLocation());
			break;
		case 13:
			Capture(*World, TEXT("Guard-Keycard"));
			Test.TestEqual(TEXT("The dropped card can be looted"), Player->GetInteractionPrompt().ToString(), FString(TEXT("Забрать пропуск охранника")));
			Tap(*PC, EKeys::E);
			Test.TestTrue(TEXT("Looting grants the keycard"), Mission->HasKeycard());
			Test.TestFalse(TEXT("The card is consumed once"), Keycard.IsValid());
			Test.TestEqual(TEXT("Stage 4 requires archive verification"), Mission->GetMissionStep(), EStealthMissionStep::VerifyArchive);
			View(*PC, *Player, FVector(5050, 1698, 98), FVector(5194, 1698, 117));
			break;
		case 14:
			Test.TestTrue(TEXT("The access warning disappears after pickup"), Player->GetAccessWarning().IsEmpty());
			Test.TestEqual(TEXT("The door offers to use the keycard"), Player->GetInteractionPrompt().ToString(), FString(TEXT("Открыть дверь пропуском")));
			Tap(*PC, EKeys::E);
			Test.TestTrue(TEXT("E opens the door with the keycard"), KeycardDoor->IsOpen());
			break;
		case 15:
			Test.TestFalse(TEXT("The opened door clears the entrance"), AnalysisDoorBlocks(*World));
			CheckRoute(*World, TEXT("Opened analysis door connects to the archive"), FVector(5020, 1698, 5), FVector(5850, 1650, 5));
			View(*PC, *Player, FVector(5820, 1640, 98), VerifyTerminal->GetActorLocation());
			break;
		case 16:
			Capture(*World, TEXT("Archive-Verification"));
			Test.TestEqual(TEXT("The analysis computer can be targeted"), Player->GetInteractionPrompt().ToString(),
				FString(TEXT("Сверить данные с резервным архивом")));
			Tap(*PC, EKeys::E);
			Test.TestEqual(TEXT("Stage 5 unlocks extraction"), Mission->GetMissionStep(), EStealthMissionStep::Extract);
			Player->SetActorLocation(Exit->GetActorLocation());
			Player->UpdateOverlaps();
			Exit->Tick(.1f);
			Test.TestEqual(TEXT("Extraction ends the actual mission"), Mission->GetMissionState(), EStealthMissionState::Won);
			if (FSceneViewport* VP = Viewport(*World)) { VP->SetFixedViewportSize(0, 0); }
			return true;
		}
		ReadyAt = World->GetTimeSeconds() + (Stage == 9 || Stage == 15 ? 1.5f : .75f);
		return false;
	}

private:
	bool CheckLayout(UWorld& World, AStealthCharacter& Player)
	{
		for (TActorIterator<AStealthObjective> It(&World); It; ++It) { Objective = *It; break; }
		for (TActorIterator<AStealthExitZone> It(&World); It; ++It) { Exit = *It; break; }
		for (TActorIterator<AStealthDoor> It(&World); It; ++It)
		{
			if (FMath::IsNearlyEqual(It->GetActorLocation().Y, 1900.0)) { ServerDoor = *It; }
			if (It->bRequiresKeycard) { KeycardDoor = *It; }
		}
		for (TActorIterator<AStealthOperationsTerminal> It(&World); It; ++It)
		{
			(It->bVerifiesArchive ? VerifyTerminal : UploadTerminal) = *It;
		}
		if (!Test.TestTrue(TEXT("The mission has a server terminal, a door and an exit"),
			Objective.IsValid() && ServerDoor.IsValid() && KeycardDoor.IsValid() && Exit.IsValid() &&
			UploadTerminal.IsValid() && VerifyTerminal.IsValid())) { return false; }
		Test.TestTrue(TEXT("The closed server door blocks the main entrance"), DoorBlocks(World));
		Test.TestTrue(TEXT("The server terminal sits in the archive"), Objective->GetActorLocation().Y > 1900);
		int32 Meshes = 0;
		for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
		{
			const UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
			if (!Mesh || !Mesh->GetStaticMesh()) { continue; }
			++Meshes;
			Test.TestTrue(FString::Printf(TEXT("Environment mesh blocks the pawn and visibility: %s"), *It->GetName()),
				Mesh->IsQueryCollisionEnabled() && Mesh->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block &&
				Mesh->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block);
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				Test.TestNotNull(TEXT("Environment material resolves"), Mesh->GetMaterial(Slot));
			}
		}
		Test.TestTrue(TEXT("The rooms contain imported laboratory props"), Meshes >= 35);
		int32 Lamps = 0;
		for (TActorIterator<ARectLight> It(&World); It; ++It)
		{
			if (It->GetActorLocation().Z > 280) { ++Lamps; }
		}
		Test.TestTrue(TEXT("The closed facility is lit by ceiling fixtures"), Lamps >= 40);
		CheckFixtures(World, Player);
		const struct { const TCHAR* Name; FVector From; FVector To; } Barriers[] = {
			{ TEXT("The floor blocks the capsule"), FVector(400, 2400, 85), FVector(400, 2400, -55) },
			{ TEXT("The ceiling blocks the capsule"), FVector(400, 2400, 235), FVector(400, 2400, 380) },
			{ TEXT("The perimeter blocks the capsule"), FVector(180, 2300, 90), FVector(-80, 2300, 90) },
			{ TEXT("The glass partition blocks the capsule"), FVector(3270, 905, 90), FVector(3270, 745, 90) },
			{ TEXT("A server rack blocks the capsule"), FVector(4230, 2310, 95), FVector(4060, 2310, 95) },
			{ TEXT("A hospital bed blocks the capsule"), FVector(1250, 2365, 72), FVector(1250, 2455, 72) },
		};
		for (const auto& Barrier : Barriers)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(LabSolid), false, &Player);
			Test.TestTrue(Barrier.Name, World.SweepTestByChannel(Barrier.From, Barrier.To, FQuat::Identity,
				ECC_Pawn, FCollisionShape::MakeCapsule(26, 50), Params));
		}
		CheckRoute(World, TEXT("Service route from arrival to archive"), Player.GetActorLocation(),
			Objective->GetActorLocation() - Objective->GetActorForwardVector() * 130);
		CheckRoute(World, TEXT("Service route from arrival to evacuation"), Player.GetActorLocation(), Exit->GetActorLocation());
		int32 Guards = 0;
		for (TActorIterator<ASoundHearingNPC> It(&World); It; ++It)
		{
			++Guards;
			if (It->bCarriesKeycard)
			{
				Test.TestFalse(TEXT("Only one guard carries the keycard"), Carrier.IsValid());
				Carrier = *It;
			}
			Test.TestNotNull(TEXT("Existing guard controller is active"), Cast<AStealthAIController>(It->GetController()));
			TArray<AStealthPatrolPoint*> Points;
			for (TActorIterator<AStealthPatrolPoint> Point(&World); Point; ++Point)
			{
				if (Point->RouteName == It->PatrolRouteName) { Points.Add(*Point); }
			}
			Points.Sort([](const AStealthPatrolPoint& A, const AStealthPatrolPoint& B) { return A.Order < B.Order; });
			Test.TestTrue(TEXT("A guard has an assigned patrol loop"), Points.Num() >= 4);
			for (int32 Index = 0; Index < Points.Num(); ++Index)
			{
				CheckRoute(World, *FString::Printf(TEXT("Patrol %s, leg %d"), *It->PatrolRouteName.ToString(), Index + 1),
					Points[Index]->GetActorLocation(), Points[(Index + 1) % Points.Num()]->GetActorLocation());
			}
			if (AController* AI = It->GetController()) { AI->SetActorTickEnabled(false); }
		}
		Test.TestEqual(TEXT("Four guards patrol the laboratory"), Guards, 4);
		if (FSceneViewport* VP = Viewport(World)) { VP->SetFixedViewportSize(1280, 720); }
		return Test.TestTrue(TEXT("A designated guard carries the keycard"), Carrier.IsValid());
	}

	void CheckFixtures(UWorld& World, AStealthCharacter& Player)
	{
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(LabFixtures), false, &Player);
		const FCollisionShape Capsule = FCollisionShape::MakeCapsule(40, 90);
		const struct { const TCHAR* Name; double X; double Y; } Doorways[] = {
			{ TEXT("Extract airlock entrance clears the radiator"), 466, 1500 },
			{ TEXT("Wet lab A entrance clears the radiator"), 2534, 1900 },
			{ TEXT("Isolation entrance clears the radiator"), 3534, 1500 },
			{ TEXT("Wet lab B entrance clears the relocated equipment"), 3400, 1900 },
		};
		for (const auto& Door : Doorways)
		{
			const FVector From(Door.X, Door.Y - 98, 94);
			const FVector To(Door.X, Door.Y + 98, 94);
			Test.TestFalse(Door.Name, World.SweepTestByChannel(From, To, FQuat::Identity, ECC_Pawn, Capsule, Params));
		}

		int32 Curtains = 0;
		int32 Boards = 0;
		int32 Radiators = 0;
		for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
		{
			const UStaticMesh* Mesh = It->GetStaticMeshComponent()->GetStaticMesh();
			if (!Mesh || !It->ActorHasTag(TEXT("Laboratory.Fixture"))) { continue; }
			const FString Name = Mesh->GetName();
			const FBox Bounds = It->GetStaticMeshComponent()->Bounds.GetBox();
			if (Name.StartsWith(TEXT("SM_PrivacyCurtain_")))
			{
				++Curtains;
				Test.TestTrue(TEXT("The privacy curtain rail meets the ceiling"), FMath::IsNearlyEqual(Bounds.Max.Z, 324.0, 0.1));
				Test.TestFalse(TEXT("The walkway stays open beside the privacy curtain"),
					World.SweepTestByChannel(FVector(1530, Bounds.Min.Y - 60, 94), FVector(1530, Bounds.Max.Y + 60, 94),
						FQuat::Identity, ECC_Pawn, Capsule, Params));
			}
			else if (Name.StartsWith(TEXT("SM_WallBoard_")))
			{
				++Boards;
				const double Rear = Bounds.GetCenter().X < 3000 ? Bounds.Max.Y : Bounds.Min.Y;
				const double Wall = Bounds.GetCenter().X < 3000 ? 2990.0 : 1910.0;
				Test.TestTrue(TEXT("The translucent board is mounted against its wall"), FMath::Abs(Rear - Wall) < 1.0);
			}
			else if (Name.StartsWith(TEXT("SM_Radiator_")))
			{
				++Radiators;
				Test.TestTrue(TEXT("The radiator back sits against the corridor wall"),
					FMath::IsNearlyEqual(Bounds.Min.Y, 1510.0, 0.1) || FMath::IsNearlyEqual(Bounds.Max.Y, 1890.0, 0.1));
			}
		}
		Test.TestEqual(TEXT("Both bed dividers are standalone actors"), Curtains, 2);
		Test.TestEqual(TEXT("Both lab boards are standalone actors"), Boards, 2);
		Test.TestEqual(TEXT("All corridor radiators are standalone actors"), Radiators, 3);
	}

	void CheckRoute(UWorld& World, const TCHAR* Description, const FVector& From, const FVector& To)
	{
		const auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
		FNavLocation A, B;
		const FVector Extent(65, 65, 170);
		const bool bProjected = Nav && Nav->ProjectPointToNavigation(From, A, Extent) && Nav->ProjectPointToNavigation(To, B, Extent);
		const UNavigationPath* Path = bProjected ? UNavigationSystemV1::FindPathToLocationSynchronously(&World, A.Location, B.Location) : nullptr;
		Test.TestTrue(Description, Path && Path->IsValid() && !Path->IsPartial());
	}

	bool DoorBlocks(UWorld& World) const
	{
		FHitResult Hit;
		return World.LineTraceSingleByChannel(Hit, FVector(4450, 1810, 124), FVector(4450, 1990, 124), ECC_Pawn) &&
			Hit.GetActor() == ServerDoor.Get();
	}

	bool AnalysisDoorBlocks(UWorld& World) const
	{
		FHitResult Hit;
		return World.LineTraceSingleByChannel(Hit, FVector(5100, 1698, 110), FVector(5310, 1698, 110), ECC_Pawn) &&
			Hit.GetActor() == KeycardDoor.Get();
	}

	static void View(APlayerController& PC, AStealthCharacter& Player, FVector From, FVector At)
	{
		Player.SetActorLocation(From);
		PC.SetControlRotation((At - Player.FindComponentByClass<UCameraComponent>()->GetComponentLocation()).Rotation());
	}

	static void Tap(APlayerController& PC, FKey Key)
	{
		PC.InputKey(FInputKeyParams(Key, IE_Pressed, 1.0f));
		PC.PlayerTick(0.016f);
		PC.InputKey(FInputKeyParams(Key, IE_Released, 0.0f));
		PC.PlayerTick(0.016f);
	}

	static FSceneViewport* Viewport(UWorld& World)
	{
		return !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")) && World.GetGameViewport()
			? World.GetGameViewport()->GetGameViewport() : nullptr;
	}

	void Capture(UWorld& World, const TCHAR* Name)
	{
		FSceneViewport* VP = Viewport(World);
		if (!VP) { return; }
		TArray<FColor> Pixels;
		if (!Test.TestTrue(TEXT("Rendered the laboratory in PIE"), GetViewportScreenShot(VP, Pixels))) { return; }
		for (FColor& Pixel : Pixels) { Pixel.A = 255; }
		TArray64<uint8> PNG;
		const FIntPoint Size = VP->GetSizeXY();
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("LaboratoryWork/Screenshots");
		IFileManager::Get().MakeDirectory(*Dir, true);
		FFileHelper::SaveArrayToFile(PNG, *(Dir / (FString(Name) + TEXT(".png"))));
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<AStealthDoor> ServerDoor;
	TWeakObjectPtr<AStealthDoor> KeycardDoor;
	TWeakObjectPtr<AStealthObjective> Objective;
	TWeakObjectPtr<AStealthOperationsTerminal> UploadTerminal, VerifyTerminal;
	TWeakObjectPtr<ASoundHearingNPC> Carrier;
	TWeakObjectPtr<AStealthKeycard> Keycard;
	TWeakObjectPtr<AStealthExitZone> Exit;
	double Started = FPlatformTime::Seconds();
	float ReadyAt = 2.0f;
	int32 Stage = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaboratoryWalkthroughTest, "FPS_Shooter.Mission.LaboratoryWalkthrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLaboratoryWalkthroughTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Laboratory/Maps/ResearchFacility"), true)) { return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FLaboratoryWalkthrough(*this));
	return true;
}

#endif
