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

#ifndef __qSlicerGUIWidgetsModuleWidget_h
#define __qSlicerGUIWidgetsModuleWidget_h

// Slicer includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerGUIWidgetsModuleExport.h"

// CTK includes
#include <ctkVTKObject.h>

// Qt includes
#include <QMap>
#include <QPointer>

// VTK includes
#include <vtkWeakPointer.h>

class qMRMLVirtualRealityView;
class qSlicerGUIWidgetsModuleWidgetPrivate;
class vtkMRMLGUIWidgetNode;
class vtkMRMLLinearTransformNode;
class vtkMRMLScene;
class QTimer;

/// \ingroup Slicer_QtModules_ExtensionTemplate
class Q_SLICER_QTMODULES_GUIWIDGETS_EXPORT qSlicerGUIWidgetsModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerGUIWidgetsModuleWidget(QWidget *parent=0);
  virtual ~qSlicerGUIWidgetsModuleWidget();

public slots:
  /// Reimplemented to (re)connect the scene's NodeRemovedEvent to onSceneNodeRemoved(), so this
  /// widget can clean up per-widget bookkeeping when a GUI widget node is deleted.
  void setMRMLScene(vtkMRMLScene* scene) override;

  QWidget* onAddHelloWorldNodeClicked();
  void onUpdateButtonLabelButtonClicked();

  void onAddHomeWidgetButtonClicked();
  void onAddDataModuleWidgetButtonClicked();
  void onAddSegmentEditorWidgetButtonClicked();
  void onAddTransformWidgetButtonClicked();
  void onSetUpInteractionButtonClicked();

  /// Assign widget to a GUIWidget markups node
  void setWidgetToGUIWidgetMarkupsNode(vtkMRMLGUIWidgetNode* node, QWidget* widget);

  /// Give the widget node a grabbable move handle: a white bar Model node below the widget,
  /// parented (along with the widget node itself) to a shared transform node. The default VR
  /// grab&move mechanism (grip button) can only pick vtkMRMLModelNode instances (see
  /// vtkVirtualRealityViewInteractorStyleDelegate::StartPositionProp()), not Markups nodes, so the
  /// widget itself cannot be grabbed directly -- grabbing this handle instead drags the shared
  /// transform, which the widget also follows (see vtkSlicerQWidgetRepresentation::UpdateFromMRML()).
  void addMoveHandle(vtkMRMLGUIWidgetNode* node);

protected slots:
  // Interaction handlers, wired to qMRMLVirtualRealityView signals by onSetUpInteractionButtonClicked().
  // Deliberately protected: they are not module API. Scripted tests can still reach them through a
  // string-based signal connection (Qt's meta-object system does not enforce access specifiers).

  /// Connected to qMRMLVirtualRealityView::leftMenuButtonClicked() by onSetUpInteractionButtonClicked().
  /// Toggles the visibility of the widget set up for interaction.
  void onMenuButtonClicked();

  /// Connected to qMRMLVirtualRealityView::rightTriggerPressed() by onSetUpInteractionButtonClicked().
  /// Ray-casts against every widget's move handle (see computeMoveHandlePointerHit()); on a hit,
  /// starts a fixed-distance move-handle drag (startMoveHandleDrag()), regardless of widget
  /// visibility. Clicking into a widget's own plane is handled natively by vtkSlicerQWidgetWidget's
  /// CanProcessInteractionEvent()/ProcessInteractionEvent() -- driven by the same trigger press,
  /// translated into Pick3DEvent -- so this handler no longer needs to do anything for that case.
  void onTriggerButtonPressed();

  /// Connected to qMRMLVirtualRealityView::rightTriggerReleased() by onSetUpInteractionButtonClicked().
  /// Stops DragTimer and, if a move-handle drag was in progress, ends it. A no-op otherwise: the
  /// matching release for a widget click is handled natively by vtkSlicerQWidgetWidget, see
  /// onTriggerButtonPressed().
  void onTriggerButtonReleased();

  /// DragTimer callback for an active move-handle drag; delegates to updateMoveHandleDrag(). The
  /// timer is only ever running during such a drag now (only started by startMoveHandleDrag()):
  /// widget-UI dragging (e.g. a slider) is driven by Move3DEvent through vtkSlicerQWidgetWidget
  /// directly, with no timer involved.
  void onDragTimerTimeout();

  /// Connected to the scene's NodeRemovedEvent by setMRMLScene(). When the removed node is a GUI
  /// widget node, drops its GUIWidgetsMap entry (the raw pointer key would dangle once the scene
  /// releases the node) and removes the companion move-handle model and move-transform nodes
  /// created by addMoveHandle(), so no orphaned handle is left floating in the scene.
  /// \note The QWidget itself is intentionally not deleted here: ownership of the widgets is
  /// currently unmanaged (they leak), and resolving that belongs to the planned GUIWidgets/
  /// VirtualReality decoupling refactor rather than this event handler.
  void onSceneNodeRemoved(vtkObject* scene, vtkObject* node);

protected:
  QScopedPointer<qSlicerGUIWidgetsModuleWidgetPrivate> d_ptr;

  virtual void setup();

  /// Computes the current pointer ray (world-space origin and unit direction) from the
  /// PointerTransform node set up by onSetUpInteractionButtonClicked(). Returns false if that
  /// node isn't found. Shared by computeMoveHandlePointerHit() and updateMoveHandleDrag().
  bool computePointerRay(double origin[3], double direction[3]);

  /// Ray-casts against every currently known GUI widget's move handle (see addMoveHandle()) and
  /// returns the closest one hit, if any, along with the world-space point picked on it. Used by
  /// onTriggerButtonPressed() to decide whether a trigger press should start a move-handle drag
  /// (startMoveHandleDrag()).
  bool computeMoveHandlePointerHit(vtkMRMLGUIWidgetNode*& hitWidgetNode, double worldPickedPoint[3]);

  /// Starts a ray-based "distance grab" of widgetNode's move handle: from now until
  /// onTriggerButtonReleased(), DragTimer calls updateMoveHandleDrag() every tick to keep the
  /// picked point on the handle -- and, since they share a transform, the widget -- at the same
  /// distance from the pointer origin, along the pointer's current direction, that it was at
  /// when the grab started.
  void startMoveHandleDrag(vtkMRMLGUIWidgetNode* widgetNode, const double worldPickedPoint[3]);

  /// DragTimer callback for an active move-handle drag; see startMoveHandleDrag(). Positions the
  /// handle at the grab's fixed distance along the pointer's current direction, and rotates the
  /// panel so it keeps facing the HMD as it is moved while always remaining upright: its up axis
  /// is pinned to the room's up direction and only the yaw about that axis tracks the headset
  /// (falling back to the previous rotation if the HMD transform node or the VR view widget is
  /// unavailable).
  void updateMoveHandleDrag();

protected:
  QMap<vtkMRMLGUIWidgetNode*, QWidget*> GUIWidgetsMap;

  /// Timer driving onDragTimerTimeout() while a move-handle drag is in progress.
  QTimer* DragTimer{nullptr};

  /// Whether a move-handle drag (see startMoveHandleDrag()) is in progress.
  bool DraggingHandle{false};
  /// The shared "*_MoveTransform" node being repositioned by the active move-handle drag.
  /// vtkWeakPointer so it safely resets to null (rather than dangling) if the node is removed from
  /// the scene mid-drag.
  vtkWeakPointer<vtkMRMLLinearTransformNode> DraggingMoveTransformNode;
  /// The point picked on the dragged handle, in DraggingMoveTransformNode's local frame, captured
  /// at grab start. The drag pivots around this exact point (see startMoveHandleDrag()).
  double DraggingHandleLocalGrabPoint[3]{0.0, 0.0, 0.0};
  /// Distance from the pointer origin to the picked point at grab start, held fixed for the
  /// whole drag -- this is what makes the drag "maintain distance" instead of snapping the panel
  /// to the controller.
  double DraggingHandleDistance{0.0};
  /// DraggingMoveTransformNode's current rotation (world, since it has no parent transform of its
  /// own): seeded from the transform at grab start, then re-aimed every tick by
  /// updateMoveHandleDrag() so the panel keeps facing the HMD while staying upright, and carried
  /// over unchanged on ticks where re-aiming is not possible (HMD transform node or VR view
  /// widget unavailable, or aim degenerate).
  double DraggingMoveTransformRotation[3][3];

  /// The VR view widget, resolved by onSetUpInteractionButtonClicked(); updateMoveHandleDrag()
  /// queries it for the room's up direction (qMRMLVirtualRealityView::physicalViewUp()) to keep
  /// a dragged panel upright. QPointer so it safely resets to null if the view is destroyed.
  QPointer<qMRMLVirtualRealityView> VRViewWidget;

private:
  Q_DECLARE_PRIVATE(qSlicerGUIWidgetsModuleWidget);
  Q_DISABLE_COPY(qSlicerGUIWidgetsModuleWidget);
};

#endif
