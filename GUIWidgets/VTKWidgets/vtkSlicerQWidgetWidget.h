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

/**
 * @class   vtkSlicerQWidgetWidget
 * @brief   3D VTK widget for a QWidget
 *
 * This 3D widget handles events between VTK and Qt for a QWidget placed in a scene, via
 * CanProcessInteractionEvent()/ProcessInteractionEvent(). It casts a ray for each incoming press,
 * and for each move, and if the ray intersects the widget's plane
 * (vtkSlicerQWidgetRepresentation::ComputeInteractionPixelPosition()), converts the press, the
 * moves that follow and the matching release into synthesized QGraphicsScene mouse events -- so a
 * press starts a click, a move while WidgetStateActive continues a drag (e.g. a slider), and the
 * release ends it.
 *
 * Moves arriving while nothing is being pressed are hover: they are forwarded as *button-less*
 * scene mouse events, which is the form QGraphicsScene turns into hover events, so the control
 * under the ray highlights before anything is clicked (UpdateHover()/EndHover()). Two aspects of
 * that are less obvious than they look and are documented where they are implemented: hover is
 * attributed to a single device at a time (HoverDevice), because every controller -- and the
 * headset -- reports a pose every frame into this same object; and which control is highlighted
 * is asserted outright rather than left to Qt to derive (ClearStaleHoverStates()).
 *
 * Both input paths run through the same state machine, differing only in how the event is
 * classified (GetInteractionEventType()) and how its ray is obtained (GetInteractionRay()):
 * - In VR, a 6dof Pick3DEvent press/release pair plus Move3DEvent, carrying the controller ray on
 *   the event data. See vtkVirtualRealityViewOpenXRInteractorStyle, which translates the right
 *   trigger into Pick3DEvent.
 * - In a desktop 3D view, LeftButtonPressEvent/MouseMoveEvent/LeftButtonReleaseEvent, whose ray is
 *   built by unprojecting the mouse's display position through the view's camera.
 *
 * The widget is therefore clickable wherever it is shown, through Slicer's normal interaction
 * event pipeline -- no external module needs to know this widget exists. A panel can be opted out
 * of interaction by locking its markups node, like any other markup.
 *
 * A press that instead hits the widget's move handle (vtkSlicerQWidgetRepresentation::
 * ComputeMoveHandleHit(); a separate companion Model node, see
 * qSlicerGUIWidgetsModuleWidget::addMoveHandle()) starts a "distance grab" of the whole panel
 * instead (WidgetStateMovingHandle): the panel is kept at the same distance from the ray origin,
 * along the ray's current direction, that it was at when the grab started, facing the render
 * window's active camera (standing in for the headset pose) while staying upright, pinned to the
 * render window's physical view-up. See StartMoveHandleDrag()/UpdateMoveHandleDrag().
 */

#ifndef vtkSlicerQWidgetWidget_h
#define vtkSlicerQWidgetWidget_h

#include "vtkSlicerGUIWidgetsModuleVTKWidgetsExport.h"

#include "vtkSlicerMarkupsWidget.h"

// VTK includes
#include <vtkEventData.h> // for vtkEventDataDevice
#include <vtkWeakPointer.h>

// Qt includes
#include <QPointF>
#include <QPointer> // for LastHoverChild

class QWidget;

class vtkMRMLInteractionEventData;
class vtkMRMLLinearTransformNode;
class vtkSlicerQWidgetRepresentation;

class VTK_SLICER_GUIWIDGETS_MODULE_VTKWIDGETS_EXPORT vtkSlicerQWidgetWidget : public vtkSlicerMarkupsWidget
{
  friend class vtkInteractionCallback;

public:
  /// Instantiate the object.
  static vtkSlicerQWidgetWidget* New();

  ///@{
  /// Standard vtkObject methods
  vtkTypeMacro(vtkSlicerQWidgetWidget, vtkSlicerMarkupsWidget);
  void PrintSelf(ostream& os, vtkIndent indent) override;
  ///@}

  /// Create instance of the markups widget
  vtkSlicerMarkupsWidget* CreateInstance() const override;

  /// Specify an instance of vtkSlicerQWidgetRepresentation used to represent this
  /// widget in the scene. Note that the representation is a subclass of vtkProp
  /// so it can be added to the renderer independent of the widget.
  void SetRepresentation(vtkMRMLAbstractWidgetRepresentation *r) override;

  // Description:
  // Disable/Enable the widget if needed.
  // Unobserved the camera if the widget is disabled.
  //void SetEnabled(int enabling) override;

  /// Return the representation as a vtkSlicerQWidgetRepresentation
  vtkSlicerQWidgetRepresentation* GetQWidgetRepresentation();

  /// Create the default widget representation and initializes the widget and representation.
  void CreateDefaultRepresentation(
    vtkMRMLMarkupsDisplayNode* markupsDisplayNode, vtkMRMLAbstractViewNode* viewNode, vtkRenderer* renderer) override;

  /// Returns true (claiming the event) if a press hits the widget's plane or its move handle, or
  /// if a drag started by such a press is still in progress (WidgetStateActive/
  /// WidgetStateMovingHandle), regardless of where the ray currently points -- see
  /// ProcessInteractionEvent() and the WidgetStateActive doc comment below for why the latter
  /// matters. distance2 is the squared world-space distance from the ray origin to the hit point,
  /// used by the displayable manager to pick the closest widget when more than one GUI widget
  /// could be hit.
  ///
  /// Returns false for a widget whose markups node is locked (vtkMRMLMarkupsNode::SetLocked(),
  /// inherited for free since GUI widget nodes are markups nodes, and saved with the scene), so a
  /// single panel can be opted out of interaction -- mouse and VR alike -- through the standard
  /// Markups mechanism rather than anything GUIWidgets-specific. Settable from the Markups
  /// module's lock/unlock button or from Python; it is not exposed in Subject Hierarchy.
  bool CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& distance2) override;

  /// Performs the click or the move-handle drag: on a press, checks the move handle first (see
  /// CanProcessInteractionEvent()), starting a drag (StartMoveHandleDrag(),
  /// WidgetStateMovingHandle) if it hits; otherwise, on a plane hit, synthesizes a QGraphicsScene
  /// mouse press at the corresponding pixel position and enters WidgetStateActive. While
  /// WidgetStateActive, a move synthesizes mouse-move events (falling back to the last known pixel
  /// position if the ray drifts off the plane, so a captured item like a slider handle keeps
  /// receiving updates), and the matching release synthesizes the mouse release. While
  /// WidgetStateMovingHandle, a move calls UpdateMoveHandleDrag() instead, and the matching
  /// release simply ends the drag. Either way, releases back to WidgetStateIdle.
  bool ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData) override;

  /// Reimplemented to end hover feedback (EndHover()) when this widget is deactivated, so the
  /// control last under the ray does not stay highlighted. Only acts on events from the device
  /// actually holding the hover, or on a teardown carrying no event data at all -- see the
  /// implementation for why this cannot be unconditional.
  void Leave(vtkMRMLInteractionEventData* eventData) override;

  ///@{
  /// Whether GUI widget panels respond to desktop mouse interaction in a 3D view, in addition to
  /// VR controller interaction (which is always enabled). Off by default: a panel that claims the
  /// left mouse button also stops that click from rotating the camera, so this is opt-in rather
  /// than something that silently changes how the 3D view behaves for any scene that happens to
  /// contain a GUI widget.
  ///
  /// Global rather than per-widget because it selects an *input method* for the module as a whole.
  /// To exclude one particular panel from interaction entirely -- mouse and VR alike -- lock its
  /// markups node instead; see CanProcessInteractionEvent().
  ///
  /// Static because these widgets are created and owned by the generic Markups displayable
  /// manager, one per node per view, leaving no accessible place to push a per-instance setting
  /// through. qSlicerGUIWidgetsModule owns the persisted value and pushes it here; see
  /// qSlicerGUIWidgetsModule::setMouseInteractionEnabled().
  static void SetMouseInteractionEnabled(bool enabled);
  static bool GetMouseInteractionEnabled();
  ///@}

protected:
  /// Whether this event came from a VR controller (Pick3DEvent/Move3DEvent, carrying a 6dof pose)
  /// rather than from the desktop mouse. Selects which ray source GetInteractionRay() uses, and
  /// which events GetMouseInteractionEnabled() gates.
  static bool IsVirtualRealityEvent(vtkMRMLInteractionEventData* eventData);

  /// Whether this event came from something the user both points and acts with: in VR the right
  /// controller only, since the trigger bound to Pick3DEvent -- and so every click and drag -- is
  /// on that hand alone; on the desktop, the mouse, which carries no device at all.
  ///
  /// Everything else reports a pose every frame without being able to act on it: the left
  /// controller, the headset (which would make panels react to being looked at), and generic
  /// trackers. See the implementation for why each is excluded.
  static bool IsPointingDeviceEvent(vtkMRMLInteractionEventData* eventData);

  /// \sa SetMouseInteractionEnabled()
  static bool MouseInteractionEnabled;

  /// The press/move/release the widget's state machine actually works in, independent of whether
  /// the event came from a VR controller or a desktop mouse. See GetInteractionEventType().
  enum InteractionEventType
  {
    InteractionEventNone = 0,
    InteractionEventPress,
    InteractionEventMove,
    InteractionEventRelease
  };

  /// Classifies an incoming event, so the VR and desktop-mouse paths share one state machine.
  /// VR delivers press and release as the same Pick3DEvent id distinguished by
  /// vtkEventDataAction, and continuous motion as Move3DEvent; the desktop mouse delivers
  /// LeftButtonPressEvent/MouseMoveEvent/LeftButtonReleaseEvent. Everything else maps to
  /// InteractionEventNone and is ignored.
  static InteractionEventType GetInteractionEventType(vtkMRMLInteractionEventData* eventData);

  /// Sends the embedded QWidget a hover update at the ray's current position on the panel, so the
  /// control under the ray highlights without anything being pressed. Ends the hover instead
  /// (EndHover()) when the ray is unusable or misses the panel. Returns whether a hover was sent.
  bool UpdateHover(vtkEventDataDevice device, bool rayValid, const double rayOrigin[3], const double rayDirection[3]);

  /// Tells the embedded QWidget that nothing is hovered any more. No-op unless a hover is
  /// currently in effect, so this is safe to call on every miss.
  void EndHover();

  /// Forgets that a hover is in effect, without telling the embedded QWidget anything. Separate
  /// from EndHover() for the press path, which must give up hover -- the press installs a mouse
  /// grabber and QGraphicsScene stops dispatching hover once one exists -- but must not send a
  /// hover-leave that would un-highlight the control at the instant it is pressed.
  void ForgetHover();

  /// Un-hovers every control in \a panel that Qt still believes the pointer is on, apart from
  /// \a hoveredWidget and its ancestors. Pass nullptr for \a hoveredWidget to un-hover everything.
  ///
  /// Needed because Qt's hover tracking desynchronizes when driven by synthesized events rather
  /// than a real cursor, and cannot recover on its own once QGraphicsScene has stopped considering
  /// the proxy item hovered -- at which point a hover-leave sent to the scene reaches nothing and
  /// the stale control stays highlighted for good. Since this widget knows exactly which control
  /// the ray is on, it states that outright instead of leaving Qt to derive it.
  static void ClearStaleHoverStates(QWidget* panel, QWidget* hoveredWidget);

  /// Whether a hover is currently in effect, i.e. whether EndHover() has anything to undo.
  bool Hovering{false};

  /// The control the pointer was last over, so ClearStaleHoverStates() runs once per transition
  /// rather than once per frame. QPointer so it cannot dangle if the panel rebuilds its contents.
  QPointer<QWidget> LastHoverChild;

  /// The device whose ray last established the hover. Its purpose is to decide whose *miss* ends
  /// the hover: both controllers report every frame, so acting on either one's miss would cancel
  /// the hover of whichever hand is actually pointing at the panel. The same reasoning as
  /// ActiveDevice, applied to hovering rather than dragging.
  ///
  /// It is not a reservation -- any device on the panel takes hover over, and this simply follows
  /// it. Reserving hover for the first device instead leaves the other one unable to hover at all
  /// once the holder stops reporting (idle, or tracking lost), since only a miss from the holder
  /// would release it and no more events arrive from it.
  vtkEventDataDevice HoverDevice{vtkEventDataDevice::Unknown};

  /// Builds the world-space picking ray (origin + unit direction) for either kind of event,
  /// returning false if it cannot be determined.
  ///
  /// VR controller events carry the ray directly on the event data. Mouse events do not:
  /// vtkMRMLViewInteractorStyle::DelegateInteractionEventToDisplayableManagers() only copies
  /// WorldPosition/WorldDirection across when the incoming event is itself a 3D device event, so
  /// for a mouse event the ray is built by unprojecting the display position at the near and far
  /// clipping planes instead.
  ///
  /// \note The event type is what selects between the two, deliberately -- NOT
  /// IsWorldPositionValid(). vtkMRMLThreeDViewInteractorStyle sets an "inaccurate" world position
  /// on mouse events too (from its QuickPick()), so that flag does not distinguish them, and the
  /// world *direction* is left unset regardless.
  bool GetInteractionRay(vtkMRMLInteractionEventData* eventData, double rayOrigin[3], double rayDirection[3]);

  /// Pixel position (within the embedded QWidget) of the most recent successful hit-test,
  /// re-sent on every move while WidgetStateActive so a drag continues even if the ray momentarily
  /// drifts off the plane.
  QPointF LastWidgetCoordinates;

  /// The device (e.g. right controller) whose press started the current WidgetStateActive/
  /// WidgetStateMovingHandle drag, set by ProcessInteractionEvent(). Both controllers
  /// independently fire Move3DEvent every frame, so CanProcessInteractionEvent() must ignore that
  /// event from any device other than this one while active -- otherwise the drag alternates
  /// between the two controllers' rays (whichever one isn't holding the drag jumping in
  /// essentially at random), rather than following the one that actually pressed.
  ///
  /// Mouse events carry no device, so they leave this at Unknown, which works out to the behavior
  /// wanted in both directions: a mouse drag is continued by further mouse events (also Unknown),
  /// while a controller's Move3DEvent arriving mid-mouse-drag is rejected, and vice versa.
  vtkEventDataDevice ActiveDevice{vtkEventDataDevice::Unknown};

  /// Widget-specific state, starting from the base class's WidgetStateUser sentinel (see
  /// vtkMRMLAbstractWidget::WidgetState doc comment).
  enum
  {
    /// Trigger (or mouse button) held down after a press hit the plane: moves are forwarded to the
    /// embedded QWidget as mouse-moves, and the matching release ends the drag.
    WidgetStateActive = WidgetStateUser,
    /// Trigger (or mouse button) held down after a press hit the move handle: moves call
    /// UpdateMoveHandleDrag(), and the matching release ends the drag.
    WidgetStateMovingHandle
  };

  /// Starts a ray-based "distance grab" of the move handle: from now until the matching release,
  /// UpdateMoveHandleDrag() (called from ProcessInteractionEvent() on every Move3DEvent) keeps the
  /// grabbed point on the handle -- and, since they share a transform, the panel -- at the same
  /// distance from rayOrigin, along the ray's current direction, that it was at grab start.
  void StartMoveHandleDrag(const double worldGrabPoint[3], const double rayOrigin[3]);

  /// Move handler for an active move-handle drag; see StartMoveHandleDrag(). Positions the
  /// handle at the grab's fixed distance along the ray's current direction, and rotates the panel
  /// so it keeps facing the render window's active camera (standing in for the headset pose, since
  /// the VR render loop keeps the renderer's camera at the current headset pose every frame) as it
  /// is moved, while always remaining upright: its up axis is pinned to the render window's
  /// physical view-up and only the yaw about that axis tracks the camera (falling back to the
  /// previous rotation if the renderer/camera/render window is unavailable, or the aim is
  /// degenerate).
  void UpdateMoveHandleDrag(const double rayOrigin[3], const double rayDirection[3]);

  /// The point grabbed on the move handle at drag start, in MoveHandleDragTransformNode's local
  /// frame. The drag pivots around this exact point (see StartMoveHandleDrag()).
  double MoveHandleLocalGrabPoint[3]{0.0, 0.0, 0.0};
  /// Distance from the ray origin to the grabbed point at drag start, held fixed for the whole
  /// drag -- this is what makes the drag "maintain distance" instead of snapping the panel to the
  /// controller.
  double MoveHandleDragDistance{0.0};
  /// MoveHandleDragTransformNode's current rotation (world, since it has no parent transform of
  /// its own): seeded from the transform at grab start, then re-aimed every tick by
  /// UpdateMoveHandleDrag(), and carried over unchanged on ticks where re-aiming is not possible.
  double MoveHandleDragRotation[3][3];
  /// The shared "*_MoveTransform" node being repositioned by an active move-handle drag.
  /// vtkWeakPointer so it safely resets to null (rather than dangling) if the node is removed from
  /// the scene mid-drag.
  vtkWeakPointer<vtkMRMLLinearTransformNode> MoveHandleDragTransformNode;

protected:
  vtkSlicerQWidgetWidget();
  ~vtkSlicerQWidgetWidget() override;

private:
  vtkSlicerQWidgetWidget(const vtkSlicerQWidgetWidget&) = delete;
  void operator=(const vtkSlicerQWidgetWidget&) = delete;
};

#endif
