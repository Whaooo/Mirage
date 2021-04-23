// Copyright Epic Games, Inc. All Rights Reserved.

#include "MiragePhysicsSampleGameMode.h"
#include "MiragePhysicsSampleCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMiragePhysicsSampleGameMode::AMiragePhysicsSampleGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
