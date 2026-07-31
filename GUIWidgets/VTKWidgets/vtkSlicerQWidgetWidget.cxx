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
#include <QHoverEvent>
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
bool vtkSlicerQWidgetWidget::MouseInteractionEnabled = false;

//------------------------------------------------------------------------------
void vtkSlicerQWidgetWidget::SetMouseInteractionEnabled(bool enabled)
{
  vtkSlicerQWidgetWidget::MouseInteractionEnabled = enabled;
}

//------------------------------------------------------------------------------
bool vtkSlicerQWidgetWidget::GetMouseInteractionEnabled()
{
  return vtkSlicerQWidgetWidget::MouseInteractionEnabled;
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
bool vtkSlicerQWidgetWidget::IsPointingDeviceEvent(vtkMRMLInteractionEventData* eventData)
{
  if (!vtkSlicerQWidgetWidget::IsVirtualRealityEvent(eventData))
  {
    // Desktop mouse events carry no device; the cursor is the pointer.
    return true;
  }

  // Only the right controller, because it is the only one that can actually do anything: the trigger
  // that becomes Pick3DEvent, and so every click and drag, is bound to the right hand alone (see
  // vtkVirtualRealityViewOpenXRInteractorStyle::ProcessControllerEvents()). Highlighting a control
  // under the left hand would promise an interaction that hand cannot perform, and -- since both
  // controllers report a pose every frame into this same object -- the two hands would trade the
  // highlight back and forth whenever both happened to be aimed at the panel.
  //
  // If the left trigger is ever mapped as well, this is the single place to widen, and the
  // device-specific handling around HoverDevice becomes load-bearing again at that point.
  //
  // The headset is excluded for a related but distinct reason: it also reports its pose as a
  // Move3DEvent every frame, with a world position and direction just like a controller's -- see
  // the "Handle head movement" block in vtkOpenXRRenderWindowInteractor::ProcessXrEvents(), where
  // VTK notes it is a carry-over kept for the interactor style's "grounded" movement. It is a head
  // pose broadcast, not something the user points with, so treating it as one made panels react to
  // merely being looked at. Generic trackers are excluded likewise.
  return eventData->GetDevice() == vtkEventDataDevice::RightController;
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

  // Only things the user actually points with may interact; see IsPointingDeviceEvent(). Checked
  // ahead of the in-flight drag fast path below because, unlike the opt-outs after it, this is not
  // a preference that can be switched off mid-drag -- a device that cannot start an interaction
  // has none in progress to finish either.
  if (!vtkSlicerQWidgetWidget::IsPointingDeviceEvent(eventData))
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

  // Both of the opt-outs below are checked *after* the in-flight drag fast path above, on purpose:
  // whichever way an interaction is switched off, a drag that is already under way must still be
  // allowed to deliver its remaining moves and its release. Rejecting those instead would leave
  // this widget stuck in WidgetStateActive and the embedded QWidget's scene believing its mouse
  // button is still held down.

  // A locked GUI widget is not interactive, the same as any other locked markup.
  vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
  if (!markupsNode || markupsNode->GetLocked())
  {
    return false;
  }

  // Desktop mouse interaction is opt-in; VR controller interaction is always on. See
  // SetMouseInteractionEnabled().
  if (!vtkSlicerQWidgetWidget::MouseInteractionEnabled && !vtkSlicerQWidgetWidget::IsVirtualRealityEvent(eventData))
  {
    return false;
  }

  double rayOrigin[3] = { 0.0 };
  double rayDirection[3] = { 0.0 };
  if (!this->GetInteractionRay(eventData, rayOrigin, rayDirection))
  {
    // The ray is gone, so any hover based on it is stale.
    this->EndHover();
    return false;
  }

  // A move while no interaction is under way is a hover: claim it if it lands on the panel, so
  // ProcessInteractionEvent() can let the embedded QWidget highlight whatever is under the ray.
  //
  // These arrive every frame from both controllers, and on every mouse motion across the view, so
  // hit-testing them was not affordable until the hit tests became closed-form intersections
  // (vtkSlicerQWidgetRepresentation::ComputeInteractionPixelPosition()); they no longer allocate
  // or build a locator.
  //
  // Ending the hover here, on a miss, is deliberate despite this being a query method: this is the
  // one place that reliably observes the ray leaving the panel. The displayable manager calls
  // CanProcessInteractionEvent() on every widget for every event, whereas it only reaches
  // ProcessInteractionEvent() on the widget that claimed the event -- which by definition is no
  // longer this one once the ray has moved off.
  if (interactionEventType == InteractionEventMove)
  {
    QPointF hoverPixelPosition;
    if (rep->ComputeInteractionPixelPosition(rayOrigin, rayDirection, hoverPixelPosition, distance2))
    {
      // Any pointing device that is on the panel may take hover over; UpdateHover() records which
      // one did. Deliberately not reserved to whichever device got there first: a device stops
      // reporting when it goes idle or loses tracking, so reserving it would leave the other
      // controller -- or the mouse -- unable to hover at all, with nothing to release the
      // reservation.
      return true;
    }

    // A miss, on the other hand, only ends the hover if it comes from the device that established
    // it. Both controllers report every frame, so treating either one's miss as authoritative
    // would cancel the hover of whichever hand is actually pointing at the panel -- the same
    // reasoning as ActiveDevice for drags, and what made hover in VR work only intermittently.
    if (!this->Hovering || this->HoverDevice == eventData->GetDevice())
    {
      this->EndHover();
    }
    return false;
  }

  // Beyond hover, only a press starts an interaction.
  if (interactionEventType != InteractionEventPress)
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

    // The press gives the scene a mouse grabber, which suppresses hover dispatch until release
    // (see QGraphicsScene::mouseMoveEvent); drop the hover state so it is re-established from
    // scratch afterwards rather than being assumed to have survived the drag. Not via EndHover(),
    // which would send a hover-leave and un-highlight the control at the instant it is pressed.
    this->ForgetHover();

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
    // Idle: the only thing a move means here is hover (see CanProcessInteractionEvent()).
    if (interactionEventType == InteractionEventMove)
    {
      return this->UpdateHover(eventData->GetDevice(), rayValid, rayOrigin, rayDirection);
    }
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
bool vtkSlicerQWidgetWidget::UpdateHover(
  vtkEventDataDevice device, bool rayValid, const double rayOrigin[3], const double rayDirection[3])
{
  vtkSlicerQWidgetRepresentation* rep = this->GetQWidgetRepresentation();
  QGraphicsScene* scene = rep ? rep->GetQWidgetTexture()->GetScene() : nullptr;
  if (!scene)
  {
    return false;
  }

  // The QGraphicsScene is shared by every view showing this widget (that is the whole point of
  // vtkSlicerQWidgetImageSource), while this widget object exists once per view. So when a panel
  // is shown in both the VR view and a desktop 3D view, two instances feed one scene. If one of
  // them is mid-click, the scene has a mouse grabber, and QGraphicsScene::mouseMoveEvent() then
  // skips hover dispatch entirely and forwards the move to that grabber instead -- so a hover
  // update from the *other* view arrives at the pressed widget as the cursor wandering off, and
  // cancels the click. Leave the scene alone whenever anything holds the grab.
  if (scene->mouseGrabberItem())
  {
    return false;
  }

  QPointF pixelPosition;
  double distance2 = 0.0;
  if (!rayValid || !rep->ComputeInteractionPixelPosition(rayOrigin, rayDirection, pixelPosition, distance2))
  {
    this->EndHover();
    return false;
  }

  this->HoverDevice = device;

  // A move carrying no buttons, while nothing holds the scene's mouse grab, is what
  // QGraphicsScene::mouseMoveEvent() turns into a QGraphicsSceneHoverEvent and dispatches to the
  // item under the cursor -- which is what makes a button under the ray highlight, a slider show
  // its hover state, and so on. Sending a *pressed* move here instead would be delivered to the
  // mouse grabber as a drag, which is the opposite of what is wanted.
  QGraphicsSceneMouseEvent hoverEvent(QEvent::GraphicsSceneMouseMove);
  hoverEvent.setScenePos(pixelPosition);
  hoverEvent.setButton(Qt::NoButton);
  hoverEvent.setButtons(Qt::NoButton);
  QApplication::sendEvent(scene, &hoverEvent);

  // Qt's own hover bookkeeping cannot be relied on here, so state explicitly which control should
  // be lit rather than leaving Qt to work the transition out. See ClearStaleHoverStates().
  QWidget* panel = rep->GetQWidgetTexture()->GetWidget();
  QWidget* under = panel ? panel->childAt(pixelPosition.toPoint()) : nullptr;
  if (under != this->LastHoverChild)
  {
    this->LastHoverChild = under;
    vtkSlicerQWidgetWidget::ClearStaleHoverStates(panel, under);
  }

  this->Hovering = true;
  return true;
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetWidget::ClearStaleHoverStates(QWidget* panel, QWidget* hoveredWidget)
{
  if (!panel)
  {
    return;
  }

  // Un-hover every control that Qt still believes the pointer is on, other than the one it is
  // actually on and that control's ancestors (a child being hovered implies its parents are too).
  //
  // This exists because Qt's hover tracking desynchronizes when it is driven by synthesized events
  // rather than a real cursor, and -- worse -- cannot then recover on its own. Once
  // QGraphicsScene stops considering the proxy item hovered, a hover-leave sent to the scene finds
  // no item to deliver to, QGraphicsProxyWidget::hoverLeaveEvent() never runs, and the child it
  // still has recorded stays highlighted permanently. Observed as two controls lit at once with
  // the pointer on neither, most often after moving quickly across a panel.
  //
  // WA_UnderMouse is what QStyle reads as State_MouseOver, so clearing it is what actually
  // un-highlights the control; the accompanying events are sent so widgets that track hover
  // themselves (rather than through the style) stay consistent too.
  const QList<QWidget*> descendants = panel->findChildren<QWidget*>();
  for (QWidget* descendant : descendants)
  {
    if (!descendant->underMouse())
    {
      continue;
    }
    const bool shouldBeHovered =
      hoveredWidget && (descendant == hoveredWidget || descendant->isAncestorOf(hoveredWidget));
    if (shouldBeHovered)
    {
      continue;
    }

    descendant->setAttribute(Qt::WA_UnderMouse, false);
    QHoverEvent hoverLeaveEvent(QEvent::HoverLeave, QPointF(-1.0, -1.0), QPointF(-1.0, -1.0));
    QApplication::sendEvent(descendant, &hoverLeaveEvent);
    QEvent leaveEvent(QEvent::Leave);
    QApplication::sendEvent(descendant, &leaveEvent);
    descendant->update();
  }
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetWidget::ForgetHover()
{
  // The three together are one piece of state: whether a hover is in effect, whose it is, and what
  // it last landed on. Clearing only some of them lets them drift -- in particular, keeping
  // LastHoverChild while Hovering goes false makes the next hover onto that same control look like
  // "no change" and skip the reconciliation that would have cleaned up after it.
  this->Hovering = false;
  this->HoverDevice = vtkEventDataDevice::Unknown;
  this->LastHoverChild = nullptr;
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetWidget::EndHover()
{
  if (!this->Hovering)
  {
    return;
  }
  this->ForgetHover();

  vtkSlicerQWidgetRepresentation* rep = this->GetQWidgetRepresentation();
  QGraphicsScene* scene = rep ? rep->GetQWidgetTexture()->GetScene() : nullptr;
  if (!scene)
  {
    return;
  }

  // Never touch a scene that something is mid-click on; see the same guard in UpdateHover() for
  // why. This is the path that actually broke desktop clicks while VR was active.
  if (scene->mouseGrabberItem())
  {
    return;
  }

  // One last button-less move at a position outside the panel: QGraphicsScene dispatches
  // hover-leave to whatever was hovered once the cursor is no longer over it. Without this, simply
  // ceasing to send events would leave the last control the ray crossed highlighted for good.
  QGraphicsSceneMouseEvent hoverLeaveEvent(QEvent::GraphicsSceneMouseMove);
  hoverLeaveEvent.setScenePos(QPointF(-1.0, -1.0));
  hoverLeaveEvent.setButton(Qt::NoButton);
  hoverLeaveEvent.setButtons(Qt::NoButton);
  QApplication::sendEvent(scene, &hoverLeaveEvent);

  // Then make sure nothing is left highlighted, whether or not that leave reached anything -- once
  // Qt has lost track of the proxy item it cannot. Telling the scene first still matters: it keeps
  // QGraphicsProxyWidget's own record of the widget under the pointer from going stale, which is
  // what makes the desync ClearStaleHoverStates() exists to repair less likely in the first place.
  vtkSlicerQWidgetWidget::ClearStaleHoverStates(rep->GetQWidgetTexture()->GetWidget(), nullptr);
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetWidget::Leave(vtkMRMLInteractionEventData* eventData)
{
  // Only end the hover when this concerns the device actually holding it.
  //
  // Leave() is not the reliable "the pointer moved off this widget" signal it looks like. The
  // displayable manager also calls it whenever focus passes to another manager
  // (vtkMRMLMarkupsDisplayableManager::SetHasFocus()), and with two controllers every single frame
  // contains an event from the hand that is not pointing at the panel -- which this widget rightly
  // declines, which hands focus elsewhere, which calls Leave(). Ending the hover unconditionally
  // here therefore cancelled, every frame, the hover the *other* hand was holding, so hover
  // survived only for whichever controller's events happened to be dispatched last.
  //
  // A Leave() carrying no event data is a genuine teardown (the widget is going away), so that one
  // always ends the hover.
  if (!eventData || !this->Hovering || eventData->GetDevice() == this->HoverDevice)
  {
    this->EndHover();
  }
  this->Superclass::Leave(eventData);
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
