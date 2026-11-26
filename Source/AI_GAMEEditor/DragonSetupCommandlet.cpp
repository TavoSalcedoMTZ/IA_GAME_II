// Dragon Rider - Generador de StateTrees, DataTables y mapa

#include "DragonSetupCommandlet.h"
#include "DragonStateTreeNodes.h"
#include "DragonTypes.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorSchema.h"
#include "StateTreeEditorModule.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeState.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Engine/DataTable.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "DragonArena.h"
#include "DragonRiderGame.h"
#include "Factories/WorldFactory.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"

DEFINE_LOG_CATEGORY_STATIC(LogDragonSetup, Log, All);

namespace DragonSetup
{
	using ETrig = EStateTreeTransitionTrigger;
	using EPrio = EStateTreeTransitionPriority;

	// ----------------------------------------------------------------

	template <class T>
	T* CreateOrLoad(const FString& PackagePath, const FString& Name)
	{
		const FString ObjectPath = PackagePath + TEXT(".") + Name;
		if (T* Existing = LoadObject<T>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*PackagePath);
		Package->FullyLoad();
		T* Obj = NewObject<T>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Obj);
		return Obj;
	}

	bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = Asset->GetPackage();
		Package->MarkPackageDirty();
		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const bool bOk = UPackage::SavePackage(Package, Asset, *File, Args);
		UE_LOG(LogDragonSetup, Display, TEXT("%s %s"), bOk ? TEXT("Guardado") : TEXT("ERROR guardando"), *File);
		return bOk;
	}

	// ----------------------------------------------------------------

	struct FMatrix4 { float M[4][4]; };

	UDataTable* BuildMarkovTable(const FString& Name, const FMatrix4& Matrix, const TArray<FString>& ActionNames)
	{
		UDataTable* Table = CreateOrLoad<UDataTable>(TEXT("/Game/DragonRider/Data/") + Name, Name);
		Table->EmptyTable();
		Table->RowStruct = FDragonMarkovRow::StaticStruct();

		for (int32 From = 0; From < 4; ++From)
		{
			float Row[4];
			float Sum = 0.f;
			for (int32 To = 0; To < 4; ++To) { Row[To] = Matrix.M[From][To]; Sum += Row[To]; }
			if (!FMath::IsNearlyEqual(Sum, 1.f, 0.0001f))
			{
				UE_LOG(LogDragonSetup, Warning, TEXT("%s fila %d suma %.4f -> normalizando a 1.0"), *Name, From, Sum);
				for (float& V : Row) V /= Sum;
			}

			FDragonMarkovRow MarkovRow;
			MarkovRow.ToApproach = Row[0];
			MarkovRow.ToAttack = Row[1];
			MarkovRow.ToManeuver = Row[2];
			MarkovRow.ToReposition = Row[3];
			MarkovRow.Notes = FString::Printf(TEXT("Desde '%s' -> [%s, %s, %s, %s] suma=%.2f"),
				*ActionNames[From], *ActionNames[0], *ActionNames[1], *ActionNames[2], *ActionNames[3], MarkovRow.Sum());
			Table->AddRow(FName(DragonRider::ActionName((EDragonAction)From)), MarkovRow);
		}
		SaveAsset(Table);
		return Table;
	}

	// ----------------------------------------------------------------

	void EnterCond(UStateTreeState& State, EDragonCheck Check, bool bNegate = false, float Threshold = 0.f, EDragonAction Action = EDragonAction::Approach)
	{
		FDragonConditionInstanceData& D = State.AddEnterCondition<FDragonCondition_Check>().GetInstanceData();
		D.Check = Check; D.bNegate = bNegate; D.Threshold = Threshold; D.Action = Action;
	}

	FStateTreeTransition& Go(UStateTreeState& From, ETrig Trigger, UStateTreeState& To, EPrio Priority = EPrio::Normal)
	{
		FStateTreeTransition& T = From.AddTransition(Trigger, EStateTreeTransitionType::GotoState, &To);
		T.Priority = Priority;
		return T;
	}

	void GoIf(UStateTreeState& From, UStateTreeState& To, EDragonCheck Check, bool bNegate = false, float Threshold = 0.f, EPrio Priority = EPrio::Normal)
	{
		FStateTreeTransition& T = Go(From, ETrig::OnTick, To, Priority);
		FDragonConditionInstanceData& D = T.AddConditionWithOuter<FDragonCondition_Check>(&From).GetInstanceData();
		D.Check = Check; D.bNegate = bNegate; D.Threshold = Threshold;
	}

	FDragonMoveInstanceData& Move(UStateTreeState& S, const FString& Label, EDragonMood Mood, EDragonMoveGoal Goal, EDragonMoveStyle Style)
	{
		FDragonMoveInstanceData& D = S.AddTask<FDragonTask_Move>().GetInstanceData();
		D.Label = Label; D.Mood = Mood; D.Goal = Goal; D.Style = Style;
		return D;
	}

	FDragonWaitInstanceData& Wait(UStateTreeState& S, const FString& Label, EDragonMood Mood, float MinT, float MaxT)
	{
		FDragonWaitInstanceData& D = S.AddTask<FDragonTask_Wait>().GetInstanceData();
		D.Label = Label; D.Mood = Mood; D.MinDuration = MinT; D.MaxDuration = MaxT;
		return D;
	}

	FDragonAttackInstanceData& Attack(UStateTreeState& S, const FString& Label, EDragonAttackType Type)
	{
		FDragonAttackInstanceData& D = S.AddTask<FDragonTask_Attack>().GetInstanceData();
		D.Label = Label; D.Mood = EDragonMood::Combat; D.AttackType = Type;
		return D;
	}

	FDragonActionRule Rule(float MinD, float MaxD, float Energy, bool bLOS, float MaxFury, float LowHealthMul, float FuryMul, float FarMul, float NearMul = 1.f)
	{
		FDragonActionRule R;
		R.MinDistance = MinD; R.MaxDistance = MaxD; R.EnergyCost = Energy; R.bRequiresLineOfSight = bLOS;
		R.MaxFury = MaxFury; R.LowHealthMultiplier = LowHealthMul; R.FuryMultiplier = FuryMul; R.FarMultiplier = FarMul; R.NearMultiplier = NearMul;
		return R;
	}

	// ----------------------------------------------------------------

	UStateTree* CreateEmptyTree(const FString& Name)
	{
		UStateTree* Tree = CreateOrLoad<UStateTree>(TEXT("/Game/DragonRider/AI/") + Name, Name);

		// Si ya existia, descartamos su EditorData anterior y la reconstruimos
		if (Tree->EditorData)
		{
			Tree->EditorData->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Tree->EditorData = nullptr;
		}

		const TNonNullSubclassOf<const UStateTreeSchema> SchemaClass(UStateTreeAIComponentSchema::StaticClass());
		FStateTreeEditorModule& EditorModule = FModuleManager::LoadModuleChecked<FStateTreeEditorModule>(TEXT("StateTreeEditorModule"));

		UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(Tree, EditorModule.GetEditorDataClass(SchemaClass), NAME_None, RF_Transactional);
		Tree->EditorData = EditorData;
		EditorData->Schema = NewObject<UStateTreeSchema>(EditorData, UStateTreeAIComponentSchema::StaticClass(), NAME_None, RF_Transactional);
		EditorData->EditorSchema = NewObject<UStateTreeEditorSchema>(EditorData, EditorModule.GetEditorSchemaClass(SchemaClass), NAME_None, RF_Transactional);
		return Tree;
	}

	bool CompileAndSave(UStateTree* Tree)
	{
		FStateTreeCompilerLog Log;
		const bool bOk = UStateTreeEditingSubsystem::CompileStateTree(Tree, Log);
		Log.DumpToLog(Tree, LogDragonSetup);
		if (!bOk)
		{
			UE_LOG(LogDragonSetup, Error, TEXT("Fallo la compilacion de %s"), *Tree->GetName());
			return false;
		}
		return SaveAsset(Tree);
	}

	struct FSpeciesSetup
	{
		EDragonSpecies Species;
		FString TreeName;
		FString TableName;
		TArray<FString> ActionNames;
		FMatrix4 Matrix;
		FDragonActionRule Rules[4];
	};

	// Esqueleto comun
	UStateTree* BuildTree(const FSpeciesSetup& Setup, UDataTable* MarkovTable)
	{
		UStateTree* Tree = CreateEmptyTree(Setup.TreeName);
		UStateTreeEditorData* ED = CastChecked<UStateTreeEditorData>(Tree->EditorData);

		const EDragonSpecies Sp = Setup.Species;
		const bool bFly = Sp == EDragonSpecies::Volador;
		const EDragonMoveStyle Style = bFly ? EDragonMoveStyle::Fly : (Sp == EDragonSpecies::Saltarin ? EDragonMoveStyle::Leap : EDragonMoveStyle::Walk);

		// Evaluator global
		{
			FDragonSensesEvaluatorInstanceData& E = ED->AddEvaluator<FDragonSensesEvaluator>().GetInstanceData();
			E.UpdateInterval = 0.15f;
			E.TargetMemory = bFly ? 4.f : 3.f;
		}

		UStateTreeState& Root = ED->AddSubTree(*FString::Printf(TEXT("Dragon %s"), DragonRider::SpeciesName(Sp)));

		// ---------- Aturdido ----------
		UStateTreeState& Stunned = Root.AddChildState(TEXT("Aturdido (Reaccion a dano)"));
		EnterCond(Stunned, EDragonCheck::IsStunned);
		{
			FDragonTaskInstanceData& D = Stunned.AddTask<FDragonTask_Stunned>().GetInstanceData();
			D.Label = TEXT("¡ATURDIDO!"); D.Mood = EDragonMood::Stunned;
		}

		// ---------- Vivo ----------
		UStateTreeState& Alive = Root.AddChildState(TEXT("Vivo"));

		UStateTreeState* Recharge = nullptr;
		UStateTreeState* FlyToNest = nullptr;
		UStateTreeState* Refill = nullptr;
		if (bFly)
		{
			Recharge = &Alive.AddChildState(TEXT("Recargar fuego (GOAP: Municion == 0)"));
			EnterCond(*Recharge, EDragonCheck::HasEnergy, true, 1.f);
			FlyToNest = &Recharge->AddChildState(TEXT("Volar al nido"));
			{
				FDragonMoveInstanceData& M = Move(*FlyToNest, TEXT("Sin fuego: volando al nido"), EDragonMood::Special, EDragonMoveGoal::Nest, EDragonMoveStyle::Fly);
				M.FlyHeight = 130.f; M.AcceptanceRadius = 200.f; M.SpeedMultiplier = 1.1f; M.MaxDuration = 12.f;
			}
			Refill = &Recharge->AddChildState(TEXT("Reponer fuego"));
			{
				FDragonRechargeInstanceData& R = Refill->AddTask<FDragonTask_Recharge>().GetInstanceData();
				R.Label = TEXT("Recargando fuego (vulnerable)"); R.Mood = EDragonMood::Special; R.EnergyPerSecond = 1.5f;
			}
		}

		// ---------- Combate ----------
		UStateTreeState& Combat = Alive.AddChildState(TEXT("Combate"));
		EnterCond(Combat, EDragonCheck::HasTarget);

		UStateTreeState* Warning = nullptr;
		if (Sp == EDragonSpecies::Impulsivo)
		{
			Warning = &Combat.AddChildState(TEXT("Advertencia (rugido inicial)"));
			FDragonWaitInstanceData& W = Wait(*Warning, TEXT("¡RUGIDO de advertencia!"), EDragonMood::Alert, 1.0f, 1.2f);
			W.bRoar = true; W.bFaceTarget = true; W.RoarFury = 0.25f;
		}

		UStateTreeState& Decide = Combat.AddChildState(TEXT("Decidir (Cadena de Markov)"));
		{
			FDragonMarkovInstanceData& K = Decide.AddTask<FDragonTask_MarkovSelect>().GetInstanceData();
			K.Label = TEXT("Decidiendo"); K.Mood = EDragonMood::Combat;
			K.MarkovTable = MarkovTable;
			K.ApproachRule = Setup.Rules[0];
			K.AttackRule = Setup.Rules[1];
			K.ManeuverRule = Setup.Rules[2];
			K.RepositionRule = Setup.Rules[3];
			K.LowHealthThreshold = 0.35f;
			K.FarDistance = bFly ? 2200.f : 1300.f;
			K.NearDistance = bFly ? 500.f : 450.f;
			K.ActionNames = Setup.ActionNames;
		}

		UStateTreeState& Execute = Combat.AddChildState(TEXT("Ejecutar accion"));
		UStateTreeState& A0 = Execute.AddChildState(*Setup.ActionNames[0]);
		UStateTreeState& A1 = Execute.AddChildState(*Setup.ActionNames[1]);
		UStateTreeState& A2 = Execute.AddChildState(*Setup.ActionNames[2]);
		UStateTreeState& A3 = Execute.AddChildState(*Setup.ActionNames[3]);
		UStateTreeState& AFallback = Execute.AddChildState(TEXT("Perseguir (respaldo)"));
		EnterCond(A0, EDragonCheck::ActionIs, false, 0.f, EDragonAction::Approach);
		EnterCond(A1, EDragonCheck::ActionIs, false, 0.f, EDragonAction::Attack);
		EnterCond(A2, EDragonCheck::ActionIs, false, 0.f, EDragonAction::Maneuver);
		EnterCond(A3, EDragonCheck::ActionIs, false, 0.f, EDragonAction::Reposition);

		const EDragonMood C = EDragonMood::Combat;
		switch (Sp)
		{
		case EDragonSpecies::Volador:
		{
			FDragonMoveInstanceData& M0 = Move(A0, Setup.ActionNames[0], C, EDragonMoveGoal::Target, Style);
			M0.FlyHeight = 450.f; M0.AcceptanceRadius = 900.f; M0.MaxDuration = 2.5f; M0.bFaceTarget = true;
			FDragonAttackInstanceData& K1 = Attack(A1, Setup.ActionNames[1], EDragonAttackType::Fireball);
			K1.Shots = 2; K1.Damage = 12.f; K1.WindupTime = 0.5f; K1.RecoveryTime = 0.6f; K1.EnergyCost = 1.f; K1.ProjectileSpeed = 2300.f;
			FDragonMoveInstanceData& M2 = Move(A2, Setup.ActionNames[2], C, EDragonMoveGoal::OrbitTarget, Style);
			M2.FlyHeight = 520.f; M2.OrbitDistance = 950.f; M2.AcceptanceRadius = 200.f; M2.MaxDuration = 2.5f;
			FDragonMoveInstanceData& M3 = Move(A3, Setup.ActionNames[3], C, EDragonMoveGoal::RetreatFromTarget, Style);
			M3.FlyHeight = 850.f; M3.RetreatDistance = 900.f; M3.AcceptanceRadius = 200.f; M3.MaxDuration = 2.0f; M3.SpeedMultiplier = 1.25f;
			break;
		}
		case EDragonSpecies::Saltarin:
		{
			FDragonMoveInstanceData& M0 = Move(A0, Setup.ActionNames[0], C, EDragonMoveGoal::Target, Style);
			M0.AcceptanceRadius = 600.f; M0.MaxDuration = 2.5f; M0.SpeedMultiplier = 1.2f; M0.LeapInterval = 1.0f; M0.LeapMaxDistance = 700.f;
			FDragonAttackInstanceData& K1 = Attack(A1, Setup.ActionNames[1], EDragonAttackType::Pounce);
			K1.Damage = 22.f; K1.WindupTime = 0.3f; K1.RecoveryTime = 0.45f; K1.EnergyCost = 25.f; K1.MeleeReach = 110.f; K1.ActiveDuration = 1.6f; K1.KnockbackStrength = 650.f; K1.LeadTime = 0.35f;
			K1.bUseWallCling = true; K1.WallSearchRadius = 1200.f; K1.ClingTime = 0.55f; K1.WallClingChance = 0.9f;
			FDragonMoveInstanceData& M2 = Move(A2, Setup.ActionNames[2], C, EDragonMoveGoal::FlankTarget, Style);
			M2.OrbitDistance = 450.f; M2.AcceptanceRadius = 150.f; M2.MaxDuration = 2.2f; M2.LeapInterval = 0.8f; M2.LeapMaxDistance = 600.f; M2.SpeedMultiplier = 1.3f;
			FDragonMoveInstanceData& M3 = Move(A3, Setup.ActionNames[3], C, EDragonMoveGoal::PerchPoint, Style);
			M3.AcceptanceRadius = 120.f; M3.MaxDuration = 3.5f; M3.LeapInterval = 0.6f; M3.LeapMaxDistance = 1200.f;
			break;
		}
		case EDragonSpecies::Impulsivo:
		default:
		{
			FDragonMoveInstanceData& M0 = Move(A0, Setup.ActionNames[0], C, EDragonMoveGoal::Target, Style);
			M0.AcceptanceRadius = 220.f; M0.MaxDuration = 2.5f; M0.SpeedMultiplier = 1.5f;
			FDragonAttackInstanceData& K1 = Attack(A1, Setup.ActionNames[1], EDragonAttackType::Charge);
			K1.Damage = 30.f; K1.WindupTime = 0.55f; K1.RecoveryTime = 0.8f; K1.EnergyCost = 30.f; K1.ActiveDuration = 1.4f; K1.MeleeReach = 80.f; K1.KnockbackStrength = 1000.f; K1.ChargeSpeedMultiplier = 2.8f; K1.ChargeTurnRate = 110.f;
			FDragonWaitInstanceData& W2 = Wait(A2, Setup.ActionNames[2], EDragonMood::Special, 1.0f, 1.3f);
			W2.bRoar = true; W2.bFaceTarget = true; W2.RoarFury = 0.35f;
			FDragonAttackInstanceData& K3 = Attack(A3, Setup.ActionNames[3], EDragonAttackType::DiveSlam);
			K3.Damage = 30.f; K3.WindupTime = 0.35f; K3.RecoveryTime = 0.9f; K3.EnergyCost = 35.f; K3.HitRadius = 300.f; K3.MeleeReach = 90.f; K3.KnockbackStrength = 800.f; K3.ActiveDuration = 2.0f; K3.LeadTime = 0.45f;
			break;
		}
		}
		{
			FDragonMoveInstanceData& MF = Move(AFallback, TEXT("Perseguir"), C, EDragonMoveGoal::Target, Style);
			MF.FlyHeight = 450.f; MF.AcceptanceRadius = bFly ? 900.f : 250.f; MF.MaxDuration = 2.f;
		}

		// ---------- Buscar huevo robado ----------
		UStateTreeState& SearchEgg = Alive.AddChildState(TEXT("Buscar huevo robado"));
		EnterCond(SearchEgg, EDragonCheck::EggStolen);
		{
			const FString L = Sp == EDragonSpecies::Impulsivo ? TEXT("FURIA: buscando al ladron") : TEXT("Buscando huevo robado");
			FDragonMoveInstanceData& M = Move(SearchEgg, L, EDragonMood::Special, EDragonMoveGoal::EggSearchPoint, Style);
			M.FlyHeight = 350.f; M.AcceptanceRadius = bFly ? 250.f : 150.f; M.MaxDuration = 5.f;
			M.SpeedMultiplier = Sp == EDragonSpecies::Impulsivo ? 1.4f : 1.15f;
			M.MaxEggSearches = Sp == EDragonSpecies::Saltarin ? 4 : 3;
		}

		// ---------- Regresar al nido ----------
		UStateTreeState& Return = Alive.AddChildState(TEXT("Regresar al nido"));
		EnterCond(Return, EDragonCheck::Leashing);
		{
			FDragonMoveInstanceData& M = Move(Return, TEXT("Regresando al nido"), EDragonMood::Alert, EDragonMoveGoal::Nest, Style);
			M.FlyHeight = 400.f; M.AcceptanceRadius = bFly ? 300.f : 250.f; M.MaxDuration = 15.f; M.SpeedMultiplier = 1.2f;
		}

		// ---------- Buscar intruso ----------
		UStateTreeState& Investigate = Alive.AddChildState(TEXT("Buscar intruso"));
		EnterCond(Investigate, EDragonCheck::Investigate);
		UStateTreeState* Roar = nullptr;
		UStateTreeState* Advance = nullptr;
		if (Sp == EDragonSpecies::Impulsivo)
		{
			Roar = &Investigate.AddChildState(TEXT("Rugir para advertir"));
			FDragonWaitInstanceData& W = Wait(*Roar, TEXT("Rugiendo (advertencia)"), EDragonMood::Alert, 1.0f, 1.2f);
			W.bRoar = true; W.bFaceTarget = true; W.RoarFury = 0.15f;
			Advance = &Investigate.AddChildState(TEXT("Avanzar amenazante"));
			FDragonMoveInstanceData& M = Move(*Advance, TEXT("Avanzando amenazante"), EDragonMood::Alert, EDragonMoveGoal::InvestigateLocation, Style);
			M.AcceptanceRadius = 200.f; M.MaxDuration = 6.f;
		}
		else
		{
			FDragonMoveInstanceData& M = Move(Investigate, bFly ? TEXT("Sobrevolando zona sospechosa") : TEXT("Acercandose a saltos a investigar"),
				EDragonMood::Alert, EDragonMoveGoal::InvestigateLocation, Style);
			M.FlyHeight = 500.f; M.AcceptanceRadius = bFly ? 300.f : 180.f; M.MaxDuration = 6.f;
		}

		// ---------- Tranquilo
		UStateTreeState& Calm = Alive.AddChildState(TEXT("Tranquilo"));
		UStateTreeState& Idle = Calm.AddChildState(TEXT("Idle"));
		Wait(Idle, TEXT("Idle (descansando)"), EDragonMood::Calm, 1.5f, 3.f);

		UStateTreeState& Patrol = Calm.AddChildState(TEXT("Patrullar"));
		{
			FDragonMoveInstanceData& M = Move(Patrol, bFly ? TEXT("Patrullando (ruta aerea)") : (Sp == EDragonSpecies::Saltarin ? TEXT("Patrullando a saltos") : TEXT("Patrullando territorio")),
				EDragonMood::Calm, EDragonMoveGoal::PatrolPoint, Style);
			M.FlyHeight = 650.f; M.AcceptanceRadius = bFly ? 250.f : 150.f; M.MaxDuration = 12.f;
			M.SpeedMultiplier = 0.6f; M.LeapInterval = 2.0f; M.LeapMaxDistance = 600.f;
		}

		UStateTreeState& GoGuard = Calm.AddChildState(Sp == EDragonSpecies::Saltarin ? TEXT("Subir a percha") : TEXT("Volver a vigilar nido"));
		{
			const EDragonMoveGoal G = Sp == EDragonSpecies::Saltarin ? EDragonMoveGoal::PerchPoint : EDragonMoveGoal::Nest;
			FDragonMoveInstanceData& M = Move(GoGuard, Sp == EDragonSpecies::Saltarin ? TEXT("Saltando a la percha") : TEXT("Volviendo al nido"), EDragonMood::Calm, G, Style);
			M.FlyHeight = 350.f; M.AcceptanceRadius = bFly ? 250.f : 130.f; M.MaxDuration = 10.f;
			M.SpeedMultiplier = 0.7f; M.LeapInterval = 0.8f; M.LeapMaxDistance = 1200.f;
		}

		UStateTreeState& Guard = Calm.AddChildState(Sp == EDragonSpecies::Saltarin ? TEXT("Vigilar desde percha") : TEXT("Vigilar nido"));
		{
			FDragonWaitInstanceData& W = Wait(Guard, Sp == EDragonSpecies::Saltarin ? TEXT("Vigilando desde la percha") : TEXT("Vigilando el nido"),
				EDragonMood::Calm, 3.f, Sp == EDragonSpecies::Saltarin ? 6.f : 4.5f);
			W.bLookAround = true;
		}

		// ======================= TRANSICIONES =======================
		Go(Stunned, ETrig::OnStateCompleted, Alive);
		GoIf(Alive, Stunned, EDragonCheck::IsStunned, false, 0.f, EPrio::Critical);

		if (bFly)
		{
			Go(*FlyToNest, ETrig::OnStateSucceeded, *Refill);
			Go(*FlyToNest, ETrig::OnStateFailed, Alive);
			Go(*Refill, ETrig::OnStateCompleted, Alive);
			GoIf(Combat, Alive, EDragonCheck::HasEnergy, true, 1.f, EPrio::High); // GOAP: sin municion
		}

		GoIf(Combat, Alive, EDragonCheck::HasTarget, true);
		if (Warning) Go(*Warning, ETrig::OnStateCompleted, Decide);
		Go(Decide, ETrig::OnStateCompleted, Execute);
		for (UStateTreeState* Action : { &A0, &A1, &A2, &A3, &AFallback })
		{
			Go(*Action, ETrig::OnStateCompleted, Decide);
		}

		Go(SearchEgg, ETrig::OnStateCompleted, Alive);
		GoIf(SearchEgg, Alive, EDragonCheck::HasTarget);

		Go(Return, ETrig::OnStateCompleted, Alive);

		if (Roar)
		{
			Go(*Roar, ETrig::OnStateCompleted, *Advance);
			Go(*Advance, ETrig::OnStateCompleted, Alive);
		}
		else
		{
			Go(Investigate, ETrig::OnStateCompleted, Alive);
		}
		GoIf(Investigate, Alive, EDragonCheck::HasTarget);

		GoIf(Calm, Alive, EDragonCheck::AnyAlert);
		Go(Idle, ETrig::OnStateCompleted, Patrol);
		Go(Patrol, ETrig::OnStateCompleted, GoGuard);
		Go(GoGuard, ETrig::OnStateCompleted, Guard);
		Go(Guard, ETrig::OnStateCompleted, Idle);

		if (!CompileAndSave(Tree)) return nullptr;
		return Tree;
	}

	TArray<FSpeciesSetup> MakeSetups()
	{
		TArray<FSpeciesSetup> Out;

		// Orden de columnas/filas
		{
			FSpeciesSetup S;
			S.Species = EDragonSpecies::Volador;
			S.TreeName = TEXT("ST_DragonVolador");
			S.TableName = TEXT("DT_Markov_Volador");
			S.ActionNames = { TEXT("Perseguir en vuelo"), TEXT("Atacar - Bola de fuego"), TEXT("Rodear en el aire"), TEXT("Ganar altura (retroceder)") };
			S.Matrix = { { { 0.20f, 0.45f, 0.25f, 0.10f },
			               { 0.15f, 0.30f, 0.35f, 0.20f },
			               { 0.20f, 0.50f, 0.15f, 0.15f },
			               { 0.30f, 0.40f, 0.20f, 0.10f } } };
			// MinD  MaxD     Energia LOS    MaxFury LowHP  Furia  Lejos
			S.Rules[0] = Rule(0.f, 100000.f, 0.f,  false, 2.f,    1.f,   1.f,   2.5f);
			S.Rules[1] = Rule(0.f, 3200.f,   1.f,  true,  2.f,    0.8f,  2.0f,  0.5f);
			S.Rules[2] = Rule(0.f, 4000.f,   0.f,  false, 2.f,    1.f,   0.8f,  1.f);
			S.Rules[3] = Rule(0.f, 100000.f, 0.f,  false, 2.f,    3.0f,  0.5f,  0.3f);
			Out.Add(S);
		}
		{
			FSpeciesSetup S;
			S.Species = EDragonSpecies::Saltarin;
			S.TreeName = TEXT("ST_DragonSaltarin");
			S.TableName = TEXT("DT_Markov_Saltarin");
			S.ActionNames = { TEXT("Perseguir a saltos"), TEXT("Atacar - Embestida saltando"), TEXT("Flanquear"), TEXT("Retirarse a percha") };
			S.Matrix = { { { 0.25f, 0.35f, 0.30f, 0.10f },
			               { 0.10f, 0.15f, 0.45f, 0.30f },
			               { 0.10f, 0.60f, 0.15f, 0.15f },
			               { 0.30f, 0.20f, 0.40f, 0.10f } } };
			S.Rules[0] = Rule(0.f, 100000.f, 0.f,  false, 2.f,    1.f,   1.f,   2.5f,  0.3f);
			S.Rules[1] = Rule(0.f, 1300.f,   25.f, true,  2.f,    0.7f,  1.8f,  1.f,   2.5f);
			S.Rules[2] = Rule(0.f, 2000.f,   10.f, false, 2.f,    1.f,   1.f,   0.5f);
			S.Rules[3] = Rule(0.f, 100000.f, 0.f,  false, 2.f,    3.0f,  0.6f,  0.5f);
			Out.Add(S);
		}
		{
			FSpeciesSetup S;
			S.Species = EDragonSpecies::Impulsivo;
			S.TreeName = TEXT("ST_DragonImpulsivo");
			S.TableName = TEXT("DT_Markov_Impulsivo");
			S.ActionNames = { TEXT("Cargar (perseguir)"), TEXT("Atacar - Embestida"), TEXT("Rugir (furia)"), TEXT("Ataque en picada") };
			S.Matrix = { { { 0.20f, 0.45f, 0.10f, 0.25f },
			               { 0.30f, 0.25f, 0.25f, 0.20f },
			               { 0.30f, 0.40f, 0.00f, 0.30f },
			               { 0.40f, 0.35f, 0.15f, 0.10f } } };
			S.Rules[0] = Rule(0.f, 100000.f, 0.f,  false, 2.f,    1.f,   1.f,   2.0f,  0.3f);
			S.Rules[1] = Rule(0.f, 1600.f,   30.f, true,  2.f,    1.8f,  2.0f,  1.f,   2.5f);
			S.Rules[2] = Rule(0.f, 100000.f, 0.f,  false, 0.85f,  0.5f,  0.3f,  1.f);
			S.Rules[3] = Rule(200.f, 1400.f, 35.f, false, 2.f,    1.5f,  1.6f,  1.f,   1.5f);
			Out.Add(S);
		}
		return Out;
	}
}

UDragonSetupCommandlet::UDragonSetupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

bool UDragonSetupCommandlet::GenerateAll()
{
	bool bAllOk = true;
	for (const DragonSetup::FSpeciesSetup& Setup : DragonSetup::MakeSetups())
	{
		UDataTable* Table = DragonSetup::BuildMarkovTable(Setup.TableName, Setup.Matrix, Setup.ActionNames);
		UStateTree* Tree = DragonSetup::BuildTree(Setup, Table);
		bAllOk &= (Table != nullptr && Tree != nullptr);
	}
	UE_LOG(LogDragonSetup, Display, TEXT("DragonSetup terminado: %s"), bAllOk ? TEXT("OK") : TEXT("CON ERRORES"));
	return bAllOk;
}

// MAPA: arena grande propia

namespace DragonSetup
{
	static AStaticMeshActor* SpawnBlock(UWorld* World, const FString& Label, const FVector& Loc, const FVector& Scale, UStaticMesh* Mesh, UMaterialInterface* Mat, const FRotator& Rot = FRotator::ZeroRotator)
	{
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(Loc, Rot);
		A->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		if (Mat) A->GetStaticMeshComponent()->SetMaterial(0, Mat);
		A->SetActorScale3D(Scale);
		A->SetActorLabel(Label);
		return A;
	}
}

bool UDragonSetupCommandlet::BuildMap()
{
	using namespace DragonSetup;

	const FString PackageName = TEXT("/Game/DragonRider/Maps/Lvl_DragonRider");
	constexpr float ArenaSize = 30000.f; // 300 m x 300 m
	constexpr float Half = ArenaSize * 0.5f;

	UPackage* Package = CreatePackage(*PackageName);
	UWorldFactory* Factory = NewObject<UWorldFactory>();
	Factory->WorldType = EWorldType::Editor;
	Factory->bInformEngineOfWorld = true;
	Factory->FeatureLevel = GMaxRHIFeatureLevel;
	UWorld* World = Cast<UWorld>(Factory->FactoryCreateNew(UWorld::StaticClass(), Package, TEXT("Lvl_DragonRider"), RF_Public | RF_Standalone, nullptr, GWarn));
	if (!World)
	{
		UE_LOG(LogDragonSetup, Error, TEXT("No se pudo crear el mapa"));
		return false;
	}

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* GridGray = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray"));
	UMaterialInterface* GridDark = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark.MI_PrototypeGrid_TopDark"));
	UMaterialInterface* GridGray2 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_02.MI_PrototypeGrid_Gray_02"));

	// Suelo y muros
	SpawnBlock(World, TEXT("Suelo"), FVector(0, 0, -50.f), FVector(ArenaSize / 100.f, ArenaSize / 100.f, 1.f), Cube, GridGray);
	const float WallH = 800.f;
	SpawnBlock(World, TEXT("Muro_N"), FVector(Half + 100.f, 0, WallH * 0.5f), FVector(2.f, ArenaSize / 100.f + 4.f, WallH / 100.f), Cube, GridDark);
	SpawnBlock(World, TEXT("Muro_S"), FVector(-Half - 100.f, 0, WallH * 0.5f), FVector(2.f, ArenaSize / 100.f + 4.f, WallH / 100.f), Cube, GridDark);
	SpawnBlock(World, TEXT("Muro_E"), FVector(0, Half + 100.f, WallH * 0.5f), FVector(ArenaSize / 100.f + 4.f, 2.f, WallH / 100.f), Cube, GridDark);
	SpawnBlock(World, TEXT("Muro_O"), FVector(0, -Half - 100.f, WallH * 0.5f), FVector(ArenaSize / 100.f + 4.f, 2.f, WallH / 100.f), Cube, GridDark);

	// Rocas de cobertura
	FRandomStream Rng(1337);
	const FVector NestCenters[] = {
		FRotator(0, 90, 0).Vector() * 7500.f, FRotator(0, 210, 0).Vector() * 7500.f, FRotator(0, 330, 0).Vector() * 7500.f };
	int32 Placed = 0;
	for (int32 Try = 0; Try < 400 && Placed < 28; ++Try)
	{
		const FVector P(Rng.FRandRange(-Half + 1500.f, Half - 1500.f), Rng.FRandRange(-Half + 1500.f, Half - 1500.f), 0.f);
		bool bOk = P.Size2D() > 1500.f;
		for (const FVector& N : NestCenters) { bOk &= FVector::Dist2D(P, N) > 3800.f; }
		if (!bOk) continue;
		const FVector S(Rng.FRandRange(3.f, 9.f), Rng.FRandRange(3.f, 9.f), Rng.FRandRange(2.f, 6.f));
		SpawnBlock(World, FString::Printf(TEXT("Roca_%02d"), Placed), FVector(P.X, P.Y, S.Z * 50.f), S, Cube, GridGray2, FRotator(0, Rng.FRandRange(0.f, 90.f), 0));
		++Placed;
	}

	// Iluminacion dinamica
	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 2000.f), FRotator(-42.f, 35.f, 0.f)))
	{
		Sun->SetActorLabel(TEXT("Sol"));
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			L->SetMobility(EComponentMobility::Movable);
			L->SetAtmosphereSunLight(true);
		}
	}
	if (ASkyAtmosphere* Sky = World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		Sky->SetActorLabel(TEXT("Atmosfera"));
	}
	if (ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(FVector(0, 0, 500.f), FRotator::ZeroRotator))
	{
		SkyLight->SetActorLabel(TEXT("LuzCielo"));
		USkyLightComponent* SL = SkyLight->GetLightComponent();
		SL->SetMobility(EComponentMobility::Movable);
		SL->SetRealTimeCapture(true);
	}
	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		Fog->SetActorLabel(TEXT("Niebla"));
	}

	// NavMesh (se genera en runtime
	if (ANavMeshBoundsVolume* Nav = World->SpawnActor<ANavMeshBoundsVolume>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		UCubeBuilder* Builder = NewObject<UCubeBuilder>();
		Builder->X = ArenaSize;
		Builder->Y = ArenaSize;
		Builder->Z = 4000.f;
		UActorFactory::CreateBrushForVolumeActor(Nav, Builder);
		Nav->SetActorLabel(TEXT("NavMeshBounds"));
	}

	// Jugador al centro mirando al nido Volador
	if (APlayerStart* Start = World->SpawnActor<APlayerStart>(FVector(0, -800.f, 120.f), FRotator(0.f, 90.f, 0.f)))
	{
		Start->SetActorLabel(TEXT("Inicio"));
	}
	if (ADragonArenaDirector* Director = World->SpawnActor<ADragonArenaDirector>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		Director->SetActorLabel(TEXT("DragonArenaDirector"));
		Director->NestDistance = 7500.f;
		Director->NestScale = 2.5f;
	}

	World->GetWorldSettings()->DefaultGameMode = ADragonRiderGameMode::StaticClass();

	FAssetRegistryModule::AssetCreated(World);
	Package->MarkPackageDirty();
	const FString File = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetMapPackageExtension());
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	const bool bOk = UPackage::SavePackage(Package, World, *File, Args);
	UE_LOG(LogDragonSetup, Display, TEXT("%s mapa %s"), bOk ? TEXT("Guardado") : TEXT("ERROR guardando"), *File);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return bOk;
}

int32 UDragonSetupCommandlet::Main(const FString& Params)
{
	const bool bAssets = GenerateAll();
	const bool bMap = Params.Contains(TEXT("-nomap")) ? true : BuildMap();
	return (bAssets && bMap) ? 0 : 1;
}

static FAutoConsoleCommand GDragonGenerateCmd(
	TEXT("DragonRider.GenerateAssets"),
	TEXT("Regenera los StateTrees y DataTables de Markov de Dragon Rider."),
	FConsoleCommandDelegate::CreateLambda([]() { UDragonSetupCommandlet::GenerateAll(); }));
