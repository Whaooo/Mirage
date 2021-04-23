// Fill out your copyright notice in the Description page of Project Settings.

#include "MiragePhysicsEd.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "MiragePhysicsEditMode.h"

#define LOCTEXT_NAMESPACE "FMiragePhysicsModuleEd"



void FMiragePhysicsEdModule::StartupModule()
{
	FEditorModeRegistry::Get().RegisterMode<FMiragePhysicsEditMode>("AnimGraph.SkeletalControl.MiragePhysics", LOCTEXT("FMiragePhysicsEditMode", "Mirage Physics"), FSlateIcon(), false);
}



void FMiragePhysicsEdModule::ShutdownModule()
{
	FEditorModeRegistry::Get().UnregisterMode("AnimGraph.SkeletalControl.MiragePhysics");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMiragePhysicsEdModule, MiragePhysicsEd)

