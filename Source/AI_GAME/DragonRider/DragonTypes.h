// Dragon Rider - Tipos compartidos de la IA

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DragonTypes.generated.h"

// Las 3 especies del GDD
UENUM(BlueprintType)
enum class EDragonSpecies : uint8
{
	Volador		UMETA(DisplayName = "Volador (Tirador a distancia)"),
	Saltarin	UMETA(DisplayName = "Saltarin (Flanqueador)"),
	Impulsivo	UMETA(DisplayName = "Impulsivo (Tanque / Berserker)")
};

// Los 4 estados de la cadena de Markov de combate
UENUM(BlueprintType)
enum class EDragonAction : uint8
{
	Approach	UMETA(DisplayName = "Perseguir"),
	Attack		UMETA(DisplayName = "Atacar"),
	Maneuver	UMETA(DisplayName = "Maniobra (Rodear/Flanquear/Rugir)"),
	Reposition	UMETA(DisplayName = "Reposicion (Retroceder/Percha/Picada)"),
	MAX			UMETA(Hidden)
};

// Estado de animo
UENUM(BlueprintType)
enum class EDragonMood : uint8
{
	Calm, // blanco
	Alert, // amarillo
	Combat, // rojo
	Stunned, // azul
	Special // naranja (GOAP / huevo robado)
};

// Fila de la matriz de transicion de Markov
USTRUCT(BlueprintType)
struct FDragonMarkovRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markov", meta = (ClampMin = "0", ClampMax = "1"))
	float ToApproach = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markov", meta = (ClampMin = "0", ClampMax = "1"))
	float ToAttack = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markov", meta = (ClampMin = "0", ClampMax = "1"))
	float ToManeuver = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markov", meta = (ClampMin = "0", ClampMax = "1"))
	float ToReposition = 0.25f;

	// Texto descriptivo para el profesor
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markov")
	FString Notes;

	float Get(EDragonAction A) const
	{
		switch (A)
		{
		case EDragonAction::Approach:   return ToApproach;
		case EDragonAction::Attack:     return ToAttack;
		case EDragonAction::Maneuver:   return ToManeuver;
		case EDragonAction::Reposition: return ToReposition;
		default: return 0.f;
		}
	}

	float Sum() const { return ToApproach + ToAttack + ToManeuver + ToReposition; }
};

// Precondiciones GOAP y moduladores de una accion
USTRUCT(BlueprintType)
struct FDragonActionRule
{
	GENERATED_BODY()

	// Precondicion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precondiciones")
	float MinDistance = 0.f;

	// Precondicion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precondiciones")
	float MaxDistance = 100000.f;

	// Precondicion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precondiciones")
	float EnergyCost = 0.f;

	// Precondicion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precondiciones")
	bool bRequiresLineOfSight = false;

	// Precondicion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Precondiciones")
	float MaxFury = 2.f;

	// Modulador
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moduladores")
	float LowHealthMultiplier = 1.f;

	// Modulador
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moduladores")
	float FuryMultiplier = 1.f;

	// Modulador
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moduladores")
	float FarMultiplier = 1.f;

	// Modulador
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moduladores")
	float NearMultiplier = 1.f;
};

// "Pizarra" del dragon
USTRUCT(BlueprintType)
struct FDragonBlackboard
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	TObjectPtr<AActor> Target = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	bool bHasTarget = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	bool bHasLineOfSight = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	float DistanceToTarget = 0.f;

	// Angulo (grados) entre el frente del dragon y la direccion al objetivo
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	float AngleToTarget = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	bool bTargetCarryingOurEgg = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	bool bTargetCarryingAnyEgg = false;

	// Investigar
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	bool bInvestigate = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	FVector InvestigateLocation = FVector::ZeroVector;

	// Huevo robado de NUESTRO nido
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	bool bEggStolen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	FVector EggThiefLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	int32 EggSearchCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	float DistanceToNest = 0.f;

	// Demasiado lejos del nido
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	bool bLeashing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	EDragonAction CurrentAction = EDragonAction::Approach;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blackboard")
	float CurrentActionProbability = 0.f;
};

namespace DragonRider
{
	inline const TCHAR* ActionName(EDragonAction A)
	{
		switch (A)
		{
		case EDragonAction::Approach:   return TEXT("Approach");
		case EDragonAction::Attack:     return TEXT("Attack");
		case EDragonAction::Maneuver:   return TEXT("Maneuver");
		case EDragonAction::Reposition: return TEXT("Reposition");
		default: return TEXT("None");
		}
	}

	inline const TCHAR* SpeciesName(EDragonSpecies S)
	{
		switch (S)
		{
		case EDragonSpecies::Volador:   return TEXT("VOLADOR");
		case EDragonSpecies::Saltarin:  return TEXT("SALTARIN");
		case EDragonSpecies::Impulsivo: return TEXT("IMPULSIVO");
		default: return TEXT("?");
		}
	}

	inline FLinearColor SpeciesColor(EDragonSpecies S)
	{
		switch (S)
		{
		case EDragonSpecies::Volador:   return FLinearColor(0.85f, 0.08f, 0.05f); // rojo
		case EDragonSpecies::Saltarin:  return FLinearColor(0.10f, 0.75f, 0.15f); // verde
		case EDragonSpecies::Impulsivo: return FLinearColor(0.45f, 0.10f, 0.75f); // morado
		default: return FLinearColor::White;
		}
	}
}
