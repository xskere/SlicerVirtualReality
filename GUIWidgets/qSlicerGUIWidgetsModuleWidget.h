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

class qSlicerGUIWidgetsModuleWidgetPrivate;
class vtkMRMLGUIWidgetNode;
class vtkMRMLScene;

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
  /// Connected to the "Enable mouse interaction in 3D views" checkbox. Applies and persists the
  /// preference through qSlicerGUIWidgetsModule, which owns it -- this panel only presents it, so
  /// the setting still works in sessions where the panel is never opened.
  void onEnableMouseInteractionToggled(bool enabled);

  /// Connected to the scene's NodeRemovedEvent by setMRMLScene(). When the removed node is a GUI
  /// widget node, drops its GUIWidgetsMap entry (the raw pointer key would dangle once the scene
  /// releases the node) and removes the companion move-handle model and move-transform nodes
  /// created by addMoveHandle(), so no orphaned handle is left floating in the scene.
  /// \note The QWidget itself is intentionally not deleted here: ownership of the widgets is
  /// currently unmanaged (they leak); resolving that is a separate concern from this handler.
  void onSceneNodeRemoved(vtkObject* scene, vtkObject* node);

protected:
  QScopedPointer<qSlicerGUIWidgetsModuleWidgetPrivate> d_ptr;

  virtual void setup();

protected:
  QMap<vtkMRMLGUIWidgetNode*, QWidget*> GUIWidgetsMap;

private:
  Q_DECLARE_PRIVATE(qSlicerGUIWidgetsModuleWidget);
  Q_DISABLE_COPY(qSlicerGUIWidgetsModuleWidget);
};

#endif
