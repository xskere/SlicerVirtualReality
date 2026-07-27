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
#include <vtkBox.h>
#include <vtkCallbackCommand.h>
#include <vtkEventData.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkOpenGLTexture.h>
#include <vtkPlane.h>
#include <vtkPlaneSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTransform.h>

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

  // PlaneSource's Origin/Point1/Point2 are in the GUI widget node's local (node) frame; the actor
  // applies the node's parent transform on top of them (see UpdateFromMRML()), so transform them
  // into world coordinates here to stay consistent with what is actually rendered (and therefore
  // clickable).
  vtkNew<vtkTransform> planeToWorldTransform;
  planeToWorldTransform->SetMatrix(this->PlaneActor->GetMatrix());

  double planePointSW[3] = { 0.0 }; // bottom left corner
  double planePointSE[3] = { 0.0 }; // bottom right corner
  double planePointNW[3] = { 0.0 }; // top left corner
  planeToWorldTransform->TransformPoint(this->PlaneSource->GetOrigin(), planePointSW);
  planeToWorldTransform->TransformPoint(this->PlaneSource->GetPoint1(), planePointSE);
  planeToWorldTransform->TransformPoint(this->PlaneSource->GetPoint2(), planePointNW);

  // Edge vectors of the panel rectangle, from its top-left corner. The panel is a flat rectangle,
  // so an exact ray/plane intersection plus a parametric bounds check does the whole job -- this
  // used to construct a vtkTransformPolyDataFilter and build a vtkCellLocator on every single
  // call, to intersect two triangles.
  double xPlaneAxis[3] = { 0.0 }; // across the panel's width
  vtkMath::Subtract(planePointSE, planePointSW, xPlaneAxis);
  double yPlaneAxis[3] = { 0.0 }; // down the panel's height
  vtkMath::Subtract(planePointSW, planePointNW, yPlaneAxis);

  double planeNormal[3] = { 0.0 };
  vtkMath::Cross(xPlaneAxis, yPlaneAxis, planeNormal);
  if (vtkMath::Normalize(planeNormal) < 1e-6)
  {
    // Degenerate (zero-area) plane; no meaningful surface to hit.
    return false;
  }

  double rayEnd[3] = { 0.0 };
  vtkSlicerQWidgetRepresentation::ComputeRayEnd(rayOrigin, rayDirection, rayEnd);

  double t = 0.0;
  double intersectionPoint[3] = { 0.0 };
  if (!vtkPlane::IntersectWithLine(rayOrigin, rayEnd, planeNormal, planePointNW, t, intersectionPoint))
  {
    return false;
  }

  // Where the hit falls within the rectangle, as a fraction of each edge. This bounds check is
  // what keeps the *infinite* plane from being clickable outside the panel's own extent -- the
  // cell locator used to enforce that implicitly by only intersecting the actual triangles.
  double cornerToIntersection[3] = { 0.0 };
  vtkMath::Subtract(intersectionPoint, planePointNW, cornerToIntersection);
  double xFraction = vtkMath::Dot(cornerToIntersection, xPlaneAxis) / vtkMath::Dot(xPlaneAxis, xPlaneAxis);
  double yFraction = vtkMath::Dot(cornerToIntersection, yPlaneAxis) / vtkMath::Dot(yPlaneAxis, yPlaneAxis);
  if (xFraction < 0.0 || xFraction > 1.0 || yFraction < 0.0 || yFraction > 1.0)
  {
    return false;
  }

  double originToIntersection[3] = { 0.0 };
  vtkMath::Subtract(intersectionPoint, rayOrigin, originToIntersection);
  distance2 = vtkMath::Dot(originToIntersection, originToIntersection);

  double xPositionMm = xFraction * vtkMath::Norm(xPlaneAxis);
  double yPositionMm = yFraction * vtkMath::Norm(yPlaneAxis);
  pixelPosition = QPointF(xPositionMm / this->SpacingMmPerPixel, yPositionMm / this->SpacingMmPerPixel);
  return true;
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetRepresentation::ComputeRayEnd(
  const double rayOrigin[3], const double rayDirection[3], double rayEnd[3])
{
  const double maxDistance = vtkSlicerQWidgetRepresentation::GetInteractionMaxDistanceMm();
  rayEnd[0] = rayOrigin[0] + rayDirection[0] * maxDistance;
  rayEnd[1] = rayOrigin[1] + rayDirection[1] * maxDistance;
  rayEnd[2] = rayOrigin[2] + rayDirection[2] * maxDistance;
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

  // Intersect the handle's oriented bounding box rather than its tessellated geometry: taking the
  // ray into the handle's own frame makes the box axis-aligned there, reducing the test to a few
  // comparisons with no locator to build per call.
  //
  // addMoveHandle() builds the handle from a vtkCubeSource, so for the handle as it actually
  // exists this is exact rather than an approximation. Were a non-box handle shape introduced, it
  // would become grabbable by its bounding box -- more forgiving than its true surface, which for
  // a grab target is the desirable direction to err in.
  vtkNew<vtkMatrix4x4> handleToWorldMatrix;
  handleModelNode->GetParentTransformNode()->GetMatrixTransformToWorld(handleToWorldMatrix);
  vtkNew<vtkMatrix4x4> worldToHandleMatrix;
  vtkMatrix4x4::Invert(handleToWorldMatrix, worldToHandleMatrix);

  double rayEnd[3] = { 0.0 };
  vtkSlicerQWidgetRepresentation::ComputeRayEnd(rayOrigin, rayDirection, rayEnd);

  double rayOriginWorld_h[4] = { rayOrigin[0], rayOrigin[1], rayOrigin[2], 1.0 };
  double rayEndWorld_h[4] = { rayEnd[0], rayEnd[1], rayEnd[2], 1.0 };
  double rayOriginHandle_h[4] = { 0.0 };
  double rayEndHandle_h[4] = { 0.0 };
  worldToHandleMatrix->MultiplyPoint(rayOriginWorld_h, rayOriginHandle_h);
  worldToHandleMatrix->MultiplyPoint(rayEndWorld_h, rayEndHandle_h);
  double rayOriginHandle[3] = { rayOriginHandle_h[0], rayOriginHandle_h[1], rayOriginHandle_h[2] };
  double rayEndHandle[3] = { rayEndHandle_h[0], rayEndHandle_h[1], rayEndHandle_h[2] };

  double handleBounds[6] = { 0.0 };
  handleModelNode->GetPolyData()->GetBounds(handleBounds);

  // Segment form (t clamped to [0,1]), so reach beyond GetInteractionMaxDistanceMm() and anything
  // behind the ray origin are both excluded, matching ComputeInteractionPixelPosition().
  double tEntry = 0.0;
  double tExit = 0.0;
  double entryPointHandle[3] = { 0.0 };
  double exitPointHandle[3] = { 0.0 };
  int entryPlane = -1;
  int exitPlane = -1;
  if (!vtkBox::IntersectWithLine(
        handleBounds, rayOriginHandle, rayEndHandle, tEntry, tExit, entryPointHandle, exitPointHandle, entryPlane, exitPlane))
  {
    return false;
  }

  double entryPointHandle_h[4] = { entryPointHandle[0], entryPointHandle[1], entryPointHandle[2], 1.0 };
  double worldHitPoint_h[4] = { 0.0 };
  handleToWorldMatrix->MultiplyPoint(entryPointHandle_h, worldHitPoint_h);
  worldHitPoint[0] = worldHitPoint_h[0];
  worldHitPoint[1] = worldHitPoint_h[1];
  worldHitPoint[2] = worldHitPoint_h[2];

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
