#include "StealthHUD.h"

#include "Components/StaticMeshComponent.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "EngineFontServices.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "SoundHearingNPC.h"
#include "StealthAIController.h"
#include "StealthCharacter.h"
#include "StealthDoor.h"
#include "StealthExitZone.h"
#include "StealthGameMode.h"
#include "StealthObjective.h"
#include "StealthKeycard.h"
#include "StealthPlayerController.h"
#include "StealthSupplyPickup.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FLinearColor Panel(0.016f, 0.023f, 0.028f, 0.96f);
const FLinearColor Track(0.13f, 0.18f, 0.20f, 1.0f);
const FLinearColor Muted(0.68f, 0.75f, 0.78f, 1.0f);
const FLinearColor Ink(0.95f, 0.97f, 0.96f, 1.0f);
const FLinearColor Mint(0.42f, 0.93f, 0.75f, 1.0f);
const FLinearColor Amber(1.0f, 0.76f, 0.32f, 1.0f);
const FLinearColor Red(1.0f, 0.36f, 0.32f, 1.0f);
const FLinearColor Backdrop(0.015f, 0.023f, 0.03f, 0.68f);
const FLinearColor AidColor(0.66f, 0.94f, 0.82f);
const FName RestartHitBox(TEXT("RestartMission"));
const FName Bold(TEXT("Bold"));
const FName Regular(TEXT("Regular"));

struct FStealthCanvas
{
	explicit FStealthCanvas(AHUD& InHUD, UCanvas& Canvas)
		: HUD(InHUD), Surface(Canvas),
		  Scale(FMath::Min(Canvas.ClipX / 960.0f, Canvas.ClipY / 540.0f)),
		  Size(Canvas.ClipX / Scale, Canvas.ClipY / Scale) {}

	AHUD& HUD;
	UCanvas& Surface;
	float Scale;
	FVector2D Size;

	void Box(const FLinearColor& Color, FVector2D At, FVector2D Extent) const
	{
		HUD.DrawRect(Color, At.X * Scale, At.Y * Scale, Extent.X * Scale, Extent.Y * Scale);
	}

	FSlateFontInfo Font(float FontScale, FName Typeface) const
	{
		const float RasterSize = 11.0f * FontScale * Scale / Surface.Canvas->GetDPIScale();
		return FSlateFontInfo(GEngine->GetMediumFont(), FMath::Max(7, FMath::RoundToInt(RasterSize)), Typeface);
	}

	FVector2D Measure(const FString& Value, float FontScale, FName Typeface = Regular) const
	{
		return FEngineFontServices::Get().GetFontMeasure()->Measure(
			Value, Font(FontScale, Typeface), Surface.Canvas->GetDPIScale()) / Scale;
	}

	void Text(const FString& Value, FVector2D At, const FLinearColor& Color = Ink, float FontScale = 1.0f,
		FName Typeface = Regular) const
	{
		const FVector2D Pixel = At * Scale;
		FCanvasTextStringViewItem Item(FVector2D(FMath::RoundToDouble(Pixel.X), FMath::RoundToDouble(Pixel.Y)),
			Value, Font(FontScale, Typeface), Color);
		Item.EnableShadow(FLinearColor(0, 0, 0, 0.8f), FVector2D(Scale, Scale));
		Surface.DrawItem(Item);
	}

	void Right(const FString& Value, FVector2D At, const FLinearColor& Color = Ink, float FontScale = 1.0f,
		FName Typeface = Regular) const
	{
		Text(Value, At - FVector2D(Measure(Value, FontScale, Typeface).X, 0), Color, FontScale, Typeface);
	}

	void Center(const FString& Value, FVector2D At, const FLinearColor& Color = Ink, float FontScale = 1.0f,
		FName Typeface = Regular) const
	{
		Text(Value, At - Measure(Value, FontScale, Typeface) * 0.5, Color, FontScale, Typeface);
	}

	void Bar(FVector2D At, FVector2D Extent, float Fraction, const FLinearColor& Color) const
	{
		Box(Track, At, Extent);
		Extent.X *= FMath::Clamp(Fraction, 0.0f, 1.0f);
		if (Extent.X > 0.0) { Box(Color, At, Extent); }
	}

	void Line(FVector2D From, FVector2D To, const FLinearColor& Color, float Thickness = 1.0f) const
	{
		HUD.DrawLine(From.X * Scale, From.Y * Scale, To.X * Scale, To.Y * Scale, Color, Thickness * Scale);
	}

	void Triangle(FVector2D A, FVector2D B, FVector2D C, FLinearColor Color) const
	{
		FCanvasTriangleItem Item(A * Scale, B * Scale, C * Scale, GWhiteTexture);
		Item.SetColor(Color);
		Item.BlendMode = SE_BLEND_Translucent;
		Surface.DrawItem(Item);
	}
};

void DrawSupplyIcon(const FStealthCanvas& UI, FVector2D At, bool bMedkit, const FLinearColor& Color)
{
	if (bMedkit)
	{
		UI.Box(Color, At - FVector2D(6, 1.5), FVector2D(12, 3));
		UI.Box(Color, At - FVector2D(1.5, 6), FVector2D(3, 12));
	}
	else
	{
		for (const double X : {-4.0, 0.0, 4.0})
		{
			UI.Box(Color, At + FVector2D(X - 1, -3), FVector2D(2, 7));
			UI.Box(Color, At + FVector2D(X - 0.5, -5), FVector2D(1, 2));
		}
	}
}

void DrawCompass(const FStealthCanvas& UI, float Yaw)
{
	const FVector2D At(UI.Size.X * 0.5, 26);
	UI.Box(Backdrop, At - FVector2D(109, 9), FVector2D(218, 28));
	const TCHAR* Directions[] = {TEXT("В"), TEXT("СВ"), TEXT("С"), TEXT("СЗ"), TEXT("З"), TEXT("ЮЗ"), TEXT("Ю"), TEXT("ЮВ")};
	const int32 FirstTick = FMath::FloorToInt(Yaw / 15.0f) - 4;
	for (int32 Tick = FirstTick; Tick <= FirstTick + 9; ++Tick)
	{
		const float Offset = (Tick * 15.0f - Yaw) * 1.8f;
		if (FMath::Abs(Offset) >= 101.0f) { continue; }
		const bool bCardinal = Tick % 3 == 0;
		UI.Line(At + FVector2D(Offset, 6), At + FVector2D(Offset, bCardinal ? 12 : 10), bCardinal ? Ink : Muted);
		if (bCardinal)
		{
			UI.Center(Directions[((Tick / 3) % 8 + 8) % 8], At + FVector2D(Offset, -1), Ink, 0.80f, Bold);
		}
	}
	UI.Box(Ink, At + FVector2D(-1, 15), FVector2D(2, 3));
}

const TCHAR* StageCaption(EStealthMissionStage Stage)
{
	switch (Stage)
	{
	case EStealthMissionStage::SafeZone: return TEXT("БЕЗОПАСНАЯ ЗОНА");
	case EStealthMissionStage::Evade: return TEXT("ОТРЫВ ОТ ПОГОНИ");
	case EStealthMissionStage::Extraction: return TEXT("ЭВАКУАЦИЯ");
	default: return TEXT("ПРОНИКНОВЕНИЕ");
	}
}
}

AStealthHUD::AStealthHUD()
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> FloorPlan(TEXT("/Game/Laboratory/Textures/T_FloorPlan.T_FloorPlan"));
	LaboratoryFloorPlan = FloorPlan.Object;
}

void AStealthHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || Canvas->ClipX <= 0.0f || Canvas->ClipY <= 0.0f)
	{
		return;
	}

	const AStealthGameMode* Mission = GetWorld()->GetAuthGameMode<AStealthGameMode>();
	if (Mission && Mission->GetMissionState() != EStealthMissionState::Playing)
	{
		if (Mission->IsMissionResultReady()) { DrawMissionResult(*Mission); }
		return;
	}

	const FStealthCanvas UI(*this, *Canvas);
	const AStealthCharacter* Character = Cast<AStealthCharacter>(GetOwningPawn());
	const AStealthPlayerController* Controller = Cast<AStealthPlayerController>(GetOwningPlayerController());
	if (Mission && Character && Controller && Controller->IsMapOpen())
	{
		DrawTacticalMap(*Mission, *Character);
		return;
	}
	if (Character && Character->GetHitFeedback() > 0.0f)
	{
		const float Strength = Character->GetHitFeedback();
		UI.Box(FLinearColor(0.55f, 0.04f, 0.03f, 0.14f * Strength), FVector2D::ZeroVector, UI.Size);
	}
	DrawGuardIndicators();
	const FVector2D Center = UI.Size * 0.5;
	constexpr float Margin = 22.0f;
	if (Character) { DrawCompass(UI, Character->GetViewRotation().Yaw); }
	if (Mission)
	{
		const EStealthMissionStage Stage = Mission->GetMissionStage();
		const FLinearColor StageColor = Stage == EStealthMissionStage::Evade ? Red : Mint;
		const FVector2D Objective(Margin, Margin);
		const FString Task = Mission->GetObjectiveText();
		const FString Caption = Mission->HasExtendedObjectives()
			? FString::Printf(TEXT("ЭТАП %d/5  •  %s"), 1 + int32(Mission->GetMissionStep()), StageCaption(Stage))
			: StageCaption(Stage);
		const float Width = FMath::Max3(163.0, UI.Measure(Task, 0.94f).X + 28.0, UI.Measure(Caption, 0.76f, Bold).X + 28.0);
		UI.Box(Backdrop, Objective, FVector2D(Width, 58));
		UI.Box(StageColor, Objective, FVector2D(2, 37));
		UI.Text(Caption, Objective + FVector2D(10, 4), StageColor, 0.76f, Bold);
		UI.Text(Task, Objective + FVector2D(10, 19), Ink, 0.94f);
		UI.Text(TEXT("[M] Карта"), Objective + FVector2D(10, 38), Muted, 0.76f);

		const float Level = Mission->GetDetectionLevel();
		if (Level > 0.015f || Stage == EStealthMissionStage::Evade)
		{
			const FLinearColor ThreatColor = Stage == EStealthMissionStage::Evade ? Red : Amber;
			UI.Center(TEXT("ОБНАРУЖЕНИЕ"), FVector2D(Center.X, 55), ThreatColor, 0.78f, Bold);
			UI.Bar(FVector2D(Center.X - 53, 65), FVector2D(106, 3), Level, ThreatColor);
		}
	}

	if (!Character) { return; }

	const FString AccessWarning = Character->GetAccessWarning().ToString();
	const FString Interaction = Character->GetInteractionPrompt().ToString();
	const bool bAccessDenied = !AccessWarning.IsEmpty() || Interaction == AStealthDoor::AccessDeniedText().ToString();
	const FString Prompt = AccessWarning.IsEmpty() ? Interaction : AccessWarning;
	const FLinearColor Dot = Prompt.IsEmpty()
		? FLinearColor(1.0f, 1.0f, 1.0f, FMath::Lerp(1.0f, 0.55f, Character->GetAimAlpha())) : Mint;
	UI.Box(FLinearColor(0, 0, 0, 0.8f), Center - FVector2D(3, 3), FVector2D(6, 6));
	UI.Box(Dot, Center - FVector2D(1.5, 1.5), FVector2D(3, 3));
	if (const float Alpha = Character->GetHitConfirmAlpha(); Alpha > 0.0f)
	{
		const FLinearColor Confirm(Ink.R, Ink.G, Ink.B, Alpha);
		for (const FVector2D Arm : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1)})
		{
			UI.Line(Center + Arm * 5.0, Center + Arm * 9.0, Confirm, 1.4f);
		}
	}
	if (!Prompt.IsEmpty())
	{
		const float PromptWidth = FMath::Min<float>(UI.Size.X - 2 * Margin, UI.Measure(Prompt, 1.0f).X + 74.0f);
		const FVector2D At(Center.X - PromptWidth * 0.5f, Center.Y + 23);
		UI.Box(Panel, At, FVector2D(PromptWidth, 34));
		UI.Box(bAccessDenied ? Red : Mint, At + FVector2D(5, 5), FVector2D(43, 24));
		UI.Center(bAccessDenied ? TEXT("!") : TEXT("E / A"), At + FVector2D(26.5, 17), Panel, 0.8f);
		const float TextScale = FMath::Min(1.0f, (PromptWidth - 65.0f) / FMath::Max(1.0, UI.Measure(Prompt, 1.0f).X));
		UI.Text(Prompt, At + FVector2D(57, 17 - UI.Measure(Prompt, TextScale).Y * 0.5), Ink, TextScale);
	}

	const float FootY = UI.Size.Y - Margin;
	const FVector2D Vitals(Margin, FootY - 26);
	UI.Box(Backdrop, Vitals, FVector2D(128, 26));
	const float Health = Character->GetHealthFraction();
	DrawSupplyIcon(UI, Vitals + FVector2D(14, 13), true, Health > 0.3f ? Ink : Red);
	UI.Bar(Vitals + FVector2D(27, 11), FVector2D(90, 4), Health, Health > 0.3f ? Ink : Red);

	const FVector2D Inventory(UI.Size.X - Margin - 143, FootY - 54);
	UI.Box(Backdrop, Inventory, FVector2D(143, 54));
	DrawSupplyIcon(UI, Inventory + FVector2D(16, 19), false, Muted);
	UI.Right(FString::FromInt(Character->GetAmmoInMagazine()), Inventory + FVector2D(90, 2),
		Character->GetAmmoInMagazine() == 0 ? Amber : Ink, 2.05f, Bold);
	UI.Text(TEXT("/"), Inventory + FVector2D(94, 12), Muted, 0.9f);
	UI.Text(FString::FromInt(Character->GetReserveAmmo()), Inventory + FVector2D(105, 12), Muted, 1.1f);
	UI.Right(FString::Printf(TEXT("[Q] Приманка  %d"), Character->GetNoiseMakerCount()),
		Inventory + FVector2D(132, 35), Muted, 0.82f);
	if (Character->IsReloading())
	{
		UI.Bar(Inventory + FVector2D(0, -3), FVector2D(143, 3), Character->GetReloadProgress(), Amber);
	}
	if (Character->GetAmmoPickupFeedback() > 0.0f)
	{
		UI.Right(FString::Printf(TEXT("+%d к запасу"), Character->GetLastAmmoPickup()),
			Inventory + FVector2D(143, -20), AidColor, 0.9f, Bold);
	}
	if (Character->IsHealing())
	{
		const FVector2D At(Center.X - 67, Center.Y + 31);
		UI.Box(Backdrop, At, FVector2D(134, 33));
		UI.Center(TEXT("ЛЕЧЕНИЕ"), At + FVector2D(67, 11), AidColor, 0.85f, Bold);
		UI.Bar(At + FVector2D(11, 24), FVector2D(112, 3), Character->GetHealingProgress(), AidColor);
	}
}

void AStealthHUD::DrawTacticalMap(const AStealthGameMode& Mission, const AStealthCharacter& Character)
{
	const bool bUseFloorPlan = LaboratoryFloorPlan && GetWorld()->GetMapName().EndsWith(TEXT("ResearchFacility"));
	if (!MapBounds.bIsValid)
	{
		if (bUseFloorPlan)
		{
			MapBounds = FBox2D(FVector2D(-100, -100), FVector2D(6300, 3500));
		}
		else
		{
			for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
			{
				const UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
				if (!Mesh || !Mesh->IsQueryCollisionEnabled() || Mesh->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block
					|| Mesh->Bounds.BoxExtent.Z < 35.0)
				{
					continue;
				}
				const FBox Bounds = Mesh->Bounds.GetBox();
				const FBox2D Footprint(FVector2D(Bounds.Min), FVector2D(Bounds.Max));
				MapObstacles.Add(Footprint);
				MapBounds += Footprint;
			}
			if (MapBounds.bIsValid) { MapBounds = MapBounds.ExpandBy(250.0); }
		}
	}

	const FStealthCanvas UI(*this, *Canvas);
	const FVector2D At = UI.Size * 0.5 - FVector2D(383, 232);
	UI.Box(FLinearColor(0.01f, 0.02f, 0.03f, 0.76f), FVector2D::ZeroVector, UI.Size);
	UI.Box(Panel, At, FVector2D(766, 464));
	UI.Box(Mint, At, FVector2D(766, 3));
	UI.Text(TEXT("КАРТА ОБЪЕКТА"), At + FVector2D(24, 19), Ink, 1.42f, Bold);
	UI.Right(TEXT("[M] / View  Закрыть"), At + FVector2D(742, 26), Muted, 0.9f);

	if (!MapBounds.bIsValid || MapBounds.GetArea() <= 0.0)
	{
		UI.Center(TEXT("Схема объекта недоступна"), UI.Size * 0.5, Muted);
		return;
	}

	const FVector2D MapAt = At + FVector2D(24, 60);
	const FVector2D MapSize(513, 360);
	UI.Box(FLinearColor(0.027f, 0.045f, 0.051f), MapAt, MapSize);
	const FVector2D Extent = MapBounds.GetSize();
	const double Fit = FMath::Min(MapSize.X / Extent.X, MapSize.Y / Extent.Y);
	const FVector2D TopLeft = MapAt + (MapSize - Extent * Fit) * 0.5;
	const auto ProjectMap = [this, TopLeft, Fit](FVector2D Position)
	{
		return TopLeft + FVector2D(Position.X - MapBounds.Min.X, MapBounds.Max.Y - Position.Y) * Fit;
	};
	if (bUseFloorPlan)
	{
		DrawTextureSimple(LaboratoryFloorPlan, TopLeft.X * UI.Scale, TopLeft.Y * UI.Scale,
			Extent.X * Fit * UI.Scale / LaboratoryFloorPlan->GetSizeX(), false);
	}
	for (const FBox2D& Obstacle : MapObstacles)
	{
		const FVector2D Position = ProjectMap(FVector2D(Obstacle.Min.X, Obstacle.Max.Y));
		const FVector2D Size = Obstacle.GetSize() * Fit;
		UI.Box(FLinearColor(0.27f, 0.34f, 0.39f), Position, FVector2D(FMath::Max(1.0, Size.X), FMath::Max(1.0, Size.Y)));
	}

	const EStealthMissionStep MissionStep = Mission.GetMissionStep();
	const bool bCanExtract = Mission.CanExtract();
	const bool bPursued = Mission.GetMissionStage() == EStealthMissionStage::Evade;
	const FLinearColor ExitColor = bCanExtract ? (bPursued ? Amber : Mint) : Muted;
	const FLinearColor PlayerColor(0.48f, 0.81f, 1.0f);
	const FVector2D PlayerPin = ProjectMap(FVector2D(Character.GetActorLocation()));
	const float ViewYaw = Character.GetViewRotation().Yaw;
	const FVector2D Forward = FVector2D(FMath::Cos(FMath::DegreesToRadians(ViewYaw)), -FMath::Sin(FMath::DegreesToRadians(ViewYaw)));
	const FVector2D Side(-Forward.Y, Forward.X);
	FLinearColor Sector = PlayerColor;
	Sector.A = 0.28f;
	for (int32 Step = 0; Step < 12; ++Step)
	{
		const auto Edge = [&](int32 Index)
		{
			const float Angle = FMath::DegreesToRadians(-34.0f + 68.0f * Index / 12.0f);
			const FVector2D Point = PlayerPin + (Forward * FMath::Cos(Angle) + Side * FMath::Sin(Angle)) * 39.0;
			return FVector2D(FMath::Clamp(Point.X, MapAt.X, MapAt.X + MapSize.X),
				FMath::Clamp(Point.Y, MapAt.Y, MapAt.Y + MapSize.Y));
		};
		UI.Triangle(PlayerPin, Edge(Step), Edge(Step + 1), Sector);
	}
	TOptional<FVector> Destination;
	for (TActorIterator<AStealthSupplyPickup> It(GetWorld()); It; ++It)
	{
		const FVector2D Pin = ProjectMap(FVector2D(It->GetActorLocation()));
		const bool bMedkit = It->GetKind() == EStealthSupplyKind::Medkit;
		UI.Box(Panel, Pin - FVector2D(8, 8), FVector2D(16, 16));
		DrawSupplyIcon(UI, Pin, bMedkit, bMedkit ? AidColor : Amber);
	}
	const auto MarkObjective = [&](const AActor& Target, const TCHAR* Label)
	{
		Destination = Target.GetActorLocation();
		const FVector2D Pin = ProjectMap(FVector2D(Destination.GetValue()));
		UI.Box(Panel, Pin - FVector2D(7, 7), FVector2D(14, 14));
		UI.Box(Amber, Pin - FVector2D(4, 4), FVector2D(8, 8));
		UI.Right(Label, Pin - FVector2D(11, 7), Amber, 0.86f);
	};
	switch (MissionStep)
	{
	case EStealthMissionStep::RetrieveData:
		for (TActorIterator<AStealthObjective> It(GetWorld()); It; ++It) { MarkObjective(**It, TEXT("Данные")); break; }
		break;
	case EStealthMissionStep::UploadData:
	case EStealthMissionStep::VerifyArchive:
		for (TActorIterator<AStealthOperationsTerminal> It(GetWorld()); It; ++It)
		{
			if (It->bVerifiesArchive == (MissionStep == EStealthMissionStep::VerifyArchive))
			{
				MarkObjective(**It, It->bVerifiesArchive ? TEXT("Сверка архива") : TEXT("Загрузка"));
				break;
			}
		}
		break;
	case EStealthMissionStep::RecoverKeycard:
		for (TActorIterator<AStealthKeycard> It(GetWorld()); It; ++It) { MarkObjective(**It, TEXT("Пропуск")); break; }
		if (!Destination)
		{
			for (TActorIterator<ASoundHearingNPC> It(GetWorld()); It; ++It)
			{
				if (It->bCarriesKeycard && !It->IsDead()) { MarkObjective(**It, TEXT("Охранник архива")); break; }
			}
		}
		break;
	case EStealthMissionStep::Extract:
		break;
	}
	for (TActorIterator<AStealthExitZone> It(GetWorld()); It; ++It)
	{
		if (bCanExtract) { Destination = It->GetActorLocation(); }
		const FVector2D Pin = ProjectMap(FVector2D(It->GetActorLocation()));
		UI.Box(ExitColor, Pin - FVector2D(8, 8), FVector2D(16, 16));
		UI.Box(Panel, Pin - FVector2D(5, 5), FVector2D(10, 10));
		UI.Text(TEXT("Эвакуация"), Pin + FVector2D(12, -7), ExitColor, 0.86f);
	}

	const FVector2D Tip = PlayerPin + Forward * 19.0;
	const FVector2D Tail = PlayerPin - Forward * 7.0;
	const FVector2D Left = Tail + Side * 7.0;
	const FVector2D Right = Tail - Side * 7.0;
	UI.Line(Tail, PlayerPin + Forward * 38.0, Panel, 3.4f);
	UI.Line(Tail, PlayerPin + Forward * 38.0, PlayerColor, 1.35f);
	UI.Triangle(Tip, Left, PlayerPin, PlayerColor);
	UI.Triangle(Tip, PlayerPin, Right, PlayerColor);
	UI.Line(Tip, Left, Panel, 1.25f);
	UI.Line(Tip, Right, Panel, 1.25f);
	UI.Line(Left, PlayerPin, Panel, 1.25f);
	UI.Line(Right, PlayerPin, Panel, 1.25f);
	const FVector2D North = MapAt + FVector2D(MapSize.X - 20, 25);
	UI.Box(Panel, North - FVector2D(12, 16), FVector2D(24, 39));
	UI.Center(TEXT("С"), North - FVector2D(0, 7), Ink, 0.88f, Bold);
	UI.Triangle(North + FVector2D(0, 2), North + FVector2D(-5, 16), North + FVector2D(5, 16), Ink);
	const FVector2D Ruler = MapAt + FVector2D(MapSize.X - 15 - 2000 * Fit, MapSize.Y - 14);
	UI.Line(Ruler, Ruler + FVector2D(2000 * Fit, 0), Ink, 1.5f);
	UI.Center(TEXT("20 м"), Ruler + FVector2D(1000 * Fit, -9), Ink, 0.72f);

	const FVector2D Legend = At + FVector2D(555, 72);
	UI.Text(TEXT("ОБОЗНАЧЕНИЯ"), Legend, Muted, 0.82f);
	const TCHAR* Headings[] = {TEXT("В"), TEXT("СВ"), TEXT("С"), TEXT("СЗ"), TEXT("З"), TEXT("ЮЗ"), TEXT("Ю"), TEXT("ЮВ")};
	const int32 Heading = FMath::RoundToInt(ViewYaw / 45.0f);
	UI.Text(FString::Printf(TEXT("Вы смотрите на %s"), Headings[(Heading % 8 + 8) % 8]),
		Legend + FVector2D(0, 31), PlayerColor);
	UI.Text(Mission.HasKeycard() ? TEXT("Пропуск получен") : TEXT("Цель миссии"), Legend + FVector2D(0, 65), Amber);
	UI.Text(TEXT("Точка эвакуации"), Legend + FVector2D(0, 99), ExitColor);
	const TCHAR* ExitStatus = !bCanExtract ? TEXT("Откроется после выполнения задач")
		: bPursued ? TEXT("Оторвитесь от охраны") : TEXT("Доступна");
	const float StatusScale = FMath::Min(.82f, 183.0f / FMath::Max(1.0, UI.Measure(ExitStatus, 1.0f).X));
	UI.Text(ExitStatus, Legend + FVector2D(0, 122), Muted, StatusScale);
	UI.Box(Track, Legend + FVector2D(0, 162), FVector2D(194, 1));
	UI.Text(TEXT("ТЕКУЩАЯ ЦЕЛЬ"), Legend + FVector2D(0, 180), Muted, 0.82f, Bold);
	const FString Active = Mission.GetObjectiveText();
	const float TaskScale = FMath::Min(.94f, 183.0f / FMath::Max(1.0f, UI.Measure(Active, 1.0f).X));
	UI.Text(Active, Legend + FVector2D(0, 205), Mint, TaskScale);
	if (Destination)
	{
		UI.Text(FString::Printf(TEXT("%d м по прямой"), FMath::RoundToInt(
			FVector::Dist2D(Character.GetActorLocation(), Destination.GetValue()) / 100.0)),
			Legend + FVector2D(0, 225), Muted, 0.85f);
	}
	DrawSupplyIcon(UI, Legend + FVector2D(6, 254), true, AidColor);
	UI.Text(TEXT("Аптечка"), Legend + FVector2D(19, 247), Ink, 0.9f);
	DrawSupplyIcon(UI, Legend + FVector2D(6, 279), false, Amber);
	UI.Text(TEXT("Патроны"), Legend + FVector2D(19, 272), Ink, 0.9f);
	UI.Text(TEXT("Игра на паузе"), At + FVector2D(24, 432), Muted, 0.86f);
}

void AStealthHUD::DrawGuardIndicators()
{
	const APlayerController* PC = GetOwningPlayerController();
	if (!PC) { return; }

	const FStealthCanvas UI(*this, *Canvas);
	FVector Eye;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(Eye, ViewRotation);
	const FVector Forward = ViewRotation.Vector();
	for (TActorIterator<ASoundHearingNPC> It(GetWorld()); It; ++It)
	{
		const ASoundHearingNPC& Guard = **It;
		const auto* AI = Cast<AStealthAIController>(Guard.GetController());
		if (!AI || Guard.IsDead()) { continue; }

		const TCHAR* Icon;
		FLinearColor Color;
		switch (AI->GetAIState())
		{
		case EStealthAIState::Suspicious: Icon = TEXT("?"); Color = Amber; break;
		case EStealthAIState::Investigate: Icon = TEXT(">"); Color = FLinearColor(0.53f, 0.81f, 1.0f); break;
		case EStealthAIState::Search: Icon = TEXT("..."); Color = FLinearColor(1.0f, 0.60f, 0.25f); break;
		case EStealthAIState::Chase: Icon = TEXT("!"); Color = Red; break;
		default: continue;
		}

		const float HalfHeight = Guard.GetSimpleCollisionHalfHeight();
		const FVector Head = Guard.GetActorLocation() + FVector(0, 0, HalfHeight * 0.75f);
		const FVector ToGuard = Head - Eye;
		if (ToGuard.SizeSquared() > FMath::Square(2500.0) || FVector::DotProduct(Forward, ToGuard) <= 0.0)
		{
			continue;
		}
		const FVector Screen = Project(Head + FVector(0, 0, 41), false);
		const FVector2D Anchor(Screen.X / UI.Scale, Screen.Y / UI.Scale);
		const FString Label = AI->GetStateDisplayName().ToString();
		const float Width = UI.Measure(Label, 0.74f).X + 36.0f;
		const FVector2D At(Anchor.X - Width * 0.5f, Anchor.Y - 22);
		if (At.X < 4 || At.Y < 4 || At.X + Width > UI.Size.X - 4 || Anchor.Y > UI.Size.Y - 4)
		{
			continue;
		}

		FCollisionQueryParams Sight(SCENE_QUERY_STAT(StealthGuardIndicator), false, GetOwningPawn());
		Sight.AddIgnoredActor(&Guard);
		if (GetWorld()->LineTraceTestByChannel(Eye, Head, ECC_Visibility, Sight)) { continue; }

		UI.Box(Panel, At, FVector2D(Width, 22));
		UI.Box(Color, At + FVector2D(3, 3), FVector2D(20, 16));
		UI.Center(Icon, At + FVector2D(13, 11), Panel, 0.82f);
		UI.Text(Label, At + FVector2D(29, 11 - UI.Measure(Label, 0.74f).Y * 0.5), Ink, 0.74f);
	}
}

void AStealthHUD::DrawMissionResult(const AStealthGameMode& Mission)
{
	const FStealthCanvas UI(*this, *Canvas);
	const FVector2D Center = UI.Size * 0.5;
	const FVector2D At = Center - FVector2D(206, 125);
	const bool bWon = Mission.GetMissionState() == EStealthMissionState::Won;
	const FLinearColor Accent = bWon ? Mint : Red;
	UI.Box(FLinearColor(0.01f, 0.02f, 0.03f, 0.82f), FVector2D::ZeroVector, UI.Size);
	UI.Box(Panel, At, FVector2D(412, 250));
	UI.Box(Accent, At, FVector2D(412, 3));

	UI.Center(bWon ? TEXT("МИССИЯ ВЫПОЛНЕНА") : TEXT("МИССИЯ ПРОВАЛЕНА"),
		At + FVector2D(206, 42), Accent, 1.7f, Bold);
	UI.Center(bWon ? TEXT("Данные получены. Эвакуация завершена.") : TEXT("Охрана вас нейтрализовала."),
		At + FVector2D(206, 78), Ink);
	UI.Box(Track, At + FVector2D(29, 105), FVector2D(354, 1));
	const int32 Seconds = FMath::FloorToInt(Mission.GetMissionTime());
	UI.Center(FString::Printf(TEXT("ВРЕМЯ  %02d:%02d      ДАННЫЕ  %s"), Seconds / 60, Seconds % 60,
		Mission.HasObjective() ? TEXT("ПОЛУЧЕНЫ") : TEXT("НЕ ПОЛУЧЕНЫ")), At + FVector2D(206, 128), Muted, 0.88f);

	const FVector2D ButtonAt = At + FVector2D(29, 157);
	const FVector2D ButtonSize(354, 43);
	AddHitBox(ButtonAt * UI.Scale, ButtonSize * UI.Scale, RestartHitBox, true);
	float MouseX, MouseY;
	const APlayerController* PC = GetOwningPlayerController();
	const bool bHovered = PC && PC->GetMousePosition(MouseX, MouseY)
		&& GetHitBoxAtCoordinates(FVector2D(MouseX, MouseY), true) == GetHitBoxWithName(RestartHitBox);
	const FLinearColor ButtonColor = bHovered ? FMath::Lerp(Accent, FLinearColor::White, 0.25f) : Accent;
	UI.Box(ButtonColor, ButtonAt, ButtonSize);
	UI.Center(TEXT("Перезапустить миссию"), ButtonAt + ButtonSize * 0.5, Panel, 1.16f, Bold);
	UI.Center(TEXT("Enter / кнопка меню"), At + FVector2D(206, 224), Muted, 0.78f);
}

void AStealthHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	if (BoxName == RestartHitBox)
	{
		if (AStealthPlayerController* PC = Cast<AStealthPlayerController>(GetOwningPlayerController()))
		{
			PC->RestartMission();
		}
	}
}
