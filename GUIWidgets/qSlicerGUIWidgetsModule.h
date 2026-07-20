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

#ifndef __qSlicerGUIWidgetsModule_h
#define __qSlicerGUIWidgetsModule_h

// Slicer includes
#include "qSlicerLoadableModule.h"

#include "qSlicerGUIWidgetsModuleExport.h"

class qSlicerGUIWidgetsModulePrivate;

/// \ingroup Slicer_QtModules_ExtensionTemplate
class Q_SLICER_QTMODULES_GUIWIDGETS_EXPORT qSlicerGUIWidgetsModule : public qSlicerLoadableModule
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.slicer.modules.loadable.qSlicerLoadableModule/1.0");
  Q_INTERFACES(qSlicerLoadableModule);

public:

  typedef qSlicerLoadableModule Superclass;
  explicit qSlicerGUIWidgetsModule(QObject *parent=0);
  virtual ~qSlicerGUIWidgetsModule();

  qSlicerGetTitleMacro(QTMODULE_TITLE);

  virtual QString helpText()const;
  virtual QString acknowledgementText()const;
  virtual QStringList contributors()const;

  virtual QIcon icon()const;

  virtual QStringList categories()const;
  virtual QStringList dependencies() const;

protected:

  /// Initialize the module. Register the volumes reader/writer
  virtual void setup();

  /// Create and return the widget representation associated to this module
  virtual qSlicerAbstractModuleRepresentation* createWidgetRepresentation();

  /// Create and return the logic associated to this module
  virtual vtkMRMLAbstractLogic* createLogic();

protected slots:
  /// Deferred out of setup() to qSlicerApplication::startupCompleted() (see setup()'s comment for
  /// why: looking up the VirtualReality module's view widget needs its setup() to have already
  /// run, and that must not be guaranteed by declaring a dependencies() on "VirtualReality", which
  /// would reorder Slicer's *global* module setup sequence rather than just this module's position
  /// in it). Connects qMRMLVirtualRealityView::leftMenuButtonClicked() to onMenuButtonClicked(),
  /// so the left menu button toggles the "HomeWidgetNode" GUI widget panel's visibility by
  /// default -- no per-session setup step, the same way clicking/dragging a panel already needs
  /// none (see VTKWidgets/vtkSlicerQWidgetWidget.cxx).
  void wireUpMenuButton();

  /// Connected to qMRMLVirtualRealityView::leftMenuButtonClicked() by wireUpMenuButton().
  /// \note Hardcoded to a specific node name; generalizing this to whichever panel the user means
  /// is a separate, so-far-unaddressed concern.
  void onMenuButtonClicked();

protected:
  QScopedPointer<qSlicerGUIWidgetsModulePrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerGUIWidgetsModule);
  Q_DISABLE_COPY(qSlicerGUIWidgetsModule);

};

#endif
