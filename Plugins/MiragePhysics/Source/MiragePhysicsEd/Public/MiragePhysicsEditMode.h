#pragma once

#include "CoreMinimal.h"
#include "UnrealWidget.h"
#include "MiragePhysicsEditModeBase.h"
#include "AnimNodeEditMode.h"
#include "AnimGraphNode_MiragePhysics.h"
#include "AnimNode_MiragePhysics.h"

class FEditorViewportClient;
class FPrimitiveDrawInterface;
class USkeletalMeshComponent;
struct FViewportClick;



class FMiragePhysicsEditMode : public FMiragePhysicsEditModeBase
{
public:

	FMiragePhysicsEditMode();
	
	/** IAnimNodeEditMode interface */
	virtual void EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode) override;
	virtual void ExitMode() override;
	virtual FVector GetWidgetLocation() const override;
	virtual FWidget::EWidgetMode GetWidgetMode() const override;
	virtual ECoordSystem GetWidgetCoordinateSystem() const override;
	virtual void DoTranslation(FVector& InTranslation) override;
	virtual void DoRotation(FRotator& InRotation) override;
	virtual void DoScale(FVector& InScale) override;

	/** FEdMode interface */
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData) override;
	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override;
	virtual bool ShouldDrawWidget() const override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;


protected:
	void OnExternalNodePropertyChange(FPropertyChangedEvent& InPropertyEvent);
	FDelegateHandle NodePropertyDelegateHandle;

private:

	/** Render each collisions */
	void RenderSphericalLimits(FPrimitiveDrawInterface* PDI);
	void RenderCapsuleLimit(FPrimitiveDrawInterface* PDI);
	void RenderPlanerLimit(FPrimitiveDrawInterface* PDI);

	/** Helper funciton for GetWidgetLocation() and joint rendering */
	FVector GetWidgetLocation(ECollisionLimitType CollisionType, int Index) const;

	// methods to find a valid widget mode for gizmo because doesn't need to show gizmo when the mode is "Ignore"
	FWidget::EWidgetMode FindValidWidgetMode(FWidget::EWidgetMode InWidgetMode) const;

	/** Checking if a collision is selected and the collision is valid */
	bool IsValidSelectCollision() const;

	FCollisionLimitBase* GetSelectCollisionLimitRuntime() const;
	FCollisionLimitBase* GetSelectCollisionLimitGraph() const;

	/** Draw text func for DrawHUD */
	void DrawTextItem(FText Text, FCanvas* Canvas, float X, float& Y, float FontHeight);
	void Draw3DTextItem(FText Text, FCanvas* Canvas, const FSceneView* View, FViewport* Viewport, FVector Location);

private:
	/** Cache the typed nodes */
	struct FAnimNode_MiragePhysics* RuntimeNode;
	UAnimGraphNode_MiragePhysics* GraphNode;

	/** The current bone selection mode */
	ECollisionLimitType SelectCollisionType = ECollisionLimitType::None;
	int SelectCollisionIndex = -1;
	bool SelectCollisionIsFromDataAsset;

	// storing current widget mode 
	mutable FWidget::EWidgetMode CurWidgetMode;
};