/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Csaba Pinter, EBATINCA, S.L., and
  development was supported by "ICEX Espana Exportacion e Inversiones" under
  the program "Inversiones de Empresas Extranjeras en Actividades de I+D
  (Fondo Tecnologico)- Convocatoria 2021"

==============================================================================*/

#include "vtkSlicerQWidgetWidget.h"

#include "vtkMRMLGUIWidgetNode.h"

#include "vtkSlicerQWidgetRepresentation.h"
#include "vtkSlicerQWidgetTexture.h"

// MRML includes
#include "vtkMRMLInteractionEventData.h"
#include "vtkMRMLLinearTransformNode.h"
#include "vtkMRMLMarkupsNode.h"
#include "vtkMRMLSliceNode.h"

// Qt includes
#include <QApplication>
#include <QEvent>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QWidget>

// VTK includes
#include "vtkCallbackCommand.h"
#include "vtkCamera.h"
#include "vtkCommand.h"
#include "vtkEvent.h"
#include "vtkEventData.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkWidgetCallbackMapper.h"
#include "vtkWidgetEvent.h"
#include "vtkWidgetEventTranslator.h"

vtkStandardNewMacro(vtkSlicerQWidgetWidget);

//------------------------------------------------------------------------------
vtkSlicerQWidgetWidget::vtkSlicerQWidgetWidget()
{
}

//------------------------------------------------------------------------------
vtkSlicerQWidgetWidget::~vtkSlicerQWidgetWidget() = default;

//------------------------------------------------------------------------------
vtkSlicerQWidgetRepresentation* vtkSlicerQWidgetWidget::GetQWidgetRepresentation()
{
  return vtkSlicerQWidgetRepresentation::SafeDownCast(this->WidgetRep);
}

//------------------------------------------------------------------------------
vtkSlicerMarkupsWidget* vtkSlicerQWidgetWidget::CreateInstance()const
{
  vtkObject* ret = vtkObjectFactory::CreateInstance("vtkSlicerQWidgetWidget");
  if(ret)
  {
    return static_cast<vtkSlicerQWidgetWidget*>(ret);
  }

  vtkSlicerQWidgetWidget* result = new vtkSlicerQWidgetWidget;
#ifdef VTK_HAS_INITIALIZE_OBJECT_BASE
  result->InitializeObjectBase();
#endif
  return result;
}

//----------------------------------------------------------------------
void vtkSlicerQWidgetWidget::CreateDefaultRepresentation(
  vtkMRMLMarkupsDisplayNode* markupsDisplayNode, vtkMRMLAbstractViewNode* viewNode, vtkRenderer* renderer)
{
  //if (!viewNode->IsA("vtkMRMLVirtualRealityViewNode"))
  if (vtkMRMLSliceNode::SafeDownCast(viewNode))
  {
    // There is no 2D representation of the GUI widget
    return;
  }

  vtkNew<vtkSlicerQWidgetRepresentation> rep;
  this->SetRenderer(renderer);
  this->SetRepresentation(rep);
  rep->SetMarkupsDisplayNode(markupsDisplayNode);
  rep->SetViewNode(viewNode);

  rep->UpdateFromMRML(nullptr, 0); // full update
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetWidget::SetRepresentation(vtkMRMLAbstractWidgetRepresentation* rep)
{
  this->Superclass::SetRepresentation(rep);

  // rep may legitimately be nullptr (e.g. when the displayable manager tears down the widget on
  // hide/delete), so only report an error if a representation of the wrong type was given.
  if (rep && !vtkSlicerQWidgetRepresentation::SafeDownCast(rep))
  {
    vtkErrorMacro("SetRepresentation: Given representation is not a vtkSlicerQWidgetRepresentation");
  }
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetWidget::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//------------------------------------------------------------------------------
bool vtkSlicerQWidgetWidget::IsVirtualRealityEvent(vtkMRMLInteractionEventData* eventData)
{
  if (!eventData)
  {
    return false;
  }
  return eventData->GetType() == vtkCommand::Pick3DEvent || eventData->GetType() == vtkCommand::Move3DEvent;
}

//------------------------------------------------------------------------------
vtkSlicerQWidgetWidget::InteractionEventType vtkSlicerQWidgetWidget::GetInteractionEventType(
  vtkMRMLInteractionEventData* eventData)
{
  if (!eventData)
  {
    return InteractionEventNone;
  }

  switch (eventData->GetType())
  {
    case vtkCommand::Pick3DEvent:
    {
      // VR delivers press and release as the same event id, told apart by the action.
      // vtkMRMLInteractionEventData derives from vtkEventDataDevice3D, so this cast always
      // succeeds; it is the action, not the cast, that carries the information here.
      vtkEventDataDevice3D* deviceEventData = eventData->GetAsEventDataDevice3D();
      if (!deviceEventData)
      {
        return InteractionEventNone;
      }
      if (deviceEventData->GetAction() == vtkEventDataAction::Press)
      {
        return InteractionEventPress;
      }
      if (deviceEventData->GetAction() == vtkEventDataAction::Release)
      {
        return InteractionEventRelease;
      }
      return InteractionEventNone;
    }
    case vtkCommand::Move3DEvent:
      return InteractionEventMove;

    // Desktop mouse in a 3D view. Note this is the plain press/move/release triple rather than
    // vtkMRMLInteractionEventData::LeftButtonClickEvent: a click event only arrives once the
    // button is released without having moved, which would rule out dragging a slider.
    case vtkCommand::LeftButtonPressEvent:
      return InteractionEventPress;
    case vtkCommand::MouseMoveEvent:
      return InteractionEventMove;
    case vtkCommand::LeftButtonReleaseEvent:
      return InteractionEventRelease;

    default:
      return InteractionEventNone;
  }
}

//------------------------------------------------------------------------------
bool vtkSlicerQWidgetWidget::GetInteractionRay(
  vtkMRMLInteractionEventData* eventData, double rayOrigin[3], double rayDirection[3])
{
  if (!eventData)
  {
    return false;
  }

  // VR controller events carry the ray on the event data already.
  if (vtkSlicerQWidgetWidget::IsVirtualRealityEvent(eventData))
  {
    if (!eventData->IsWorldPositionValid())
    {
      return false;
    }
    eventData->GetWorldPosition(rayOrigin);
    const double* worldDirection = eventData->GetWorldDirection();
    if (!worldDirection)
    {
      return false;
    }
    rayDirection[0] = worldDirection[0];
    rayDirection[1] = worldDirection[1];
    rayDirection[2] = worldDirection[2];
    return vtkMath::Normalize(rayDirection) > 1e-6;
  }

  // Mouse events carry only a display position, so unproject it at the near and far clipping
  // planes and use the segment between them as the ray (see this method's doc comment for why the
  // event's own world position/direction cannot be used).
  //
  // The widget's own renderer is used rather than eventData->GetRenderer(): each view gets its own
  // widget instance from the displayable manager, so this is by construction the renderer whose
  // camera the panel is being viewed through, and it is what UpdateMoveHandleDrag() already uses.
  vtkRenderer* renderer = this->GetRenderer();
  if (!renderer || !eventData->IsDisplayPositionValid())
  {
    return false;
  }
  const int* displayPosition = eventData->GetDisplayPosition();

  double nearPoint[4] = { 0.0, 0.0, 0.0, 1.0 };
  renderer->SetDisplayPoint(displayPosition[0], displayPosition[1], 0.0);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(nearPoint);

  double farPoint[4] = { 0.0, 0.0, 0.0, 1.0 };
  renderer->SetDisplayPoint(displayPosition[0], displayPosition[1], 1.0);
  renderer->DisplayToWorld();
  renderer->GetWorldPoint(farPoint);

  if (nearPoint[3] == 0.0 || farPoint[3] == 0.0)
  {
    return false;
  }
  for (int i = 0; i < 3; ++i)
  {
    nearPoint[i] /= nearPoint[3];
    farPoint[i] /= farPoint[3];
  }

  rayOrigin[0] = nearPoint[0];
  rayOrigin[1] = nearPoint[1];
  rayOrigin[2] = nearPoint[2];
  vtkMath::Subtract(farPoint, nearPoint, rayDirection);
  return vtkMath::Normalize(rayDirection) > 1e-6;
}

//------------------------------------------------------------------------------
bool vtkSlicerQWidgetWidget::CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& distance2)
{
  vtkSlicerQWidgetRepresentation* rep = this->GetQWidgetRepresentation();
  if (!rep || !eventData)
  {
    return false;
  }

  const InteractionEventType interactionEventType = vtkSlicerQWidgetWidget::GetInteractionEventType(eventData);
  if (interactionEventType == InteractionEventNone)
  {
    return false;
  }

  // Once a press has been claimed, keep this widget locked onto the drag (moves for continued
  // dragging, and the matching release) regardless of where the ray points by the time those
  // events arrive -- otherwise a fast-moving ray could drift off the plane mid-drag and orphan the
  // release, leaving the embedded widget's scene thinking the mouse button is still held down.
  // Mirrors vtkSlicerPlaneWidget's WidgetStateTranslatePlane pattern. Restricted to the device
  // that actually started the drag (see ActiveDevice doc comment): both controllers independently
  // fire Move3DEvent every frame, and claiming it regardless of device would make the drag
  // alternate between both controllers' rays instead of following one.
  if (this->WidgetState == WidgetStateActive || this->WidgetState == WidgetStateMovingHandle)
  {
    if (eventData->GetDevice() != this->ActiveDevice)
    {
      return false;
    }
    distance2 = 0.0;
    return true;
  }

  // A locked GUI widget is not interactive, the same as any other locked markup.
  //
  // Checked *after* the in-flight drag fast path above, on purpose: a drag that is already under
  // way must still be allowed to deliver its remaining moves and its release. Rejecting those
  // instead would leave this widget stuck in WidgetStateActive and the embedded QWidget's scene
  // believing its mouse button is still held down.
  vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
  if (!markupsNode || markupsNode->GetLocked())
  {
    return false;
  }

  // Only a press starts an interaction. Moves while idle are deliberately not hit-tested: they
  // arrive every frame from both controllers (and on every mouse motion over the view), and the
  // hit tests currently rebuild a cell locator per call.
  if (interactionEventType != InteractionEventPress)
  {
    return false;
  }

  double rayOrigin[3] = { 0.0 };
  double rayDirection[3] = { 0.0 };
  if (!this->GetInteractionRay(eventData, rayOrigin, rayDirection))
  {
    return false;
  }

  // The move handle takes priority over the panel's own clickable plane -- in practice the two
  // never overlap (addMoveHandle() offsets the handle below the panel specifically to avoid this
  // ambiguity), so this is only ever a real choice between "hit one" and "hit neither".
  double worldHitPoint[3] = { 0.0 };
  if (rep->ComputeMoveHandleHit(rayOrigin, rayDirection, worldHitPoint, distance2))
  {
    return true;
  }

  QPointF pixelPosition;
  return rep->ComputeInteractionPixelPosition(rayOrigin, rayDirection, pixelPosition, distance2);
}

//------------------------------------------------------------------------------
bool vtkSlicerQWidgetWidget::ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData)
{
  vtkSlicerQWidgetRepresentation* rep = this->GetQWidgetRepresentation();
  if (!rep || !eventData)
  {
    return false;
  }

  const InteractionEventType interactionEventType = vtkSlicerQWidgetWidget::GetInteractionEventType(eventData);
  if (interactionEventType == InteractionEventNone)
  {
    return false;
  }

  double rayOrigin[3] = { 0.0 };
  double rayDirection[3] = { 0.0 };
  const bool rayValid = this->GetInteractionRay(eventData, rayOrigin, rayDirection);

  if (interactionEventType == InteractionEventPress)
  {
    if (!rayValid)
    {
      return false;
    }

    // The move handle takes priority over the panel's own clickable plane, mirroring
    // CanProcessInteractionEvent()'s same priority.
    double worldHitPoint[3] = { 0.0 };
    double distance2 = 0.0;
    if (rep->ComputeMoveHandleHit(rayOrigin, rayDirection, worldHitPoint, distance2))
    {
      this->StartMoveHandleDrag(worldHitPoint, rayOrigin);
      this->ActiveDevice = eventData->GetDevice();
      this->SetWidgetState(WidgetStateMovingHandle);
      return true;
    }

    QGraphicsScene* scene = rep->GetQWidgetTexture()->GetScene();
    if (!scene)
    {
      return false;
    }
    if (!rep->ComputeInteractionPixelPosition(rayOrigin, rayDirection, this->LastWidgetCoordinates, distance2))
    {
      return false;
    }

    this->ActiveDevice = eventData->GetDevice();

    QGraphicsSceneMouseEvent pressEvent(QEvent::GraphicsSceneMousePress);
    pressEvent.setScenePos(this->LastWidgetCoordinates);
    pressEvent.setButton(Qt::LeftButton);
    pressEvent.setButtons(Qt::LeftButton);
    QApplication::sendEvent(scene, &pressEvent);

    this->SetWidgetState(WidgetStateActive);
    return true;
  }

  if (this->WidgetState == WidgetStateMovingHandle)
  {
    if (interactionEventType == InteractionEventMove)
    {
      if (rayValid)
      {
        this->UpdateMoveHandleDrag(rayOrigin, rayDirection);
      }
      return true;
    }
    if (interactionEventType == InteractionEventRelease)
    {
      this->SetWidgetState(WidgetStateIdle);
      this->ActiveDevice = vtkEventDataDevice::Unknown;
      this->MoveHandleDragTransformNode = nullptr;
      return true;
    }
    return false;
  }

  if (this->WidgetState != WidgetStateActive)
  {
    return false;
  }

  QGraphicsScene* scene = rep->GetQWidgetTexture()->GetScene();

  if (interactionEventType == InteractionEventMove)
  {
    if (!scene)
    {
      return false;
    }
    // Keep sending moves at the last known pixel position if the ray has drifted off the plane
    // (ComputeInteractionPixelPosition() leaves LastWidgetCoordinates untouched on a miss) --
    // QGraphicsScene's implicit mouse grab from the press still expects updates for whatever item
    // captured it (e.g. a slider handle).
    if (rayValid)
    {
      double distance2 = 0.0;
      rep->ComputeInteractionPixelPosition(rayOrigin, rayDirection, this->LastWidgetCoordinates, distance2);
    }

    QGraphicsSceneMouseEvent moveEvent(QEvent::GraphicsSceneMouseMove);
    moveEvent.setScenePos(this->LastWidgetCoordinates);
    moveEvent.setButton(Qt::NoButton);
    moveEvent.setButtons(Qt::LeftButton);
    QApplication::sendEvent(scene, &moveEvent);
    return true;
  }

  if (interactionEventType == InteractionEventRelease)
  {
    // The state is reset even if the scene has gone away in the meantime: leaving the widget in
    // WidgetStateActive would make CanProcessInteractionEvent()'s fast path above claim every
    // subsequent event from this device forever.
    if (scene)
    {
      QGraphicsSceneMouseEvent releaseEvent(QEvent::GraphicsSceneMouseRelease);
      releaseEvent.setScenePos(this->LastWidgetCoordinates);
      releaseEvent.setButton(Qt::LeftButton);
      QApplication::sendEvent(scene, &releaseEvent);
    }

    this->SetWidgetState(WidgetStateIdle);
    this->ActiveDevice = vtkEventDataDevice::Unknown;
    return true;
  }

  return false;
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetWidget::StartMoveHandleDrag(const double worldGrabPoint[3], const double rayOrigin[3])
{
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(this->GetMarkupsNode());
  if (!widgetNode)
  {
    return;
  }
  // The widget's own parent transform link, not a name-derived scene lookup -- see the comment in
  // qSlicerGUIWidgetsModuleWidget::addMoveHandle() on why a name-derived lookup resolves to the
  // wrong transform whenever more than one widget of the same demo type exists.
  vtkMRMLLinearTransformNode* moveTransformNode = vtkMRMLLinearTransformNode::SafeDownCast(widgetNode->GetParentTransformNode());
  if (!moveTransformNode)
  {
    return;
  }

  this->MoveHandleDragTransformNode = moveTransformNode;

  double originToGrabPoint[3] = { 0.0 };
  vtkMath::Subtract(worldGrabPoint, rayOrigin, originToGrabPoint);
  this->MoveHandleDragDistance = vtkMath::Norm(originToGrabPoint);

  // moveTransformNode has no parent transform of its own (it is a top-level node, see
  // qSlicerGUIWidgetsModuleWidget::addMoveHandle()), so ToWorld == ToParent here and in
  // UpdateMoveHandleDrag().
  vtkNew<vtkMatrix4x4> currentMoveTransformToWorld;
  moveTransformNode->GetMatrixTransformToWorld(currentMoveTransformToWorld);

  // The drag pivots around the exact point grabbed on the handle (not the handle's center): store
  // it in the move transform's local frame, so UpdateMoveHandleDrag() can keep that same material
  // point on the ray. Pivoting around e.g. the handle's center instead would make the panel jump
  // sideways at grab start whenever the grab point lands away from the center of the bar.
  vtkNew<vtkMatrix4x4> worldToMoveTransform;
  vtkMatrix4x4::Invert(currentMoveTransformToWorld, worldToMoveTransform);
  double worldGrabPoint_h[4] = { worldGrabPoint[0], worldGrabPoint[1], worldGrabPoint[2], 1.0 };
  double localGrabPoint_h[4] = { 0.0 };
  worldToMoveTransform->MultiplyPoint(worldGrabPoint_h, localGrabPoint_h);
  this->MoveHandleLocalGrabPoint[0] = localGrabPoint_h[0];
  this->MoveHandleLocalGrabPoint[1] = localGrabPoint_h[1];
  this->MoveHandleLocalGrabPoint[2] = localGrabPoint_h[2];

  // Seed the drag's rotation from the transform's current one; UpdateMoveHandleDrag() re-aims it
  // toward the camera every tick (and keeps this seed whenever it cannot).
  for (int row = 0; row < 3; ++row)
  {
    for (int col = 0; col < 3; ++col)
    {
      this->MoveHandleDragRotation[row][col] = currentMoveTransformToWorld->GetElement(row, col);
    }
  }
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetWidget::UpdateMoveHandleDrag(const double rayOrigin[3], const double rayDirection[3])
{
  if (!this->MoveHandleDragTransformNode)
  {
    // The transform node was removed from the scene mid-drag.
    this->SetWidgetState(WidgetStateIdle);
    this->ActiveDevice = vtkEventDataDevice::Unknown;
    return;
  }

  double desiredWorldGrabPoint[3] = {
    rayOrigin[0] + rayDirection[0] * this->MoveHandleDragDistance,
    rayOrigin[1] + rayDirection[1] * this->MoveHandleDragDistance,
    rayOrigin[2] + rayDirection[2] * this->MoveHandleDragDistance
  };

  // Re-aim the panel so it keeps facing the user wherever it is dragged, while always remaining
  // upright: its up axis (local +Z; see vtkSlicerQWidgetRepresentation::PlaceWidget(): the plane
  // lies in the local XZ plane, +Z up) is pinned to the render window's physical view-up direction
  // (world coordinates -- NOT a world-axis constant, since complex-gesture world rotations change
  // what direction "up in the room" is in world), and the plane's front normal (local +Y) is
  // yawed about that axis toward the render window's active camera, which the VR render loop
  // keeps at the current headset pose every frame -- a cylindrical billboard, so the panel never
  // rolls or tilts. If the renderer/camera/render window is unavailable, or the aim is degenerate
  // (camera straight above/below the handle), the previous tick's rotation is kept for this tick.
  vtkRenderer* renderer = this->GetRenderer();
  vtkCamera* camera = renderer ? renderer->GetActiveCamera() : nullptr;
  vtkRenderWindow* renderWindow = renderer ? renderer->GetRenderWindow() : nullptr;
  if (camera && renderWindow)
  {
    double zAxis[3] = { 0.0 };
    renderWindow->GetPhysicalViewUp(zAxis);
    if (vtkMath::Normalize(zAxis) > 1e-3)
    {
      double cameraPosition[3] = { 0.0 };
      camera->GetPosition(cameraPosition);

      // Facing direction toward the camera, projected onto the horizontal plane (perpendicular to
      // the up axis) so only the yaw tracks it.
      double yAxis[3] = { 0.0 };
      vtkMath::Subtract(cameraPosition, desiredWorldGrabPoint, yAxis);
      double facingDotUp = vtkMath::Dot(yAxis, zAxis);
      for (int i = 0; i < 3; ++i)
      {
        yAxis[i] -= facingDotUp * zAxis[i];
      }
      if (vtkMath::Normalize(yAxis) > 1e-3)
      {
        // x = y x z completes the right-handed frame (columns then satisfy x x y = z).
        double xAxis[3] = { 0.0 };
        vtkMath::Cross(yAxis, zAxis, xAxis);
        for (int row = 0; row < 3; ++row)
        {
          this->MoveHandleDragRotation[row][0] = xAxis[row];
          this->MoveHandleDragRotation[row][1] = yAxis[row];
          this->MoveHandleDragRotation[row][2] = zAxis[row];
        }
      }
    }
  }

  // Solve for the translation that places the grab point (fixed in the move transform's local
  // frame, see StartMoveHandleDrag()) at desiredWorldGrabPoint, given the rotation R chosen above:
  //   R * localGrabPoint + T = desired  =>  T = desired - R * localGrabPoint.
  double rotatedLocalGrabPoint[3] = { 0.0 };
  for (int row = 0; row < 3; ++row)
  {
    rotatedLocalGrabPoint[row] =
      this->MoveHandleDragRotation[row][0] * this->MoveHandleLocalGrabPoint[0] +
      this->MoveHandleDragRotation[row][1] * this->MoveHandleLocalGrabPoint[1] +
      this->MoveHandleDragRotation[row][2] * this->MoveHandleLocalGrabPoint[2];
  }

  vtkNew<vtkMatrix4x4> newMoveTransformToWorld;
  newMoveTransformToWorld->Identity();
  for (int row = 0; row < 3; ++row)
  {
    for (int col = 0; col < 3; ++col)
    {
      newMoveTransformToWorld->SetElement(row, col, this->MoveHandleDragRotation[row][col]);
    }
    newMoveTransformToWorld->SetElement(row, 3, desiredWorldGrabPoint[row] - rotatedLocalGrabPoint[row]);
  }

  // See vtkVirtualRealityViewInteractorStyleDelegate::PositionProp()'s rationale for why
  // SetMatrixTransformToParent() is used here rather than fetching/SetMatrix()-ing the underlying
  // vtkTransform directly: it guarantees TransformModifiedEvent actually fires.
  this->MoveHandleDragTransformNode->SetMatrixTransformToParent(newMoveTransformToWorld);
}
