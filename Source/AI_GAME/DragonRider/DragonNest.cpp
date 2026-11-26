// Dragon Rider - Nidos, huevos e incubadora

#include "DragonNest.h"
#include "DragonCharacter.h"
#include "DragonArena.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UStaticMeshComponent* DragonRider::MakePrimitive(AActor* Owner, USceneComponent* Parent, const TCHAR* Shape, const FVector& RelLoc,
	const FVector& Scale, const FLinearColor& Color, bool bCollision, const FRotator& Rot)
{
	const FString MeshPath = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape);
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
	Comp->SetStaticMesh(Mesh);
	Comp->SetupAttachment(Parent);
	Comp->SetRelativeLocation(RelLoc);
	Comp->SetRelativeRotation(Rot);
	Comp->SetRelativeScale3D(Scale);
	Comp->SetCanEverAffectNavigation(bCollision); // con NavMesh dinamico, los caminantes rodean pilares/rocas
	Comp->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollision)
	{
		Comp->SetCollisionProfileName(TEXT("BlockAll"));
	}
	Comp->RegisterComponent();
	Owner->AddInstanceComponent(Comp);

	if (Mat)
	{
		Comp->SetMaterial(0, Mat);
		if (UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
	return Comp;
}

// NIDO

ADragonNest::ADragonNest()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(60.f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADragonNest::InitNest(EDragonSpecies InSpecies)
{
	if (bInitialized) return;
	bInitialized = true;
	Species = InSpecies;

	const FLinearColor Brown(0.25f, 0.13f, 0.05f);
	const FLinearColor Stone(0.35f, 0.35f, 0.38f);
	const float S = FMath::Max(0.5f, NestScale);

	switch (Species)
	{
	case EDragonSpecies::Volador:
	{
		// Nido elevado de dificil acceso
		NestHeight = 160.f * S;
		const float Half = 250.f * S;
		DragonRider::MakePrimitive(this, Root, TEXT("Cube"), FVector(0, 0, NestHeight * 0.5f), FVector(5.f * S, 5.f * S, NestHeight / 100.f), Stone, true);
		const float RampLen = 950.f * S;
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(NestHeight, RampLen));
		const float HalfRun = 0.5f * RampLen * FMath::Cos(FMath::DegreesToRadians(Pitch));
		DragonRider::MakePrimitive(this, Root, TEXT("Cube"), FVector(Half + HalfRun - 10.f, 0, NestHeight * 0.5f - 8.f),
			FVector(RampLen / 100.f, 3.f * S, 0.2f), Stone, true, FRotator(-Pitch, 0, 0));
		// Postes altos como referencia visual del nido volador
		for (int32 i = 0; i < 4; ++i)
		{
			const float Ang = FMath::DegreesToRadians(135.f + i * 30.f);
			DragonRider::MakePrimitive(this, Root, TEXT("Cylinder"), FVector(FMath::Cos(Ang) * Half * 0.85f, FMath::Sin(Ang) * Half * 0.85f, NestHeight + 300.f * S),
				FVector(0.3f * S, 0.3f * S, 6.f * S), Brown, true);
		}
		break;
	}
	case EDragonSpecies::Saltarin:
	{
		// Pilares-percha alrededor del nido
		NestHeight = 0.f;
		const int32 NumPillars = 4;
		for (int32 i = 0; i < NumPillars; ++i)
		{
			const float Ang = FMath::DegreesToRadians(45.f + i * 90.f);
			const FVector Offset(FMath::Cos(Ang) * 600.f * S, FMath::Sin(Ang) * 600.f * S, 0.f);
			const float H = (200.f + 50.f * i) * S;
			DragonRider::MakePrimitive(this, Root, TEXT("Cylinder"), Offset + FVector(0, 0, H * 0.5f), FVector(1.4f * S, 1.4f * S, H / 100.f), Stone, true);
			PerchPoints.Add(GetActorTransform().TransformPosition(Offset + FVector(0, 0, H + 60.f)));
		}
		// Muros de roca escalables
		for (int32 i = 0; i < 6; ++i)
		{
			const float Yaw = i * 60.f;
			const float Ang = FMath::DegreesToRadians(Yaw);
			const FVector Offset(FMath::Cos(Ang) * 950.f * S, FMath::Sin(Ang) * 950.f * S, 0.f);
			const float WallH = 240.f * S; // ~6 m con S = 2.5
			DragonRider::MakePrimitive(this, Root, TEXT("Cube"), Offset + FVector(0, 0, WallH * 0.5f),
				FVector(0.6f * S, 3.2f * S, WallH / 100.f), FLinearColor(0.28f, 0.3f, 0.25f), true, FRotator(0, Yaw, 0));
		}
		break;
	}
	case EDragonSpecies::Impulsivo:
	default:
	{
		// Circulo de rocas territoriales
		NestHeight = 0.f;
		for (int32 i = 0; i < 8; ++i)
		{
			const float Ang = FMath::DegreesToRadians(i * 45.f + 20.f);
			DragonRider::MakePrimitive(this, Root, TEXT("Cube"), FVector(FMath::Cos(Ang) * 480.f * S, FMath::Sin(Ang) * 480.f * S, 40.f * S),
				FVector(0.9f * S, 0.9f * S, 0.8f * S), Stone, true, FRotator(0, i * 37.f, 0));
		}
		break;
	}
	}

	// Cuenco del nido
	DragonRider::MakePrimitive(this, Root, TEXT("Cylinder"), FVector(0, 0, NestHeight + 15.f), FVector(2.2f * S, 2.2f * S, 0.3f), Brown, false);

	Label->SetWorldSize(60.f * S);
	Label->SetRelativeLocation(FVector(0, 0, NestHeight + 420.f * S));
	Label->SetText(FText::FromString(FString::Printf(TEXT("NIDO %s"), DragonRider::SpeciesName(Species))));
	Label->SetTextRenderColor(DragonRider::SpeciesColor(Species).ToFColor(true));

	// Huevos
	for (int32 i = 0; i < NumEggs; ++i)
	{
		const float EggAng = FMath::DegreesToRadians(i * 360.f / FMath::Max(1, NumEggs));
		const FVector EggLoc = GetActorLocation() + FVector(FMath::Cos(EggAng) * 110.f, FMath::Sin(EggAng) * 110.f, NestHeight + 70.f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ADragonEgg* Egg = GetWorld()->SpawnActor<ADragonEgg>(EggLoc, FRotator::ZeroRotator, Params))
		{
			Egg->InitEgg(this, Species);
			Eggs.Add(Egg);
		}
	}
}

FVector ADragonNest::GetNestTopLocation() const
{
	return GetActorLocation() + FVector(0, 0, NestHeight);
}

void ADragonNest::AddGuardian(ADragonCharacter* Dragon)
{
	if (Dragon)
	{
		Guardians.AddUnique(Dragon);
		Dragon->SetHomeNest(this);
	}
}

void ADragonNest::RemoveGuardian(ADragonCharacter* Dragon)
{
	Guardians.Remove(Dragon);
}

void ADragonNest::OnEggTaken(ADragonEgg* Egg, AActor* Thief)
{
	for (ADragonCharacter* Dragon : Guardians)
	{
		if (IsValid(Dragon))
		{
			Dragon->NotifyEggStolen(Thief);
		}
	}
}

// HUEVO

ADragonEgg::ADragonEgg()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(90.f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
	SetRootComponent(Trigger);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.6f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereMesh.Succeeded()) Mesh->SetStaticMesh(SphereMesh.Object);
	if (Mat.Succeeded()) Mesh->SetMaterial(0, Mat.Object);
}

void ADragonEgg::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADragonEgg::OnTriggerOverlap);
}

void ADragonEgg::InitEgg(ADragonNest* Nest, EDragonSpecies InSpecies)
{
	HomeNest = Nest;
	Species = InSpecies;
	HomeLocation = GetActorLocation();
	if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
	{
		// Huevo claro con el tono de la especie
		MID->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(DragonRider::SpeciesColor(Species), FLinearColor::White, 0.45f));
	}
}

void ADragonEgg::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Carrier) return;
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled()) return;

	UDragonRiderSubsystem* Sub = GetWorld()->GetSubsystem<UDragonRiderSubsystem>();
	const int32 Slot = Sub ? Sub->GetEggsCarriedBy(Pawn).Num() : 0; // se pueden llevar varios huevos

	// Los huevos se acomodan en "la mochila" del jugador
	static const FVector Slots[] = {
		FVector(60.f, 40.f, -30.f), FVector(60.f, -40.f, -30.f), FVector(-35.f, 0.f, 10.f),
		FVector(-35.f, 45.f, -15.f), FVector(-35.f, -45.f, -15.f), FVector(-45.f, 0.f, 50.f) };
	constexpr int32 NumSlots = UE_ARRAY_COUNT(Slots);
	const FVector Offset = Slots[Slot % NumSlots] + FVector(0.f, 0.f, 12.f * (Slot / NumSlots));

	Carrier = Pawn;
	AttachToActor(Pawn, FAttachmentTransformRules::KeepWorldTransform);
	SetActorRelativeLocation(Offset);
	SetActorRelativeScale3D(FVector(0.55f));
	if (Sub) Sub->AddCarried(Pawn, this);
	if (HomeNest) HomeNest->OnEggTaken(this, Pawn);

	GetWorldTimerManager().SetTimer(WatchTimer, this, &ADragonEgg::WatchCarrier, 0.5f, true);
}

void ADragonEgg::WatchCarrier()
{
	APawn* Pawn = Cast<APawn>(Carrier);
	if (!IsValid(Pawn) || Pawn->GetController() == nullptr || Pawn->IsHidden())
	{
		ReturnToNest();
	}
}

void ADragonEgg::ReturnToNest()
{
	if (UDragonRiderSubsystem* Sub = GetWorld()->GetSubsystem<UDragonRiderSubsystem>())
	{
		Sub->RemoveCarried(Carrier, this);
	}
	GetWorldTimerManager().ClearTimer(WatchTimer);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Carrier = nullptr;
	SetActorScale3D(FVector(1.f));
	SetActorLocation(HomeLocation);
}
