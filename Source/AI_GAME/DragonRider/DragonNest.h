// Dragon Rider - Nidos, huevos e incubadora

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DragonTypes.h"
#include "DragonNest.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class ADragonCharacter;
class ADragonEgg;

// Helper: crea en runtime una primitiva coloreada
namespace DragonRider
{
	UStaticMeshComponent* MakePrimitive(AActor* Owner, USceneComponent* Parent, const TCHAR* Shape, const FVector& RelLoc,
		const FVector& Scale, const FLinearColor& Color, bool bCollision, const FRotator& Rot = FRotator::ZeroRotator);
}

// Nido de una especie
UCLASS()
class AI_GAME_API ADragonNest : public AActor
{
	GENERATED_BODY()

public:
	ADragonNest();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nest")
	EDragonSpecies Species = EDragonSpecies::Impulsivo;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nest")
	int32 NumEggs = 3;

	// Escala de toda la geometria del nido
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nest")
	float NestScale = 1.f;

	// Puntos elevados donde el Saltarin vigila
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nest")
	TArray<FVector> PerchPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nest")
	TArray<TObjectPtr<ADragonCharacter>> Guardians;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nest")
	TArray<TObjectPtr<ADragonEgg>> Eggs;

	// Construye la geometria y los huevos
	void InitNest(EDragonSpecies InSpecies);

	FVector GetNestTopLocation() const;

	void AddGuardian(ADragonCharacter* Dragon);
	void RemoveGuardian(ADragonCharacter* Dragon);

	// Un huevo de este nido fue tomado por el jugador
	void OnEggTaken(ADragonEgg* Egg, AActor* Thief);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Nest")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Nest")
	TObjectPtr<UTextRenderComponent> Label;

	float NestHeight = 0.f;
	bool bInitialized = false;
};

// Huevo: se recoge al tocarlo, se lleva pegado al jugador y se entrega
UCLASS()
class AI_GAME_API ADragonEgg : public AActor
{
	GENERATED_BODY()

public:
	ADragonEgg();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Egg")
	EDragonSpecies Species = EDragonSpecies::Impulsivo;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Egg")
	TObjectPtr<ADragonNest> HomeNest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Egg")
	TObjectPtr<AActor> Carrier;

	void InitEgg(ADragonNest* Nest, EDragonSpecies InSpecies);
	void ReturnToNest();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void WatchCarrier();

	UPROPERTY(VisibleAnywhere, Category = "Egg")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Egg")
	TObjectPtr<UStaticMeshComponent> Mesh;

	FVector HomeLocation = FVector::ZeroVector;
	FTimerHandle WatchTimer;
};
