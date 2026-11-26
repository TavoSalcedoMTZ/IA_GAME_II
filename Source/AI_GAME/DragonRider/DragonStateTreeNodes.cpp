// Dragon Rider - Nodos de StateTree

#include "DragonStateTreeNodes.h"
#include "DragonCharacter.h"
#include "DragonNest.h"
#include "DragonArena.h"
#include "DragonFireball.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Damage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDragonAI, Log, All);

namespace DragonST
{
	static ADragonCharacter* GetDragon(const FDragonNodeInstanceData& Data)
	{
		ADragonCharacter* Dragon = Cast<ADragonCharacter>(Data.Actor);
		return (Dragon && !Dragon->IsDead()) ? Dragon : nullptr;
	}

	static AAIController* GetAIC(ADragonCharacter* Dragon)
	{
		return Dragon ? Cast<AAIController>(Dragon->GetController()) : nullptr;
	}

	static void ApplyLabel(ADragonCharacter* Dragon, const FDragonTaskInstanceData& Data)
	{
		if (Dragon && !Data.Label.IsEmpty())
		{
			Dragon->SetStatus(Data.Label, Data.Mood);
		}
	}

	// Gira suavemente al dragon
	static void FaceTowards(ADragonCharacter* Dragon, const FVector& Point, float DeltaTime, float Speed = 8.f)
	{
		const FVector Dir = (Point - Dragon->GetActorLocation()).GetSafeNormal2D();
		if (Dir.IsNearlyZero()) return;
		const FRotator Want(0.f, Dir.Rotation().Yaw, 0.f);
		Dragon->SetActorRotation(FMath::RInterpTo(Dragon->GetActorRotation(), Want, DeltaTime, Speed));
	}

	static bool ProjectToNav(UWorld* World, const FVector& In, FVector& Out)
	{
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			FNavLocation Loc;
			if (Nav->ProjectPointToNavigation(In, Loc, FVector(400.f, 400.f, 800.f)))
			{
				Out = Loc.Location;
				return true;
			}
		}
		return false;
	}

	static FString EnumText(const UEnum* Enum, int64 Value)
	{
		return Enum ? Enum->GetDisplayNameTextByValue(Value).ToString() : FString();
	}
}

// EVALUATOR SENSORIAL

void FDragonSensesEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	// Desfase aleatorio
	const float Now = Context.GetWorld() ? Context.GetWorld()->GetTimeSeconds() : 0.f;
	Data.NextUpdateTime = Now + FMath::FRandRange(0.f, Data.UpdateInterval);
}

void FDragonSensesEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon) return;

	const float Now = Context.GetWorld()->GetTimeSeconds();
	if (Now < Data.NextUpdateTime)
	{
		return; // early-out barato
	}
	Data.NextUpdateTime = Now + Data.UpdateInterval;
	Sense(Dragon, Data, Now);
}

void FDragonSensesEvaluator::Sense(ADragonCharacter* Dragon, FInstanceDataType& Data, float Now) const
{
	FDragonBlackboard& BB = Dragon->Blackboard;
	UWorld* World = Dragon->GetWorld();

	AActor* Seen = nullptr;
	AActor* DamagedBy = nullptr;
	bool bNewNoise = false;
	FVector NoiseLocation = FVector::ZeroVector;

	// ---------- 1) Leer AIPerception
	AAIController* AIC = DragonST::GetAIC(Dragon);
	UAIPerceptionComponent* Perception = AIC ? AIC->GetPerceptionComponent() : nullptr;
	if (Perception)
	{
		const FAISenseID SightID = UAISense::GetSenseID<UAISense_Sight>();
		const FAISenseID HearingID = UAISense::GetSenseID<UAISense_Hearing>();
		const FAISenseID DamageID = UAISense::GetSenseID<UAISense_Damage>();

		TArray<AActor*> Known;
		Perception->GetKnownPerceivedActors(nullptr, Known);
		for (AActor* Actor : Known)
		{
			const APawn* Pawn = Cast<APawn>(Actor);
			if (!Pawn || !Pawn->IsPlayerControlled()) continue;

			const FActorPerceptionInfo* Info = Perception->GetActorInfo(*Actor);
			if (!Info) continue;

			for (const FAIStimulus& Stim : Info->LastSensedStimuli)
			{
				if (!Stim.IsValid()) continue;
				if (Stim.Type == SightID)
				{
					if (Stim.WasSuccessfullySensed()) { Seen = Actor; }
				}
				else if (Stim.Type == HearingID)
				{
					const float StimTime = Now - Stim.GetAge();
					if (Stim.WasSuccessfullySensed() && Stim.GetAge() < Data.InvestigateMemory && StimTime > Data.LastHandledNoiseTime + 0.5f)
					{
						bNewNoise = true;
						NoiseLocation = Stim.StimulusLocation;
						Data.LastHandledNoiseTime = StimTime;
					}
				}
				else if (Stim.Type == DamageID)
				{
					if (Stim.WasSuccessfullySensed() && Stim.GetAge() < 1.5f) { DamagedBy = Actor; }
				}
			}
		}
	}

	// ---------- 2) Objetivo
	if (Seen)
	{
		BB.Target = Seen;
		Data.LastSeenTime = Now;
	}
	else if (DamagedBy)
	{
		BB.Target = DamagedBy; // Reaccion a dano
		Data.LastSeenTime = Now;
	}

	const APawn* TargetPawn = Cast<APawn>(BB.Target);
	const bool bTargetAlive = IsValid(BB.Target) && (!TargetPawn || TargetPawn->GetController() != nullptr);
	const bool bRemember = bTargetAlive && (Now - Data.LastSeenTime) < Data.TargetMemory;

	// ---------- 3) Estado del objetivo
	UDragonRiderSubsystem* Sub = World->GetSubsystem<UDragonRiderSubsystem>();
	const TArray<ADragonEgg*> CarriedEggs = (Sub && bTargetAlive) ? Sub->GetEggsCarriedBy(BB.Target) : TArray<ADragonEgg*>();
	int32 OurEggs = 0;
	for (const ADragonEgg* Egg : CarriedEggs)
	{
		if (Egg && Egg->HomeNest == Dragon->HomeNest) ++OurEggs;
	}
	BB.bTargetCarryingAnyEgg = CarriedEggs.Num() > 0;
	BB.bTargetCarryingOurEgg = OurEggs > 0;
	// Ver al ladron con NUESTROS huevos enfurece
	if (OurEggs > 0 && Seen == BB.Target)
	{
		Dragon->AddFury(0.02f * OurEggs);
	}
	if (BB.bTargetCarryingOurEgg && bRemember)
	{
		BB.EggThiefLocation = BB.Target->GetActorLocation();
		BB.bEggStolen = true;
	}

	// ---------- 4) Correa al nido ----------
	BB.DistanceToNest = FVector::Dist2D(Dragon->GetActorLocation(), Dragon->GetNestLocation());
	const float Leash = Dragon->LeashRadius * ((BB.bTargetCarryingOurEgg || BB.bEggStolen) ? 1.8f : 1.f);
	if (BB.DistanceToNest > Leash)
	{
		BB.bLeashing = true;
	}
	else if (BB.DistanceToNest < Dragon->LeashRadius * 0.5f)
	{
		BB.bLeashing = false;
	}

	// ---------- 5) HasTarget / Investigar ----------
	BB.bHasTarget = bRemember && !BB.bLeashing;

	if (BB.bHasTarget)
	{
		BB.TargetLocation = BB.Target->GetActorLocation();
		BB.bInvestigate = false;

		// Comunicacion de escuadron
		if (!Data.bHadTarget && Dragon->HomeNest)
		{
			for (ADragonCharacter* Mate : Dragon->HomeNest->Guardians)
			{
				if (IsValid(Mate) && Mate != Dragon && !Mate->Blackboard.bHasTarget)
				{
					Mate->Blackboard.bInvestigate = true;
					Mate->Blackboard.InvestigateLocation = BB.TargetLocation;
				}
			}
		}
	}
	else
	{
		// Se perdio al objetivo
		if (Data.bHadTarget && !BB.bLeashing)
		{
			BB.bInvestigate = true;
			BB.InvestigateLocation = BB.TargetLocation;
		}
		if (!bRemember)
		{
			BB.Target = nullptr;
		}
		if (bNewNoise && !BB.bLeashing)
		{
			BB.bInvestigate = true;
			BB.InvestigateLocation = NoiseLocation;
		}
		if (BB.bLeashing)
		{
			BB.bInvestigate = false;
		}
	}
	Data.bHadTarget = BB.bHasTarget;

	// ---------- 6) Geometria relativa ----------
	if (IsValid(BB.Target))
	{
		const FVector ToTarget = BB.Target->GetActorLocation() - Dragon->GetActorLocation();
		BB.DistanceToTarget = ToTarget.Size();
		const float Dot = FVector::DotProduct(Dragon->GetActorForwardVector().GetSafeNormal2D(), ToTarget.GetSafeNormal2D());
		BB.AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f)));
		BB.bHasLineOfSight = (Seen == BB.Target);
	}
	else
	{
		BB.DistanceToTarget = 100000.f;
		BB.AngleToTarget = 180.f;
		BB.bHasLineOfSight = false;
	}

	// ---------- 7) Salidas visibles en el StateTree Debugger ----------
	Data.Target = BB.Target;
	Data.bHasTarget = BB.bHasTarget;
	Data.DistanceToTarget = BB.DistanceToTarget;
	Data.AngleToTarget = BB.AngleToTarget;
	Data.bHasLineOfSight = BB.bHasLineOfSight;
	Data.HealthPct = Dragon->GetHealthPct();
	Data.Energy = Dragon->Energy;
	Data.Fury = Dragon->Fury;
	Data.bTargetCarryingOurEgg = BB.bTargetCarryingOurEgg;
	Data.bEggStolen = BB.bEggStolen;
	Data.bInvestigate = BB.bInvestigate;
	Data.bLeashing = BB.bLeashing;
	Data.DistanceToNest = BB.DistanceToNest;
	Data.CurrentAction = BB.CurrentAction;
}

// CONDICION

bool FDragonCondition_Check::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const ADragonCharacter* Dragon = Cast<ADragonCharacter>(Data.Actor);
	if (!Dragon) return false;
	const FDragonBlackboard& BB = Dragon->Blackboard;

	bool bResult = false;
	switch (Data.Check)
	{
	case EDragonCheck::HasTarget:            bResult = BB.bHasTarget; break;
	case EDragonCheck::HasLineOfSight:       bResult = BB.bHasLineOfSight; break;
	case EDragonCheck::EggStolen:            bResult = BB.bEggStolen; break;
	case EDragonCheck::Investigate:          bResult = BB.bInvestigate; break;
	case EDragonCheck::AnyAlert:             bResult = BB.bHasTarget || BB.bEggStolen || BB.bInvestigate || BB.bLeashing; break;
	case EDragonCheck::Leashing:             bResult = BB.bLeashing; break;
	case EDragonCheck::IsStunned:            bResult = Dragon->IsStunned(); break;
	case EDragonCheck::HasEnergy:            bResult = Dragon->Energy + KINDA_SMALL_NUMBER >= Data.Threshold; break;
	case EDragonCheck::LowHealth:            bResult = Dragon->GetHealthPct() < Data.Threshold; break;
	case EDragonCheck::TargetCarryingOurEgg: bResult = BB.bTargetCarryingOurEgg; break;
	case EDragonCheck::ActionIs:             bResult = BB.CurrentAction == Data.Action; break;
	case EDragonCheck::FuryAbove:            bResult = Dragon->Fury > Data.Threshold; break;
	case EDragonCheck::TargetCloserThan:     bResult = BB.bHasTarget && BB.DistanceToTarget < Data.Threshold; break;
	default: break;
	}
	return Data.bNegate ? !bResult : bResult;
}

#if WITH_EDITOR
FText FDragonCondition_Check::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	FString Text = TEXT("Dragon: ");
	if (const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>())
	{
		if (Data->bNegate) Text += TEXT("NO ");
		Text += DragonST::EnumText(StaticEnum<EDragonCheck>(), (int64)Data->Check);
		if (Data->Check == EDragonCheck::ActionIs)
		{
			Text += TEXT(" = ") + DragonST::EnumText(StaticEnum<EDragonAction>(), (int64)Data->Action);
		}
		else if (Data->Check == EDragonCheck::HasEnergy || Data->Check == EDragonCheck::LowHealth || Data->Check == EDragonCheck::FuryAbove || Data->Check == EDragonCheck::TargetCloserThan)
		{
			Text += FString::Printf(TEXT(" (%.2f)"), Data->Threshold);
		}
	}
	return FText::FromString(Text);
}
#endif

// MARKOV

EStateTreeRunStatus FDragonTask_MarkovSelect::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon) return EStateTreeRunStatus::Failed;

	FDragonBlackboard& BB = Dragon->Blackboard;
	const EDragonAction From = BB.CurrentAction;
	constexpr int32 N = (int32)EDragonAction::MAX;

	// ---------- 1) Fila de la matriz
	float P[N] = { 0.25f, 0.25f, 0.25f, 0.25f };
	if (Data.MarkovTable)
	{
		if (const FDragonMarkovRow* Row = Data.MarkovTable->FindRow<FDragonMarkovRow>(FName(DragonRider::ActionName(From)), TEXT("DragonMarkov"), false))
		{
			const float Sum = Row->Sum();
			for (int32 i = 0; i < N; ++i) P[i] = Row->Get((EDragonAction)i);
			if (!FMath::IsNearlyEqual(Sum, 1.f, 0.001f))
			{
				UE_LOG(LogDragonAI, Warning, TEXT("[Markov] Fila '%s' de %s suma %.3f (debe ser 1.0). Se renormaliza."),
					DragonRider::ActionName(From), *Data.MarkovTable->GetName(), Sum);
				for (int32 i = 0; i < N; ++i) P[i] = Sum > 0.f ? P[i] / Sum : 0.25f;
			}
		}
	}

	// ---------- 2) Precondiciones GOAP + 3) Moduladores dinamicos
	const FDragonActionRule* Rules[N] = { &Data.ApproachRule, &Data.AttackRule, &Data.ManeuverRule, &Data.RepositionRule };
	const float Dist = BB.DistanceToTarget;
	const bool bLowHealth = Dragon->GetHealthPct() < Data.LowHealthThreshold;

	float W[N];
	float Total = 0.f;
	for (int32 i = 0; i < N; ++i)
	{
		const FDragonActionRule& R = *Rules[i];
		const bool bPreconditions =
			Dist >= R.MinDistance && Dist <= R.MaxDistance &&
			Dragon->Energy + KINDA_SMALL_NUMBER >= R.EnergyCost &&
			(!R.bRequiresLineOfSight || BB.bHasLineOfSight) &&
			Dragon->Fury < R.MaxFury;

		float Weight = bPreconditions ? P[i] : 0.f;
		if (bLowHealth) Weight *= R.LowHealthMultiplier;
		Weight *= FMath::Lerp(1.f, R.FuryMultiplier, Dragon->Fury);
		if (Dist > Data.FarDistance) Weight *= R.FarMultiplier;
		if (Dist < Data.NearDistance) Weight *= R.NearMultiplier;

		W[i] = FMath::Max(0.f, Weight);
		Total += W[i];
	}

	// ---------- 4) Renormalizar a 1.0 y muestrear ----------
	if (Total <= KINDA_SMALL_NUMBER)
	{
		for (int32 i = 0; i < N; ++i) W[i] = 0.f;
		W[(int32)EDragonAction::Approach] = 1.f;
		Total = 1.f;
	}
	for (int32 i = 0; i < N; ++i) W[i] /= Total;

	const float Roll = FMath::FRand();
	float Accum = 0.f;
	int32 Selected = N - 1;
	for (int32 i = 0; i < N; ++i)
	{
		Accum += W[i];
		if (Roll <= Accum) { Selected = i; break; }
	}

	BB.CurrentAction = (EDragonAction)Selected;
	BB.CurrentActionProbability = W[Selected];
	Data.SelectedAction = BB.CurrentAction;
	Data.SelectedProbability = W[Selected];
	Data.FinalRow = FString::Printf(TEXT("[%.2f %.2f %.2f %.2f]"), W[0], W[1], W[2], W[3]);

	auto NameOf = [&Data](int32 Idx) -> FString
	{
		return Data.ActionNames.IsValidIndex(Idx) ? Data.ActionNames[Idx] : FString(DragonRider::ActionName((EDragonAction)Idx));
	};

	Dragon->SetStatus(FString::Printf(TEXT("Markov -> %s"), *NameOf(Selected)), EDragonMood::Combat);
	Dragon->SetDebugDetail(FString::Printf(TEXT("%s->%s p=%.2f %s"), *NameOf((int32)From), *NameOf(Selected), W[Selected], *Data.FinalRow));

	UE_LOG(LogDragonAI, Log, TEXT("[Markov] %s: %s -> %s (p=%.2f) fila final %s | dist=%.0f salud=%.2f furia=%.2f energia=%.1f"),
		*Dragon->GetName(), *NameOf((int32)From), *NameOf(Selected), W[Selected], *Data.FinalRow,
		Dist, Dragon->GetHealthPct(), Dragon->Fury, Dragon->Energy);

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FDragonTask_MarkovSelect::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>();
	return FText::FromString(FString::Printf(TEXT("Markov 4x4 (%s)"), (Data && Data->MarkovTable) ? *Data->MarkovTable->GetName() : TEXT("sin tabla")));
}
#endif

// MOVER

bool FDragonTask_Move::ComputeGoal(ADragonCharacter* Dragon, FInstanceDataType& Data) const
{
	const FDragonBlackboard& BB = Dragon->Blackboard;
	const FVector MyLoc = Dragon->GetActorLocation();
	const bool bHasTarget = IsValid(BB.Target);
	const FVector TargetLoc = bHasTarget ? BB.Target->GetActorLocation() : BB.TargetLocation;
	FVector Goal = MyLoc;
	bool bProject = Data.Style != EDragonMoveStyle::Fly;

	switch (Data.Goal)
	{
	case EDragonMoveGoal::Target:
		if (!bHasTarget) return false;
		Goal = TargetLoc;
		break;

	case EDragonMoveGoal::InvestigateLocation:
		Goal = BB.InvestigateLocation;
		break;

	case EDragonMoveGoal::Nest:
		Goal = Dragon->GetNestLocation();
		if (Data.Style != EDragonMoveStyle::Fly)
		{
			Goal += FMath::VRand().GetSafeNormal2D() * 200.f;
		}
		break;

	case EDragonMoveGoal::PatrolPoint:
		Goal = Dragon->GetNextPatrolPoint();
		break;

	case EDragonMoveGoal::OrbitTarget:
	{
		if (!bHasTarget) return false;
		FVector Dir = (MyLoc - TargetLoc).GetSafeNormal2D();
		if (Dir.IsNearlyZero()) Dir = FVector::ForwardVector;
		Dir = Dir.RotateAngleAxis(60.f * Data.OrbitSign, FVector::UpVector);
		Goal = TargetLoc + Dir * Data.OrbitDistance;
		break;
	}

	case EDragonMoveGoal::FlankTarget:
	{
		if (!bHasTarget) return false;
		const FVector Back = -BB.Target->GetActorForwardVector().GetSafeNormal2D();
		const FVector Side = BB.Target->GetActorRightVector().GetSafeNormal2D() * Data.OrbitSign;
		Goal = TargetLoc + (Back + Side).GetSafeNormal() * Data.OrbitDistance;
		break;
	}

	case EDragonMoveGoal::RetreatFromTarget:
	{
		FVector Away = (MyLoc - TargetLoc).GetSafeNormal2D();
		if (Away.IsNearlyZero()) Away = -Dragon->GetActorForwardVector();
		Goal = MyLoc + Away * Data.RetreatDistance;
		// no salirse de la correa
		if (FVector::Dist2D(Goal, Dragon->GetNestLocation()) > Dragon->LeashRadius * 0.9f)
		{
			Goal = FMath::Lerp(Goal, Dragon->GetNestLocation(), 0.6f);
		}
		break;
	}

	case EDragonMoveGoal::PerchPoint:
	{
		FVector Perch;
		if (Dragon->GetRandomPerchPoint(Perch))
		{
			Goal = Perch;
			bProject = false; // las perchas estan sobre pilares
		}
		else
		{
			Goal = Dragon->GetNextPatrolPoint();
		}
		break;
	}

	case EDragonMoveGoal::EggSearchPoint:
	{
		Goal = BB.EggThiefLocation + FMath::VRand().GetSafeNormal2D() * FMath::FRandRange(150.f, 600.f);
		const FVector Nest = Dragon->GetNestLocation();
		const float MaxR = Dragon->LeashRadius * 1.5f;
		if (FVector::Dist2D(Goal, Nest) > MaxR)
		{
			Goal = Nest + (Goal - Nest).GetSafeNormal2D() * MaxR;
		}
		break;
	}
	}

	if (bProject)
	{
		FVector Projected;
		if (DragonST::ProjectToNav(Dragon->GetWorld(), Goal, Projected))
		{
			Goal = Projected;
		}
	}
	Data.GoalLocation = Goal;
	return true;
}

void FDragonTask_Move::IssueMove(ADragonCharacter* Dragon, FInstanceDataType& Data) const
{
	if (Data.Style == EDragonMoveStyle::Fly) return;
	if (AAIController* AIC = DragonST::GetAIC(Dragon))
	{
		const EPathFollowingRequestResult::Type Result = AIC->MoveToLocation(Data.GoalLocation, Data.AcceptanceRadius * 0.7f, true, true, true, true, nullptr, true);
		Data.bUsingNav = Result != EPathFollowingRequestResult::Failed;
	}
}

EStateTreeRunStatus FDragonTask_Move::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon) return EStateTreeRunStatus::Failed;

	DragonST::ApplyLabel(Dragon, Data);

	const float Now = Context.GetWorld()->GetTimeSeconds();
	Data.StartTime = Now;
	Data.NextRepathTime = Now + 0.4f;
	Data.NextLeapTime = Now + FMath::FRandRange(0.1f, 0.4f);
	Data.OrbitSign = FMath::RandBool() ? 1.f : -1.f;

	if (!ComputeGoal(Dragon, Data))
	{
		return EStateTreeRunStatus::Failed;
	}

	UCharacterMovementComponent* Move = Dragon->GetCharacterMovement();
	Move->MaxWalkSpeed = Dragon->BaseSpeed * Data.SpeedMultiplier;
	Move->MaxFlySpeed = Dragon->BaseSpeed * Data.SpeedMultiplier;

	if (AAIController* AIC = DragonST::GetAIC(Dragon))
	{
		if (Data.bFaceTarget && IsValid(Dragon->Blackboard.Target))
		{
			AIC->SetFocus(Dragon->Blackboard.Target);
		}
		else
		{
			AIC->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}

	IssueMove(Dragon, Data);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FDragonTask_Move::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon) return EStateTreeRunStatus::Failed;

	const float Now = Context.GetWorld()->GetTimeSeconds();
	const float Elapsed = Now - Data.StartTime;
	if (Data.MaxDuration > 0.f && Elapsed >= Data.MaxDuration)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// Objetivos dinamicos
	const bool bDynamic = Data.Goal == EDragonMoveGoal::Target || Data.Goal == EDragonMoveGoal::OrbitTarget || Data.Goal == EDragonMoveGoal::FlankTarget;
	if (bDynamic && Now >= Data.NextRepathTime)
	{
		Data.NextRepathTime = Now + 0.4f;
		if (!ComputeGoal(Dragon, Data))
		{
			return EStateTreeRunStatus::Failed;
		}
		IssueMove(Dragon, Data);
	}

	const FVector MyLoc = Dragon->GetActorLocation();
	UCharacterMovementComponent* Move = Dragon->GetCharacterMovement();

	if (Data.Style == EDragonMoveStyle::Fly)
	{
		// Steering 3D
		const FVector Desired = Data.GoalLocation + FVector(0.f, 0.f, Data.FlyHeight);
		const FVector ToGoal = Desired - MyLoc;
		if (ToGoal.Size() <= Data.AcceptanceRadius)
		{
			return EStateTreeRunStatus::Succeeded;
		}
		if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
		Dragon->AddMovementInput(ToGoal.GetSafeNormal(), 1.f);
		return EStateTreeRunStatus::Running;
	}

	// Caminar / Saltar
	const float Dist2D = FVector::Dist2D(MyLoc, Data.GoalLocation);
	const float HalfHeight = Dragon->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float DZ = FMath::Abs(MyLoc.Z - HalfHeight - Data.GoalLocation.Z);
	if (Dist2D <= Data.AcceptanceRadius && DZ < 220.f && Move->IsMovingOnGround())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (Data.Style == EDragonMoveStyle::Leap && Move->IsMovingOnGround() && Now >= Data.NextLeapTime && Dist2D > 250.f)
	{
		Data.NextLeapTime = Now + Data.LeapInterval;
		const FVector Dir = (Data.GoalLocation - MyLoc).GetSafeNormal2D();
		FVector LeapTarget = Dist2D > Data.LeapMaxDistance
			? FVector(MyLoc.X, MyLoc.Y, MyLoc.Z - HalfHeight) + Dir * Data.LeapMaxDistance
			: Data.GoalLocation;
		LeapTarget.Z += HalfHeight;
		FVector LaunchVel;
		if (UGameplayStatics::SuggestProjectileVelocity_CustomArc(Dragon, LaunchVel, MyLoc, LeapTarget, 0.f, 0.45f))
		{
			Dragon->LaunchCharacter(LaunchVel, true, true);
			Dragon->PlayPulse(1.15f, 0.3f);
		}
		Data.NextRepathTime = Now + 0.6f;
	}
	else if (!Data.bUsingNav || !Move->IsMovingOnGround())
	{
		// Sin camino de NavMesh
		Dragon->AddMovementInput((Data.GoalLocation - MyLoc).GetSafeNormal2D(), 1.f);
		if (!bDynamic && Now >= Data.NextRepathTime && Move->IsMovingOnGround())
		{
			Data.NextRepathTime = Now + 1.f;
			IssueMove(Dragon, Data);
		}
	}

	return EStateTreeRunStatus::Running;
}

void FDragonTask_Move::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = Cast<ADragonCharacter>(Data.Actor);
	if (!Dragon) return;

	if (AAIController* AIC = DragonST::GetAIC(Dragon))
	{
		AIC->StopMovement();
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
	}
	Dragon->GetCharacterMovement()->MaxWalkSpeed = Dragon->BaseSpeed;
	Dragon->GetCharacterMovement()->MaxFlySpeed = Dragon->BaseSpeed;

	FDragonBlackboard& BB = Dragon->Blackboard;
	if (Data.Goal == EDragonMoveGoal::InvestigateLocation)
	{
		BB.bInvestigate = false; // ya se reviso esa ubicacion
	}
	else if (Data.Goal == EDragonMoveGoal::EggSearchPoint)
	{
		if (++BB.EggSearchCount >= Data.MaxEggSearches && !BB.bTargetCarryingOurEgg)
		{
			BB.bEggStolen = false; // se rinde tras revisar varias zonas
			BB.EggSearchCount = 0;
			Dragon->SetDebugDetail(TEXT("Busqueda abandonada"));
		}
	}
}

#if WITH_EDITOR
FText FDragonTask_Move::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>();
	if (!Data) return FText::FromString(TEXT("Dragon: Mover"));
	return FText::FromString(FString::Printf(TEXT("Mover a %s [%s]"),
		*DragonST::EnumText(StaticEnum<EDragonMoveGoal>(), (int64)Data->Goal),
		*DragonST::EnumText(StaticEnum<EDragonMoveStyle>(), (int64)Data->Style)));
}
#endif

// ATACAR

namespace DragonST
{
	// Posicion futura del objetivo
	static FVector PredictTarget(const AActor* Target, float LeadTime)
	{
		const FVector Vel = Target->GetVelocity();
		return Target->GetActorLocation() + FVector(Vel.X, Vel.Y, 0.f) * LeadTime;
	}
}

EStateTreeRunStatus FDragonTask_Attack::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon || !IsValid(Dragon->Blackboard.Target)) return EStateTreeRunStatus::Failed;

	// Precondicion GOAP
	const float Needed = Data.AttackType == EDragonAttackType::Fireball ? FMath::Max(1.f, Data.EnergyCost) : Data.EnergyCost;
	if (Dragon->Energy + KINDA_SMALL_NUMBER < Needed)
	{
		Dragon->SetDebugDetail(TEXT("GOAP: sin energia -> no puede atacar"));
		return EStateTreeRunStatus::Failed;
	}
	if (Data.AttackType != EDragonAttackType::Fireball)
	{
		Dragon->ConsumeEnergy(Data.EnergyCost);
	}

	DragonST::ApplyLabel(Dragon, Data);
	Dragon->SetLastAttackName(Data.Label);
	if (AAIController* AIC = DragonST::GetAIC(Dragon))
	{
		AIC->StopMovement();
	}

	Data.Phase = 0;
	Data.SubPhase = 0;
	Data.PhaseStart = Context.GetWorld()->GetTimeSeconds();
	Data.ShotsFired = 0;
	Data.bHit = false;

	// Telegrafiado
	Dragon->SetAttackTell(true);
	Dragon->PlayPulse(Data.AttackType == EDragonAttackType::Fireball ? 1.15f : 0.8f, FMath::Max(0.1f, Data.WindupTime));
	return EStateTreeRunStatus::Running;
}

void FDragonTask_Attack::FireOne(ADragonCharacter* Dragon, FInstanceDataType& Data) const
{
	if (!Dragon->ConsumeEnergy(FMath::Max(1.f, Data.EnergyCost))) return;
	AActor* Target = Dragon->Blackboard.Target;
	if (!IsValid(Target)) return;

	const FVector Start = Dragon->GetMouthLocation();
	const float Dist = FVector::Dist(Start, Target->GetActorLocation());
	const FVector Lead = Target->GetVelocity() * (Dist / Data.ProjectileSpeed) * 0.6f;
	FVector Dir = (Target->GetActorLocation() + Lead - Start).GetSafeNormal();
	Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(3.f));

	FActorSpawnParameters Params;
	Params.Owner = Dragon;
	Params.Instigator = Dragon;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ADragonFireball* Ball = Dragon->GetWorld()->SpawnActor<ADragonFireball>(Start, Dir.Rotation(), Params))
	{
		Ball->Damage = Data.Damage;
		Ball->Launch(Dir, Data.ProjectileSpeed);
	}
	++Data.ShotsFired;
}

void FDragonTask_Attack::LaunchPounce(ADragonCharacter* Dragon, FInstanceDataType& Data, float Arc) const
{
	AActor* Target = Dragon->Blackboard.Target;
	const FVector MyLoc = Dragon->GetActorLocation();
	const FVector Aim = IsValid(Target) ? DragonST::PredictTarget(Target, Data.LeadTime) : MyLoc + Dragon->GetActorForwardVector() * 600.f;
	const FVector Dir = (Aim - MyLoc).GetSafeNormal2D();
	Data.LockedDir = Dir;

	UCharacterMovementComponent* Move = Dragon->GetCharacterMovement();
	if (Move->MovementMode == MOVE_Flying && !Dragon->bCanFly)
	{
		Move->SetMovementMode(MOVE_Falling);
	}

	FVector Vel;
	// Apuntamos un poco "a traves" del objetivo para no quedarnos cortos
	if (!UGameplayStatics::SuggestProjectileVelocity_CustomArc(Dragon, Vel, MyLoc, Aim + Dir * 80.f, 0.f, Arc))
	{
		Vel = Dir * 1400.f + FVector(0.f, 0.f, 450.f);
	}
	Dragon->LaunchCharacter(Vel, true, true);
	DragonST::FaceTowards(Dragon, Aim, 1.f, 100.f);
}

bool FDragonTask_Attack::FindClingPoint(ADragonCharacter* Dragon, FInstanceDataType& Data) const
{
	AActor* Target = Dragon->Blackboard.Target;
	if (!IsValid(Target)) return false;

	UWorld* World = Dragon->GetWorld();
	const FVector MyLoc = Dragon->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();
	const float Radius = Dragon->GetCapsuleComponent()->GetScaledCapsuleRadius();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DragonWallSearch), false, Dragon);
	Params.AddIgnoredActor(Target);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);

	float BestScore = -BIG_NUMBER;
	bool bFound = false;
	const float StartYaw = FMath::FRandRange(0.f, 30.f);
	for (int32 i = 0; i < 12; ++i)
	{
		const FVector Dir = FRotator(0.f, StartYaw + i * 30.f, 0.f).Vector();
		const FVector Start = MyLoc + FVector(0.f, 0.f, 60.f);
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit, Start, Start + Dir * Data.WallSearchRadius, Objects, Params)) continue;
		if (FMath::Abs(Hit.ImpactNormal.Z) > 0.35f || Hit.Distance < 150.f) continue; // solo superficies verticales

		// La pared debe ser lo bastante alta para pegarse a ~2 m
		FHitResult HighHit;
		const FVector HighStart = Start + FVector(0.f, 0.f, 200.f);
		if (!World->LineTraceSingleByObjectType(HighHit, HighStart, HighStart + Dir * (Hit.Distance + 80.f), Objects, Params)) continue;

		const FVector Point = HighHit.ImpactPoint + HighHit.ImpactNormal * (Radius + 15.f);
		const float DistToTarget = FVector::Dist2D(Point, TargetLoc);
		const float DistNow = FVector::Dist2D(MyLoc, TargetLoc);
		// Pared alcanzable, cerca del objetivo y que no nos aleje de el
		if (Hit.Distance > 900.f || DistToTarget < 250.f || DistToTarget > 1300.f || DistToTarget > DistNow + 300.f) continue;

		// Desde la pared debe ver al objetivo
		FHitResult Block;
		if (World->LineTraceSingleByChannel(Block, Point, TargetLoc, ECC_Visibility, Params)) continue;

		const float Score = -FMath::Abs(DistToTarget - 500.f) - 0.3f * Hit.Distance + FMath::FRandRange(0.f, 150.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Data.ClingPoint = Point;
			Data.ClingNormal = HighHit.ImpactNormal;
			bFound = true;
		}
	}
	return bFound;
}

void FDragonTask_Attack::TryMeleeHit(ADragonCharacter* Dragon, FInstanceDataType& Data, float Reach, float UpKick) const
{
	AActor* Target = Dragon->Blackboard.Target;
	if (Data.bHit || !IsValid(Target) || !Dragon->IsInMeleeReach(Target, Reach)) return;

	Data.bHit = true;
	const FVector Dir = (Target->GetActorLocation() - Dragon->GetActorLocation()).GetSafeNormal2D();
	Dragon->DealDamageTo(Target, Data.Damage, Dir * Data.KnockbackStrength + FVector(0.f, 0.f, UpKick));
	Dragon->PlayPulse(1.3f, 0.25f);
	Dragon->SetDebugDetail(FString::Printf(TEXT("¡GOLPE! -%d"), FMath::RoundToInt(Data.Damage)));
	UE_LOG(LogDragonAI, Log, TEXT("[Golpe] %s conecto %s: -%.0f"), *Dragon->GetName(), *Data.Label, Data.Damage);
}

void FDragonTask_Attack::StartActive(ADragonCharacter* Dragon, FInstanceDataType& Data, float Now) const
{
	Data.Phase = 1;
	Data.PhaseStart = Now;
	Data.SubPhaseStart = Now;
	Dragon->SetAttackTell(false);

	AActor* Target = Dragon->Blackboard.Target;
	const FVector MyLoc = Dragon->GetActorLocation();
	const FVector TargetLoc = IsValid(Target) ? DragonST::PredictTarget(Target, Data.LeadTime) : MyLoc + Dragon->GetActorForwardVector() * 500.f;
	Data.LockedDir = (TargetLoc - MyLoc).GetSafeNormal2D();

	switch (Data.AttackType)
	{
	case EDragonAttackType::Fireball:
		FireOne(Dragon, Data);
		Data.NextShotTime = Now + Data.ShotInterval;
		break;

	case EDragonAttackType::Pounce:
		// Si el objetivo ya esta encima, ataca directo
		if (Data.bUseWallCling && IsValid(Target) && FVector::Dist2D(MyLoc, Target->GetActorLocation()) > 350.f
			&& FMath::FRand() < Data.WallClingChance && FindClingPoint(Dragon, Data))
		{
			// 1) Saltar a la pared
			Data.SubPhase = 1;
			FVector Vel;
			if (!UGameplayStatics::SuggestProjectileVelocity_CustomArc(Dragon, Vel, MyLoc, Data.ClingPoint, 0.f, 0.4f))
			{
				Vel = (Data.ClingPoint - MyLoc).GetSafeNormal() * 1300.f + FVector(0.f, 0.f, 400.f);
			}
			Dragon->LaunchCharacter(Vel, true, true);
			Dragon->SetStatus(TEXT("¡Salta a la pared!"), EDragonMood::Combat);
		}
		else
		{
			// Embestida directa
			Data.SubPhase = 3;
			LaunchPounce(Dragon, Data, 0.3f);
		}
		break;

	case EDragonAttackType::DiveSlam:
	{
		FVector Vel;
		if (UGameplayStatics::SuggestProjectileVelocity_CustomArc(Dragon, Vel, MyLoc, TargetLoc, 0.f, 0.82f))
		{
			Dragon->LaunchCharacter(Vel, true, true);
		}
		break;
	}

	case EDragonAttackType::Charge:
		Dragon->GetCharacterMovement()->MaxWalkSpeed = Dragon->BaseSpeed * Data.ChargeSpeedMultiplier;
		Dragon->GetCharacterMovement()->MaxAcceleration = 8000.f;
		break;
	}
}

EStateTreeRunStatus FDragonTask_Attack::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon) return EStateTreeRunStatus::Failed;

	const float Now = Context.GetWorld()->GetTimeSeconds();
	const float InPhase = Now - Data.PhaseStart;
	const float InSub = Now - Data.SubPhaseStart;
	AActor* Target = Dragon->Blackboard.Target;
	UCharacterMovementComponent* Move = Dragon->GetCharacterMovement();

	switch (Data.Phase)
	{
	case 0: // Preparacion
		if (IsValid(Target)) DragonST::FaceTowards(Dragon, Target->GetActorLocation(), DeltaTime, 10.f);
		if (InPhase >= Data.WindupTime)
		{
			StartActive(Dragon, Data, Now);
		}
		return EStateTreeRunStatus::Running;

	case 1: // Activo
	{
		bool bDone = false;
		switch (Data.AttackType)
		{
		case EDragonAttackType::Fireball:
			if (IsValid(Target)) DragonST::FaceTowards(Dragon, Target->GetActorLocation(), DeltaTime, 10.f);
			if (Data.ShotsFired < Data.Shots && Now >= Data.NextShotTime && Dragon->Energy >= 1.f)
			{
				FireOne(Dragon, Data);
				Data.NextShotTime = Now + Data.ShotInterval;
			}
			bDone = Data.ShotsFired >= Data.Shots || Dragon->Energy < 1.f;
			break;

		case EDragonAttackType::Charge:
		{
			// Embestida que corrige el rumbo hacia el objetivo
			if (IsValid(Target))
			{
				const FVector Desired = (Target->GetActorLocation() - Dragon->GetActorLocation()).GetSafeNormal2D();
				if (!Desired.IsNearlyZero())
				{
					Data.LockedDir = FMath::VInterpNormalRotationTo(Data.LockedDir, Desired, DeltaTime, Data.ChargeTurnRate);
				}
			}
			Dragon->AddMovementInput(Data.LockedDir, 1.f);
			Dragon->SetActorRotation(FRotator(0.f, Data.LockedDir.Rotation().Yaw, 0.f));
			TryMeleeHit(Dragon, Data, Data.MeleeReach, 380.f);

			const bool bPassed = IsValid(Target) && InPhase > 0.4f &&
				FVector::DotProduct(Data.LockedDir, Target->GetActorLocation() - Dragon->GetActorLocation()) < -150.f;
			bDone = Data.bHit || bPassed || InPhase >= Data.ActiveDuration;
			break;
		}

		case EDragonAttackType::Pounce:
		{
			if (Data.SubPhase == 1) // volando hacia la pared
			{
				const float DistToCling = FVector::Dist(Dragon->GetActorLocation(), Data.ClingPoint);
				FHitResult WallHit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(DragonWallTouch), false, Dragon);
				const bool bTouchingWall = InSub > 0.15f && Dragon->GetWorld()->LineTraceSingleByObjectType(WallHit,
					Dragon->GetActorLocation(), Dragon->GetActorLocation() - Data.ClingNormal * (Dragon->GetCapsuleComponent()->GetScaledCapsuleRadius() + 45.f),
					FCollisionObjectQueryParams(ECC_WorldStatic), Params);

				if (DistToCling < 140.f || bTouchingWall)
				{
					// 2) Pegarse a la pared
					Move->SetMovementMode(MOVE_Flying);
					Move->StopMovementImmediately();
					if (DistToCling < 250.f) Dragon->SetActorLocation(Data.ClingPoint, true);
					Data.SubPhase = 2;
					Data.SubPhaseStart = Now;
					Dragon->SetStatus(TEXT("¡PEGADO A LA PARED!"), EDragonMood::Combat);
					UE_LOG(LogDragonAI, Log, TEXT("[Pared] %s se pego a la pared en %s"), *Dragon->GetName(), *Data.ClingPoint.ToCompactString());
					Dragon->SetAttackTell(true);
					Dragon->PlayPulse(1.2f, Data.ClingTime);
				}
				else if (InSub > 1.3f)
				{
					// No alcanzo la pared
					Data.SubPhase = 3;
					Data.SubPhaseStart = Now;
					LaunchPounce(Dragon, Data, 0.3f);
				}
			}
			else if (Data.SubPhase == 2) // pegado: apunta y se impulsa
			{
				Move->Velocity = FVector::ZeroVector;
				if (IsValid(Target)) DragonST::FaceTowards(Dragon, Target->GetActorLocation(), DeltaTime, 14.f);
				if (InSub >= Data.ClingTime)
				{
					// 3) Embestida desde la pared
					Dragon->SetAttackTell(false);
					Data.SubPhase = 3;
					Data.SubPhaseStart = Now;
					LaunchPounce(Dragon, Data, 0.12f);
					Dragon->SetStatus(TEXT("¡EMBESTIDA DESDE LA PARED!"), EDragonMood::Combat);
				}
			}
			else // 3: en el aire hacia el objetivo
			{
				// Correccion de rumbo en el aire
				if (IsValid(Target) && !Move->IsMovingOnGround())
				{
					const FVector V = Move->Velocity;
					const FVector V2D(V.X, V.Y, 0.f);
					const float Speed2D = V2D.Size();
					const FVector Desired = (Target->GetActorLocation() - Dragon->GetActorLocation()).GetSafeNormal2D();
					if (Speed2D > 10.f && !Desired.IsNearlyZero())
					{
						const FVector NewDir = FMath::VInterpNormalRotationTo(V2D / Speed2D, Desired, DeltaTime, 160.f);
						Move->Velocity = FVector(NewDir.X * Speed2D, NewDir.Y * Speed2D, V.Z);
					}
				}
				TryMeleeHit(Dragon, Data, Data.MeleeReach, 280.f);
				const bool bLanded = InSub > 0.2f && Move->IsMovingOnGround();
				if (bLanded)
				{
					TryMeleeHit(Dragon, Data, Data.MeleeReach * 1.6f, 200.f); // zarpazo al caer
				}
				bDone = bLanded || InSub >= Data.ActiveDuration;
			}
			break;
		}

		case EDragonAttackType::DiveSlam:
		{
			TryMeleeHit(Dragon, Data, Data.MeleeReach, 300.f); // contacto directo en el aire
			const bool bLanded = InPhase > 0.25f && Move->IsMovingOnGround();
			if (bLanded && !Data.bHit)
			{
				// Impacto en area al caer
				Data.bHit = true;
				Dragon->PlayPulse(1.45f, 0.35f);
				if (IsValid(Target) && Dragon->IsInMeleeReach(Target, Data.HitRadius))
				{
					const FVector Push = (Target->GetActorLocation() - Dragon->GetActorLocation()).GetSafeNormal2D() * Data.KnockbackStrength + FVector(0, 0, 550.f);
					Dragon->DealDamageTo(Target, Data.Damage, Push);
					Dragon->SetDebugDetail(FString::Printf(TEXT("¡IMPACTO! -%d"), FMath::RoundToInt(Data.Damage)));
					UE_LOG(LogDragonAI, Log, TEXT("[Golpe] %s conecto %s (area): -%.0f"), *Dragon->GetName(), *Data.Label, Data.Damage);
				}
			}
			bDone = bLanded || InPhase >= Data.ActiveDuration;
			break;
		}
		}

		if (bDone)
		{
			Data.Phase = 2;
			Data.PhaseStart = Now;
			Move->MaxWalkSpeed = Dragon->BaseSpeed;
			Move->MaxAcceleration = 2048.f;
			if (Move->MovementMode == MOVE_Flying && !Dragon->bCanFly) Move->SetMovementMode(MOVE_Falling);
			Dragon->SetAttackTell(false);
			if (Data.AttackType != EDragonAttackType::Fireball)
			{
				Dragon->SetStatus(Data.bHit ? TEXT("Recuperandose") : TEXT("Fallo: recuperandose (vulnerable)"), EDragonMood::Alert);
			}
		}
		return EStateTreeRunStatus::Running;
	}

	default: // Recuperacion
		return InPhase >= Data.RecoveryTime ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
	}
}

void FDragonTask_Attack::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (ADragonCharacter* Dragon = Cast<ADragonCharacter>(Data.Actor))
	{
		UCharacterMovementComponent* Move = Dragon->GetCharacterMovement();
		Move->MaxWalkSpeed = Dragon->BaseSpeed;
		Move->MaxAcceleration = 2048.f;
		// Si fue interrumpido estando pegado a la pared, vuelve a caer
		if (Move->MovementMode == MOVE_Flying && !Dragon->bCanFly)
		{
			Move->SetMovementMode(MOVE_Falling);
		}
		Dragon->SetAttackTell(false);
	}
}

#if WITH_EDITOR
FText FDragonTask_Attack::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>();
	if (!Data) return FText::FromString(TEXT("Dragon: Atacar"));
	return FText::FromString(FString::Printf(TEXT("Atacar: %s%s"), *DragonST::EnumText(StaticEnum<EDragonAttackType>(), (int64)Data->AttackType),
		Data->bUseWallCling ? TEXT(" (desde la pared)") : TEXT("")));
}
#endif

// ESPERAR / VIGILAR / RUGIR

EStateTreeRunStatus FDragonTask_Wait::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon) return EStateTreeRunStatus::Failed;

	DragonST::ApplyLabel(Dragon, Data);
	const float Now = Context.GetWorld()->GetTimeSeconds();
	Data.EndTime = Now + FMath::FRandRange(Data.MinDuration, FMath::Max(Data.MinDuration, Data.MaxDuration));
	Data.NextLookTime = Now;
	Data.LookYaw = Dragon->GetActorRotation().Yaw;

	if (AAIController* AIC = DragonST::GetAIC(Dragon))
	{
		AIC->StopMovement();
	}

	if (Data.bRoar)
	{
		Dragon->AddFury(Data.RoarFury);
		Dragon->PlayPulse(1.35f, 0.8f);
		// El rugido alerta a la manada
		if (Dragon->HomeNest)
		{
			const FVector AlertLoc = IsValid(Dragon->Blackboard.Target) ? Dragon->Blackboard.Target->GetActorLocation() : Dragon->GetActorLocation();
			for (ADragonCharacter* Mate : Dragon->HomeNest->Guardians)
			{
				if (IsValid(Mate) && Mate != Dragon && !Mate->Blackboard.bHasTarget)
				{
					Mate->Blackboard.bInvestigate = true;
					Mate->Blackboard.InvestigateLocation = AlertLoc;
				}
			}
		}
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FDragonTask_Wait::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon) return EStateTreeRunStatus::Failed;

	const float Now = Context.GetWorld()->GetTimeSeconds();
	const FDragonBlackboard& BB = Dragon->Blackboard;

	if (Data.bFaceTarget && (IsValid(BB.Target) || BB.bInvestigate))
	{
		DragonST::FaceTowards(Dragon, IsValid(BB.Target) ? BB.Target->GetActorLocation() : BB.InvestigateLocation, DeltaTime, 6.f);
	}
	else if (Data.bLookAround)
	{
		if (Now >= Data.NextLookTime)
		{
			Data.NextLookTime = Now + FMath::FRandRange(0.8f, 1.6f);
			Data.LookYaw = Dragon->GetActorRotation().Yaw + FMath::FRandRange(-120.f, 120.f);
		}
		const FRotator Want(0.f, Data.LookYaw, 0.f);
		Dragon->SetActorRotation(FMath::RInterpTo(Dragon->GetActorRotation(), Want, DeltaTime, 3.f));
	}

	return Now >= Data.EndTime ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

// RECARGAR (GOAP)

EStateTreeRunStatus FDragonTask_Recharge::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon) return EStateTreeRunStatus::Failed;
	DragonST::ApplyLabel(Dragon, Data);
	Dragon->SetDebugDetail(TEXT("GOAP: Municion = 0 -> Recargar"));
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FDragonTask_Recharge::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon) return EStateTreeRunStatus::Failed;

	Dragon->AddEnergy(Data.EnergyPerSecond * DeltaTime);
	if (Dragon->Energy >= Dragon->MaxEnergy - KINDA_SMALL_NUMBER)
	{
		Dragon->SetDebugDetail(TEXT("Fuego recargado"));
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

// ATURDIDO

EStateTreeRunStatus FDragonTask_Stunned::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon) return EStateTreeRunStatus::Failed;

	DragonST::ApplyLabel(Dragon, Data);
	if (AAIController* AIC = DragonST::GetAIC(Dragon))
	{
		AIC->StopMovement();
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
	}
	Dragon->GetCharacterMovement()->StopMovementImmediately();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FDragonTask_Stunned::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ADragonCharacter* Dragon = DragonST::GetDragon(Data);
	if (!Dragon) return EStateTreeRunStatus::Failed;

	// Visual: tambaleo
	Dragon->AddActorWorldRotation(FRotator(0.f, 200.f * DeltaTime, 0.f));
	return Dragon->IsStunned() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}
