// Copyright 2026 kokage. All Rights Reserved.

using UnrealBuildTool;

public class SequencerSubtitles : ModuleRules
{
	public SequencerSubtitles(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"MovieScene",
				"MovieSceneTracks",
				"UMG",
				"Slate",
				"SlateCore",
				"DeveloperSettings"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"ApplicationCore"
			}
		);
		
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"LevelEditor",
					"UnrealEd"
				}
			);
		}
	}
}
