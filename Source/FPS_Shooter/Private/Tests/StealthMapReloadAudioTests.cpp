#if WITH_DEV_AUTOMATION_TESTS

#include "ActiveSound.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NoiseMakerPickup.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Slate/SceneViewport.h"
#include "Sound/SoundBase.h"
#include "SoundHearingNPC.h"
#include "SoundRayEmitterComponent.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"
#include "StealthSupplyPickup.h"
#include "Tests/AutomationCommon.h"

namespace
{
class FStealthMapReloadAudioCheck final : public IAutomationLatentCommand
{
public:
	explicit FStealthMapReloadAudioCheck(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 120.0)
		{
			Test.AddError(FString::Printf(TEXT("Map, reload and audio check stalled at stage %d"), Stage));
			return true;
		}
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!World || World->WorldType != EWorldType::PIE || World->GetTimeSeconds() < ReadyAt) { return false; }
		auto* PC = Cast<AStealthPlayerController>(World->GetFirstPlayerController());
		auto* Player = PC ? Cast<AStealthCharacter>(PC->GetPawn()) : nullptr;
		auto* Mission = World->GetAuthGameMode<AStealthGameMode>();
		if (!Player || !Mission) { Test.AddError(TEXT("Missing player or mission")); return true; }

		switch (Stage)
		{
		case 0:
			if (!Prepare(*World, *Player)) { return true; }
			Tap(*PC, EKeys::M);
			Test.TestTrue(TEXT("M opens the map and pauses the mission"), PC->IsMapOpen() && UGameplayStatics::IsGamePaused(World));
			break;
		case 1:
			Capture(*World, TEXT("Map-Intel"));
			Tap(*PC, EKeys::M);
			Test.TestTrue(TEXT("M closes the map and resumes the mission"), !PC->IsMapOpen() && !UGameplayStatics::IsGamePaused(World));
			break;
		case 2:
			Capture(*World, TEXT("Industrial-Staging"));
			Player->SetActorLocation(Clearing);
			Mission->Tick(0.01f);
			Test.TestTrue(TEXT("Objective collection unlocks the exit"), Mission->CollectObjective(Player));
			Tap(*PC, EKeys::Gamepad_Special_Left);
			Test.TestTrue(TEXT("Gamepad View opens the extraction map"), PC->IsMapOpen());
			break;
		case 3:
			Capture(*World, TEXT("Map-Extraction"));
			Tap(*PC, EKeys::Gamepad_Special_Left);
			PC->SetControlRotation(FRotator::ZeroRotator);
			Key(*PC, EKeys::RightMouseButton, true);
			break;
		case 4:
			Test.TestTrue(TEXT("Reload test starts in ADS"), Player->GetAimAlpha() > 0.9f);
			Tap(*PC, EKeys::LeftMouseButton);
			Tap(*PC, EKeys::R);
			Test.TestTrue(TEXT("R starts reloading from ADS"), Player->IsReloading());
			break;
		case 5:
			if (Player->IsReloading())
			{
				SampleReload(*World, *Player);
				return false;
			}
			Test.TestTrue(TEXT("The rifle stays in front of the camera near plane"), MinRifleDepth > GEngine->NearClipPlane);
			Test.TestTrue(TEXT("The reload clip keeps magazine travel within hand reach"), MaxMagazineTravel > 6.0 && MaxMagazineTravel < 36.0);
			Test.AddInfo(FString::Printf(TEXT("Reload clearance %.1f cm; magazine travel %.1f cm"), MinRifleDepth, MaxMagazineTravel));
			Test.TestEqual(TEXT("Reload restores the magazine"), Player->GetAmmoInMagazine(), 30);
			Test.TestEqual(TEXT("Reload transfers exactly one round"), Player->GetReserveAmmo(), 89);
			break;
		case 6:
		{
			Test.TestTrue(TEXT("Held ADS resumes after reloading"), Player->GetAimAlpha() > 0.9f);
			Key(*PC, EKeys::RightMouseButton, false);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Guard = World->SpawnActor<ASoundHearingNPC>(Clearing + FVector(320, 180, 0), FRotator::ZeroRotator, Params);
			if (!Test.TestNotNull(TEXT("Guard footstep fixture"), Guard.Get())) { return true; }
			Guard->GetController()->SetActorTickEnabled(false);
			AudioDeadline = World->GetTimeSeconds() + 1.5f;
			break;
		}
		case 7:
			if (World->GetTimeSeconds() < AudioDeadline)
			{
				Player->AddMovementInput(FVector::ForwardVector);
				Guard->AddMovementInput(FVector::ForwardVector);
				ObserveAudio(*World);
				return false;
			}
			Test.TestEqual(TEXT("Player walking still emits a footstep ray event"),
				Player->GetSoundEmitter()->GetLastEmittedEvent().Type, ESoundEventType::Footstep);
			Test.TestTrue(TEXT("Moving guard does not alert other guards with its own footsteps"),
				!Guard->SoundEmitter->GetLastEmittedEvent().EventId.IsValid());
			if (World->GetAudioDevice().IsValid())
			{
				Test.TestEqual(TEXT("Audio mixer played local player and spatial guard steps"), AudioKinds->Load(), uint8(3));
				if (AudioKinds->Load() == 3) { Test.AddInfo(TEXT("Observed both footstep sources in the active audio mixer")); }
			}
			else
			{
				Test.AddInfo(TEXT("Audio mixer check requires an enabled audio device"));
			}
			Player->SetActorLocation(FVector(3200, 4080, 100));
			PC->SetControlRotation(FRotator(0, 12, 0));
			Tap(*PC, EKeys::LeftMouseButton);
			Tap(*PC, EKeys::LeftMouseButton);
			Test.TestEqual(TEXT("First tap fires immediately"), Player->GetAmmoInMagazine(), 29);
			break;
		case 8:
			Test.TestEqual(TEXT("Second tap during cooldown fires when the rifle is ready"), Player->GetAmmoInMagazine(), 28);
			Capture(*World, TEXT("Industrial-Yard"));
			Player->SetActorLocation(FVector(1020, 1460, 100));
			PC->SetControlRotation(FRotator(-8, -124, 0));
			break;
		case 9:
			Capture(*World, TEXT("Industrial-Extraction"));
			if (FSceneViewport* Viewport = GetViewport(*World)) { Viewport->SetFixedViewportSize(0, 0); }
			return true;
		}

		++Stage;
		if (Stage == 2 || Stage == 4 || Stage == 6 || Stage == 8 || Stage == 9)
		{
			ReadyAt = World->GetTimeSeconds() + 0.5f;
		}
		return false;
	}

private:
	bool Prepare(UWorld& World, AStealthCharacter& Player)
	{
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("Greybox.QA.Movement"))) { Clearing = It->GetActorLocation(); break; }
		}
		if (Clearing.IsNearlyZero()) { Test.AddError(TEXT("Missing reload test clearing")); return false; }
		for (TActorIterator<ASoundHearingNPC> It(&World); It; ++It)
		{
			if (AController* AI = It->GetController()) { AI->SetActorTickEnabled(false); }
		}
		int32 RussianSigns = 0;
		for (TActorIterator<ATextRenderActor> It(&World); It; ++It)
		{
			const FString Caption = It->GetTextRender()->Text.ToString();
			if (!Caption.IsEmpty() && Caption[0] >= TEXT('А') && Caption[0] <= TEXT('Я'))
			{
				++RussianSigns;
				const AActor* Parent = It->GetAttachParentActor();
				Test.TestTrue(TEXT("World signage is attached to a physical plate"),
					Parent && Parent->ActorHasTag(TEXT("Industrial.SignBacking")));
			}
		}
		Test.TestTrue(TEXT("Level uses Cyrillic signage"), RussianSigns >= 17);
		int32 GroundedPickups = 0;
		for (TActorIterator<ANoiseMakerPickup> It(&World); It; ++It)
		{
			const UStaticMeshComponent* Mesh = It->FindComponentByClass<UStaticMeshComponent>();
			if (Mesh && FMath::IsNearlyZero(Mesh->Bounds.GetBox().Min.Z, 2.0)) { ++GroundedPickups; }
		}
		Test.TestEqual(TEXT("Both pickups sit on the yard floor"), GroundedPickups, 2);
		int32 Cranes = 0;
		int32 IndustrialCover = 0;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			Cranes += It->ActorHasTag(TEXT("Industrial.Crane")) ? 1 : 0;
			IndustrialCover += It->ActorHasTag(TEXT("Industrial.Cover")) ? 1 : 0;
		}
		Test.TestEqual(TEXT("Both construction cranes are present"), Cranes, 2);
		Test.TestTrue(TEXT("Imported assets dress the actual cover"), IndustrialCover >= 17);
		for (USkeletalMeshComponent* Mesh : TInlineComponentArray<USkeletalMeshComponent*>(&Player))
		{
			if (Mesh->GetFName() == TEXT("WeaponMesh")) { Rifle = Mesh; break; }
		}
		if (!Test.TestNotNull(TEXT("Player rifle mesh"), Rifle.Get())) { return false; }
		RestMagazine = Rifle->GetSocketTransform(TEXT("Clip_Bone"), RTS_Component).GetLocation();
		if (FSceneViewport* Viewport = GetViewport(World)) { Viewport->SetFixedViewportSize(1280, 720); }
		return true;
	}

	void SampleReload(UWorld& World, const AStealthCharacter& Player)
	{
		const auto* Camera = Player.FindComponentByClass<UCameraComponent>();
		const FTransform RifleInCamera = Rifle->GetComponentTransform().GetRelativeTransform(Camera->GetComponentTransform());
		const FBoxSphereBounds Bounds = Rifle->GetSkeletalMeshAsset()->GetImportedBounds().TransformBy(RifleInCamera);
		MinRifleDepth = FMath::Min(MinRifleDepth, Bounds.Origin.X - Bounds.BoxExtent.X);
		const FVector Clip = Rifle->GetSocketTransform(TEXT("Clip_Bone"), RTS_Component).GetLocation();
		MaxMagazineTravel = FMath::Max(MaxMagazineTravel, FVector::Dist(RestMagazine, Clip));
		const float Phases[] = {0.22f, 0.47f, 0.74f};
		if (ReloadFrame < UE_ARRAY_COUNT(Phases) && Player.GetReloadProgress() >= Phases[ReloadFrame])
		{
			Capture(World, FString::Printf(TEXT("Reload-%d"), ++ReloadFrame));
		}
	}

	void ObserveAudio(UWorld& World)
	{
		const FAudioDeviceHandle Device = World.GetAudioDevice();
		if (!Device.IsValid() || AudioKinds->Load() == 3) { return; }
		FAudioThread::RunCommandOnAudioThread([Device, Kinds = AudioKinds]()
		{
			for (const FActiveSound* Sound : Device->GetActiveSounds())
			{
				if (Sound->GetSound() && Sound->GetSound()->GetFName() == TEXT("ConcreteStep"))
				{
					*Kinds |= Sound->bLocationDefined ? uint8(2) : uint8(1);
				}
			}
		});
	}

	static void Key(APlayerController& PC, FKey Value, bool bDown)
	{
		PC.InputKey(FInputKeyParams(Value, bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f));
		PC.PlayerTick(0.016f);
	}

	static void Tap(APlayerController& PC, FKey Value)
	{
		Key(PC, Value, true);
		Key(PC, Value, false);
	}

	static FSceneViewport* GetViewport(UWorld& World)
	{
		const bool bRendered = !FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
		return bRendered && World.GetGameViewport() ? World.GetGameViewport()->GetGameViewport() : nullptr;
	}

	void Capture(UWorld& World, const FString& Name)
	{
		FSceneViewport* Viewport = GetViewport(World);
		if (!Viewport) { return; }
		TArray<FColor> Pixels;
		const FIntPoint Size = Viewport->GetSizeXY();
		if (!GetViewportScreenShot(Viewport, Pixels) || Pixels.Num() != Size.X * Size.Y)
		{
			Test.AddError(FString::Printf(TEXT("Could not read the PIE viewport for %s"), *Name));
			return;
		}
		for (FColor& Pixel : Pixels) { Pixel.A = 255; }
		if (Name.StartsWith(TEXT("Map-")))
		{
			const double Scale = FMath::Min(Size.X / 960.0, Size.Y / 540.0);
			const int32 Left = FMath::RoundToInt(Size.X * 0.5 - 359.0 * Scale);
			const int32 Top = FMath::RoundToInt(Size.Y * 0.5 - 213.0 * Scale);
			int32 LightPixels = 0;
			for (int32 Y = Top; Y < Top + FMath::RoundToInt(30.0 * Scale); ++Y)
			{
				for (int32 X = Left; X < Left + FMath::RoundToInt(185.0 * Scale); ++X)
				{
					const FColor& Pixel = Pixels[Y * Size.X + X];
					LightPixels += Pixel.R > 150 && Pixel.G > 150 && Pixel.B > 150 ? 1 : 0;
				}
			}
			Test.TestTrue(TEXT("The Cyrillic map title renders over its dark panel"), LightPixels > 60);
		}
		TArray64<uint8> PNG;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("RussianMapWork/Screenshots");
		IFileManager::Get().MakeDirectory(*Directory, true);
		FFileHelper::SaveArrayToFile(PNG, *(Directory / (Name + TEXT(".png"))));
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<USkeletalMeshComponent> Rifle;
	TWeakObjectPtr<ASoundHearingNPC> Guard;
	TSharedRef<TAtomic<uint8>, ESPMode::ThreadSafe> AudioKinds = MakeShared<TAtomic<uint8>, ESPMode::ThreadSafe>(0);
	FVector Clearing = FVector::ZeroVector;
	FVector RestMagazine = FVector::ZeroVector;
	double MinRifleDepth = TNumericLimits<double>::Max();
	double MaxMagazineTravel = 0.0;
	double Started = FPlatformTime::Seconds();
	float ReadyAt = 2.0f;
	float AudioDeadline = 0.0f;
	int32 Stage = 0;
	int32 ReloadFrame = 0;
};

class FStealthSuppliesCheck final : public IAutomationLatentCommand
{
public:
	explicit FStealthSuppliesCheck(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 100.0)
		{
			Test.AddError(FString::Printf(TEXT("Resupply check stalled at stage %d"), Stage));
			return true;
		}
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!World || World->WorldType != EWorldType::PIE || World->GetTimeSeconds() < ReadyAt) { return false; }
		auto* PC = Cast<AStealthPlayerController>(World->GetFirstPlayerController());
		auto* Player = PC ? Cast<AStealthCharacter>(PC->GetPawn()) : nullptr;
		if (!Player) { Test.AddError(TEXT("Missing player for resupply check")); return true; }
		switch (Stage++)
		{
		case 0:
			if (!Prepare(*World)) { return true; }
			Player->SetActorLocation(FVector(4270, 2100, 98));
			PC->SetControlRotation(FRotator(-4, 14, 0));
			ReadyAt = World->GetTimeSeconds() + 0.3f;
			break;
		case 1:
			Capture(*World, TEXT("Southern-Yard"));
			Focus(*PC, *Player, *Medkit);
			Test.TestTrue(TEXT("Full health has a contextual hint"), Player->GetInteractionPrompt().ToString() == TEXT("Здоровье полное"));
			Tap(*PC, EKeys::E);
			Test.TestTrue(TEXT("Full health does not waste the kit"), Medkit.IsValid() && !Player->IsHealing());
			UGameplayStatics::ApplyDamage(Player, 50.0f, nullptr, nullptr, nullptr);
			Focus(*PC, *Player, *Medkit);
			ReadyAt = World->GetTimeSeconds() + 0.14f;
			break;
		case 2:
			Capture(*World, TEXT("Medkit"));
			Test.TestEqual(TEXT("Aiming at the kit shows its use prompt"), Player->GetInteractionPrompt().ToString(), FString(TEXT("Использовать аптечку")));
			Tap(*PC, EKeys::E);
			Test.TestTrue(TEXT("E starts first-person healing"), Player->IsHealing());
			Test.TestEqual(TEXT("Healing is not applied before the animation"), Player->GetHealth(), 50.0f);
			ReadyAt = World->GetTimeSeconds() + 0.68f;
			break;
		case 3:
		{
			Test.TestEqual(TEXT("The arms are in the healing state"), Player->GetArmsState(), EStealthArmsState::Heal);
			const UStaticMeshComponent* Pouch = Player->FindComponentByClass<UStaticMeshComponent>();
			Test.TestTrue(TEXT("The animated treatment pouch is visible in hand"), Pouch && Pouch->IsVisible());
			PC->SetControlRotation(FRotator(-4, 18, 0));
			const int32 Magazine = Player->GetAmmoInMagazine();
			Tap(*PC, EKeys::LeftMouseButton);
			Test.TestEqual(TEXT("The rifle cannot fire during treatment"), Player->GetAmmoInMagazine(), Magazine);
			ReadyAt = World->GetTimeSeconds() + 0.17f;
			break;
		}
		case 4:
			Capture(*World, TEXT("Healing"));
			UGameplayStatics::ApplyDamage(Player, 8.0f, nullptr, nullptr, nullptr);
			Test.TestTrue(TEXT("Damage interrupts treatment without consuming the kit"), Medkit.IsValid() && !Player->IsHealing());
			Focus(*PC, *Player, *Medkit);
			Tap(*PC, EKeys::E);
			Test.TestTrue(TEXT("The same kit can be used again"), Player->IsHealing());
			ReadyAt = World->GetTimeSeconds() + 1.85f;
			break;
		case 5:
			Test.TestEqual(TEXT("Treatment restores 50 health"), Player->GetHealth(), 92.0f);
			Test.TestFalse(TEXT("Completed treatment consumes the kit"), Medkit.IsValid());
			Focus(*PC, *Player, *FirstAmmo);
			Tap(*PC, EKeys::E);
			Test.TestEqual(TEXT("E adds 45 rounds to reserve"), Player->GetReserveAmmo(), 135);
			Test.TestFalse(TEXT("Used ammo case is consumed"), FirstAmmo.IsValid());
			ReadyAt = World->GetTimeSeconds() + 0.18f;
			break;
		case 6:
			Capture(*World, TEXT("Ammo-Resupply"));
			Focus(*PC, *Player, *SecondAmmo);
			Tap(*PC, EKeys::E);
			Test.TestEqual(TEXT("The next pickup respects the reserve limit"), Player->GetReserveAmmo(), Player->GetReserveAmmoCapacity());
			Test.TestEqual(TEXT("Ammo pickups leave the magazine untouched"), Player->GetAmmoInMagazine(), 30);
			Focus(*PC, *Player, *MedkitTwo);
			Tap(*PC, EKeys::E);
			ReadyAt = World->GetTimeSeconds() + 1.85f;
			break;
		case 7:
			Test.TestEqual(TEXT("The second treatment clamps health to maximum"), Player->GetHealth(), 100.0f);
			if (FSceneViewport* View = Viewport(*World)) { View->SetFixedViewportSize(0, 0); }
			return true;
		}
		return false;
	}

private:
	bool Prepare(UWorld& World)
	{
		for (TActorIterator<ASoundHearingNPC> It(&World); It; ++It)
		{
			if (AController* AI = It->GetController()) { AI->SetActorTickEnabled(false); }
		}
		TArray<AStealthSupplyPickup*> Medkits, Ammo;
		for (TActorIterator<AStealthSupplyPickup> It(&World); It; ++It)
		{
			(It->GetKind() == EStealthSupplyKind::Medkit ? Medkits : Ammo).Add(*It);
			Test.TestTrue(TEXT("Supply cases sit on the floor"), FMath::IsNearlyEqual(It->GetActorLocation().Z, 14.0, 2.0));
		}
		if (!Test.TestTrue(TEXT("Two placed medkits and two ammo cases load in PIE"), Medkits.Num() == 2 && Ammo.Num() == 2)) { return false; }
		Medkits.Sort([](const AStealthSupplyPickup& A, const AStealthSupplyPickup& B) { return A.GetActorLocation().X < B.GetActorLocation().X; });
		Ammo.Sort([](const AStealthSupplyPickup& A, const AStealthSupplyPickup& B) { return A.GetActorLocation().X < B.GetActorLocation().X; });
		Medkit = Medkits[0];
		MedkitTwo = Medkits[1];
		FirstAmmo = Ammo[0];
		SecondAmmo = Ammo[1];
		for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
		{
			const auto* Mesh = It->GetStaticMeshComponent();
			if (!Mesh || !Mesh->GetStaticMesh()) { continue; }
			for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I)
			{
				Test.TestTrue(FString::Printf(TEXT("Material resolves on %s, slot %d"), *It->GetName(), I), IsValid(Mesh->GetMaterial(I)));
			}
			if (Mesh->GetStaticMesh()->GetFName() == TEXT("SM_FreightContainer_01"))
			{
				const FVector Scale = It->GetActorScale3D();
				Test.TestTrue(TEXT("Freight containers preserve their authored proportions"),
					FMath::IsNearlyEqual(Scale.X, Scale.Y) && FMath::IsNearlyEqual(Scale.Y, Scale.Z));
				Test.TestTrue(TEXT("Container hardware keeps its authored material"), Mesh->GetMaterial(0) == Mesh->GetStaticMesh()->GetMaterial(0));
			}
		}
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
		if (!Test.TestNotNull(TEXT("South logistics uses the yard navigation mesh"), Nav)) { return false; }
		const FVector Waypoints[] = {
			FVector(2910, 3750, 98), Medkit->GetActorLocation(), FVector(4680, 2000, 98),
			FirstAmmo->GetActorLocation(), FVector(7130, 2260, 98), MedkitTwo->GetActorLocation(),
			FVector(9330, 3900, 98)
		};
		double Total = 0.0;
		for (int32 I = 1; I < UE_ARRAY_COUNT(Waypoints); ++I)
		{
			FNavLocation A, B;
			const FVector Extent(95, 95, 220);
			if (!Test.TestTrue(TEXT("Southern route junctions and supplies project onto NavMesh"),
				Nav->ProjectPointToNavigation(Waypoints[I - 1], A, Extent) && Nav->ProjectPointToNavigation(Waypoints[I], B, Extent))) { return false; }
			const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(&World, A.Location, B.Location);
			if (!Test.TestTrue(TEXT("The southern loop connects without a long detour"),
				Path && Path->IsValid() && !Path->IsPartial() && Path->GetPathLength() < FVector::Dist2D(A.Location, B.Location) + 1350.0)) { return false; }
			Total += Path->GetPathLength();
		}
		FNavLocation Northern;
		Test.TestTrue(TEXT("Northern service ammo is reachable"), Nav->ProjectPointToNavigation(SecondAmmo->GetActorLocation(), Northern, FVector(90, 90, 220)));
		Test.AddInfo(FString::Printf(TEXT("Southern path through supply pockets: %.1f m"), Total / 100.0));
		if (FSceneViewport* View = Viewport(World)) { View->SetFixedViewportSize(1280, 720); }
		return true;
	}

	static void Focus(APlayerController& PC, AStealthCharacter& Player, const AActor& Supply)
	{
		const FVector Location = Supply.GetActorLocation();
		Player.SetActorLocation(FVector(Location.X - 120, Location.Y, 98));
		UCameraComponent* Camera = Player.FindComponentByClass<UCameraComponent>();
		PC.SetControlRotation((Location - Camera->GetComponentLocation()).Rotation());
		// This fixture sends input in the same frame as teleporting and setting the controller rotation.
		FMinimalViewInfo View;
		Camera->GetCameraView(0.0f, View);
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

	void Capture(UWorld& World, const FString& Name)
	{
		FSceneViewport* View = Viewport(World);
		if (!View) { return; }
		TArray<FColor> Pixels;
		if (!Test.TestTrue(TEXT("Rendered the resupply scene"), GetViewportScreenShot(View, Pixels))) { return; }
		for (FColor& Pixel : Pixels) { Pixel.A = 255; }
		TArray64<uint8> PNG;
		const FIntPoint Size = View->GetSizeXY();
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
		const FString Folder = FPaths::ProjectSavedDir() / TEXT("YardResupply/Screenshots");
		IFileManager::Get().MakeDirectory(*Folder, true);
		FFileHelper::SaveArrayToFile(PNG, *(Folder / (Name + TEXT(".png"))));
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<AStealthSupplyPickup> Medkit, MedkitTwo, FirstAmmo, SecondAmmo;
	double Started = FPlatformTime::Seconds();
	float ReadyAt = 2.0f;
	int32 Stage = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthMapReloadAudioTest, "FPS_Shooter.UI.MapReloadAndFootstepsPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStealthMapReloadAudioTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Maps/ThirdPersonMap"), true)) { return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FStealthMapReloadAudioCheck(*this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthSuppliesPIETest, "FPS_Shooter.Mission.YardSuppliesPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStealthSuppliesPIETest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Maps/ThirdPersonMap"), true)) { return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FStealthSuppliesCheck(*this));
	return true;
}

#endif
