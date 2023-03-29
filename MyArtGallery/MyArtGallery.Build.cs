// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MyArtGallery : ModuleRules
{
	public MyArtGallery(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput", "HTTP", "Json", "JsonUtilities", "ImageWrapper" });
	}
}
