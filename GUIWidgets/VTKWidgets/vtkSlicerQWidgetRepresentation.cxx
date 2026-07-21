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

#include "vtkSlicerQWidgetRepresentation.h"

#include "vtkSlicerQWidgetTexture.h"

// GUI Widget includes
#include "vtkMRMLGUIWidgetNode.h"
#include "vtkMRMLGUIWidgetDisplayNode.h"

// MRML includes
#include <vtkMRMLModelNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkActor.h>
#include <vtkCallbackCommand.h>
#include <vtkCellLocator.h>
#include <vtkEventData.h>
#include <vtkGenericCell.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkOpenGLTexture.h>
#include <vtkPlaneSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

// STD includes
#include <string>

// Qt includes
#include <QRect>
#include <QWidget>

vtkStandardNewMacro(vtkSlicerQWidgetRepresentation);

//------------------------------------------------------------------------------
vtkSlicerQWidgetRepresentation::vtkSlicerQWidgetRepresentation()
{
  this->PlaneSource = vtkPlaneSource::New();
  this->PlaneSource->SetOutputPointsPrecision(vtkAlgorithm::DOUBLE_PRECISION);

  this->PlaneMapper = vtkPolyDataMapper::New();
  this->PlaneMapper->SetInputConnection(this->PlaneSource->GetOutputPort());

  this->TextureCallbackCommand = vtkCallbackCommand::New();
  this->TextureCallbackCommand->SetClientData( reinterpret_cast<void *>(this) );
  this->TextureCallbackCommand->SetCallback( vtkSlicerQWidgetRepresentation::OnTextureModified ); 

  this->QWidgetTexture = vtkSlicerQWidgetTexture::New();
  this->QWidgetTexture->AddObserver(vtkCommand::ModifiedEvent, this->TextureCallbackCommand);

  this->PlaneActor = vtkActor::New();
  this->PlaneActor->SetMapper(this->PlaneMapper);
  this->PlaneActor->SetTexture(this->QWidgetTexture);

  this->PlaneActor->GetProperty()->SetAmbient(1.0);
  this->PlaneActor->GetProperty()->SetDiffuse(0.0);

  // Define the point coordinates
  double bounds[6] = {-80, 80, -0.5, 0.5, -50, 50 }; // Width, Depth, Height

  // Initial creation of the widget, serves to initialize it
  this->PlaceWidget(bounds);
}

//------------------------------------------------------------------------------
vtkSlicerQWidgetRepresentation::~vtkSlicerQWidgetRepresentation()
{
  if (this->PlaneSource)
  {
    this->PlaneSource->Delete();
    this->PlaneSource = nullptr;
  }
  if (this->PlaneMapper)
  {
    this->PlaneMapper->Delete();
    this->PlaneMapper = nullptr;
  }
  if (this->PlaneActor)
  {
    this->PlaneActor->Delete();
    this->PlaneActor = nullptr;
  }

  if (this->QWidgetTexture)
  {
    this->QWidgetTexture->RemoveObserver(this->TextureCallbackCommand);
    this->QWidgetTexture->Delete();
    this->QWidgetTexture = nullptr;
  }
  if (this->TextureCallbackCommand)
  {
    this->TextureCallbackCommand->SetClientData(nullptr);
    this->TextureCallbackCommand->Delete();
    this->TextureCallbackCommand = nullptr;
  }
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetRepresentation::SetWidget(QWidget* w)
{
  // just pass down to the QWidgetTexture
  this->QWidgetTexture->SetWidget(w);
  this->Modified();
}

//------------------------------------------------------------------------------
double* vtkSlicerQWidgetRepresentation::GetBounds()
{
  //this->BuildRepresentation();
  return this->PlaneActor->GetBounds();
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetRepresentation::GetActors(vtkPropCollection* pc)
{
  this->Superclass::GetActors(pc);
  this->PlaneActor->GetActors(pc);
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetRepresentation::ReleaseGraphicsResources(vtkWindow* w)
{
  this->Superclass::ReleaseGraphicsResources(w);
  this->PlaneActor->ReleaseGraphicsResources(w);
  this->PlaneMapper->ReleaseGraphicsResources(w);
  this->QWidgetTexture->ReleaseGraphicsResources(w);
}

//------------------------------------------------------------------------------
int vtkSlicerQWidgetRepresentation::RenderOpaqueGeometry(vtkViewport* v)
{
  int count = this->Superclass::RenderOpaqueGeometry(v);

  if (this->PlaneActor->GetVisibility())
  {
    this->PlaneActor->SetPropertyKeys(this->GetPropertyKeys());

    count += this->PlaneActor->RenderOpaqueGeometry(v);
  }

  return count;
}

//------------------------------------------------------------------------------
int vtkSlicerQWidgetRepresentation::RenderTranslucentPolygonalGeometry(vtkViewport* viewport)
{
  int count=0;
  count = this->Superclass::RenderTranslucentPolygonalGeometry(viewport);
  if (this->PlaneActor->GetVisibility())
  {
    // The internal actor needs to share property keys.
    // This ensures the mapper state is consistent and allows depth peeling to work as expected.
    this->PlaneActor->SetPropertyKeys(this->GetPropertyKeys());

    count += this->PlaneActor->RenderTranslucentPolygonalGeometry(viewport);
  }
  return count;
}

//------------------------------------------------------------------------------
vtkTypeBool vtkSlicerQWidgetRepresentation::HasTranslucentPolygonalGeometry()
{
  if (this->Superclass::HasTranslucentPolygonalGeometry())
  {
    return true;
  }
  if (this->PlaneActor->GetVisibility() && this->PlaneActor->HasTranslucentPolygonalGeometry())
  {
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  // this->InteractionState is printed in superclass
  // this is commented to avoid PrintSelf errors
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetRepresentation::PlaceWidget(double bds[6])
{
  this->PlaneSource->SetOrigin(bds[1], bds[2], bds[4]);
  this->PlaneSource->SetPoint1(bds[0], bds[2], bds[4]);
  this->PlaneSource->SetPoint2(bds[1], bds[2], bds[5]);
}

//----------------------------------------------------------------------
void vtkSlicerQWidgetRepresentation::UpdateFromMRML(vtkMRMLNode* caller, unsigned long event, void *callData /*=nullptr*/)
{
  Superclass::UpdateFromMRML(caller, event, callData);

  this->NeedToRenderOn();

  vtkMRMLGUIWidgetNode* guiWidgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(this->GetMarkupsNode());
  vtkMRMLGUIWidgetDisplayNode* displayNode = vtkMRMLGUIWidgetDisplayNode::SafeDownCast(this->GetMarkupsDisplayNode());
  if (!guiWidgetNode || !this->IsDisplayable() || !displayNode)
  {
    this->VisibilityOff();
    return;
  }

  if (guiWidgetNode->GetWidget() != this->GetQWidgetTexture()->GetWidget())
  {
    if (guiWidgetNode->GetWidget() == nullptr)
    {
      this->QWidgetTexture->SetWidget(nullptr);
    }
    else
    {
      QWidget* widget = reinterpret_cast<QWidget*>(guiWidgetNode->GetWidget());
      if (widget != this->QWidgetTexture->GetWidget())
      {
        this->QWidgetTexture->SetWidget(widget);
      }
    }
  }

  if (!this->QWidgetTexture->GetWidget() || !this->ViewNode)
  {
    this->VisibilityOff();
    this->PlaneActor->SetVisibility(false);
    return;
  }

  // Respect the node's parent transform (e.g. a controller transform the widget is attached to),
  // so the rendered plane follows it instead of always sitting at PlaneSource's fixed local
  // position. PlaneSource's Origin/Point1/Point2 remain in this node-local frame; callers that
  // need world-space corners (e.g. ray-casting for picking) must apply this same matrix.
  vtkMRMLTransformNode* transformNode = guiWidgetNode->GetParentTransformNode();
  if (transformNode)
  {
    vtkNew<vtkMatrix4x4> transformToWorld;
    transformNode->GetMatrixTransformToWorld(transformToWorld);
    this->PlaneActor->SetUserMatrix(transformToWorld);
  }
  else
  {
    this->PlaneActor->SetUserMatrix(nullptr);
  }

  this->VisibilityOn();
  this->PlaneActor->SetVisibility(true);
}

//------------------------------------------------------------------------------
bool vtkSlicerQWidgetRepresentation::ComputeInteractionPixelPosition(
  const double rayOrigin[3], const double rayDirection[3], QPointF& pixelPosition, double& distance2)
{
  QWidget* qWidget = this->QWidgetTexture->GetWidget();
  if (!qWidget)
  {
    return false;
  }
  QRect rect = qWidget->geometry();
  if (rect.width() < 2 || rect.height() < 2)
  {
    return false;
  }

  // Must match the VR laser beam's visible length (see
  // qSlicerGUIWidgetsModuleWidget::onSetUpInteractionButtonClicked()'s maxDistanceForInteraction):
  // a hit beyond this distance is not reachable by the visible beam.
  const double maxDistanceForInteraction = 2000.0; // mm
  double rayEnd[3] = {
    rayOrigin[0] + rayDirection[0] * maxDistanceForInteraction,
    rayOrigin[1] + rayDirection[1] * maxDistanceForInteraction,
    rayOrigin[2] + rayDirection[2] * maxDistanceForInteraction
  };

  // PlaneSource's Origin/Point1/Point2 are in the GUI widget node's local (node) frame; the actor
  // applies the node's parent transform on top of them (see UpdateFromMRML()), so transform them
  // into world coordinates here to stay consistent with what is actually rendered (and therefore
  // clickable).
  vtkNew<vtkTransform> planeToWorldTransform;
  planeToWorldTransform->SetMatrix(this->PlaneActor->GetMatrix());

  vtkNew<vtkTransformPolyDataFilter> planeToWorldFilter;
  planeToWorldFilter->SetInputConnection(this->PlaneSource->GetOutputPort());
  planeToWorldFilter->SetTransform(planeToWorldTransform);
  planeToWorldFilter->Update();

  double planePointSW[3] = { 0.0 }; // bottom left corner
  double planePointSE[3] = { 0.0 }; // bottom right corner
  double planePointNW[3] = { 0.0 }; // top left corner
  planeToWorldTransform->TransformPoint(this->PlaneSource->GetOrigin(), planePointSW);
  planeToWorldTransform->TransformPoint(this->PlaneSource->GetPoint1(), planePointSE);
  planeToWorldTransform->TransformPoint(this->PlaneSource->GetPoint2(), planePointNW);
  double translationWtoE[3] = { 0.0 };
  vtkMath::Subtract(planePointSE, planePointSW, translationWtoE);
  double planePointNE[3] = { 0.0 };
  vtkMath::Add(planePointNW, translationWtoE, planePointNE);

  vtkNew<vtkCellLocator> cellLocator;
  cellLocator->SetDataSet(planeToWorldFilter->GetOutput());
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
    return false;
  }

  double originToIntersection[3] = { 0.0 };
  vtkMath::Subtract(intersectionPoint, rayOrigin, originToIntersection);
  distance2 = vtkMath::Dot(originToIntersection, originToIntersection);

  // Project the hit point onto the plane's local X/Y axes (NW corner as origin) to get its
  // position in mm within the plane, then convert to pixels.
  double intersectionPointVector[3] = { intersectionPoint[0] - planePointNW[0], intersectionPoint[1] - planePointNW[1],
    intersectionPoint[2] - planePointNW[2] };
  double xPlaneAxis[3] = {
    planePointNE[0] - planePointNW[0], planePointNE[1] - planePointNW[1], planePointNE[2] - planePointNW[2] };
  double yPlaneAxis[3] = {
    planePointSW[0] - planePointNW[0], planePointSW[1] - planePointNW[1], planePointSW[2] - planePointNW[2] };
  vtkMath::MultiplyScalar(xPlaneAxis, vtkMath::Dot(intersectionPointVector, xPlaneAxis) / vtkMath::Dot(xPlaneAxis, xPlaneAxis));
  vtkMath::MultiplyScalar(yPlaneAxis, vtkMath::Dot(intersectionPointVector, yPlaneAxis) / vtkMath::Dot(yPlaneAxis, yPlaneAxis));
  double xPositionMm = vtkMath::Norm(xPlaneAxis);
  double yPositionMm = vtkMath::Norm(yPlaneAxis);

  pixelPosition = QPointF(xPositionMm / this->SpacingMmPerPixel, yPositionMm / this->SpacingMmPerPixel);
  return true;
}

//------------------------------------------------------------------------------
bool vtkSlicerQWidgetRepresentation::ComputeMoveHandleHit(
  const double rayOrigin[3], const double rayDirection[3], double worldHitPoint[3], double& distance2)
{
  vtkMRMLGUIWidgetNode* widgetNode = vtkMRMLGUIWidgetNode::SafeDownCast(this->GetMarkupsNode());
  if (!widgetNode)
  {
    return false;
  }

  vtkMRMLModelNode* handleModelNode = widgetNode->GetMoveHandleNode();
  if (!handleModelNode || !handleModelNode->GetPolyData() || !handleModelNode->GetParentTransformNode())
  {
    return false;
  }

  // Must match ComputeInteractionPixelPosition()'s own maxDistanceForInteraction -- the two ray
  // casts (this widget's move handle vs. its own panel) need to agree on how far out interaction
  // reaches.
  const double maxDistanceForInteraction = 2000.0; // mm
  double rayEnd[3] = {
    rayOrigin[0] + rayDirection[0] * maxDistanceForInteraction,
    rayOrigin[1] + rayDirection[1] * maxDistanceForInteraction,
    rayOrigin[2] + rayDirection[2] * maxDistanceForInteraction
  };

  // Ray-cast against the handle's actual geometry transformed to world -- the same pattern as
  // ComputeInteractionPixelPosition() uses for the widget plane, so any handle shape works.
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
  if (!cellLocator->IntersectWithLine(rayOrigin, rayEnd, tolerance, t, worldHitPoint, pcoords, subId, cellId, cell))
  {
    return false;
  }

  double originToHit[3] = { 0.0 };
  vtkMath::Subtract(worldHitPoint, rayOrigin, originToHit);
  distance2 = vtkMath::Dot(originToHit, originToHit);
  return true;
}

//---------------------------------------------------------------------------
void vtkSlicerQWidgetRepresentation::OnTextureModified(
  vtkObject* vtkNotUsed(caller), unsigned long vtkNotUsed(eid), void* clientData, void* vtkNotUsed(callData))
{
  vtkSlicerQWidgetRepresentation* self = reinterpret_cast<vtkSlicerQWidgetRepresentation*>(clientData);

  // Redefine widget plane. vtkPlaneSource's setters no-op when the values are unchanged, so
  // this is idempotent for the common content-only texture updates.
  QWidget* widget = self->QWidgetTexture->GetWidget();
  if (!widget)
  {
    return;
  }

  QRect rect = widget->geometry();
  if (rect.width() < 2 || rect.height() < 2)
  {
    return;
  }
  double bounds[6] = {
    -(double)(rect.width()/2)*self->SpacingMmPerPixel, (double)rect.width()/2*self->SpacingMmPerPixel,
    -0.5, 0.5,
    -(double)(rect.height()/2)*self->SpacingMmPerPixel, (double)rect.height()/2*self->SpacingMmPerPixel
  };
  self->PlaceWidget(bounds);

  // Trigger rendering of this representation's own view. Every view showing the widget gets its
  // own copy of this notification through its own texture, which forwards the shared image
  // source's ModifiedEvent (see vtkSlicerQWidgetImageSource) -- so there is no need for any
  // cross-view coordination here, and no VR special-casing.
  if (self->GetViewNode())
  {
    self->GetViewNode()->Modified();
  }
}
