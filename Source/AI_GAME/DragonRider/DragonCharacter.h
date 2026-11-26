// Dragon Rider - Personaje base de los dragones

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DragonTypes.h"
#include "DragonCharacter.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;
class UStateTree;
class ADragonNest;

// Dragon base
UCLASS(Abstract)
class AI_GAME_API ADragonCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADragonCharacter();

	// ---------- Configuracion del arquetipo ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon")
	EDragonSpecies Species = EDragonSpecies::Impulsivo;

	// StateTree que ejecutara el AIController al poseer a este dragon
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon|AI")
	TSoftObjectPtr<UStateTree> StateTreeAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon|Stats")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dragon|Stats")
	float Health = 100.f;

	// Municion / estamina
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon|Stats")
	float MaxEnergy = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dragon|Stats")
	float Energy = 100.f;

	// Regeneracion pasiva de energia por segundo
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon|Stats")
	float EnergyRegenPerSecond = 10.f;

	// Furia 0..1
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dragon|Stats")
	float Fury = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon|Stats")
	float FuryDecayPerSecond = 0.04f;

	// Dano acumulado necesario para aturdir
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon|Stun")
	float StunThreshold = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon|Stun")
	float StunDuration = 2.5f;

	// Radio de la "correa" al nido
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon|AI")
	float LeashRadius = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon|AI")
	float PatrolRadius = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon|Movement")
	float BaseSpeed = 450.f;

	// Los voladores usan MOVE_Flying
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dragon|Movement")
	bool bCanFly = false;

	// ---------- Estado runtime ----------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dragon|AI")
	FDragonBlackboard Blackboard;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dragon|AI")
	TObjectPtr<ADragonNest> HomeNest;

	// ---------- API usada por el StateTree ----------

	void SetHomeNest(ADragonNest* Nest);
	FVector GetNestLocation() const;
	FVector GetNextPatrolPoint();
	bool GetRandomPerchPoint(FVector& OutPoint) const;

	bool IsStunned() const;
	bool IsDead() const { return bIsDead; }
	float GetHealthPct() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	bool ConsumeEnergy(float Amount);
	void AddEnergy(float Amount);
	void AddFury(float Amount);

	// Llamado por el nido cuando el jugador roba un huevo
	void NotifyEggStolen(AActor* Thief);

	// Texto de estado sobre la cabeza
	void SetStatus(const FString& InStatus, EDragonMood Mood);
	void SetDebugDetail(const FString& InDetail);

	// Pequeño "pulso" visual
	void PlayPulse(float Scale, float Duration);

	// Telegrafiado del ataque
	void SetAttackTell(bool bOn);

	void SetLastAttackName(const FString& InName) { LastAttackName = InName; }
	const FString& GetLastAttackName() const { return LastAttackName; }

	// ¿El objetivo esta al alcance cuerpo a cuerpo?
	bool IsInMeleeReach(const AActor* Target, float Reach) const;

	// Aplica dano a un actor
	void DealDamageTo(AActor* Victim, float Amount, const FVector& Knockback);

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	FVector GetMouthLocation() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Cada subclase construye su silueta con primitivas
	virtual void BuildBody() {}

	UStaticMeshComponent* AddShape(FName Name, const TCHAR* MeshPath, const FVector& RelLoc, const FVector& Scale, const FRotator& Rot = FRotator::ZeroRotator, bool bAccent = false);

	void ApplyColors();
	void RefreshText();
	void Die();
	void SlowUpdate(); // 4 Hz: regeneracion de energia, decaimiento de furia, texto

	UPROPERTY(VisibleAnywhere, Category = "Dragon|Visual")
	TObjectPtr<USceneComponent> BodyRoot;

	UPROPERTY(VisibleAnywhere, Category = "Dragon|Visual")
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> AccentParts;

	UPROPERTY(EditAnywhere, Category = "Dragon|Visual")
	FLinearColor BodyColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, Category = "Dragon|Visual")
	FLinearColor AccentColor = FLinearColor(1.f, 0.8f, 0.2f);

	UPROPERTY(EditAnywhere, Category = "Dragon|Visual")
	float TextHeight = 170.f;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMIDs;

	FString LastAttackName = TEXT("Ataque");
	bool bAttackTell = false;

	FString StatusString = TEXT("Idle");
	FString DetailString;
	EDragonMood CurrentMood = EDragonMood::Calm;

	float StunAccum = 0.f;
	float StunEndTime = -1.f;
	float LastDamageTime = -100.f;
	bool bIsDead = false;

	int32 PatrolIndex = 0;
	TArray<FVector> PatrolPoints;

	float PulseEndTime = 0.f;
	float PulseDuration = 0.f;
	float PulseScale = 1.f;

	FTimerHandle SlowTimer;
};

// Arquetipo 1
UCLASS()
class AI_GAME_API ADragonVolador : public ADragonCharacter
{
	GENERATED_BODY()
public:
	ADragonVolador();
protected:
	virtual void BuildBody() override;
};

// Arquetipo 2
UCLASS()
class AI_GAME_API ADragonSaltarin : public ADragonCharacter
{
	GENERATED_BODY()
public:
	ADragonSaltarin();
protected:
	virtual void BuildBody() override;
};

// Arquetipo 3
UCLASS()
class AI_GAME_API ADragonImpulsivo : public ADragonCharacter
{
	GENERATED_BODY()
public:
	ADragonImpulsivo();
protected:
	virtual void BuildBody() override;
};
