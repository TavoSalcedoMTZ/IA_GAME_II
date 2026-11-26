// Dragon Rider - Jugador y GameMode

#pragma once

#include "CoreMinimal.h"
#include "AI_GAMECharacter.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "DragonRiderGame.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UInputMappingContext;

// Jugador en primera persona
UCLASS()
class AI_GAME_API ADragonRiderPlayer : public AAI_GAMECharacter
{
	GENERATED_BODY()

public:
	ADragonRiderPlayer();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float MaxHealth = 150.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
	float Health = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float FireCooldown = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float BoltDamage = 10.f;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	float GetHealthPct() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }
	int32 GetDeaths() const { return Deaths; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Fire();
	void Respawn();

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UStaticMeshComponent> Crossbow;

	FTransform SpawnTransform;
	float LastFireTime = -10.f;
	int32 Deaths = 0;
};

// Dardo del jugador
UCLASS()
class AI_GAME_API ADragonPlayerBolt : public AActor
{
	GENERATED_BODY()

public:
	ADragonPlayerBolt();

	float Damage = 10.f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> Movement;
};

UCLASS()
class AI_GAME_API ADragonRiderPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADragonRiderPlayerController();

	// Consola (~)
	UFUNCTION(Exec)
	void DRNido(int32 Index);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> MappingContexts;
};

UCLASS()
class AI_GAME_API ADragonRiderGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADragonRiderGameMode();
};
