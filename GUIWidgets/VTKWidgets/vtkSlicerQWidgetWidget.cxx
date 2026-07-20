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
#include "vtkMRMLSliceNode.h"

// Qt includes
#include <QApplication>
#include <QEvent>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QWidget>

// VTK includes
#include "vtkCallbackCommand.h"
#include "vtkCommand.h"
#include "vtkEvent.h"
#include "vtkEventData.h"
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
bool vtkSlicerQWidgetWidget::CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& distance2)
{
  vtkSlicerQWidgetRepresentation* rep = this->GetQWidgetRepresentation();
  if (!rep || !eventData)
  {
    return false;
  }

  vtkEventDataDevice3D* deviceEventData = eventData->GetAsEventDataDevice3D();
  if (!deviceEventData)
  {
    return false;
  }

  // Once a press has been claimed, keep this widget locked onto the drag (Move3DEvent for
  // continued dragging, and the matching Pick3DEvent release) regardless of where the ray points
  // by the time those events arrive -- otherwise a fast-moving ray could drift off the plane
  // mid-drag and orphan the release, leaving the embedded widget's scene thinking the mouse
  // button is still held down. Mirrors vtkSlicerPlaneWidget's WidgetStateTranslatePlane pattern.
  // Restricted to the device that actually started the drag (see ActiveDevice doc comment): both
  // controllers independently fire Move3DEvent every frame, and claiming it regardless of device
  // would make the drag alternate between both controllers' rays instead of following one.
  if (this->WidgetState == WidgetStateActive)
  {
    if (deviceEventData->GetDevice() != this->ActiveDevice)
    {
      return false;
    }
    distance2 = 0.0;
    return true;
  }

  if (eventData->GetType() != vtkCommand::Pick3DEvent)
  {
    return false;
  }
  if (deviceEventData->GetAction() != vtkEventDataAction::Press || !eventData->IsWorldPositionValid())
  {
    return false;
  }

  QPointF pixelPosition;
  return rep->ComputeInteractionPixelPosition(eventData->GetWorldPosition(), eventData->GetWorldDirection(), pixelPosition, distance2);
}

//------------------------------------------------------------------------------
bool vtkSlicerQWidgetWidget::ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData)
{
  vtkSlicerQWidgetRepresentation* rep = this->GetQWidgetRepresentation();
  if (!rep || !eventData)
  {
    return false;
  }
  QGraphicsScene* scene = rep->GetQWidgetTexture()->GetScene();
  if (!scene)
  {
    return false;
  }
  vtkEventDataDevice3D* deviceEventData = eventData->GetAsEventDataDevice3D();
  if (!deviceEventData)
  {
    return false;
  }

  if (eventData->GetType() == vtkCommand::Pick3DEvent && deviceEventData->GetAction() == vtkEventDataAction::Press)
  {
    double distance2 = 0.0;
    if (!rep->ComputeInteractionPixelPosition(
      eventData->GetWorldPosition(), eventData->GetWorldDirection(), this->LastWidgetCoordinates, distance2))
    {
      return false;
    }

    this->ActiveDevice = deviceEventData->GetDevice();

    QGraphicsSceneMouseEvent pressEvent(QEvent::GraphicsSceneMousePress);
    pressEvent.setScenePos(this->LastWidgetCoordinates);
    pressEvent.setButton(Qt::LeftButton);
    pressEvent.setButtons(Qt::LeftButton);
    QApplication::sendEvent(scene, &pressEvent);

    this->SetWidgetState(WidgetStateActive);
    return true;
  }

  if (this->WidgetState != WidgetStateActive)
  {
    return false;
  }

  if (eventData->GetType() == vtkCommand::Move3DEvent)
  {
    // Keep sending moves at the last known pixel position if the ray has drifted off the plane
    // (ComputeInteractionPixelPosition() leaves LastWidgetCoordinates untouched on a miss) --
    // QGraphicsScene's implicit mouse grab from the press still expects updates for whatever item
    // captured it (e.g. a slider handle).
    double distance2 = 0.0;
    rep->ComputeInteractionPixelPosition(eventData->GetWorldPosition(), eventData->GetWorldDirection(), this->LastWidgetCoordinates, distance2);

    QGraphicsSceneMouseEvent moveEvent(QEvent::GraphicsSceneMouseMove);
    moveEvent.setScenePos(this->LastWidgetCoordinates);
    moveEvent.setButton(Qt::NoButton);
    moveEvent.setButtons(Qt::LeftButton);
    QApplication::sendEvent(scene, &moveEvent);
    return true;
  }

  if (eventData->GetType() == vtkCommand::Pick3DEvent && deviceEventData->GetAction() == vtkEventDataAction::Release)
  {
    QGraphicsSceneMouseEvent releaseEvent(QEvent::GraphicsSceneMouseRelease);
    releaseEvent.setScenePos(this->LastWidgetCoordinates);
    releaseEvent.setButton(Qt::LeftButton);
    QApplication::sendEvent(scene, &releaseEvent);

    this->SetWidgetState(WidgetStateIdle);
    this->ActiveDevice = vtkEventDataDevice::Unknown;
    return true;
  }

  return false;
}
