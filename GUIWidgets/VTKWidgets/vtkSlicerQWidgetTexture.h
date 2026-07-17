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

#ifndef vtkSlicerQWidgetTexture_h
#define vtkSlicerQWidgetTexture_h

#include "vtkSlicerGUIWidgetsModuleVTKWidgetsExport.h"

// VTK includes
#include <vtkOpenGLTexture.h>
#include <vtkSmartPointer.h>

class QGraphicsScene;
class QWidget;
class vtkCallbackCommand;
class vtkSlicerQWidgetImageSource;

/**
 * @class vtkSlicerQWidgetTexture
 * @brief Per-view OpenGL texture rendering a QWidget
 *
 * One instance exists per view showing a given QWidget (each
 * vtkSlicerQWidgetRepresentation owns one). The widget image itself comes from
 * the vtkSlicerQWidgetImageSource shared by all textures showing the same
 * widget -- see that class for why the Qt side must be shared. This class only
 * connects its pipeline input to the shared image and forwards the source's
 * ModifiedEvent to its own observers (the owning representation), so each view
 * updates independently through normal VTK mechanisms.
 */
class VTK_SLICER_GUIWIDGETS_MODULE_VTKWIDGETS_EXPORT vtkSlicerQWidgetTexture : public vtkOpenGLTexture
{
public:
  static vtkSlicerQWidgetTexture* New();
  vtkTypeMacro(vtkSlicerQWidgetTexture, vtkOpenGLTexture);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /**
   * Set/Get the QWidget that this texture renders. Setting the widget acquires the shared
   * image source for it (see vtkSlicerQWidgetImageSource::GetSourceForWidget()).
   */
  void SetWidget(QWidget* w);
  QWidget* GetWidget();
  ///@}

  /**
   * Get the QGraphicsScene the widget is embedded in (owned by the shared image source); this
   * is where synthesized mouse events must be sent.
   */
  QGraphicsScene* GetScene();

  /**
   * Free resources
   */
  void ReleaseGraphicsResources(vtkWindow* win) override;

protected:
  vtkSlicerQWidgetTexture();
  ~vtkSlicerQWidgetTexture() override;

  /// Forwards the shared image source's ModifiedEvent to this texture's own observers, so the
  /// owning representation (one per view) updates its plane and requests a render of its view.
  static void OnImageSourceModified(vtkObject* caller, unsigned long eid, void* clientData, void* callData);

  /// Shared per-widget image source; holds one of the references keeping it alive.
  vtkSmartPointer<vtkSlicerQWidgetImageSource> ImageSource;
  vtkCallbackCommand* ImageSourceCallbackCommand{nullptr};

private:
  vtkSlicerQWidgetTexture(const vtkSlicerQWidgetTexture&) = delete;
  void operator=(const vtkSlicerQWidgetTexture&) = delete;
};

#endif
