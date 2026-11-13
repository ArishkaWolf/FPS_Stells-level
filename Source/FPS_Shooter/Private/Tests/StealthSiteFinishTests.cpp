#if WITH_DEV_AUTOMATION_TESTS

#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Slate/SceneViewport.h"
#include "SoundHearingNPC.h"
#include "StealthCharacter.h"
#include "StealthDoor.h"
#include "StealthGameMode.h"
#include "StealthObjective.h"
#include "Tests/AutomationCommon.h"

namespace
{
class FSiteFinishWalkthrough final : public IAutomationLatentCommand
{
public:
	explicit FSiteFinishWalkthrough(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 75.0)
		{
			Test.AddError(TEXT("Timed out while checking the dressed construction site"));
			return true;
		}
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!World || World->WorldType != EWorldType::PIE || World->GetTimeSeconds() < ReadyAt) { return false; }
		APlayerController* PC = World->GetFirstPlayerController();
		auto* Player = PC ? Cast<AStealthCharacter>(PC->GetPawn()) : nullptr;
		auto* Mission = World->GetAuthGameMode<AStealthGameMode>();
		if (!Player || !Mission) { Test.AddError(TEXT("Site finish test needs the real mission and pawn")); return true; }

		switch (Stage++)
		{
		case 0:
			if (!Prepare(*World)) { return true; }
			View(*PC, *Player, FVector(1880, 3730, 98), FVector(2390, 3745, 149));
			break;
		case 1:
			Capture(*World, TEXT("Gate-and-graffiti"));
			View(*PC, *Player, FVector(4555, 4590, 98), FVector(4590, 4800, 154));
			break;
		case 2:
			Capture(*World, TEXT("Shutter-closed"));
			for (int32 I = 0; I < Doors.Num(); ++I)
			{
				Test.TestTrue(TEXT("The closed leaf blocks the entrance"), HitsDoor(*World, I));
				Doors[I]->SetOpen(true, Player);
			}
			break;
		case 3:
			Capture(*World, TEXT("Shutter-open"));
			for (int32 I = 0; I < Doors.Num(); ++I)
			{
				Test.TestFalse(TEXT("The open leaf clears the entrance"), HitsDoor(*World, I));
				const FVector From(EntryX[I], EntryY[I] - 125, 25);
				const FVector To(EntryX[I], EntryY[I] + 128, 25);
				const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, To);
				Test.TestTrue(TEXT("The service door opens onto a walkable pocket"), Path && Path->IsValid() &&
					!Path->IsPartial() && Path->GetPathLength() < 360.0);
				const FVector SideExit(ExitX[I], ExitY[I], 25);
				FNavLocation WalkableExit;
				const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
				const bool bProjected = Nav && Nav->ProjectPointToNavigation(SideExit, WalkableExit, FVector(44, 60, 110));
				const UNavigationPath* Exit = bProjected
					? UNavigationSystemV1::FindPathToLocationSynchronously(World, To, WalkableExit.Location) : nullptr;
				Test.TestTrue(FString::Printf(TEXT("Service pocket %d has a short route to its side exit"), I + 1),
					Exit && Exit->IsValid() && !Exit->IsPartial() && Exit->GetPathLength() < 560.0);
				if (Exit)
				{
					Test.AddInfo(FString::Printf(TEXT("Service pocket %d path: %.0f cm, partial %d"), I + 1,
						Exit->GetPathLength(), Exit->IsPartial()));
				}
				Doors[I]->SetOpen(false, Player);
			}
			View(*PC, *Player, FVector(10050, 6675, 98), FVector(10600, 7100, 250));
			break;
		case 4:
			Capture(*World, TEXT("Crane-footing"));
			for (int32 I = 0; I < Doors.Num(); ++I)
			{
				Test.TestTrue(TEXT("The leaf returns to the original frame when closed"), HitsDoor(*World, I));
			}
			View(*PC, *Player, FVector(10276, 4100, 98), Intel->GetActorLocation());
			break;
		case 5:
			Capture(*World, TEXT("Intel-terminal"));
			Test.TestEqual(TEXT("Crosshair can interact with the real terminal"), Player->GetInteractionPrompt().ToString(),
				FString(TEXT("Забрать данные")));
			Key(*PC, IE_Pressed);
			Key(*PC, IE_Released);
			Test.TestTrue(TEXT("Taking data still unlocks extraction"), Mission->HasObjective() && !Intel.IsValid());
			if (FSceneViewport* VP = Viewport(*World)) { VP->SetFixedViewportSize(0, 0); }
			return true;
		}

		ReadyAt = World->GetTimeSeconds() + 1.25;
		return false;
	}

private:
	bool Prepare(UWorld& World)
	{
		for (TActorIterator<ASoundHearingNPC> It(&World); It; ++It)
		{
			if (AController* AI = It->GetController()) { AI->SetActorTickEnabled(false); }
		}
		Doors.SetNum(2);
		for (TActorIterator<AStealthDoor> It(&World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("Greybox.Distraction")))
			{
				Doors[It->GetActorLocation().X < 6000 ? 0 : 1] = *It;
			}
		}
		if (!Test.TestTrue(TEXT("Both service shutters exist"), Doors[0].IsValid() && Doors[1].IsValid())) { return false; }
		for (TActorIterator<ATextRenderActor> It(&World); It; ++It)
		{
			const FString Caption = It->GetTextRender()->Text.ToString();
			const int32 Slot = Caption == TEXT("ИНСТРУМЕНТЫ") ? 0 : Caption == TEXT("СЕРВИС 03") ? 1 : INDEX_NONE;
			if (Slot == INDEX_NONE) { continue; }
			++Signs;
			const AActor* Plate = It->GetAttachParentActor();
			Test.TestTrue(TEXT("The door sign is on a separate plate outside the swing"),
				Plate && Plate->ActorHasTag(TEXT("Industrial.SignBacking")) &&
				Plate->GetComponentsBoundingBox(true).Min.X > Doors[Slot]->GetActorLocation().X + 14.0);
		}
		Test.TestEqual(TEXT("Both plaques are fixed alongside the entrances"), Signs, 2);
		int32 Cranes = 0, Footings = 0, Rubble = 0, Paint = 0;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("Industrial.Crane")))
			{
				++Cranes;
				const FBox B = It->GetComponentsBoundingBox(true);
				Test.TestTrue(TEXT("The full crane geometry fits within the perimeter"),
					B.Min.X > 22 && B.Max.X < 11978 && B.Min.Y > 22 && B.Max.Y < 8378 &&
					FMath::IsNearlyEqual(B.Min.Z, 32.0, 3.0));
			}
			Footings += It->ActorHasTag(TEXT("Industrial.CraneFooting"));
			Rubble += It->ActorHasTag(TEXT("Industrial.Rubble"));
			Paint += It->FindComponentByClass<UDecalComponent>() != nullptr;
		}
		Test.TestTrue(TEXT("Both cranes have footings and the yard has actual rubble and decals"), Cranes == 2 && Footings == 2 &&
			Rubble >= 15 && Paint >= 10);
		for (TActorIterator<AStealthObjective> It(&World); It; ++It)
		{
			Intel = *It;
			int32 Parts = 0;
			bool bTexturedDisplay = false;
			for (UStaticMeshComponent* Mesh : TInlineComponentArray<UStaticMeshComponent*>(*It))
			{
				++Parts;
				bTexturedDisplay |= Mesh->GetName() == TEXT("DisplaySurface") && Mesh->GetMaterial(0) &&
					Mesh->GetMaterial(0)->GetFName() == TEXT("M_IntelDisplay");
			}
			Test.TestTrue(TEXT("Data is a built terminal with its own screen material"), Parts >= 8 && bTexturedDisplay);
		}
		if (FSceneViewport* VP = Viewport(World)) { VP->SetFixedViewportSize(1280, 720); }
		return Test.TestTrue(TEXT("Mission terminal is present"), Intel.IsValid());
	}

	bool HitsDoor(UWorld& World, int32 I) const
	{
		FHitResult Hit;
		const FVector From(EntryX[I], EntryY[I] - 125, 110);
		const FVector To(EntryX[I], EntryY[I] + 128, 110);
		return World.LineTraceSingleByChannel(Hit, From, To, ECC_Visibility) && Hit.GetActor() == Doors[I].Get();
	}

	static void View(APlayerController& PC, AStealthCharacter& Player, FVector From, FVector At)
	{
		Player.SetActorLocation(From);
		PC.SetControlRotation((At - From - FVector(0, 0, 64)).Rotation());
	}

	static void Key(APlayerController& PC, EInputEvent Event)
	{
		PC.InputKey(FInputKeyParams(EKeys::E, Event, Event == IE_Pressed ? 1.0f : 0.0f));
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
		if (!Test.TestTrue(TEXT("PIE viewport capture"), GetViewportScreenShot(VP, Pixels))) { return; }
		for (FColor& P : Pixels) { P.A = 255; }
		TArray64<uint8> PNG;
		const FIntPoint Size = VP->GetSizeXY();
		if (FCString::Strcmp(Name, TEXT("Intel-terminal")) == 0)
		{
			int32 LitPixels = 0;
			for (int32 Y = Size.Y * 39 / 100; Y < Size.Y * 48 / 100; ++Y)
			{
				for (int32 X = Size.X * 43 / 100; X < Size.X * 57 / 100; ++X)
				{
					const FColor P = Pixels[Y * Size.X + X];
					LitPixels += P.G > 45 && P.G > P.R + 9 && P.B > P.R + 5;
				}
			}
			Test.TestTrue(TEXT("The terminal actually renders its screen artwork in the viewport"), LitPixels > 100);
		}
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("SiteFinish/Screenshots");
		IFileManager::Get().MakeDirectory(*Dir, true);
		FFileHelper::SaveArrayToFile(PNG, *(Dir / FString::Printf(TEXT("%s.png"), Name)));
	}

	FAutomationTestBase& Test;
	TArray<TWeakObjectPtr<AStealthDoor>> Doors;
	TWeakObjectPtr<AStealthObjective> Intel;
	double Started = FPlatformTime::Seconds();
	double ReadyAt = 2.0;
	int32 Stage = 0;
	int32 Signs = 0;
	const double EntryX[2] = {4525, 7425};
	const double EntryY[2] = {4800, 5100};
	const double ExitX[2] = {4950, 7165};
	const double ExitY[2] = {4945, 5260};
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthSiteFinishTest, "FPS_Shooter.Mission.SiteFinishPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStealthSiteFinishTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Maps/ThirdPersonMap"), true)) { return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FSiteFinishWalkthrough(*this));
	return true;
}

#endif
