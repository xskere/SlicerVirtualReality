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

// GUI Widgets includes
#include "qSlicerGUIWidgetsModuleWidget.h"
#include "ui_qSlicerGUIWidgetsModuleWidget.h"

#include "vtkMRMLGUIWidgetNode.h"
#include "vtkMRMLGUIWidgetDisplayNode.h"

// VirtualReality Widgets includes
#include "qMRMLVirtualRealityHomeWidget.h"
#include "qMRMLVirtualRealityDataModuleWidget.h"
#include "qMRMLVirtualRealitySegmentEditorWidget.h"
#include "qMRMLVirtualRealityTransformWidget.h"

// VirtualReality Logic includes
#include "vtkSlicerVirtualRealityLogic.h"

// VirtualReality MRML includes
#include "vtkMRMLVirtualRealityViewNode.h"

// Slicer includes
#include "qSlicerApplication.h"

#include "vtkSlicerApplicationLogic.h"

// Markups Logic includes
#include <vtkSlicerMarkupsLogic.h>

// MRML includes
#include "vtkMRMLLinearTransformNode.h"
#include "vtkMRMLModelDisplayNode.h"
#include "vtkMRMLModelNode.h"
#include "vtkMRMLScene.h"

// VTK includes
#include "vtkCubeSource.h"

// STD includes
#include <string>

// Qt includes
#include <QDebug>
#include <QWidget>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_ExtensionTemplate
class qSlicerGUIWidgetsModuleWidgetPrivate: public Ui_qSlicerGUIWidgetsModuleWidget
{
public:
  qSlicerGUIWidgetsModuleWidgetPrivate();
};

//-----------------------------------------------------------------------------
// qSlicerGUIWidgetsModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerGUIWidgetsModuleWidgetPrivate::qSlicerGUIWidgetsModuleWidgetPrivate()
{
}


//-----------------------------------------------------------------------------
// qSlicerGUIWidgetsModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerGUIWidgetsModuleWidget::qSlicerGUIWidgetsModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerGUIWidgetsModuleWidgetPrivate )
{
}

//-----------------------------------------------------------------------------
qSlicerGUIWidgetsModuleWidget::~qSlicerGUIWidgetsModuleWidget()
{
  this->GUIWidgetsMap.clear();
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::setup()
{
  Q_D(qSlicerGUIWidgetsModuleWidget);
  d->setupUi(this);
  this->Superclass::setup();

  QObject::connect(d->AddHelloWorldGUIWidgetNodeButton, SIGNAL(clicked()), this, SLOT(onAddHelloWorldNodeClicked()));
  QObject::connect(d->UpdateButtonLabelButton, SIGNAL(clicked()), this, SLOT(onUpdateButtonLabelButtonClicked()));

  QObject::connect(d->AddHomeWidgetButton, SIGNAL(clicked()), this, SLOT(onAddHomeWidgetButtonClicked()));
  QObject::connect(d->AddDataModuleWidgetButton, SIGNAL(clicked()), this, SLOT(onAddDataModuleWidgetButtonClicked()));
  QObject::connect(d->AddSegmentEditorWidgetButton, SIGNAL(clicked()), this, SLOT(onAddSegmentEditorWidgetButtonClicked()));
  QObject::connect(d->AddTransformWidgetButton, SIGNAL(clicked()), this, SLOT(onAddTransformWidgetButtonClicked()));
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  this->qvtkReconnect(this->mrmlScene(), scene, vtkMRMLScene::NodeRemovedEvent,
    this, SLOT(onSceneNodeRemoved(vtkObject*, vtkObject*)));
  this->Superclass::setMRMLScene(scene);
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onSceneNodeRemoved(vtkObject* sceneObject, vtkObject* nodeObject)
{
  vtkMRMLScene* scene = vtkMRMLScene::SafeDownCast(sceneObject);
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(nodeObject);
  if (!scene || !widgetNode)
  {
    return;
  }

  // Drop the bookkeeping entry while the node pointer is still valid (it dangles once the scene
  // releases its reference). The QWidget itself is intentionally left alive; see the header doc.
  this->GUIWidgetsMap.remove(widgetNode);

  // Remove the companion nodes created by addMoveHandle(), so no orphaned handle bar is left
  // floating in the scene. During scene close these are being removed anyway, in which case the
  // lookups simply return null.
  vtkMRMLNode* handleNode = widgetNode->GetMoveHandleNode();
  if (handleNode)
  {
    scene->RemoveNode(handleNode);
  }
  if (widgetNode->GetName())
  {
    std::string moveTransformName = std::string(widgetNode->GetName()) + "_MoveTransform";
    vtkMRMLNode* moveTransformNode = scene->GetFirstNodeByName(moveTransformName.c_str());
    if (moveTransformNode)
    {
      scene->RemoveNode(moveTransformNode);
    }
  }
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::setWidgetToGUIWidgetMarkupsNode(vtkMRMLGUIWidgetNode* node, QWidget* widget)
{
  if (!node)
  {
    return;
  }

  node->SetWidget((void*)widget);

  this->GUIWidgetsMap[node] = widget;

  this->addMoveHandle(node);
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::addMoveHandle(vtkMRMLGUIWidgetNode* widgetNode)
{
  if (!widgetNode || !widgetNode->GetName())
  {
    return;
  }

  vtkMRMLScene* scene = qSlicerApplication::application()->mrmlScene();

  // Shared transform that both the widget node and its move handle are parented to. Grabbing the
  // handle drags this transform (via the default grip grab&move mechanism, which only knows how
  // to pick vtkMRMLModelNode -- see the addMoveHandle() doc comment in the header), and the widget
  // follows along because vtkSlicerQWidgetRepresentation::UpdateFromMRML() now applies the same
  // parent transform to the widget's plane. Found via the widget's own parent transform link
  // (unique per node object) rather than by a name derived from the widget's own name: demo widget
  // nodes created by this panel's buttons are all given the same hardcoded name for their type (see
  // e.g. onAddHomeWidgetButtonClicked()), so a name-derived lookup would resolve to whichever same-
  // type widget happened to create the transform first, silently sharing one transform (and so one
  // move handle drag) across every widget of that type.
  vtkMRMLLinearTransformNode* moveTransformNode = vtkMRMLLinearTransformNode::SafeDownCast(widgetNode->GetParentTransformNode());
  if (!moveTransformNode)
  {
    std::string moveTransformName = std::string(widgetNode->GetName()) + "_MoveTransform";
    vtkNew<vtkMRMLLinearTransformNode> newMoveTransformNode;
    newMoveTransformNode->SetName(moveTransformName.c_str());
    scene->AddNode(newMoveTransformNode);
    moveTransformNode = newMoveTransformNode;
    widgetNode->SetAndObserveTransformNodeID(moveTransformNode->GetID());
  }

  // Handle model: a white bar below the widget's local origin (the widget plane lies in the
  // local XZ plane with +Z up, see vtkSlicerQWidgetRepresentation::PlaceWidget()), so it does not
  // overlap the widget's own clickable surface (hit-tested separately, see
  // vtkSlicerQWidgetRepresentation::ComputeInteractionPixelPosition()). The offset is a fixed
  // approximation -- it does not track the widget's actual current pixel size (see
  // vtkSlicerQWidgetRepresentation::OnTextureModified()) -- good enough to keep the handle clear of
  // most panels without adding that coupling. Kept as a node reference on the widget node (see
  // vtkMRMLGUIWidgetNode::GetMoveHandleNodeReferenceRole()) rather than found by name so it stays
  // linked even if the widget node is later renamed; the "_MoveHandle" name is still assigned below
  // purely so the node is recognizable when browsing the scene.
  vtkMRMLModelNode* handleModelNode = widgetNode->GetMoveHandleNode();
  if (!handleModelNode)
  {
    std::string handleName = std::string(widgetNode->GetName()) + "_MoveHandle";
    const double handleWidth = 200.0; // mm, along the widget's width (local X)
    const double handleThickness = 10.0; // mm, along the plane normal (local Y)
    const double handleHeight = 20.0; // mm, along the widget's up axis (local Z)
    const double handleOffsetBelowWidget = 120.0; // mm

    vtkNew<vtkCubeSource> cubeSource;
    cubeSource->SetXLength(handleWidth);
    cubeSource->SetYLength(handleThickness);
    cubeSource->SetZLength(handleHeight);
    cubeSource->SetCenter(0.0, 0.0, -handleOffsetBelowWidget);

    vtkNew<vtkMRMLModelNode> newHandleModelNode;
    newHandleModelNode->SetName(handleName.c_str());
    newHandleModelNode->SetPolyDataConnection(cubeSource->GetOutputPort());
    scene->AddNode(newHandleModelNode);

    vtkNew<vtkMRMLModelDisplayNode> handleDisplayNode;
    handleDisplayNode->SetColor(1.0, 1.0, 1.0);
    scene->AddNode(handleDisplayNode);
    newHandleModelNode->SetAndObserveDisplayNodeID(handleDisplayNode->GetID());

    handleModelNode = newHandleModelNode;
    widgetNode->SetAndObserveMoveHandleNodeID(handleModelNode->GetID());
  }
  handleModelNode->SetAndObserveTransformNodeID(moveTransformNode->GetID());
}

//-----------------------------------------------------------------------------
QWidget* qSlicerGUIWidgetsModuleWidget::onAddHelloWorldNodeClicked()
{
  qSlicerApplication* app = qSlicerApplication::application();
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(app->mrmlScene()->AddNewNodeByClass("vtkMRMLGUIWidgetNode") );
  widgetNode->SetName("TestButtonWidgetNode");

  QPushButton* newButton = new QPushButton("Hello world!");
  this->setWidgetToGUIWidgetMarkupsNode(widgetNode, newButton);

  return newButton;
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onUpdateButtonLabelButtonClicked()
{
  Q_D(qSlicerGUIWidgetsModuleWidget);

  // Get last widget
  QWidget* lastWidget = this->GUIWidgetsMap.last();

  QPushButton* button = qobject_cast<QPushButton*>(lastWidget);
  if (button)
  {
    button->setText(d->NewLabelLineEdit->text());
  }
  else
  {
    qCritical() << Q_FUNC_INFO << ": Widget is not a push button";
  }
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onAddHomeWidgetButtonClicked()
{
  Q_D(qSlicerGUIWidgetsModuleWidget);

  qSlicerApplication* app = qSlicerApplication::application();
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(app->mrmlScene()->AddNewNodeByClass("vtkMRMLGUIWidgetNode") );
  widgetNode->SetName("HomeWidgetNode");
  // Marks this widget as one the VR view's left menu button shows/hides (see
  // qSlicerGUIWidgetsModule::onMenuButtonClicked()); the name above is for human readability only.
  widgetNode->SetIsMenuWidget(true);

  vtkSlicerVirtualRealityLogic* vrLogic = vtkSlicerVirtualRealityLogic::SafeDownCast(app->applicationLogic()->GetModuleLogic("VirtualReality"));
  if (!vrLogic)
  {
    qCritical() << Q_FUNC_INFO << " : invalid VR logic";
    return;
  }

  qMRMLVirtualRealityHomeWidget* widget = new qMRMLVirtualRealityHomeWidget();
  widget->setMRMLScene(app->mrmlScene());
  // Without this, updateWidgetFromMRML() treats the view node as null and permanently disables
  // the motion sensitivity/fly speed sliders and magnification buttons (setMRMLScene() alone does
  // not populate it).
  widget->setVirtualRealityViewNode(vrLogic->GetVirtualRealityViewNode());
  this->setWidgetToGUIWidgetMarkupsNode(widgetNode, widget);
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onAddDataModuleWidgetButtonClicked()
{
  Q_D(qSlicerGUIWidgetsModuleWidget);

  qSlicerApplication* app = qSlicerApplication::application();
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(app->mrmlScene()->AddNewNodeByClass("vtkMRMLGUIWidgetNode") );
  widgetNode->SetName("DataModuleWidgetNode");

  qMRMLVirtualRealityDataModuleWidget* widget = new qMRMLVirtualRealityDataModuleWidget();
  widget->setMRMLScene(app->mrmlScene());
  this->setWidgetToGUIWidgetMarkupsNode(widgetNode, widget);
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onAddSegmentEditorWidgetButtonClicked()
{
  Q_D(qSlicerGUIWidgetsModuleWidget);

  qSlicerApplication* app = qSlicerApplication::application();
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(app->mrmlScene()->AddNewNodeByClass("vtkMRMLGUIWidgetNode") );
  widgetNode->SetName("SegmentEditorWidgetNode");

  qMRMLVirtualRealitySegmentEditorWidget* widget = new qMRMLVirtualRealitySegmentEditorWidget();
  widget->setMRMLScene(app->mrmlScene());
  this->setWidgetToGUIWidgetMarkupsNode(widgetNode, widget);
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onAddTransformWidgetButtonClicked()
{
  Q_D(qSlicerGUIWidgetsModuleWidget);

  qSlicerApplication* app = qSlicerApplication::application();
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(app->mrmlScene()->AddNewNodeByClass("vtkMRMLGUIWidgetNode") );
  widgetNode->SetName("TransformWidgetNode");

  vtkSlicerVirtualRealityLogic* vrLogic = vtkSlicerVirtualRealityLogic::SafeDownCast(app->applicationLogic()->GetModuleLogic("VirtualReality"));
  if (!vrLogic)
  {
    qCritical() << Q_FUNC_INFO << " : invalid VR logic";
    return;
  }

  qMRMLVirtualRealityTransformWidget* widget = new qMRMLVirtualRealityTransformWidget(vrLogic->GetVirtualRealityViewNode());
  widget->setMRMLScene(app->mrmlScene());
  this->setWidgetToGUIWidgetMarkupsNode(widgetNode, widget);
}
