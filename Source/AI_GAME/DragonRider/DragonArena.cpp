// Dragon Rider - Director de la arena y subsistema de juego

#include "DragonArena.h"
#include "DragonNest.h"
#include "DragonCharacter.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// SUBSISTEMA

bool UDragonRiderSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TArray<ADragonEgg*> UDragonRiderSubsystem::GetEggsCarriedBy(const AActor* Carrier) const
{
	TArray<ADragonEgg*> Out;
	if (!Carrier) return Out;
	if (const TArray<TWeakObjectPtr<ADragonEgg>>* Found = CarriedEggs.Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(Carrier))))
	{
		for (const TWeakObjectPtr<ADragonEgg>& Egg : *Found)
		{
			if (Egg.IsValid()) Out.Add(Egg.Get());
		}
	}
	return Out;
}

ADragonEgg* UDragonRiderSubsystem::GetEggCarriedBy(const AActor* Carrier) const
{
	const TArray<ADragonEgg*> Eggs = GetEggsCarriedBy(Carrier);
	return Eggs.Num() > 0 ? Eggs[0] : nullptr;
}

void UDragonRiderSubsystem::AddCarried(AActor* Carrier, ADragonEgg* Egg)
{
	if (Carrier && Egg) CarriedEggs.FindOrAdd(Carrier).AddUnique(Egg);
}

void UDragonRiderSubsystem::RemoveCarried(AActor* Carrier, ADragonEgg* Egg)
{
	if (!Carrier) return;
	if (TArray<TWeakObjectPtr<ADragonEgg>>* Found = CarriedEggs.Find(Carrier))
	{
		Found->Remove(Egg);
		if (Found->Num() == 0) CarriedEggs.Remove(Carrier);
	}
}

// DIRECTOR

ADragonArenaDirector::ADragonArenaDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADragonArenaDirector::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(SetupTimer, this, &ADragonArenaDirector::SetupArena, 0.2f, false);
}

FVector ADragonArenaDirector::GroundPoint(const FVector& XY) const
{
	FHitResult Hit;
	const FVector Start(XY.X, XY.Y, GetActorLocation().Z + 5000.f);
	const FVector End(XY.X, XY.Y, GetActorLocation().Z - 5000.f);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		return Hit.ImpactPoint;
	}
	return FVector(XY.X, XY.Y, GetActorLocation().Z);
}

void ADragonArenaDirector::SetupArena()
{
	if (bSetupDone) return;
	bSetupDone = true;

	struct FNestDef { EDragonSpecies Species; float Angle; TSubclassOf<ADragonCharacter> Class; };
	const FNestDef Defs[] = {
		{ EDragonSpecies::Volador,     90.f, ADragonVolador::StaticClass() },
		{ EDragonSpecies::Impulsivo,  210.f, ADragonImpulsivo::StaticClass() },
		{ EDragonSpecies::Saltarin,   330.f, ADragonSaltarin::StaticClass() },
	};

	const FVector Center = GetActorLocation();
	for (const FNestDef& Def : Defs)
	{
		const FVector Dir = FRotator(0.f, Def.Angle, 0.f).Vector();
		const FVector Loc = GroundPoint(Center + Dir * NestDistance);
		if (ADragonNest* Nest = SpawnNest(Def.Species, Loc, Def.Angle))
		{
			SpawnGuardians(Nest, Def.Class);
		}
	}

}

ADragonNest* ADragonArenaDirector::SpawnNest(EDragonSpecies Species, const FVector& Location, float Yaw)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// +X local del nido
	ADragonNest* Nest = GetWorld()->SpawnActor<ADragonNest>(Location, FRotator(0.f, Yaw + 180.f, 0.f), Params);
	if (Nest)
	{
		Nest->NestScale = NestScale;
		Nest->InitNest(Species);
	}
	return Nest;
}

void ADragonArenaDirector::SpawnGuardians(ADragonNest* Nest, TSubclassOf<ADragonCharacter> DragonClass)
{
	for (int32 i = 0; i < DragonsPerNest; ++i)
	{
		const float Ang = FMath::DegreesToRadians(90.f + i * (360.f / DragonsPerNest));
		const float Radius = 450.f * NestScale;
		FVector Loc = GroundPoint(Nest->GetActorLocation() + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * Radius);
		Loc.Z += 150.f;
		if (DragonClass->GetDefaultObject<ADragonCharacter>()->bCanFly)
		{
			Loc.Z += 600.f;
		}

		// Spawn diferido
		const FTransform Xf(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Loc);
		ADragonCharacter* Dragon = GetWorld()->SpawnActorDeferred<ADragonCharacter>(DragonClass, Xf, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Dragon)
		{
			Nest->AddGuardian(Dragon);
			UGameplayStatics::FinishSpawningActor(Dragon, Xf);
			Dragon->SetHomeNest(Nest);
		}
	}
}
