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

// GUIWidgets MRML includes
#include <vtkMRMLGUIWidgetNode.h>

// GUIWidgets Logic includes
#include <vtkSlicerGUIWidgetsLogic.h>

// GUIWidgets VTKWidgets includes
#include <vtkSlicerQWidgetWidget.h>

// GUIWidgets includes
#include "qSlicerGUIWidgetsModule.h"
#include "qSlicerGUIWidgetsModuleWidget.h"

// VirtualReality Widgets includes
#include "qMRMLVirtualRealityView.h"

// Markups Logic includes
#include <vtkSlicerMarkupsLogic.h>

// Markups Widgets includes
#include "qMRMLMarkupsOptionsWidgetsFactory.h"

// Slicer includes
#include "qSlicerAbstractCoreModule.h"
#include "qSlicerApplication.h"
#include "qSlicerModuleManager.h"

// MRML includes
#include <vtkMRMLDisplayNode.h>
#include <vtkMRMLScene.h>

// Qt includes
#include <QDebug>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_ExtensionTemplate
class qSlicerGUIWidgetsModulePrivate
{
public:
  qSlicerGUIWidgetsModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerGUIWidgetsModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerGUIWidgetsModulePrivate::qSlicerGUIWidgetsModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerGUIWidgetsModule methods

//-----------------------------------------------------------------------------
qSlicerGUIWidgetsModule::qSlicerGUIWidgetsModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerGUIWidgetsModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerGUIWidgetsModule::~qSlicerGUIWidgetsModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerGUIWidgetsModule::helpText() const
{
  return "This is a loadable module that can be bundled in an extension";
}

//-----------------------------------------------------------------------------
QString qSlicerGUIWidgetsModule::acknowledgementText() const
{
  return "This work was partially funded by the grant 'ICEX Espana Exportacion e Inversiones' under\
 the program 'Inversiones de Empresas Extranjeras en Actividades de I+D\
 (Fondo Tecnologico)- Convocatoria 2021'";
}

//-----------------------------------------------------------------------------
QStringList qSlicerGUIWidgetsModule::contributors() const
{
  QStringList moduleContributors;
  moduleContributors << QString("Csaba Pinter (Ebatinca)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerGUIWidgetsModule::icon() const
{
  return QIcon(":/Icons/GUIWidgets.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerGUIWidgetsModule::categories() const
{
  return QStringList() << "Virtual Reality";
}

//-----------------------------------------------------------------------------
QStringList qSlicerGUIWidgetsModule::dependencies() const
{
  // Deliberately NOT declaring "VirtualReality" here even though setup() below ends up needing
  // its view widget: dependencies() affects Slicer's *global* module setup order, not just the
  // order between these two modules, and pulling VirtualReality's setup() (which can eagerly
  // reconnect to VR hardware if the view node's visibility was already true from a previous
  // session) earlier in that global order caused it to run before whatever module normally sets
  // up the reference 3D view node first. See the deferred lookup in setup() instead.
  return QStringList() << "Markups";
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModule::setup()
{
  this->Superclass::setup();

  // No displayable manager is registered here: since vtkMRMLGUIWidgetNode subclasses the plane
  // markup node, the generic Markups displayable manager (vtkMRMLMarkupsDisplayableManager,
  // already registered by the Markups module) takes over automatically, using the widget class
  // registered below. vtkSlicerQWidgetWidget's CanProcessInteractionEvent()/
  // ProcessInteractionEvent() overrides (see VTKWidgets/vtkSlicerQWidgetWidget.cxx) are what let
  // it actually handle interaction, through that same generic dispatch.

  // Register markups
  vtkSlicerApplicationLogic* appLogic = this->appLogic();
  if (!appLogic)
  {
    qCritical() << Q_FUNC_INFO << " : invalid application logic.";
    return;
  }
  vtkSlicerMarkupsLogic* markupsLogic = vtkSlicerMarkupsLogic::SafeDownCast(appLogic->GetModuleLogic("Markups"));
  if (!markupsLogic)
  {
    qCritical() << Q_FUNC_INFO << " : invalid markups logic.";
    return;
  }
  vtkNew<vtkMRMLGUIWidgetNode> guiWidgetNode;
  vtkNew<vtkSlicerQWidgetWidget> vtkQWidgetWidget;
  markupsLogic->RegisterMarkupsNode(guiWidgetNode, vtkQWidgetWidget);

  // Create and configure the additional widgets
  //auto optionsWidgetFactory = qSlicerMarkupsAdditionalOptionsWidgetsFactory::instance();
  //optionsWidgetFactory->registerAdditionalOptionsWidget(new qSlicerMarkupsGUIWidget());

  // Deferred rather than done here directly: at this point, during this module's OWN setup(), the
  // VirtualReality module's setup() (which constructs its view widget) is not guaranteed to have
  // run yet -- and forcing that via a dependencies() on "VirtualReality" would reorder Slicer's
  // *global* module setup sequence, not just the relative order of these two modules, which was
  // observed to make VirtualReality's setup() run early enough to eagerly reconnect to VR hardware
  // before the reference 3D view node existed yet. qSlicerApplication::startupCompleted() instead
  // fires once, after every discovered module has actually been loaded (see
  // qSlicerApplicationHelper.hxx's module-loading loop, which finishes before this signal's
  // wiring/emission is reached) -- unlike qSlicerModuleFactoryManager::modulesLoaded(), which is
  // never actually emitted by a real running application (only by one isolated unit test); its
  // loadModules() is not what real startup calls, which loads modules one at a time instead.
  QObject::connect(qSlicerApplication::application(), SIGNAL(startupCompleted()),
    this, SLOT(wireUpMenuButton()), Qt::UniqueConnection);
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModule::wireUpMenuButton()
{
  // Reached dynamically (QMetaObject::invokeMethod) rather than linked directly, so this module
  // does not need to depend on the VR rendering backend for this.
  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* vrModule = app->moduleManager()->module("VirtualReality");
  if (!vrModule)
  {
    qCritical() << Q_FUNC_INFO << ": VirtualReality module not found";
    return;
  }
  qMRMLVirtualRealityView* vrViewWidget = nullptr;
  QMetaObject::invokeMethod(vrModule, "viewWidget", Qt::DirectConnection,
    Q_RETURN_ARG(qMRMLVirtualRealityView*, vrViewWidget));
  if (!vrViewWidget)
  {
    qCritical() << Q_FUNC_INFO << ": VR view widget not found even after all modules finished loading.";
    return;
  }
  // Qt::UniqueConnection since modulesLoaded() can fire more than once over the application's
  // lifetime (e.g. modules loaded later via the Extension Manager), and this must not end up
  // connected twice to the same view widget.
  QObject::connect(vrViewWidget, SIGNAL(leftMenuButtonClicked()), this, SLOT(onMenuButtonClicked()), Qt::UniqueConnection);
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModule::onMenuButtonClicked()
{
  vtkMRMLScene* scene = qSlicerApplication::application()->mrmlScene();
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(scene->GetFirstNodeByName("HomeWidgetNode"));
  if (!widgetNode)
  {
    qCritical() << Q_FUNC_INFO << ": GUI widget node was not found in scene";
    return;
  }
  vtkMRMLDisplayNode* displayNode = widgetNode->GetDisplayNode();
  if (!displayNode)
  {
    qCritical() << Q_FUNC_INFO << ": GUI widget node has no display node";
    return;
  }
  displayNode->SetVisibility(!displayNode->GetVisibility());
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerGUIWidgetsModule::createWidgetRepresentation()
{
  return new qSlicerGUIWidgetsModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerGUIWidgetsModule::createLogic()
{
  return vtkSlicerGUIWidgetsLogic::New();
}
