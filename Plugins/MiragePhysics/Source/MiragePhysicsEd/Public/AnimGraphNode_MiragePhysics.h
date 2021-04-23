#pragma once

#include "AnimGraphNode_Base.h"
#include "AnimNode_MiragePhysics.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "EdGraph/EdGraphNodeUtils.h"

#include "AnimGraphNode_MiragePhysics.generated.h"

class FCompilerResultsLog;

UCLASS()
class UAnimGraphNode_MiragePhysics : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_MiragePhysics Node;

	UPROPERTY(EditAnywhere, Category = Debug)
	bool bEnableDebugDrawBone = true;
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bEnableDebugBoneLengthRate= true;
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bEnableDebugDrawAngleLimit = true;
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bEnableDebugDrawSphereLimit = true;
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bEnableDebugDrawCapsuleLimit = true;
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bEnableDebugDrawPlanerLimit = true;
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bEnableDebugDrawBoneLimit = true;

public:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

protected:

	// UAnimGraphNode_Base interface
	virtual FEditorModeID GetEditorMode() const;
	virtual void ValidateAnimNodePostCompile(FCompilerResultsLog& MessageLog, UAnimBlueprintGeneratedClass* CompiledClass, int32 CompiledNodeIndex) override;
	// End of UAnimGraphNode_Base interface


	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
	virtual void Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const override;
	// End of UAnimGraphNode_SkeletalControlBase interface

private:
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTitleTextTable CachedNodeTitles;
};