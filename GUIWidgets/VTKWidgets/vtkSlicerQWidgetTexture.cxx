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

#include "vtkSlicerQWidgetTexture.h"

#include "vtkSlicerQWidgetImageSource.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkObjectFactory.h>

//------------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerQWidgetTexture);

//------------------------------------------------------------------------------
vtkSlicerQWidgetTexture::vtkSlicerQWidgetTexture()
{
  this->ImageSourceCallbackCommand = vtkCallbackCommand::New();
  this->ImageSourceCallbackCommand->SetClientData(reinterpret_cast<void*>(this));
  this->ImageSourceCallbackCommand->SetCallback(vtkSlicerQWidgetTexture::OnImageSourceModified);
}

//------------------------------------------------------------------------------
vtkSlicerQWidgetTexture::~vtkSlicerQWidgetTexture()
{
  // Deliberately NOT calling SetWidget(nullptr) here: it performs pipeline operations
  // (SetInputConnection), which Register/UnRegister this algorithm -- doing that while the
  // destructor is running (reference count already zero) re-enters `delete this` and crashes
  // with runaway re-entrant destruction. Only drop the observer and our reference to the shared
  // source; vtkAlgorithm's own destructor releases the input connection safely, and the
  // pipeline's reference on the source's producer keeps the image data valid until then.
  if (this->ImageSource)
  {
    this->ImageSource->RemoveObserver(this->ImageSourceCallbackCommand);
    this->ImageSource = nullptr;
  }

  if (this->ImageSourceCallbackCommand)
  {
    this->ImageSourceCallbackCommand->SetClientData(nullptr);
    this->ImageSourceCallbackCommand->Delete();
    this->ImageSourceCallbackCommand = nullptr;
  }
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetTexture::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetTexture::ReleaseGraphicsResources(vtkWindow* win)
{
  this->Superclass::ReleaseGraphicsResources(win);
}

//------------------------------------------------------------------------------
QWidget* vtkSlicerQWidgetTexture::GetWidget()
{
  return this->ImageSource ? this->ImageSource->GetWidget() : nullptr;
}

//------------------------------------------------------------------------------
QGraphicsScene* vtkSlicerQWidgetTexture::GetScene()
{
  return this->ImageSource ? this->ImageSource->GetScene() : nullptr;
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetTexture::SetWidget(QWidget* w)
{
  if (this->GetWidget() == w)
  {
    return;
  }

  if (this->ImageSource)
  {
    // Disconnect the pipeline *before* releasing our reference to the shared source below: if
    // this is the last reference, resetting ImageSource destroys it -- and the vtkTrivialProducer
    // feeding this texture along with it -- as a side effect. Clearing the input connection first
    // ensures this texture is never left wired to an already-freed producer, even momentarily.
    this->SetInputConnection(nullptr);
    this->ImageSource->RemoveObserver(this->ImageSourceCallbackCommand);
    this->ImageSource = nullptr;
  }

  if (w)
  {
    this->ImageSource = vtkSlicerQWidgetImageSource::GetSourceForWidget(w);
    this->ImageSource->AddObserver(vtkCommand::ModifiedEvent, this->ImageSourceCallbackCommand);
    this->SetInputConnection(this->ImageSource->GetOutputPort());
  }

  this->Modified();
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetTexture::OnImageSourceModified(
  vtkObject* vtkNotUsed(caller), unsigned long vtkNotUsed(eid), void* clientData, void* vtkNotUsed(callData))
{
  vtkSlicerQWidgetTexture* self = reinterpret_cast<vtkSlicerQWidgetTexture*>(clientData);
  if (!self)
  {
    return;
  }
  // Forward to this texture's own observers: the owning representation reacts by updating its
  // plane geometry and requesting a render of its own view (see
  // vtkSlicerQWidgetRepresentation::OnTextureModified()). Every view showing the widget gets
  // notified this way through its own texture, symmetrically -- no cross-view coordination.
  self->Modified();
}
