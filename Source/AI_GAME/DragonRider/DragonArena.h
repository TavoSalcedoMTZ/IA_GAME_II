// Dragon Rider - Director de la arena

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "DragonTypes.h"
#include "DragonArena.generated.h"

class ADragonEgg;
class ADragonNest;
class ADragonCharacter;

// Subsistema del mundo
UCLASS()
class AI_GAME_API UDragonRiderSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// Todos los huevos que lleva el jugador
	TArray<ADragonEgg*> GetEggsCarriedBy(const AActor* Carrier) const;
	ADragonEgg* GetEggCarriedBy(const AActor* Carrier) const;
	void AddCarried(AActor* Carrier, ADragonEgg* Egg);
	void RemoveCarried(AActor* Carrier, ADragonEgg* Egg);

private:
	TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<ADragonEgg>>> CarriedEggs;
};

// Monta el escenario alrededor de su posicion
UCLASS()
class AI_GAME_API ADragonArenaDirector : public AActor
{
	GENERATED_BODY()

public:
	ADragonArenaDirector();

	// Guardianes por nido
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena", meta = (ClampMin = "1", ClampMax = "2"))
	int32 DragonsPerNest = 2;

	// Distancia de cada nido al centro de la arena
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float NestDistance = 7500.f;

	// Escala de los nidos
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float NestScale = 2.5f;

protected:
	virtual void BeginPlay() override;

	void SetupArena();
	FVector GroundPoint(const FVector& XY) const;
	ADragonNest* SpawnNest(EDragonSpecies Species, const FVector& Location, float Yaw);
	void SpawnGuardians(ADragonNest* Nest, TSubclassOf<ADragonCharacter> DragonClass);

	FTimerHandle SetupTimer;
	bool bSetupDone = false;
};
