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
 * This 3D widget handles events between VTK and Qt for a QWidget placed in a scene. It takes 6dof
 * events (from VR controllers, via CanProcessInteractionEvent()/ProcessInteractionEvent()) and,
 * if they intersect the widget's plane (vtkSlicerQWidgetRepresentation::
 * ComputeInteractionPixelPosition()), converts them into synthesized QGraphicsScene mouse events.
 * A Pick3DEvent press starts a click (see vtkVirtualRealityViewOpenXRInteractorStyle, which
 * translates the right trigger into Pick3DEvent); Move3DEvent while WidgetStateActive continues a
 * drag (e.g. a slider); the matching Pick3DEvent release ends it. This makes the widget clickable
 * automatically wherever it is shown (VR or desktop 3D view), through Slicer's normal interaction
 * event pipeline -- no external module needs to know this widget exists.
 */

#ifndef vtkSlicerQWidgetWidget_h
#define vtkSlicerQWidgetWidget_h

#include "vtkSlicerGUIWidgetsModuleVTKWidgetsExport.h"

#include "vtkSlicerMarkupsWidget.h"

// VTK includes
#include <vtkEventData.h> // for vtkEventDataDevice

// Qt includes
#include <QPointF>

class vtkMRMLInteractionEventData;
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

  /// Returns true (claiming the event) if a Pick3DEvent press hits the widget's plane, or if a
  /// drag started by such a press is still in progress (WidgetStateActive), regardless of where
  /// the ray currently points -- see ProcessInteractionEvent() and the WidgetStateActive doc
  /// comment below for why the latter matters. distance2 is the squared world-space distance from
  /// the ray origin to the hit point, used by the displayable manager to pick the closest widget
  /// when more than one GUI widget could be hit.
  bool CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& distance2) override;

  /// Performs the click: on a Pick3DEvent press that hits the plane (see
  /// CanProcessInteractionEvent()), synthesizes a QGraphicsScene mouse press at the corresponding
  /// pixel position and enters WidgetStateActive. While active, Move3DEvent synthesizes mouse-move
  /// events (falling back to the last known pixel position if the ray drifts off the plane, so a
  /// captured item like a slider handle keeps receiving updates), and the matching Pick3DEvent
  /// release synthesizes the mouse release and returns to WidgetStateIdle.
  bool ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData) override;

protected:
  /// Pixel position (within the embedded QWidget) of the most recent successful hit-test,
  /// re-sent on every Move3DEvent while WidgetStateActive so a drag continues even if the ray
  /// momentarily drifts off the plane.
  QPointF LastWidgetCoordinates;

  /// The device (e.g. right controller) whose Pick3DEvent press started the current
  /// WidgetStateActive drag, set by ProcessInteractionEvent(). Both controllers independently
  /// fire Move3DEvent every frame, so CanProcessInteractionEvent() must ignore that event from any
  /// device other than this one while active -- otherwise the drag alternates between the two
  /// controllers' rays (whichever one isn't holding the drag jumping in essentially at random),
  /// rather than following the one that actually pressed.
  vtkEventDataDevice ActiveDevice{vtkEventDataDevice::Unknown};

  /// Widget-specific state, starting from the base class's WidgetStateUser sentinel (see
  /// vtkMRMLAbstractWidget::WidgetState doc comment).
  enum
  {
    /// Trigger held down after a Pick3DEvent press hit the plane: Move3DEvent forwards to the
    /// embedded QWidget as a mouse-move, and the matching Pick3DEvent release ends the drag.
    WidgetStateActive = WidgetStateUser
  };

protected:
  vtkSlicerQWidgetWidget();
  ~vtkSlicerQWidgetWidget() override;

private:
  vtkSlicerQWidgetWidget(const vtkSlicerQWidgetWidget&) = delete;
  void operator=(const vtkSlicerQWidgetWidget&) = delete;
};

#endif
