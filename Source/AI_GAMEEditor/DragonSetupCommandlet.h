#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DragonSetupCommandlet.generated.h"

// Genera (o regenera) los assets de IA de Dragon Rider
UCLASS()
class UDragonSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDragonSetupCommandlet();
	virtual int32 Main(const FString& Params) override;

	// StateTrees + DataTables
	static bool GenerateAll();

	// Mapa /Game/DragonRider/Maps/Lvl_DragonRider
	static bool BuildMap();
};
