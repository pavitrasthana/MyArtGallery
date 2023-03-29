// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyArtGalleryGameMode.h"
#include "MyArtGalleryCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMyArtGalleryGameMode::AMyArtGalleryGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
