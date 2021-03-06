#include "AnimGraphNode_MiragePhysics.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Materials/MaterialInstanceDynamic.h"

#define LOCTEXT_NAMESPACE "MiragePhysics"

// ----------------------------------------------------------------------------
UAnimGraphNode_MiragePhysics::UAnimGraphNode_MiragePhysics(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_MiragePhysics::GetControllerDescription() const
{
	return LOCTEXT("Mirage Physics", "Mirage Physics");
}


// ----------------------------------------------------------------------------
FText UAnimGraphNode_MiragePhysics::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle))
	{
		return GetControllerDescription();
	}
	// @TODO: the bone can be altered in the property editor, so we have to 
	//        choose to mark this dirty when that happens for this to properly work
	else //if (!CachedNodeTitles.IsTitleCached(TitleType, this))
	{
		//FFormatNamedArguments Args;
		//Args.Add(TEXT("ControllerDescription"), GetControllerDescription());
		//Args.Add(TEXT("RootBoneName"), FText::FromName(Node.RootBone.BoneName));

		//// FText::Format() is slow, so we cache this to save on performance
		//if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		//{
		//	CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_MiragePhysics_ListTitle", "{ControllerDescription} - Root: {RootBoneName}"), Args), this);
		//}
		//else
		//{
		//	CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_MiragePhysics_Title", "{ControllerDescription}\nRoot: {RootBoneName} "), Args), this);
		//}
	}
	//return CachedNodeTitles[TitleType];
	return FText::FromName(FName("aaa"));
}

void UAnimGraphNode_MiragePhysics::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	Node.ModifyBones.Empty();
	ReconstructNode();
}

FEditorModeID UAnimGraphNode_MiragePhysics::GetEditorMode() const
{
	return "AnimGraph.SkeletalControl.MiragePhysics";
}

void UAnimGraphNode_MiragePhysics::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const
{
	if (!SkelMeshComp)
	{
		return;
	}

	FAnimNode_MiragePhysics* ActiveNode = GetActiveInstanceNode<FAnimNode_MiragePhysics>(SkelMeshComp->GetAnimInstance());
	if (!ActiveNode)
	{
		return;
	}

	if (bEnableDebugDrawBone)
	{
		for (auto& Bone : ActiveNode->ModifyBones)
		{
			PDI->DrawPoint(Bone.Location, FLinearColor::White, 5.0f, SDPG_Foreground);

			if (Bone.PhysicsSettings.Radius > 0)
			{
				auto Color = Bone.bDummy ? FColor::Red : FColor::Yellow;
				DrawWireSphere(PDI, Bone.Location, Color, Bone.PhysicsSettings.Radius, 16, SDPG_Foreground);
			}

			for (int ChildIndex : Bone.ChildIndexs)
			{
				DrawDashedLine(PDI, Bone.Location, ActiveNode->ModifyBones[ChildIndex].Location,
					FLinearColor::White, 1, SDPG_Foreground);
			}
		}
	}

	if (bEnableDebugDrawBoneLimit)
	{
		for (auto& Bone : ActiveNode->ModifyBones)
		{
			for (auto& LimitBoneIndex : Bone.LimitBones)
			{
				DrawDashedLine(PDI, Bone.Location, ActiveNode->ModifyBones[LimitBoneIndex].Location,
					FLinearColor::White, 1, SDPG_Foreground);
			}
		}
	}

	if (bEnableDebugDrawAngleLimit)
	{
		for (auto& Bone : ActiveNode->ModifyBones)
		{
			if (Bone.ParentIndex < 0 || Bone.PhysicsSettings.LimitAngle <= 0)
			{
				continue;
			}

			auto& ParentBone = ActiveNode->ModifyBones[Bone.ParentIndex];
			FTransform ParentBoneTransform =
				FTransform(FQuat::FindBetween(FVector::ForwardVector, Bone.PoseLocation - ParentBone.PoseLocation), ParentBone.Location);
			TArray<FVector> Verts;
			DrawWireCone(PDI, Verts, ParentBoneTransform, (Bone.PoseLocation - ParentBone.PoseLocation).Size(),
				Bone.PhysicsSettings.LimitAngle, 16, FColor::Green, SDPG_World);
		}
	}

}

void UAnimGraphNode_MiragePhysics::ValidateAnimNodePostCompile(FCompilerResultsLog& MessageLog, UAnimBlueprintGeneratedClass* CompiledClass, int32 CompiledNodeIndex)
{
	UAnimGraphNode_SkeletalControlBase::ValidateAnimNodePostCompile(MessageLog, CompiledClass, CompiledNodeIndex);
	for (auto& RootBone : Node.RootBones)
	{
		RootBone.Initialize(CompiledClass->TargetSkeleton);
		if (RootBone.BoneIndex >= 0)
		{
			if (Node.ExcludeBones.Contains(RootBone))
			{
				MessageLog.Warning(TEXT("@@ ExcludeBones should NOT has RootBone."), this);
			}
		}
		else
		{
			MessageLog.Warning(TEXT("@@ RootBone is empty."), this);
		}
	}
}


#undef LOCTEXT_NAMESPACE