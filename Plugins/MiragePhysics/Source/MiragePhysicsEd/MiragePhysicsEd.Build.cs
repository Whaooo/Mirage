// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class MiragePhysicsEd : ModuleRules
{
	public MiragePhysicsEd(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "MiragePhysics" });
        PrivateDependencyModuleNames.AddRange(new string[] { "AnimGraph", "BlueprintGraph", "Persona", "UnrealEd", "AnimGraphRuntime", "SlateCore" });


		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
