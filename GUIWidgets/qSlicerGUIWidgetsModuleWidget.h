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

// Qt includes
#include <QMap>
#include <QPointer>
#include <QPointF>

class qSlicerGUIWidgetsModuleWidgetPrivate;
class vtkMRMLGUIWidgetNode;
class QGraphicsScene;
class QTimer;

/// \ingroup Slicer_QtModules_ExtensionTemplate
class Q_SLICER_QTMODULES_GUIWIDGETS_EXPORT qSlicerGUIWidgetsModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerGUIWidgetsModuleWidget(QWidget *parent=0);
  virtual ~qSlicerGUIWidgetsModuleWidget();

public slots:
  QWidget* onAddHelloWorldNodeClicked();
  void onUpdateButtonLabelButtonClicked();

  void onAddHomeWidgetButtonClicked();
  void onAddDataModuleWidgetButtonClicked();
  void onAddSegmentEditorWidgetButtonClicked();
  void onAddTransformWidgetButtonClicked();
  void onSetUpInteractionButtonClicked();
  void onStartInteractionButtonClicked();

  /// Assign widget to a GUIWidget markups node
  void setWidgetToGUIWidgetMarkupsNode(vtkMRMLGUIWidgetNode* node, QWidget* widget);

protected slots:
  /// Connected to qMRMLVirtualRealityView::leftMenuButtonClicked() by onSetUpInteractionButtonClicked().
  /// Toggles the visibility of the widget set up for interaction.
  void onMenuButtonClicked();

  /// Connected to qMRMLVirtualRealityView::rightTriggerPressed() by onSetUpInteractionButtonClicked().
  /// If the widget is currently shown, ray-casts the pointer against it (see
  /// computeWidgetPointerHit()), sends a mouse press at the hit position, and starts DragTimer so
  /// held-down drags (e.g. dragging a slider) keep tracking the pointer with mouse move events
  /// until onTriggerButtonReleased(). Does nothing while the widget is hidden, leaving the default
  /// grab&move interaction (grip-driven) unaffected.
  void onTriggerButtonPressed();

  /// Connected to qMRMLVirtualRealityView::rightTriggerReleased() by onSetUpInteractionButtonClicked().
  /// Stops DragTimer and, if a drag was actually started by onTriggerButtonPressed(), sends the
  /// matching mouse release. A no-op otherwise.
  void onTriggerButtonReleased();

  /// DragTimer callback: while a drag is active, re-runs the ray-cast every tick and sends a mouse
  /// move at the new hit position, so QGraphicsScene's implicit mouse grab (from the initial press)
  /// keeps delivering drag updates to whichever item captured the press (e.g. a slider handle).
  void onDragTimerTimeout();

protected:
  QScopedPointer<qSlicerGUIWidgetsModuleWidgetPrivate> d_ptr;

  virtual void setup();

  /// Ray-casts the pointer from PointerTransform against the HomeWidgetNode's plane and, on hit,
  /// outputs the corresponding pixel position within its QWidget and the QGraphicsScene to send
  /// synthesized mouse events to. Returns false (leaving the outputs untouched) if the widget
  /// isn't set up or the ray misses. Shared by the manual test button
  /// (onStartInteractionButtonClicked()) and the real-time trigger press/move/release handlers.
  bool computeWidgetPointerHit(QGraphicsScene*& scene, QPointF& pixelPosition);

protected:
  QMap<vtkMRMLGUIWidgetNode*, QWidget*> GUIWidgetsMap;

  /// Timer driving onDragTimerTimeout() while a trigger-initiated drag is in progress.
  QTimer* DragTimer{nullptr};
  /// Whether onTriggerButtonPressed() actually started a drag (i.e. the initial ray-cast hit),
  /// so onTriggerButtonReleased() knows whether a matching mouse release is owed.
  bool Dragging{false};
  /// Scene and pixel position from the most recent successful ray-cast during a drag, used as a
  /// fallback by onTriggerButtonReleased() if the pointer has drifted off the widget by release
  /// time. QPointer so it safely resets to null (rather than dangling) if the scene is destroyed
  /// mid-drag, e.g. the widget being removed while the trigger is still held.
  QPointer<QGraphicsScene> LastDragScene;
  QPointF LastDragPixelPosition;

private:
  Q_DECLARE_PRIVATE(qSlicerGUIWidgetsModuleWidget);
  Q_DISABLE_COPY(qSlicerGUIWidgetsModuleWidget);
};

#endif
