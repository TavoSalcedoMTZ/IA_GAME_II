// Dragon Rider - Nodos de StateTree

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "DragonTypes.h"
#include "DragonStateTreeNodes.generated.h"

class UDataTable;
class ADragonCharacter;

// ENUMS DE PARAMETROS

UENUM()
enum class EDragonCheck : uint8
{
	HasTarget				UMETA(DisplayName = "Tiene objetivo"),
	HasLineOfSight			UMETA(DisplayName = "Linea de vision"),
	EggStolen				UMETA(DisplayName = "Huevo robado"),
	Investigate				UMETA(DisplayName = "Debe investigar"),
	AnyAlert				UMETA(DisplayName = "Cualquier alerta (objetivo/huevo/investigar)"),
	Leashing				UMETA(DisplayName = "Lejos del nido"),
	IsStunned				UMETA(DisplayName = "Aturdido"),
	HasEnergy				UMETA(DisplayName = "Energia >= Threshold"),
	LowHealth				UMETA(DisplayName = "Salud < Threshold"),
	TargetCarryingOurEgg	UMETA(DisplayName = "Objetivo lleva NUESTRO huevo"),
	ActionIs				UMETA(DisplayName = "Accion Markov actual == Action"),
	FuryAbove				UMETA(DisplayName = "Furia > Threshold"),
	TargetCloserThan		UMETA(DisplayName = "Distancia objetivo < Threshold")
};

UENUM()
enum class EDragonMoveGoal : uint8
{
	Target				UMETA(DisplayName = "Objetivo (perseguir)"),
	InvestigateLocation	UMETA(DisplayName = "Ultima ubicacion conocida / ruido"),
	Nest				UMETA(DisplayName = "Nido"),
	PatrolPoint			UMETA(DisplayName = "Siguiente punto de patrulla"),
	OrbitTarget			UMETA(DisplayName = "Rodear al objetivo"),
	FlankTarget			UMETA(DisplayName = "Flanquear (espalda del objetivo)"),
	RetreatFromTarget	UMETA(DisplayName = "Alejarse del objetivo"),
	PerchPoint			UMETA(DisplayName = "Percha elevada"),
	EggSearchPoint		UMETA(DisplayName = "Zona donde se vio al ladron")
};

UENUM()
enum class EDragonMoveStyle : uint8
{
	Walk	UMETA(DisplayName = "Caminar/Correr (NavMesh)"),
	Leap	UMETA(DisplayName = "Saltos (NavMesh + LaunchCharacter)"),
	Fly		UMETA(DisplayName = "Volar (steering 3D)")
};

UENUM()
enum class EDragonAttackType : uint8
{
	Fireball	UMETA(DisplayName = "Bola de fuego (distancia)"),
	Pounce		UMETA(DisplayName = "Embestida saltando"),
	Charge		UMETA(DisplayName = "Embestida en carga"),
	DiveSlam	UMETA(DisplayName = "Caida en picada (area)")
};

// INSTANCE DATA BASE

// Todos los nodos reciben el Actor
USTRUCT()
struct AI_GAME_API FDragonNodeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;
};

// Base de las tareas
USTRUCT()
struct AI_GAME_API FDragonTaskInstanceData : public FDragonNodeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FString Label;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EDragonMood Mood = EDragonMood::Calm;
};

// EVALUATOR

USTRUCT()
struct AI_GAME_API FDragonSensesEvaluatorInstanceData : public FDragonNodeInstanceData
{
	GENERATED_BODY()

	// Intervalo de sensado
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float UpdateInterval = 0.15f;

	// Segundos que recuerda al objetivo despues de perderlo de vista
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float TargetMemory = 3.f;

	// Edad maxima de un ruido para investigarlo
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float InvestigateMemory = 6.f;

	// ---- Salidas
	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<AActor> Target = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bHasTarget = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float DistanceToTarget = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float AngleToTarget = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bHasLineOfSight = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float HealthPct = 1.f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float Energy = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float Fury = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bTargetCarryingOurEgg = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bEggStolen = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bInvestigate = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bLeashing = false;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float DistanceToNest = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	EDragonAction CurrentAction = EDragonAction::Approach;

	// runtime (no reflejado)
	float NextUpdateTime = 0.f;
	float LastSeenTime = -100.f;
	float LastHandledNoiseTime = -100.f;
	bool bHadTarget = false;
};

// Evaluator multi-parametro desacoplado
USTRUCT(meta = (DisplayName = "Dragon: Evaluador Sensorial", Category = "Dragon Rider"))
struct AI_GAME_API FDragonSensesEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDragonSensesEvaluatorInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

private:
	void Sense(ADragonCharacter* Dragon, FInstanceDataType& Data, float Now) const;
};

// CONDICION

USTRUCT()
struct AI_GAME_API FDragonConditionInstanceData : public FDragonNodeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EDragonCheck Check = EDragonCheck::HasTarget;

	// Solo para Check = ActionIs
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EDragonAction Action = EDragonAction::Approach;

	// Para HasEnergy / LowHealth / FuryAbove / TargetCloserThan
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Threshold = 0.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bNegate = false;
};

// Condicion generica que lee la Blackboard del dragon
USTRUCT(meta = (DisplayName = "Dragon: Condicion", Category = "Dragon Rider"))
struct AI_GAME_API FDragonCondition_Check : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDragonConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

// TAREA: MARKOV

USTRUCT()
struct AI_GAME_API FDragonMarkovInstanceData : public FDragonTaskInstanceData
{
	GENERATED_BODY()

	// DataTable con filas FDragonMarkovRow
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UDataTable> MarkovTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FDragonActionRule ApproachRule;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FDragonActionRule AttackRule;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FDragonActionRule ManeuverRule;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FDragonActionRule RepositionRule;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float LowHealthThreshold = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float FarDistance = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float NearDistance = 450.f;

	// Nombres legibles de las 4 acciones para esta especie
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FString> ActionNames;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	EDragonAction SelectedAction = EDragonAction::Approach;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float SelectedProbability = 0.f;

	// Fila final
	UPROPERTY(VisibleAnywhere, Category = "Output")
	FString FinalRow;
};

// Selecciona la siguiente accion de combate con una Cadena de Markov 4x4
USTRUCT(meta = (DisplayName = "Dragon: Seleccion Markov", Category = "Dragon Rider"))
struct AI_GAME_API FDragonTask_MarkovSelect : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FDragonTask_MarkovSelect() { bShouldCallTick = false; }

	using FInstanceDataType = FDragonMarkovInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

// TAREA: MOVER

USTRUCT()
struct AI_GAME_API FDragonMoveInstanceData : public FDragonTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EDragonMoveGoal Goal = EDragonMoveGoal::Target;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EDragonMoveStyle Style = EDragonMoveStyle::Walk;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float SpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadius = 150.f;

	// 0 = sin limite
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MaxDuration = 0.f;

	// Altura sobre el destino
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float FlyHeight = 400.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float LeapInterval = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float LeapMaxDistance = 900.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float OrbitDistance = 700.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RetreatDistance = 900.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bFaceTarget = false;

	// Para Goal = EggSearchPoint
	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 MaxEggSearches = 3;

	// runtime
	FVector GoalLocation = FVector::ZeroVector;
	float StartTime = 0.f;
	float NextRepathTime = 0.f;
	float NextLeapTime = 0.f;
	float OrbitSign = 1.f;
	bool bUsingNav = false;
};

// Movimiento parametrizado
USTRUCT(meta = (DisplayName = "Dragon: Mover", Category = "Dragon Rider"))
struct AI_GAME_API FDragonTask_Move : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDragonMoveInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

private:
	bool ComputeGoal(ADragonCharacter* Dragon, FInstanceDataType& Data) const;
	void IssueMove(ADragonCharacter* Dragon, FInstanceDataType& Data) const;
};

// TAREA: ATACAR

USTRUCT()
struct AI_GAME_API FDragonAttackInstanceData : public FDragonTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EDragonAttackType AttackType = EDragonAttackType::Pounce;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Damage = 12.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float WindupTime = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RecoveryTime = 0.6f;

	// Costo de energia
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float EnergyCost = 0.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 Shots = 1;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ShotInterval = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ProjectileSpeed = 2200.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ChargeSpeedMultiplier = 2.6f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ActiveDuration = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float HitRadius = 170.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float KnockbackStrength = 600.f;

	// Alcance cuerpo a cuerpo
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MeleeReach = 90.f;

	// Carga: velocidad de giro
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ChargeTurnRate = 110.f;

	// Prediccion
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float LeadTime = 0.35f;

	// Saltarin: se pega a una pared y embiste desde ahi
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bUseWallCling = false;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float WallSearchRadius = 1100.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ClingTime = 0.55f;

	// Probabilidad de usar la pared si encuentra una
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float WallClingChance = 0.8f;

	// runtime
	int32 Phase = 0; // 0 preparacion, 1 activo, 2 recuperacion
	int32 SubPhase = 0; // Pounce: 1 saltando a la pared, 2 pegado, 3 embestida
	float PhaseStart = 0.f;
	float SubPhaseStart = 0.f;
	int32 ShotsFired = 0;
	float NextShotTime = 0.f;
	bool bHit = false;
	FVector LockedDir = FVector::ForwardVector;
	FVector ClingPoint = FVector::ZeroVector;
	FVector ClingNormal = FVector::ZeroVector;
};

// Ataques especificos por arquetipo
USTRUCT(meta = (DisplayName = "Dragon: Atacar", Category = "Dragon Rider"))
struct AI_GAME_API FDragonTask_Attack : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDragonAttackInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

private:
	void StartActive(ADragonCharacter* Dragon, FInstanceDataType& Data, float Now) const;
	void FireOne(ADragonCharacter* Dragon, FInstanceDataType& Data) const;
	void LaunchPounce(ADragonCharacter* Dragon, FInstanceDataType& Data, float Arc) const;
	bool FindClingPoint(ADragonCharacter* Dragon, FInstanceDataType& Data) const;
	void TryMeleeHit(ADragonCharacter* Dragon, FInstanceDataType& Data, float Reach, float UpKick) const;
};

// TAREA: ESPERAR / VIGILAR / RUGIR

USTRUCT()
struct AI_GAME_API FDragonWaitInstanceData : public FDragonTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MinDuration = 1.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MaxDuration = 2.f;

	// Gira mirando alrededor
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bLookAround = false;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bFaceTarget = false;

	// Rugido: sube la furia, pulso visual y alerta a los demas guardianes
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bRoar = false;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RoarFury = 0.3f;

	// runtime
	float EndTime = 0.f;
	float NextLookTime = 0.f;
	float LookYaw = 0.f;
};

USTRUCT(meta = (DisplayName = "Dragon: Esperar / Vigilar / Rugir", Category = "Dragon Rider"))
struct AI_GAME_API FDragonTask_Wait : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDragonWaitInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

// TAREA: RECARGAR

USTRUCT()
struct AI_GAME_API FDragonRechargeInstanceData : public FDragonTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float EnergyPerSecond = 1.5f;
};

// Repone municion
USTRUCT(meta = (DisplayName = "Dragon: Recargar en el nido", Category = "Dragon Rider"))
struct AI_GAME_API FDragonTask_Recharge : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDragonRechargeInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

// TAREA: ATURDIDO

USTRUCT(meta = (DisplayName = "Dragon: Aturdido", Category = "Dragon Rider"))
struct AI_GAME_API FDragonTask_Stunned : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FDragonTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
