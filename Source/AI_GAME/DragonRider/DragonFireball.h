// Dragon Rider - Bola de fuego del Dragon Volador

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DragonFireball.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS()
class AI_GAME_API ADragonFireball : public AActor
{
	GENERATED_BODY()

public:
	ADragonFireball();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireball")
	float Damage = 12.f;

	void Launch(const FVector& Direction, float Speed);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnStop(const FHitResult& ImpactResult);

	UPROPERTY(VisibleAnywhere, Category = "Fireball")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Fireball")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Fireball")
	TObjectPtr<UProjectileMovementComponent> Movement;
};
