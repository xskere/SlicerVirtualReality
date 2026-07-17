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

#ifndef vtkSlicerQWidgetImageSource_h
#define vtkSlicerQWidgetImageSource_h

#include "vtkSlicerGUIWidgetsModuleVTKWidgetsExport.h"

// VTK includes
#include <vtkImageData.h>
#include <vtkObject.h>
#include <vtkSmartPointer.h>
#include <vtkTrivialProducer.h>

#include <functional> // for ivar

class QGraphicsScene;
class QWidget;
class vtkAlgorithmOutput;

/**
 * @class vtkSlicerQWidgetImageSource
 * @brief Shared per-QWidget source that renders the widget into a vtkImageData
 *
 * Exactly one instance exists per displayed QWidget, shared by every
 * vtkSlicerQWidgetTexture -- and therefore every view -- showing that widget.
 * It owns the single QGraphicsScene the widget is embedded in: a QWidget can
 * only be embedded in one QGraphicsScene at a time, so per-view scenes (the
 * previous design) left every view but the last one connected to a scene that
 * no longer contained the widget and therefore never reported changes. That is
 * the root cause behind the texture-update workarounds this class replaced
 * (objectName-rename broadcasts, the WaitingForTextureUpdate view-node
 * attribute, and the Selectable-flag recursion guard).
 *
 * Whenever the scene reports a change, the widget is re-grabbed into ImageData
 * and ModifiedEvent is invoked, so each observing texture can update its own
 * view independently.
 */
class VTK_SLICER_GUIWIDGETS_MODULE_VTKWIDGETS_EXPORT vtkSlicerQWidgetImageSource : public vtkObject
{
public:
  /// Get the shared image source for a widget, creating it on first use. All callers asking for
  /// the same widget get the same instance. The instance is destroyed -- and the widget detached
  /// from its scene, so it can be re-embedded later -- when the last reference is released.
  static vtkSmartPointer<vtkSlicerQWidgetImageSource> GetSourceForWidget(QWidget* w);

  vtkTypeMacro(vtkSlicerQWidgetImageSource, vtkObject);

  /// The widget this source renders.
  QWidget* GetWidget() { return this->Widget; }

  /// The scene the widget is embedded in; synthesized mouse events must be sent here.
  QGraphicsScene* GetScene() { return this->Scene; }

  /// Pipeline output producing the widget image, for textures to use as input connection.
  vtkAlgorithmOutput* GetOutputPort();

protected:
  vtkSlicerQWidgetImageSource();
  ~vtkSlicerQWidgetImageSource() override;

  /// Only used via GetSourceForWidget(), which guarantees one instance per widget.
  static vtkSlicerQWidgetImageSource* New();

  /// Embed the widget in the scene and render it for the first time. Called exactly once per
  /// instance, by GetSourceForWidget().
  void SetWidget(QWidget* w);

  QGraphicsScene* Scene{nullptr};
  QWidget* Widget{nullptr};

  vtkSmartPointer<vtkImageData> ImageData;
  vtkSmartPointer<vtkTrivialProducer> TrivialProducer;

  /// Re-grabs the widget into ImageData and invokes ModifiedEvent; connected to the scene's
  /// changed signal.
  std::function<void()> UpdateImageMethod;

private:
  vtkSlicerQWidgetImageSource(const vtkSlicerQWidgetImageSource&) = delete;
  void operator=(const vtkSlicerQWidgetImageSource&) = delete;
};

#endif
