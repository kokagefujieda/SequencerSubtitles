// Copyright 2026 kokage. All Rights Reserved.

using UnrealBuildTool;

public class SequencerSubtitlesEditor : ModuleRules
{
	public SequencerSubtitlesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"SequencerSubtitles"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"ApplicationCore",
				"Sequencer",
				"SequencerCore",
				"MovieScene",
				"MovieSceneTools",
				"MovieSceneTracks",
				"UnrealEd",
				"WorkspaceMenuStructure",
				"Json",
				"JsonUtilities"
			}
		);
	}
}
