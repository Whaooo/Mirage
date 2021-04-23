#include "AnimNode_MiragePhysics.h"

#include "MiragePhysics.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"
#include "Curves/CurveFloat.h"
#include "MiragePhysicsLimitsDataAsset.h"

TAutoConsoleVariable<int32> CVarEnableOldPhysicsMethodGrayity(TEXT("p.MiragePhysics.EnableOldPhysicsMethodGravity"), 0, 
	TEXT("Enables/Disables old physics method for gravity before v1.3.1. This is the setting for the transition period when changing the physical calculation."));
TAutoConsoleVariable<int32> CVarEnableOldPhysicsMethodSphereLimit(TEXT("p.MiragePhysics.EnableOldPhysicsMethodSphereLimit"), 0,
	TEXT("Enables/Disables old physics method for sphere limit before v1.3.1. This is the setting for the transition period when changing the physical calculation."));

FAnimNode_MiragePhysics::FAnimNode_MiragePhysics()
{

}

void FAnimNode_MiragePhysics::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
	FBoneContainer& RequiredBones = Context.AnimInstanceProxy->GetRequiredBones();

	ApplyLimitsDataAsset(RequiredBones);

	InitializeBoneReferences(RequiredBones);

	ModifyBones.Empty();

	// For Avoiding Zero Divide in the first frame
	DeltaTimeOld = 1.0f / TargetFramerate;

	bResetDynamics = false;

#if WITH_EDITOR
	auto World = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetWorld();
	if (World->WorldType == EWorldType::Editor ||
		World->WorldType == EWorldType::EditorPreview)
	{
		bEditing = true;
	}
#endif
}

void FAnimNode_MiragePhysics::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	FAnimNode_SkeletalControlBase::CacheBones_AnyThread(Context);

}

void FAnimNode_MiragePhysics::ResetDynamics(ETeleportType InTeleportType)
{
	bResetDynamics |= (ETeleportType::ResetPhysics == InTeleportType);
}

void FAnimNode_MiragePhysics::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);

	DeltaTime = Context.GetDeltaTime();
}

DECLARE_CYCLE_STAT(TEXT("MiragePhysics_Eval"), STAT_MiragePhysics_Eval, STATGROUP_Anim);

void FAnimNode_MiragePhysics::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_MiragePhysics_Eval);

	check(OutBoneTransforms.Num() == 0);

	if(bResetDynamics)
	{
		ModifyBones.Empty(ModifyBones.Num());
		bResetDynamics = false;
	}

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

#if WITH_EDITOR
	// sync editing on other Nodes
	if (LimitsDataAsset)
	{ 
		ApplyLimitsDataAsset(BoneContainer);
	}
#endif

	/*if (!RootBone.IsValidToEvaluate(BoneContainer))
	{
		return;
	}*/

	if (RootBones.Num() <= 0) 
	{
		return;
	}


	if (ModifyBones.Num() == 0)
	{
		InitModifyBones(Output, BoneContainer);
		PreSkelCompTransform = ComponentTransform;
	}

	// Update each parameters and collision
	if (!bInitPhysicsSettings || bUpdatePhysicsSettingsInGame)
	{
		UpdatePhysicsSettingsOfModifyBones();
		
#if WITH_EDITORONLY_DATA
		if (!bEditing)
#endif
		{
			bInitPhysicsSettings = true;
		}
	}
	UpdateSphericalLimits(SphericalLimits, Output, BoneContainer, ComponentTransform);
	UpdateSphericalLimits(SphericalLimitsData, Output, BoneContainer, ComponentTransform);
	UpdateCapsuleLimits(CapsuleLimits, Output, BoneContainer, ComponentTransform);
	UpdateCapsuleLimits(CapsuleLimitsData, Output, BoneContainer, ComponentTransform);
	UpdatePlanerLimits(PlanarLimits,Output, BoneContainer, ComponentTransform);
	UpdatePlanerLimits(PlanarLimitsData, Output, BoneContainer, ComponentTransform);
	for (auto& Bone : ModifyBones)
	{
		if (!Bone.bDummy)
		{
			Bone.UpdatePoseTranform(BoneContainer, Output.Pose);
		}
		else
		{
			auto ParentBone = ModifyBones[Bone.ParentIndex];
			Bone.PoseLocation = ParentBone.PoseLocation + GetBoneForwardVector(ParentBone.PoseRotation) * DummyBoneLength;
			Bone.PoseRotation = ParentBone.PoseRotation;
			Bone.PoseScale = ParentBone.PoseScale;
		}
	}
	

	// Calc SkeletalMeshComponent movement in World Space
	SkelCompMoveVector = ComponentTransform.InverseTransformPosition(PreSkelCompTransform.GetLocation());
	if (SkelCompMoveVector.SizeSquared() > TeleportDistanceThreshold * TeleportDistanceThreshold)
	{
		SkelCompMoveVector = FVector::ZeroVector;
	}

	SkelCompMoveRotation = ComponentTransform.InverseTransformRotation(PreSkelCompTransform.GetRotation());
	if ( TeleportRotationThreshold >= 0 && FMath::RadiansToDegrees( SkelCompMoveRotation.GetAngle() ) > TeleportRotationThreshold )
	{
		SkelCompMoveRotation = FQuat::Identity;
	}

	PreSkelCompTransform = ComponentTransform;

	// Simulate Physics and Apply
	SimulateModifyBones(Output, BoneContainer, ComponentTransform);
	ApplySimuateResult(Output, BoneContainer, OutBoneTransforms);
}

bool FAnimNode_MiragePhysics::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return RootBones.Num() > 0;
}

void FAnimNode_MiragePhysics::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	//RootBone.Initialize(RequiredBones);
	for (auto& RootBone : RootBones)
	{
		RootBone.Initialize(RequiredBones);
	}

	for (auto& Bone : ModifyBones)
	{
		Bone.BoneRef.Initialize(RequiredBones);
	}

	for (auto& Sphere : SphericalLimits)
	{
		Sphere.DrivingBone.Initialize(RequiredBones);
	}
	for (auto& Capsule : CapsuleLimits)
	{
		Capsule.DrivingBone.Initialize(RequiredBones);
	}
	for (auto& Planer : PlanarLimits)
	{
		Planer.DrivingBone.Initialize(RequiredBones);
	}

}

DECLARE_CYCLE_STAT(TEXT("MiragePhysics_InitModifyBones"), STAT_MiragePhysics_InitModifyBones, STATGROUP_Anim);

void FAnimNode_MiragePhysics::InitModifyBones(FComponentSpacePoseContext& Output, const FBoneContainer& BoneContainer)
{
	SCOPE_CYCLE_COUNTER(STAT_MiragePhysics_InitModifyBones);

	auto Skeleton = BoneContainer.GetSkeletonAsset();
	auto& RefSkeleton = Skeleton->GetReferenceSkeleton();

	ModifyBones.Empty();
	for (auto& RootBone : RootBones) 
	{
		AddModifyBone(Output, BoneContainer, RefSkeleton, RefSkeleton.FindBoneIndex(RootBone.BoneName));
	}
	if (ModifyBones.Num() > 0)
	{
		CalcBoneLength(ModifyBones[0], BoneContainer.GetRefPoseCompactArray());
	}
	CollectLimitBones();
}



void FAnimNode_MiragePhysics::ApplyLimitsDataAsset(const FBoneContainer& RequiredBones)
{
	SphericalLimitsData.Empty();
	CapsuleLimitsData.Empty();
	PlanarLimitsData.Empty();
	if (LimitsDataAsset)
	{
		SphericalLimitsData = LimitsDataAsset->SphericalLimits;
		CapsuleLimitsData = LimitsDataAsset->CapsuleLimits;
		PlanarLimitsData = LimitsDataAsset->PlanarLimits;
	}

	for (auto& Sphere : SphericalLimitsData)
	{
		Sphere.DrivingBone.Initialize(RequiredBones);
	}

	for (auto& Capsule : CapsuleLimitsData)
	{
		Capsule.DrivingBone.Initialize(RequiredBones);
	}
	for (auto& Planer : PlanarLimitsData)
	{
		Planer.DrivingBone.Initialize(RequiredBones);
	}
}

int FAnimNode_MiragePhysics::AddModifyBone(FComponentSpacePoseContext& Output, const FBoneContainer& BoneContainer, 
	const FReferenceSkeleton& RefSkeleton, int BoneIndex)
{
	if (BoneIndex < 0 || RefSkeleton.GetNum() < BoneIndex)
	{
		return INDEX_NONE;
	}

	FBoneReference BoneRef;
	BoneRef.BoneName = RefSkeleton.GetBoneName(BoneIndex);

	if (ExcludeBones.Num() > 0 && ExcludeBones.Find(BoneRef) >= 0)
	{
		return INDEX_NONE;
	}

	FMiragePhysicsModifyBone NewModifyBone;
	NewModifyBone.BoneRef = BoneRef;
	NewModifyBone.BoneRef.Initialize(BoneContainer);
	if (NewModifyBone.BoneRef.CachedCompactPoseIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	auto& RefBonePoseTransform = Output.Pose.GetComponentSpaceTransform(NewModifyBone.BoneRef.CachedCompactPoseIndex);
	NewModifyBone.Location = RefBonePoseTransform.GetLocation();  
	NewModifyBone.PrevLocation = NewModifyBone.Location;
	NewModifyBone.PoseLocation = NewModifyBone.Location;
	NewModifyBone.PrevRotation = RefBonePoseTransform.GetRotation();
	NewModifyBone.PoseRotation = NewModifyBone.PrevRotation;
	NewModifyBone.PoseScale = RefBonePoseTransform.GetScale3D();
	int ModifyBoneIndex = ModifyBones.Add(NewModifyBone);

	TArray<int32> ChildBoneIndexs;
	CollectChildBones(RefSkeleton, BoneIndex, ChildBoneIndexs);

	/*TArray<FBoneReference> LimitBones;
	CollectLimitBones(RefSkeleton, BoneIndex, LimitBones);*/


	if (ChildBoneIndexs.Num() > 0)
	{
		for (auto ChildBoneIndex : ChildBoneIndexs)
		{
			auto ChildModifyBoneIndex = AddModifyBone(Output, BoneContainer, RefSkeleton, ChildBoneIndex);
			if (ChildModifyBoneIndex >= 0)
			{
				ModifyBones[ModifyBoneIndex].ChildIndexs.Add(ChildModifyBoneIndex);
				ModifyBones[ChildModifyBoneIndex].ParentIndex = ModifyBoneIndex;
			}
		}
	}
	else if(DummyBoneLength > 0.0f)
	{
		// Add dummy modify bone
		FMiragePhysicsModifyBone DummyModifyBone;
		DummyModifyBone.bDummy = true;
		DummyModifyBone.Location = NewModifyBone.Location + GetBoneForwardVector(NewModifyBone.PrevRotation) * DummyBoneLength;
		DummyModifyBone.PrevLocation = DummyModifyBone.Location;
		DummyModifyBone.PoseLocation = DummyModifyBone.Location;
		DummyModifyBone.PrevRotation = NewModifyBone.PrevRotation;
		DummyModifyBone.PoseRotation = DummyModifyBone.PrevRotation;
		DummyModifyBone.PoseScale = RefBonePoseTransform.GetScale3D();

		int DummyBoneIndex = ModifyBones.Add(DummyModifyBone);
		ModifyBones[ModifyBoneIndex].ChildIndexs.Add(DummyBoneIndex);
		ModifyBones[DummyBoneIndex].ParentIndex = ModifyBoneIndex;
	}


	return ModifyBoneIndex;
}

int32 FAnimNode_MiragePhysics::CollectChildBones(const FReferenceSkeleton& RefSkeleton, int32 ParentBoneIndex, TArray<int32> & Children) const
{
	Children.Reset();

	const int32 NumBones = RefSkeleton.GetNum();
	for (int32 ChildIndex = ParentBoneIndex + 1; ChildIndex < NumBones; ChildIndex++)
	{
		if (ParentBoneIndex == RefSkeleton.GetParentIndex(ChildIndex))
		{
			Children.Add(ChildIndex);
		}
	}

	return Children.Num();
}

void FAnimNode_MiragePhysics::CollectLimitBones()
{
	if(BoneLimits.Num()<=0)
	{
		return;
	}
	for (auto& BoneLimit : BoneLimits)
	{
		int Master = -1;
		int Follower = -1;
		for (int i = 0; i< ModifyBones.Num(); i++)
		{
			if (ModifyBones[i].BoneRef == BoneLimit.Master)
			{
				//Bone.LimitBones.Add(BoneLimit.Follower);
				Master = i;
			}
			if (ModifyBones[i].BoneRef == BoneLimit.Master) 
			{
				Follower = i;
			}
			if (Master >= 0 && Follower >= 0)
			{
				ModifyBones[Master].LimitBones.Add(Follower);
				if (BoneLimit.IsMutual)
				{
					ModifyBones[Follower].LimitBones.Add(Master);
				}
				break;
			}
		}
		/*ModifyBones[Master].LimitBones.Add(Follower);
		if (BoneLimit.IsMutual) 
		{
			ModifyBones[Follower].LimitBones.Add(Master);
		}*/
	}
}

void FAnimNode_MiragePhysics::CalcBoneLength(FMiragePhysicsModifyBone& Bone, const TArray<FTransform>& RefBonePose)
{
	if (Bone.ParentIndex < 0)
	{
		Bone.LengthFromRoot = 0.0f;
	}
	else
	{
		if (!Bone.bDummy)
		{
			Bone.LengthFromRoot = ModifyBones[Bone.ParentIndex].LengthFromRoot
				+ RefBonePose[Bone.BoneRef.BoneIndex].GetLocation().Size();
		}
		else
		{
			Bone.LengthFromRoot = ModifyBones[Bone.ParentIndex].LengthFromRoot + DummyBoneLength;
		}
		
		TotalBoneLength = FMath::Max(TotalBoneLength, Bone.LengthFromRoot);
	}

	for (int ChildIndex : Bone.ChildIndexs)
	{
		CalcBoneLength(ModifyBones[ChildIndex], RefBonePose);
	}
}

DECLARE_CYCLE_STAT(TEXT("MiragePhysics_UpdatePhysicsSetting"), STAT_MiragePhysics_UpdatePhysicsSetting, STATGROUP_Anim);

void FAnimNode_MiragePhysics::UpdatePhysicsSettingsOfModifyBones()
{
	for (auto& Bone : ModifyBones)
	{
		SCOPE_CYCLE_COUNTER(STAT_MiragePhysics_UpdatePhysicsSetting);

		float LengthRate = Bone.LengthFromRoot / TotalBoneLength;

		// Damping
		Bone.PhysicsSettings.Damping = PhysicsSettings.Damping;
		if (TotalBoneLength > 0 && DampingCurve && DampingCurve->GetCurves().Num() > 0)
		{
			Bone.PhysicsSettings.Damping *= DampingCurve->GetFloatValue(LengthRate);
		}
		Bone.PhysicsSettings.Damping = FMath::Clamp<float>(Bone.PhysicsSettings.Damping, 0.0f, 1.0f);

		// WorldLocationDamping
		Bone.PhysicsSettings.WorldDampingLocation = PhysicsSettings.WorldDampingLocation;
		if (TotalBoneLength > 0 && WorldDampingLocationCurve && WorldDampingLocationCurve->GetCurves().Num() > 0)
		{
			Bone.PhysicsSettings.WorldDampingLocation *= WorldDampingLocationCurve->GetFloatValue(LengthRate);
		}
		Bone.PhysicsSettings.WorldDampingLocation = FMath::Clamp<float>(Bone.PhysicsSettings.WorldDampingLocation, 0.0f, 1.0f);

		// WorldRotationDamping
		Bone.PhysicsSettings.WorldDampingRotation = PhysicsSettings.WorldDampingRotation;
		if (TotalBoneLength > 0 && WorldDampingRotationCurve && WorldDampingRotationCurve->GetCurves().Num() > 0)
		{
			Bone.PhysicsSettings.WorldDampingRotation *= WorldDampingRotationCurve->GetFloatValue(LengthRate);
		}
		Bone.PhysicsSettings.WorldDampingRotation = FMath::Clamp<float>(Bone.PhysicsSettings.WorldDampingRotation, 0.0f, 1.0f);

		// Stiffness
		Bone.PhysicsSettings.Stiffness = PhysicsSettings.Stiffness;
		if (TotalBoneLength > 0 && StiffnessCurve && StiffnessCurve->GetCurves().Num() > 0)
		{
			Bone.PhysicsSettings.Stiffness *= StiffnessCurve->GetFloatValue(LengthRate);
		}
		Bone.PhysicsSettings.Stiffness = FMath::Clamp<float>(Bone.PhysicsSettings.Stiffness, 0.0f, 1.0f);

		// Radius
		Bone.PhysicsSettings.Radius = PhysicsSettings.Radius;
		if (TotalBoneLength > 0 && RadiusCurve && RadiusCurve->GetCurves().Num() > 0)
		{
			Bone.PhysicsSettings.Radius *= RadiusCurve->GetFloatValue(LengthRate);
		}
		Bone.PhysicsSettings.Radius = FMath::Max<float>(Bone.PhysicsSettings.Radius, 0.0f);

		// LimitAngle
		Bone.PhysicsSettings.LimitAngle = PhysicsSettings.LimitAngle;
		if (TotalBoneLength > 0 && LimitAngleCurve && LimitAngleCurve->GetCurves().Num() > 0)
		{
			Bone.PhysicsSettings.LimitAngle *= LimitAngleCurve->GetFloatValue(LengthRate);
		}
		Bone.PhysicsSettings.LimitAngle = FMath::Max<float>(Bone.PhysicsSettings.LimitAngle, 0.0f);
	}
}

DECLARE_CYCLE_STAT(TEXT("MiragePhysics_UpdatePhysicsSetting"), STAT_MiragePhysics_UpdateSphericalLimits, STATGROUP_Anim);

void FAnimNode_MiragePhysics::UpdateSphericalLimits(TArray<FSphericalLimit>& Limits, FComponentSpacePoseContext& Output, const FBoneContainer& BoneContainer, FTransform& ComponentTransform)
{
	for (auto& Sphere : Limits)
	{
		SCOPE_CYCLE_COUNTER(STAT_MiragePhysics_UpdateSphericalLimits);

		if (Sphere.DrivingBone.BoneIndex >= 0)
		{		
			FCompactPoseBoneIndex CompactPoseIndex = Sphere.DrivingBone.GetCompactPoseIndex(BoneContainer);
			FTransform BoneTransform = Output.Pose.GetComponentSpaceTransform(CompactPoseIndex);

			FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, BoneTransform, CompactPoseIndex, BCS_BoneSpace);
			BoneTransform.SetRotation(Sphere.OffsetRotation.Quaternion() * BoneTransform.GetRotation());
			BoneTransform.AddToTranslation(Sphere.OffsetLocation);

			FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, BoneTransform, CompactPoseIndex, BCS_BoneSpace);
			Sphere.Location = BoneTransform.GetLocation();
			Sphere.Rotation = BoneTransform.GetRotation();
		}
		else
		{
			Sphere.Location = Sphere.OffsetLocation;
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("MiragePhysics_UpdatePhysicsSetting"), STAT_MiragePhysics_UpdateCapsuleLimits, STATGROUP_Anim);

void FAnimNode_MiragePhysics::UpdateCapsuleLimits(TArray<FCapsuleLimit>& Limits, FComponentSpacePoseContext& Output, const FBoneContainer& BoneContainer, FTransform& ComponentTransform)
{
	for (auto& Capsule : Limits)
	{
		SCOPE_CYCLE_COUNTER(STAT_MiragePhysics_UpdateCapsuleLimits);

		if (Capsule.DrivingBone.BoneIndex >= 0)
		{			
			FCompactPoseBoneIndex CompactPoseIndex = Capsule.DrivingBone.GetCompactPoseIndex(BoneContainer);
			FTransform BoneTransform = Output.Pose.GetComponentSpaceTransform(CompactPoseIndex);

			FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, BoneTransform, CompactPoseIndex, BCS_BoneSpace);
			BoneTransform.SetRotation(Capsule.OffsetRotation.Quaternion() * BoneTransform.GetRotation());
			BoneTransform.AddToTranslation(Capsule.OffsetLocation);

			FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, BoneTransform, CompactPoseIndex, BCS_BoneSpace);
			Capsule.Location = BoneTransform.GetLocation();
			Capsule.Rotation = BoneTransform.GetRotation();
		}
		else
		{			
			Capsule.Location = Capsule.OffsetLocation;
			Capsule.Rotation = Capsule.OffsetRotation.Quaternion();
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("MiragePhysics_UpdatePhysicsSetting"), STAT_MiragePhysics_UpdatePlanerLimits, STATGROUP_Anim);

void FAnimNode_MiragePhysics::UpdatePlanerLimits(TArray<FPlanarLimit>& Limits, FComponentSpacePoseContext& Output, const FBoneContainer& BoneContainer, FTransform& ComponentTransform)
{
	for (auto& Planar : Limits)
	{
		SCOPE_CYCLE_COUNTER(STAT_MiragePhysics_UpdatePlanerLimits);

		if (Planar.DrivingBone.BoneIndex >= 0)
		{
			FCompactPoseBoneIndex CompactPoseIndex = Planar.DrivingBone.GetCompactPoseIndex(BoneContainer);
			FTransform BoneTransform = Output.Pose.GetComponentSpaceTransform(CompactPoseIndex);

			FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, BoneTransform, CompactPoseIndex, BCS_BoneSpace);
			BoneTransform.SetRotation(Planar.OffsetRotation.Quaternion() * BoneTransform.GetRotation());
			BoneTransform.AddToTranslation(Planar.OffsetLocation);

			FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, BoneTransform, CompactPoseIndex, BCS_BoneSpace);
			Planar.Location = BoneTransform.GetLocation();
			Planar.Rotation = BoneTransform.GetRotation();
			Planar.Rotation.Normalize();
			Planar.Plane = FPlane(Planar.Location, Planar.Rotation.GetUpVector());
		}
		else
		{
			Planar.Location = Planar.OffsetLocation;
			Planar.Rotation = Planar.OffsetRotation.Quaternion();
			Planar.Rotation.Normalize();
			Planar.Plane = FPlane(Planar.Location, Planar.Rotation.GetUpVector());
		}
	}
}



DECLARE_CYCLE_STAT(TEXT("MiragePhysics_SimulatemodifyBones"), STAT_MiragePhysics_SimulatemodifyBones, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("MiragePhysics_SimulatemodifyBone"), STAT_MiragePhysics_SimulatemodifyBone, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("MiragePhysics_AdjustBone"), STAT_MiragePhysics_AdjustBone, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("MiragePhysics_Wind"), STAT_MiragePhysics_Wind, STATGROUP_Anim);

void FAnimNode_MiragePhysics::SimulateModifyBones(FComponentSpacePoseContext& Output, const FBoneContainer& BoneContainer, FTransform& ComponentTransform)
{
	SCOPE_CYCLE_COUNTER(STAT_MiragePhysics_SimulatemodifyBones);

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	// for wind
	FVector WindDirection;
	float WindSpeed;
	float WindMinGust;
	float WindMaxGust;

	const USkeletalMeshComponent* SkelComp = Output.AnimInstanceProxy->GetSkelMeshComponent();
	const UWorld* World = SkelComp ? SkelComp->GetWorld() : nullptr;
	FSceneInterface* Scene = World && World->Scene ? World->Scene : nullptr;
	const float Exponent = TargetFramerate * DeltaTime;

	//transform gravity to component space
	FVector GravityCS = ComponentTransform.InverseTransformVector(Gravity);

	for (int i = 0; i < ModifyBones.Num(); ++i)
	{
		SCOPE_CYCLE_COUNTER(STAT_MiragePhysics_SimulatemodifyBone);

		auto& Bone = ModifyBones[i];
		if (Bone.BoneRef.BoneIndex < 0 && !Bone.bDummy)
		{
			continue;
		}

		if (Bone.ParentIndex < 0)
		{
			Bone.PrevLocation = Bone.Location;
			Bone.Location = Bone.PoseLocation;
			continue;
		}

		auto& ParentBone = ModifyBones[Bone.ParentIndex];
		FVector BonePoseLocation = Bone.PoseLocation;
		FVector ParentBonePoseLocation = ParentBone.PoseLocation;

		// Move using Velocity( = movement amount in pre frame ) and Damping
		{
			FVector Velocity = (Bone.Location - Bone.PrevLocation) / DeltaTimeOld;
			Bone.PrevLocation = Bone.Location;
			Velocity *= (1.0f - Bone.PhysicsSettings.Damping);

			// wind
			if (bEnableWind && Scene)
			{
				SCOPE_CYCLE_COUNTER(STAT_MiragePhysics_Wind);

				Scene->GetWindParameters_GameThread(ComponentTransform.TransformPosition(Bone.PoseLocation), WindDirection, WindSpeed, WindMinGust, WindMaxGust);
				WindDirection = ComponentTransform.Inverse().TransformVector(WindDirection);
				FVector WindVelocity = WindDirection * WindSpeed * WindScale;

				// TODO:Migrate if there are more good method (Currently copying AnimDynamics implementation)
				WindVelocity *= FMath::FRandRange(0.0f, 2.0f);

				Velocity += WindVelocity * TargetFramerate;
			}
			Bone.Location += Velocity * DeltaTime;
		}

		// Follow Translation
        Bone.Location += SkelCompMoveVector * (1.0f - Bone.PhysicsSettings.WorldDampingLocation);

		// Follow Rotation
		Bone.Location += (SkelCompMoveRotation.RotateVector(Bone.PrevLocation) - Bone.PrevLocation)
			* (1.0f - Bone.PhysicsSettings.WorldDampingRotation);

		// Gravity
		// TODO:Migrate if there are more good method (Currently copying AnimDynamics implementation)
		if (CVarEnableOldPhysicsMethodGrayity.GetValueOnAnyThread() == 0)
		{
			Bone.Location += 0.5 * GravityCS * DeltaTime * DeltaTime;
		}
		else
		{
			Bone.Location += GravityCS * DeltaTime;
		}
		
		// Pull to Pose Location
		FVector BaseLocation = ParentBone.Location + (BonePoseLocation - ParentBonePoseLocation);
		Bone.Location += (BaseLocation - Bone.Location) *
			(1.0f - FMath::Pow(1.0f - Bone.PhysicsSettings.Stiffness, Exponent));

		{
			SCOPE_CYCLE_COUNTER(STAT_MiragePhysics_AdjustBone);

			// Adjust by each collisions
			//AdjustBySphereCollision(Bone, SphericalLimits);
			//AdjustBySphereCollision(Bone, SphericalLimitsData);
			//AdjustByCapsuleCollision(Bone, CapsuleLimits);
			//AdjustByCapsuleCollision(Bone, CapsuleLimitsData);
			//AdjustByPlanerCollision(Bone, PlanarLimits);
			//AdjustByPlanerCollision(Bone, PlanarLimitsData);

			//// Adjust by angle limit
			//AdjustByAngleLimit(Output, BoneContainer, ComponentTransform, Bone, ParentBone);

			//// Adjust by Planar Constraint
			//AdjustByPlanarConstraint(Bone, ParentBone);
		}

		// Restore Bone Length
		float BoneLength = (BonePoseLocation - ParentBonePoseLocation).Size();
		Bone.Location = (Bone.Location - ParentBone.Location).GetSafeNormal() * BoneLength + ParentBone.Location;
	}
	DeltaTimeOld = DeltaTime;
}
/*
void FAnimNode_KawaiiPhysics::AdjustBySphereCollision(FKawaiiPhysicsModifyBone& Bone, TArray<FSphericalLimit>& Limits)
{
	for (auto& Sphere : Limits)
	{
		if (Sphere.Radius <= 0.0f)
		{
			continue;
		}

		float LimitDistance = Bone.PhysicsSettings.Radius + Sphere.Radius;
		if (Sphere.LimitType == ESphericalLimitType::Outer)
		{
			if ((Bone.Location - Sphere.Location).SizeSquared() > LimitDistance * LimitDistance)
			{
				continue;
			}
			else
			{
				Bone.Location += (LimitDistance - (Bone.Location - Sphere.Location).Size())
					* (Bone.Location - Sphere.Location).GetSafeNormal();
			}
		}
		else
		{
			if ((Bone.Location - Sphere.Location).SizeSquared() < LimitDistance * LimitDistance)
			{
				continue;
			}
			else
			{
				if (CVarEnableOldPhysicsMethodSphereLimit.GetValueOnAnyThread() == 0)
				{
					Bone.Location = Sphere.Location + (Sphere.Radius - Bone.PhysicsSettings.Radius) * (Bone.Location - Sphere.Location).GetSafeNormal();
				}
				else
				{
					Bone.Location = Sphere.Location + Sphere.Radius * (Bone.Location - Sphere.Location).GetSafeNormal();
				}
			}
		}
	}
}

void FAnimNode_KawaiiPhysics::AdjustByCapsuleCollision(FKawaiiPhysicsModifyBone& Bone, TArray<FCapsuleLimit>& Limits)
{
	for (auto& Capsule : Limits)
	{
		if (Capsule.Radius <= 0 || Capsule.Length <= 0)
		{
			continue;
		}

		FVector StartPoint = Capsule.Location + Capsule.Rotation.GetAxisZ() * Capsule.Length * 0.5f;
		FVector EndPoint = Capsule.Location + Capsule.Rotation.GetAxisZ() * Capsule.Length * -0.5f;
		float DistSquared = FMath::PointDistToSegmentSquared(Bone.Location, StartPoint, EndPoint);

		float LimitDistance = Bone.PhysicsSettings.Radius + Capsule.Radius;
		if (DistSquared < LimitDistance* LimitDistance)
		{
			FVector ClosestPoint = FMath::ClosestPointOnSegment(Bone.Location, StartPoint, EndPoint);
			Bone.Location = ClosestPoint + (Bone.Location - ClosestPoint).GetSafeNormal() * LimitDistance;
		}
	}
}

void FAnimNode_KawaiiPhysics::AdjustByPlanerCollision(FKawaiiPhysicsModifyBone& Bone, TArray<FPlanarLimit>& Limits)
{
	for (auto& Planar : Limits)
	{
		FVector PointOnPlane = FVector::PointPlaneProject(Bone.Location, Planar.Plane);
		float DistSquared = (Bone.Location - PointOnPlane).SizeSquared();

		FVector IntersectionPoint;
		if (DistSquared < Bone.PhysicsSettings.Radius * Bone.PhysicsSettings.Radius ||
			FMath::SegmentPlaneIntersection(Bone.Location, Bone.PrevLocation, Planar.Plane, IntersectionPoint))
		{
			Bone.Location = PointOnPlane + Planar.Rotation.GetUpVector() * Bone.PhysicsSettings.Radius;
			continue;;
		}
	}
}

void FAnimNode_KawaiiPhysics::AdjustByAngleLimit(FComponentSpacePoseContext& Output, const FBoneContainer& BoneContainer, FTransform& ComponentTransform, 
	FKawaiiPhysicsModifyBone& Bone, FKawaiiPhysicsModifyBone& ParentBone)
{
	if (Bone.PhysicsSettings.LimitAngle == 0.0f)
	{
		return;
	}

	FVector BoneDir = (Bone.Location - ParentBone.Location).GetSafeNormal();
	FVector PoseDir = (Bone.PoseLocation - ParentBone.PoseLocation).GetSafeNormal();
	FVector Axis = FVector::CrossProduct(PoseDir, BoneDir);
	float Angle = FMath::Atan2(Axis.Size(), FVector::DotProduct(PoseDir, BoneDir));
	float AngleOverLimit= FMath::RadiansToDegrees(Angle) - Bone.PhysicsSettings.LimitAngle;

	if (AngleOverLimit > 0.0f)
	{
		BoneDir = BoneDir.RotateAngleAxis(-AngleOverLimit, Axis);
		Bone.Location = BoneDir * (Bone.Location - ParentBone.Location).Size() + ParentBone.Location;
	}
}

void FAnimNode_KawaiiPhysics::AdjustByPlanarConstraint(FKawaiiPhysicsModifyBone& Bone, FKawaiiPhysicsModifyBone& ParentBone)
{
	FPlane Plane;
	if (PlanarConstraint != EPlanarConstraint::None)
	{
		switch (PlanarConstraint)
		{
		case EPlanarConstraint::X:
			Plane = FPlane(ParentBone.Location, ParentBone.PoseRotation.GetAxisX());
			break;
		case EPlanarConstraint::Y:
			Plane = FPlane(ParentBone.Location, ParentBone.PoseRotation.GetAxisY());
			break;
		case EPlanarConstraint::Z:
			Plane = FPlane(ParentBone.Location, ParentBone.PoseRotation.GetAxisZ());
			break;
		}
		Bone.Location  = FVector::PointPlaneProject(Bone.Location, Plane);
	}
}*/

void FAnimNode_MiragePhysics::ApplySimuateResult(FComponentSpacePoseContext& Output, const FBoneContainer& BoneContainer, TArray<FBoneTransform>& OutBoneTransforms)
{
	for (int i = 0; i < ModifyBones.Num(); ++i)
	{
		OutBoneTransforms.Add(FBoneTransform(ModifyBones[i].BoneRef.GetCompactPoseIndex(BoneContainer), 
			FTransform(ModifyBones[i].PoseRotation, ModifyBones[i].PoseLocation, ModifyBones[i].PoseScale)));
	}	

	for (int i = 1; i < ModifyBones.Num(); ++i)
	{
		FMiragePhysicsModifyBone& Bone = ModifyBones[i];
		if(Bone.ParentIndex < 0)
		{
			continue;
		}
		FMiragePhysicsModifyBone& ParentBone = ModifyBones[Bone.ParentIndex];

		if (ParentBone.ChildIndexs.Num() <= 1)
		{
			if (ParentBone.BoneRef.BoneIndex >= 0)
			{
				FVector PoseVector = Bone.PoseLocation - ParentBone.PoseLocation;
				FVector SimulateVector = Bone.Location - ParentBone.Location;

				if (PoseVector.GetSafeNormal() == SimulateVector.GetSafeNormal())
				{
					continue;
				}

				if (BoneForwardAxis == EBoneForwardAxis::X_Negative || BoneForwardAxis == EBoneForwardAxis::Y_Negative || BoneForwardAxis == EBoneForwardAxis::Z_Negative)
				{
					PoseVector *= -1;
					SimulateVector *= -1;
				}

				FQuat SimulateRotation = FQuat::FindBetweenVectors(PoseVector, SimulateVector) * ParentBone.PoseRotation;
				OutBoneTransforms[Bone.ParentIndex].Transform.SetRotation(SimulateRotation);
				ParentBone.PrevRotation = SimulateRotation;
			}
		}

		if (Bone.BoneRef.BoneIndex >= 0 && !Bone.bDummy)
		{
			OutBoneTransforms[i].Transform.SetLocation(Bone.Location);
		}
	}

	OutBoneTransforms.RemoveAll([](const FBoneTransform& BoneTransform) {
		return BoneTransform.BoneIndex < 0;
	});

	// for check in FCSPose<PoseType>::LocalBlendCSBoneTransforms
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
