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

#include "vtkSlicerQWidgetWidget.h"
#include "vtkSlicerQWidgetRepresentation.h"

#include "vtkSlicerQWidgetTexture.h"

// VirtualReality Widgets includes
#include "qMRMLVirtualRealityHomeWidget.h"
#include "qMRMLVirtualRealityDataModuleWidget.h"
#include "qMRMLVirtualRealitySegmentEditorWidget.h"
#include "qMRMLVirtualRealityTransformWidget.h"
#include "qMRMLVirtualRealityView.h"

// VirtualReality Logic includes
#include "vtkSlicerVirtualRealityLogic.h"

// VirtualReality MRML includes
#include "vtkMRMLVirtualRealityViewNode.h"

// Slicer includes
#include "qSlicerAbstractCoreModule.h"
#include "qSlicerApplication.h"
#include "qSlicerLayoutManager.h"
#include "qSlicerModuleManager.h"

#include "vtkSlicerApplicationLogic.h"

// qMRMLWidget includes
#include "qMRMLThreeDView.h"
#include "qMRMLThreeDWidget.h"

// Markups Logic includes
#include <vtkSlicerMarkupsLogic.h>

// MRML includes
#include "vtkMRMLDisplayNode.h"
#include "vtkMRMLLinearTransformNode.h"
#include "vtkMRMLModelDisplayNode.h"
#include "vtkMRMLModelNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLViewNode.h"

#include "vtkMRMLMarkupsDisplayableManager.h"
#include "vtkMRMLCameraDisplayableManager.h"
#include "vtkMRMLMarkupsDisplayableManagerHelper.h"
#include "vtkMRMLVirtualRealityViewDisplayableManagerFactory.h"

// VTK includes
#include "vtkActor.h"
#include "vtkRenderer.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkPlaneSource.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper.h"
#include "vtkDataSet.h"
#include "vtkCellLocator.h"
#include "vtkCubeSource.h"
#include "vtkLineSource.h"
#include "vtkTransform.h"
#include "vtkTransformPolyDataFilter.h"
#include "vtkTubeFilter.h"

// STD includes
#include <string>
#include <vector>

// Qt includes
#include <QDebug>
#include <QRect>
#include <QWidget>
#include <QEvent>
#include <QMouseEvent>
#include <QGraphicsScene>
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QTimer>

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
  this->DragTimer = new QTimer(this);
  this->DragTimer->setInterval(16); // ~60Hz
  QObject::connect(this->DragTimer, SIGNAL(timeout()), this, SLOT(onDragTimerTimeout()));
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

  QObject::connect(d->SetUpInteractionButton, SIGNAL(clicked()), this, SLOT(onSetUpInteractionButtonClicked()));
  QObject::connect(d->StartInteractionButton, SIGNAL(clicked()), this, SLOT(onStartInteractionButtonClicked()));
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
  if (widgetNode->GetName())
  {
    std::string handleName = std::string(widgetNode->GetName()) + "_MoveHandle";
    vtkMRMLNode* handleNode = scene->GetFirstNodeByName(handleName.c_str());
    if (handleNode)
    {
      scene->RemoveNode(handleNode);
    }
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
  // parent transform to the widget's plane.
  std::string moveTransformName = std::string(widgetNode->GetName()) + "_MoveTransform";
  vtkMRMLLinearTransformNode* moveTransformNode = vtkMRMLLinearTransformNode::SafeDownCast(
    scene->GetFirstNodeByName(moveTransformName.c_str()));
  if (!moveTransformNode)
  {
    vtkNew<vtkMRMLLinearTransformNode> newMoveTransformNode;
    newMoveTransformNode->SetName(moveTransformName.c_str());
    scene->AddNode(newMoveTransformNode);
    moveTransformNode = newMoveTransformNode;
  }
  widgetNode->SetAndObserveTransformNodeID(moveTransformNode->GetID());

  // Handle model: a white bar below the widget's local origin (the widget plane lies in the
  // local XZ plane with +Z up, see vtkSlicerQWidgetRepresentation::PlaceWidget()), so it does not
  // overlap the widget's own clickable surface (picked separately by the trigger, see
  // onTriggerButtonPressed()). The offset is a fixed approximation -- it does not track the
  // widget's actual current pixel size (see vtkSlicerQWidgetRepresentation::OnTextureModified()) --
  // good enough to keep the handle clear of most panels without adding that coupling.
  std::string handleName = std::string(widgetNode->GetName()) + "_MoveHandle";
  vtkMRMLModelNode* handleModelNode = vtkMRMLModelNode::SafeDownCast(scene->GetFirstNodeByName(handleName.c_str()));
  if (!handleModelNode)
  {
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

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onSetUpInteractionButtonClicked()
{
  // Maximum distance for interaction. Must match onStartInteractionButtonClicked, as the
  // pointer model built here is the visualization of the exact same ray that is used there
  // to compute the intersection with the GUI widget.
  const double maxDistanceForInteraction = 2000; // mm

  qSlicerApplication* app = qSlicerApplication::application();

  vtkSlicerVirtualRealityLogic* vrLogic = vtkSlicerVirtualRealityLogic::SafeDownCast(app->applicationLogic()->GetModuleLogic("VirtualReality"));
  if (!vrLogic)
  {
    qCritical() << Q_FUNC_INFO << ": invalid VR logic";
    return;
  }
  vtkMRMLVirtualRealityViewNode* vrViewNode = vrLogic->GetVirtualRealityViewNode();
  if (!vrViewNode)
  {
    qCritical() << Q_FUNC_INFO << ": VR view node not found. Make sure Virtual Reality has been activated at least once.";
    return;
  }

  // Make sure the controller transform nodes exist, then get the one for the right controller,
  // which the pointer will be parented to (so that it moves and points along with the hand).
  vrViewNode->CreateDefaultControllerTransformNodes();
  // Also have the render loop keep the VirtualReality.HMD transform node updated (it creates the
  // node itself, see qMRMLVirtualRealityViewPrivate::doOpenVirtualReality()):
  // updateMoveHandleDrag() uses the headset position to keep a dragged widget facing the user.
  vrViewNode->SetHMDTransformUpdate(true);
  vtkMRMLLinearTransformNode* rightControllerTransformNode = vrViewNode->GetRightControllerTransformNode();
  if (!rightControllerTransformNode)
  {
    qCritical() << Q_FUNC_INFO << ": right controller transform not found. Make sure Virtual Reality is active.";
    return;
  }

  vtkMRMLScene* scene = app->mrmlScene();

  // Get or create the pointer transform node. onStartInteractionButtonClicked computes its
  // click ray-cast from this node, so parenting it under the right controller transform is
  // what makes the pointer (and the interaction it drives) follow the controller.
  vtkMRMLLinearTransformNode* pointerTransformNode = vtkMRMLLinearTransformNode::SafeDownCast(
    scene->GetFirstNodeByName("PointerTransform"));
  if (!pointerTransformNode)
  {
    vtkNew<vtkMRMLLinearTransformNode> newPointerTransformNode;
    newPointerTransformNode->SetName("PointerTransform");
    scene->AddNode(newPointerTransformNode);
    pointerTransformNode = newPointerTransformNode;
  }
  pointerTransformNode->SetAndObserveTransformNodeID(rightControllerTransformNode->GetID());

  // Get or create the pointer model (the laser beam visualization), parented to the pointer
  // transform. Its geometry (origin to -Z, same length as maxDistanceForInteraction) matches
  // the ray tested in onStartInteractionButtonClicked so the visible beam is exactly what gets
  // clicked against.
  vtkMRMLModelNode* pointerModelNode = vtkMRMLModelNode::SafeDownCast(scene->GetFirstNodeByName("PointerModel"));
  if (!pointerModelNode)
  {
    vtkNew<vtkLineSource> lineSource;
    lineSource->SetPoint1(0.0, 0.0, 0.0);
    lineSource->SetPoint2(0.0, 0.0, -maxDistanceForInteraction);

    vtkNew<vtkTubeFilter> tubeFilter;
    tubeFilter->SetInputConnection(lineSource->GetOutputPort());
    tubeFilter->SetRadius(1.0);
    tubeFilter->SetNumberOfSides(16);

    vtkNew<vtkMRMLModelNode> newPointerModelNode;
    newPointerModelNode->SetName("PointerModel");
    newPointerModelNode->SetPolyDataConnection(tubeFilter->GetOutputPort());
    newPointerModelNode->SetSelectable(false);
    scene->AddNode(newPointerModelNode);

    vtkNew<vtkMRMLModelDisplayNode> pointerModelDisplayNode;
    pointerModelDisplayNode->SetColor(1.0, 0.0, 0.0);
    pointerModelDisplayNode->SetOpacity(0.6);
    scene->AddNode(pointerModelDisplayNode);
    newPointerModelNode->SetAndObserveDisplayNodeID(pointerModelDisplayNode->GetID());

    pointerModelNode = newPointerModelNode;
  }
  pointerModelNode->SetAndObserveTransformNodeID(pointerTransformNode->GetID());

  // Bind the left menu button to show/hide the widget, and the right trigger to click it
  // (ray-cast pick, see onStartInteractionButtonClicked()) while it is shown. While hidden, the
  // trigger is left unbound, so the default grab&move interaction (driven by the grip buttons) is
  // unaffected. qSlicerVirtualRealityModule is reached dynamically (QMetaObject::invokeMethod)
  // rather than linked directly, so this module does not need to depend on the VR rendering
  // backend.
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
    qCritical() << Q_FUNC_INFO << ": VR view widget not found. Make sure Virtual Reality has been activated at least once.";
    return;
  }
  this->VRViewWidget = vrViewWidget;
  QObject::connect(vrViewWidget, SIGNAL(leftMenuButtonClicked()), this, SLOT(onMenuButtonClicked()), Qt::UniqueConnection);
  QObject::connect(vrViewWidget, SIGNAL(rightTriggerPressed()), this, SLOT(onTriggerButtonPressed()), Qt::UniqueConnection);
  QObject::connect(vrViewWidget, SIGNAL(rightTriggerReleased()), this, SLOT(onTriggerButtonReleased()), Qt::UniqueConnection);
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onMenuButtonClicked()
{
  qSlicerApplication* app = qSlicerApplication::application();
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(app->mrmlScene()->GetFirstNodeByName("HomeWidgetNode"));
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
bool qSlicerGUIWidgetsModuleWidget::computeWidgetPointerHit(QGraphicsScene*& scene, QPointF& pixelPosition)
{
  // Pointer transform
  qSlicerApplication* app = qSlicerApplication::application();
  vtkMRMLLinearTransformNode* transformNode = vtkMRMLLinearTransformNode::SafeDownCast(app->mrmlScene()->GetFirstNodeByName("PointerTransform"));
  if (!transformNode)
  {
    qCritical() << Q_FUNC_INFO << ": Pointer transform was not found in scene";
    return false;
  }

  // Define maximum distance for interaction
  double maxDistanceForInteraction = 2000; // mm

  // Line points
  double pointA_h[4] = { 0.0, 0.0, 0.0, 1.0 };
  double pointB_h[4] = { 0.0, 0.0, -maxDistanceForInteraction, 1.0 };

  // Get transformed line points
  double pointA_transf_h[4] = { 0.0, 0.0, 0.0, 1.0 };
  double pointB_transf_h[4] = { 0.0, 0.0, 0.0, 1.0 };
  vtkNew<vtkMatrix4x4> matrixTransformToWorld;
  transformNode->GetMatrixTransformToWorld(matrixTransformToWorld);
  matrixTransformToWorld->MultiplyPoint(pointA_h, pointA_transf_h);
  matrixTransformToWorld->MultiplyPoint(pointB_h, pointB_transf_h);
  double pointA_transf[3] = { pointA_transf_h[0], pointA_transf_h[1], pointA_transf_h[2] };
  double pointB_transf[3] = { pointB_transf_h[0], pointB_transf_h[1], pointB_transf_h[2] };

  // Get GUI widget
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(app->mrmlScene()->GetFirstNodeByName("HomeWidgetNode"));
  if (!widgetNode)
  {
    qCritical() << Q_FUNC_INFO << ": GUI widget node was not found in scene";
    return false;
  }

  // Get displayable manager
  qSlicerLayoutManager* layoutManager = qSlicerApplication::application()->layoutManager();
  if (!layoutManager)
  {
    // application is closing
    return false;
  }
  qMRMLThreeDWidget* threeDWidget = layoutManager->threeDWidget(0);
  vtkMRMLMarkupsDisplayableManager* markupsDisplayableManager = vtkMRMLMarkupsDisplayableManager::SafeDownCast(
    threeDWidget->threeDView()->displayableManagerByClassName("vtkMRMLMarkupsDisplayableManager"));
  if (!markupsDisplayableManager)
  {
    qCritical() << Q_FUNC_INFO << ": Markups displayable manager was not found";
    return false;
  }

  // Get widget representation from displayabale manager
  vtkMRMLMarkupsDisplayableManagerHelper* helper = markupsDisplayableManager->GetHelper();
  vtkSlicerQWidgetWidget* widget = vtkSlicerQWidgetWidget::SafeDownCast(helper->GetWidget(widgetNode->GetMarkupsDisplayNode()));
  if (!widget)
  {
    qCritical() << Q_FUNC_INFO << ": No widget was found for the GUI widget node. Make sure it has been shown in this view.";
    return false;
  }
  vtkSlicerQWidgetRepresentation* rep = vtkSlicerQWidgetRepresentation::SafeDownCast(widget->GetRepresentation());
  if (!rep)
  {
    qCritical() << Q_FUNC_INFO << ": Invalid widget representation";
    return false;
  }

  // Get plane source
  vtkPlaneSource* planeSource = vtkPlaneSource::SafeDownCast(rep->GetPlaneSource());
  if (!planeSource)
  {
    qCritical() << Q_FUNC_INFO << ": Invalid plane source";
    return false;
  }

  // PlaneSource's Origin/Point1/Point2/Normal are in the GUI widget node's local (node) frame;
  // the representation's actor applies the node's parent transform on top of them (see
  // vtkSlicerQWidgetRepresentation::UpdateFromMRML()), so transform them into world coordinates
  // here to stay consistent with what is actually rendered (and therefore clickable).
  vtkNew<vtkTransform> planeToWorldTransform;
  planeToWorldTransform->SetMatrix(rep->GetPlaneActor()->GetMatrix());

  vtkNew<vtkTransformPolyDataFilter> planeToWorldFilter;
  planeToWorldFilter->SetInputConnection(planeSource->GetOutputPort());
  planeToWorldFilter->SetTransform(planeToWorldTransform);
  planeToWorldFilter->Update();

  // Get plane normal
  double planeNormal[3] = { 0.0, 0.0, 0.0 };
  planeToWorldTransform->TransformVector(planeSource->GetNormal(), planeNormal);
  //std::cout << "Plane normal: [" << planeNormal[0] << ", " << planeNormal[1] << ", " << planeNormal[2] << "] \n";

  // Get plane reference points
  double planePointSW[3] = { 0.0, 0.0, 0.0 }; // bottom left corner
  double planePointSE[3] = { 0.0, 0.0, 0.0 }; // bottom right corner
  double planePointNW[3] = { 0.0, 0.0, 0.0 }; // top left corner
  planeToWorldTransform->TransformPoint(planeSource->GetOrigin(), planePointSW);
  planeToWorldTransform->TransformPoint(planeSource->GetPoint1(), planePointSE);
  planeToWorldTransform->TransformPoint(planeSource->GetPoint2(), planePointNW);
  double translationWtoE[3] = {0.0, 0.0, 0.0};
  vtkMath::Subtract(planePointSE, planePointSW, translationWtoE);
  double planePointNE[3] = { 0.0, 0.0, 0.0 };
  vtkMath::Add(planePointNW, translationWtoE, planePointNE);
  //std::cout << "Plane point NW: [" << planePointNW[0] << ", " << planePointNW[1] << ", " << planePointNW[2] << "] \n";
  //std::cout << "Plane point NE: [" << planePointNE[0] << ", " << planePointNE[1] << ", " << planePointNE[2] << "] \n";
  //std::cout << "Plane point SW: [" << planePointSW[0] << ", " << planePointSW[1] << ", " << planePointSW[2] << "] \n";
  //std::cout << "Plane point SE: [" << planePointSE[0] << ", " << planePointSE[1] << ", " << planePointSE[2] << "] \n";

  // Compute intersection point
  vtkNew<vtkCellLocator> cellLocator;
  cellLocator->SetDataSet(planeToWorldFilter->GetOutput());
  cellLocator->BuildLocator();
  double tolerance = 0.001;
  double t = 0.0;
  double pcoords[3] = { 0.0 };
  int subId = 0;
  vtkIdType cellId = 0;
  vtkNew <vtkGenericCell> cell;
  double intersectionPoint[3]= { 0.0, 0.0, 0.0 };
  int foundIntersection = cellLocator->IntersectWithLine(pointA_transf, pointB_transf, tolerance, t, intersectionPoint, pcoords, subId, cellId, cell);
  if (foundIntersection)
  {
    //std::cout << "Intersection point: [" << intersectionPoint[0] << ", " << intersectionPoint[1] << ", " << intersectionPoint[2] << "] \n";
  }
  else
  {
    //std::cout << "No intersection was found \n";
    return false;
  }

  // Get plane dimensions
  vtkSlicerQWidgetTexture* texture = rep->GetQWidgetTexture();
  QWidget* qWidget = texture->GetWidget();
  if (!qWidget)
  {
    return false;
  }
  QRect rect = qWidget->geometry();
  if (rect.width() < 2 || rect.height() < 2)
  {
    return false;
  }
  //std::cout << "Widget dimensions: width = " << rect.width() << " and height = " << rect.height() << "\n";
  double spacingMmPerPixel = rep->GetSpacingMmPerPixel();
  double bounds[6] = {
    -(double)(rect.width() / 2) * spacingMmPerPixel, (double)rect.width() / 2 * spacingMmPerPixel,
    -0.5, 0.5,
    -(double)(rect.height() / 2) * spacingMmPerPixel, (double)rect.height() / 2 * spacingMmPerPixel
  };
  //std::cout << "Widget bounds: [ " << bounds[0] << ", " << bounds[1] << ", " << bounds[2] << ", " << bounds[3] << ", " << bounds[4] << ", " << bounds[5] << "\n";

  // Compute pixel position
  double intersectionPointVector[3] = { intersectionPoint[0] - planePointNW[0], intersectionPoint[1] - planePointNW[1], intersectionPoint[2] - planePointNW[2] };
  double xPlaneAxis[3] = { planePointNE[0] - planePointNW[0], planePointNE[1] - planePointNW[1], planePointNE[2] - planePointNW[2] };
  double yPlaneAxis[3] = { planePointSW[0] - planePointNW[0], planePointSW[1] - planePointNW[1], planePointSW[2] - planePointNW[2] };
  vtkMath::MultiplyScalar(xPlaneAxis, vtkMath::Dot(intersectionPointVector, xPlaneAxis) / vtkMath::Dot(xPlaneAxis, xPlaneAxis));
  vtkMath::MultiplyScalar(yPlaneAxis, vtkMath::Dot(intersectionPointVector, yPlaneAxis) / vtkMath::Dot(yPlaneAxis, yPlaneAxis));
  double xIntersectionPoint[3] = { 0.0, 0.0, 0.0 };
  double yIntersectionPoint[3] = { 0.0, 0.0, 0.0 };
  vtkMath::Add(planePointNW, xPlaneAxis, xIntersectionPoint);
  vtkMath::Add(planePointNW, yPlaneAxis, yIntersectionPoint);
  vtkMath::Subtract(xIntersectionPoint, planePointNW, xIntersectionPoint); // subtract plane origin
  vtkMath::Subtract(yIntersectionPoint, planePointNW, yIntersectionPoint); // subtract plane origin
  double xPositionMm = vtkMath::Norm(xIntersectionPoint);
  double yPositionMm = vtkMath::Norm(yIntersectionPoint);
  //std::cout << "Pointer intersection position (mm): [ " << xPositionMm << ", " << yPositionMm << "] \n";
  int xPositionPixels = xPositionMm / spacingMmPerPixel;
  int yPositionPixels = yPositionMm / spacingMmPerPixel;
  //std::cout << "Pointer intersection position (pixels): [ " << xPositionPixels << ", " << yPositionPixels << "] \n";

  scene = texture->GetScene();
  pixelPosition = QPointF(xPositionPixels, yPositionPixels);
  return true;
}

//-----------------------------------------------------------------------------
bool qSlicerGUIWidgetsModuleWidget::computePointerRay(double origin[3], double direction[3])
{
  vtkMRMLLinearTransformNode* transformNode = vtkMRMLLinearTransformNode::SafeDownCast(
    qSlicerApplication::application()->mrmlScene()->GetFirstNodeByName("PointerTransform"));
  if (!transformNode)
  {
    return false;
  }

  vtkNew<vtkMatrix4x4> matrixTransformToWorld;
  transformNode->GetMatrixTransformToWorld(matrixTransformToWorld);

  double originLocal_h[4] = { 0.0, 0.0, 0.0, 1.0 };
  double origin_h[4] = { 0.0, 0.0, 0.0, 0.0 };
  matrixTransformToWorld->MultiplyPoint(originLocal_h, origin_h);
  origin[0] = origin_h[0];
  origin[1] = origin_h[1];
  origin[2] = origin_h[2];

  // w=0 so the translation column of the matrix drops out, transforming this as a direction
  // vector rather than a point (valid because the matrix is a rigid/affine controller pose).
  double directionLocal_h[4] = { 0.0, 0.0, -1.0, 0.0 };
  double direction_h[4] = { 0.0, 0.0, 0.0, 0.0 };
  matrixTransformToWorld->MultiplyPoint(directionLocal_h, direction_h);
  direction[0] = direction_h[0];
  direction[1] = direction_h[1];
  direction[2] = direction_h[2];
  vtkMath::Normalize(direction);

  return true;
}

//-----------------------------------------------------------------------------
bool qSlicerGUIWidgetsModuleWidget::computeMoveHandlePointerHit(vtkMRMLGUIWidgetNode*& hitWidgetNode, double worldPickedPoint[3])
{
  hitWidgetNode = nullptr;

  double rayOrigin[3] = { 0.0 };
  double rayDirection[3] = { 0.0 };
  if (!this->computePointerRay(rayOrigin, rayDirection))
  {
    return false;
  }

  // Must match the pointer length used elsewhere (onSetUpInteractionButtonClicked,
  // computeWidgetPointerHit): a handle beyond this distance is not reachable by the visible beam.
  const double maxDistanceForInteraction = 2000.0; // mm
  double rayEnd[3] = {
    rayOrigin[0] + rayDirection[0] * maxDistanceForInteraction,
    rayOrigin[1] + rayDirection[1] * maxDistanceForInteraction,
    rayOrigin[2] + rayDirection[2] * maxDistanceForInteraction
  };

  vtkMRMLScene* scene = qSlicerApplication::application()->mrmlScene();
  double bestDistance = maxDistanceForInteraction;
  bool found = false;

  // Iterate over the GUI widget nodes currently in the scene -- NOT over GUIWidgetsMap: its keys
  // are raw pointers that dangle once a node is deleted (use-after-free on the next trigger
  // press, which runs this hit test first regardless of what is aimed at). onSceneNodeRemoved()
  // prunes the map, but the scene is the authority on which widgets exist.
  std::vector<vtkMRMLNode*> guiWidgetNodes;
  scene->GetNodesByClass("vtkMRMLGUIWidgetNode", guiWidgetNodes);
  for (vtkMRMLNode* candidateNode : guiWidgetNodes)
  {
    vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(candidateNode);
    if (!widgetNode || !widgetNode->GetName())
    {
      continue;
    }
    std::string handleName = std::string(widgetNode->GetName()) + "_MoveHandle";
    vtkMRMLModelNode* handleModelNode = vtkMRMLModelNode::SafeDownCast(scene->GetFirstNodeByName(handleName.c_str()));
    if (!handleModelNode || !handleModelNode->GetPolyData() || !handleModelNode->GetParentTransformNode())
    {
      continue;
    }

    // Ray-cast against the handle's actual geometry transformed to world -- the same pattern
    // computeWidgetPointerHit() uses for the widget plane, so any handle shape works.
    vtkNew<vtkMatrix4x4> handleToWorldMatrix;
    handleModelNode->GetParentTransformNode()->GetMatrixTransformToWorld(handleToWorldMatrix);
    vtkNew<vtkTransform> handleToWorldTransform;
    handleToWorldTransform->SetMatrix(handleToWorldMatrix);

    vtkNew<vtkTransformPolyDataFilter> handleToWorldFilter;
    handleToWorldFilter->SetInputData(handleModelNode->GetPolyData());
    handleToWorldFilter->SetTransform(handleToWorldTransform);
    handleToWorldFilter->Update();

    vtkNew<vtkCellLocator> cellLocator;
    cellLocator->SetDataSet(handleToWorldFilter->GetOutput());
    cellLocator->BuildLocator();
    double tolerance = 0.001;
    double t = 0.0;
    double pcoords[3] = { 0.0 };
    int subId = 0;
    vtkIdType cellId = 0;
    vtkNew<vtkGenericCell> cell;
    double intersectionPoint[3] = { 0.0 };
    if (!cellLocator->IntersectWithLine(rayOrigin, rayEnd, tolerance, t, intersectionPoint, pcoords, subId, cellId, cell))
    {
      continue;
    }

    double originToIntersection[3] = { 0.0 };
    vtkMath::Subtract(intersectionPoint, rayOrigin, originToIntersection);
    double distance = vtkMath::Norm(originToIntersection);
    if (distance >= bestDistance)
    {
      // Farther than the closest hit found so far.
      continue;
    }

    bestDistance = distance;
    found = true;
    hitWidgetNode = widgetNode;
    worldPickedPoint[0] = intersectionPoint[0];
    worldPickedPoint[1] = intersectionPoint[1];
    worldPickedPoint[2] = intersectionPoint[2];
  }

  return found;
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::startMoveHandleDrag(vtkMRMLGUIWidgetNode* widgetNode, const double worldPickedPoint[3])
{
  if (!widgetNode || !widgetNode->GetName())
  {
    return;
  }

  vtkMRMLScene* scene = qSlicerApplication::application()->mrmlScene();
  std::string moveTransformName = std::string(widgetNode->GetName()) + "_MoveTransform";
  vtkMRMLLinearTransformNode* moveTransformNode = vtkMRMLLinearTransformNode::SafeDownCast(
    scene->GetFirstNodeByName(moveTransformName.c_str()));
  if (!moveTransformNode)
  {
    return;
  }

  double rayOrigin[3] = { 0.0 };
  double rayDirection[3] = { 0.0 };
  if (!this->computePointerRay(rayOrigin, rayDirection))
  {
    return;
  }

  this->DraggingMoveTransformNode = moveTransformNode;
  double originToPickedPoint[3] = { 0.0 };
  vtkMath::Subtract(worldPickedPoint, rayOrigin, originToPickedPoint);
  this->DraggingHandleDistance = vtkMath::Norm(originToPickedPoint);

  // moveTransformNode has no parent transform of its own (it is a top-level node, see
  // addMoveHandle()), so ToWorld == ToParent here and in updateMoveHandleDrag().
  vtkNew<vtkMatrix4x4> currentMoveTransformToWorld;
  moveTransformNode->GetMatrixTransformToWorld(currentMoveTransformToWorld);

  // The drag pivots around the exact point picked on the handle (not the handle's center): store
  // it in the move transform's local frame, so updateMoveHandleDrag() can keep that same material
  // point on the ray. Pivoting around e.g. the handle's center instead would make the panel jump
  // sideways at grab start whenever the pick lands away from the center of the bar.
  vtkNew<vtkMatrix4x4> worldToMoveTransform;
  vtkMatrix4x4::Invert(currentMoveTransformToWorld, worldToMoveTransform);
  double worldPickedPoint_h[4] = { worldPickedPoint[0], worldPickedPoint[1], worldPickedPoint[2], 1.0 };
  double localGrabPoint_h[4] = { 0.0 };
  worldToMoveTransform->MultiplyPoint(worldPickedPoint_h, localGrabPoint_h);
  this->DraggingHandleLocalGrabPoint[0] = localGrabPoint_h[0];
  this->DraggingHandleLocalGrabPoint[1] = localGrabPoint_h[1];
  this->DraggingHandleLocalGrabPoint[2] = localGrabPoint_h[2];

  // Seed the drag's rotation from the transform's current one; updateMoveHandleDrag() re-aims it
  // toward the HMD every tick (and keeps this seed whenever it cannot).
  for (int row = 0; row < 3; ++row)
  {
    for (int col = 0; col < 3; ++col)
    {
      this->DraggingMoveTransformRotation[row][col] = currentMoveTransformToWorld->GetElement(row, col);
    }
  }

  this->DraggingHandle = true;
  this->DragTimer->start();
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::updateMoveHandleDrag()
{
  if (!this->DraggingMoveTransformNode)
  {
    // The transform node was removed from the scene mid-drag.
    this->DraggingHandle = false;
    this->DragTimer->stop();
    return;
  }

  double rayOrigin[3] = { 0.0 };
  double rayDirection[3] = { 0.0 };
  if (!this->computePointerRay(rayOrigin, rayDirection))
  {
    return;
  }

  double desiredWorldGrabPoint[3] = {
    rayOrigin[0] + rayDirection[0] * this->DraggingHandleDistance,
    rayOrigin[1] + rayDirection[1] * this->DraggingHandleDistance,
    rayOrigin[2] + rayDirection[2] * this->DraggingHandleDistance
  };

  // Re-aim the panel so it keeps facing the user wherever it is dragged, while always remaining
  // upright: its up axis (local +Z; see vtkSlicerQWidgetRepresentation::PlaceWidget(): the plane
  // lies in the local XZ plane, +Z up -- which is also why addMoveHandle() puts the handle bar
  // below the widget on -Z) is pinned to the room's up direction (the render window's PhysicalViewUp, in world
  // coordinates -- NOT a world-axis constant, since complex-gesture world rotations change what
  // direction "up in the room" is in world), and the plane's front normal (local +Y) is yawed
  // about that axis toward the HMD -- a cylindrical billboard, so the panel never rolls or tilts.
  // The HMD transform node is kept updated by the VR render loop because
  // onSetUpInteractionButtonClicked() enables HMDTransformUpdate. If the HMD node or the VR view
  // widget is unavailable, or the aim is degenerate (HMD straight above/below the handle), the
  // previous tick's rotation is kept for this tick instead.
  vtkMRMLLinearTransformNode* hmdTransformNode = vtkMRMLLinearTransformNode::SafeDownCast(
    qSlicerApplication::application()->mrmlScene()->GetFirstNodeByName("VirtualReality.HMD"));
  double zAxis[3] = { 0.0 };
  if (hmdTransformNode && this->VRViewWidget && this->VRViewWidget->physicalViewUp(zAxis)
    && vtkMath::Normalize(zAxis) > 1e-3)
  {
    vtkNew<vtkMatrix4x4> hmdToWorld;
    hmdTransformNode->GetMatrixTransformToWorld(hmdToWorld);
    double hmdPosition[3] = {
      hmdToWorld->GetElement(0, 3), hmdToWorld->GetElement(1, 3), hmdToWorld->GetElement(2, 3) };

    // Facing direction toward the HMD, projected onto the horizontal plane (perpendicular to the
    // up axis) so only the yaw tracks the headset.
    double yAxis[3] = { 0.0 };
    vtkMath::Subtract(hmdPosition, desiredWorldGrabPoint, yAxis);
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
        this->DraggingMoveTransformRotation[row][0] = xAxis[row];
        this->DraggingMoveTransformRotation[row][1] = yAxis[row];
        this->DraggingMoveTransformRotation[row][2] = zAxis[row];
      }
    }
  }

  // Solve for the translation that places the grab point (fixed in the move transform's local
  // frame, see startMoveHandleDrag()) at desiredWorldGrabPoint, given the rotation R chosen above:
  //   R * localGrabPoint + T = desired  =>  T = desired - R * localGrabPoint.
  double rotatedLocalGrabPoint[3] = { 0.0 };
  for (int row = 0; row < 3; ++row)
  {
    rotatedLocalGrabPoint[row] =
      this->DraggingMoveTransformRotation[row][0] * this->DraggingHandleLocalGrabPoint[0] +
      this->DraggingMoveTransformRotation[row][1] * this->DraggingHandleLocalGrabPoint[1] +
      this->DraggingMoveTransformRotation[row][2] * this->DraggingHandleLocalGrabPoint[2];
  }

  vtkNew<vtkMatrix4x4> newMoveTransformToWorld;
  newMoveTransformToWorld->Identity();
  for (int row = 0; row < 3; ++row)
  {
    for (int col = 0; col < 3; ++col)
    {
      newMoveTransformToWorld->SetElement(row, col, this->DraggingMoveTransformRotation[row][col]);
    }
    newMoveTransformToWorld->SetElement(row, 3, desiredWorldGrabPoint[row] - rotatedLocalGrabPoint[row]);
  }

  // See PositionProp()'s rationale (vtkVirtualRealityViewInteractorStyleDelegate.cxx) for why
  // SetMatrixTransformToParent() is used here rather than fetching/SetMatrix()-ing the underlying
  // vtkTransform directly: it guarantees TransformModifiedEvent actually fires.
  this->DraggingMoveTransformNode->SetMatrixTransformToParent(newMoveTransformToWorld);
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onStartInteractionButtonClicked()
{
  QGraphicsScene* scene = nullptr;
  QPointF pixelPosition;
  if (!this->computeWidgetPointerHit(scene, pixelPosition))
  {
    return;
  }

  // Send press event
  QGraphicsSceneMouseEvent pressEvent(QEvent::GraphicsSceneMousePress);
  pressEvent.setScenePos(pixelPosition);
  pressEvent.setButton(Qt::LeftButton);
  pressEvent.setButtons(Qt::LeftButton);
  QApplication::sendEvent(scene, &pressEvent);

  // Send release event
  QGraphicsSceneMouseEvent releaseEvent(QEvent::GraphicsSceneMouseRelease);
  releaseEvent.setScenePos(pixelPosition);
  releaseEvent.setButton(Qt::LeftButton);
  QApplication::sendEvent(scene, &releaseEvent);
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onTriggerButtonPressed()
{
  // Move handles are hit-tested first and regardless of widget visibility (see addMoveHandle():
  // the handle model is always visible even while its widget is hidden), so grabbing the handle
  // always takes priority over -- and does not require -- clicking into the widget itself.
  vtkMRMLGUIWidgetNode* hitHandleWidgetNode = nullptr;
  double worldPickedPoint[3] = { 0.0 };
  if (this->computeMoveHandlePointerHit(hitHandleWidgetNode, worldPickedPoint))
  {
    this->startMoveHandleDrag(hitHandleWidgetNode, worldPickedPoint);
    return;
  }

  qSlicerApplication* app = qSlicerApplication::application();
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(app->mrmlScene()->GetFirstNodeByName("HomeWidgetNode"));
  vtkMRMLDisplayNode* displayNode = widgetNode ? widgetNode->GetDisplayNode() : nullptr;
  if (!displayNode || !displayNode->GetVisibility())
  {
    // Widget is hidden: leave the trigger unbound, so the default grab&move interaction
    // (driven by the grip buttons) is unaffected.
    return;
  }

  QGraphicsScene* scene = nullptr;
  QPointF pixelPosition;
  if (!this->computeWidgetPointerHit(scene, pixelPosition))
  {
    return;
  }

  QGraphicsSceneMouseEvent pressEvent(QEvent::GraphicsSceneMousePress);
  pressEvent.setScenePos(pixelPosition);
  pressEvent.setButton(Qt::LeftButton);
  pressEvent.setButtons(Qt::LeftButton);
  QApplication::sendEvent(scene, &pressEvent);

  this->Dragging = true;
  this->LastDragScene = scene;
  this->LastDragPixelPosition = pixelPosition;
  this->DragTimer->start();
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onTriggerButtonReleased()
{
  this->DragTimer->stop();

  if (this->DraggingHandle)
  {
    this->DraggingHandle = false;
    this->DraggingMoveTransformNode = nullptr;
    return;
  }

  if (!this->Dragging)
  {
    return;
  }
  this->Dragging = false;

  // Prefer a fresh ray-cast for the release position, but fall back to the last position tracked
  // by onTriggerButtonPressed()/onDragTimerTimeout() if the pointer has drifted off the widget by
  // release time. The release must still be delivered to whatever item captured the press (e.g.
  // a slider handle), or that item is left thinking the mouse button is still held down.
  QGraphicsScene* scene = nullptr;
  QPointF pixelPosition;
  if (!this->computeWidgetPointerHit(scene, pixelPosition))
  {
    scene = this->LastDragScene;
    pixelPosition = this->LastDragPixelPosition;
  }
  if (!scene)
  {
    return;
  }

  QGraphicsSceneMouseEvent releaseEvent(QEvent::GraphicsSceneMouseRelease);
  releaseEvent.setScenePos(pixelPosition);
  releaseEvent.setButton(Qt::LeftButton);
  QApplication::sendEvent(scene, &releaseEvent);

  this->LastDragScene = nullptr;
}

//-----------------------------------------------------------------------------
void qSlicerGUIWidgetsModuleWidget::onDragTimerTimeout()
{
  if (this->DraggingHandle)
  {
    this->updateMoveHandleDrag();
    return;
  }

  QGraphicsScene* scene = nullptr;
  QPointF pixelPosition;
  if (!this->computeWidgetPointerHit(scene, pixelPosition))
  {
    // Pointer has drifted off the widget; skip this tick and keep the last known position/scene
    // as the fallback for onTriggerButtonReleased(), rather than sending a bogus move.
    return;
  }

  QGraphicsSceneMouseEvent moveEvent(QEvent::GraphicsSceneMouseMove);
  moveEvent.setScenePos(pixelPosition);
  moveEvent.setButton(Qt::NoButton);
  moveEvent.setButtons(Qt::LeftButton);
  QApplication::sendEvent(scene, &moveEvent);

  this->LastDragScene = scene;
  this->LastDragPixelPosition = pixelPosition;
}
