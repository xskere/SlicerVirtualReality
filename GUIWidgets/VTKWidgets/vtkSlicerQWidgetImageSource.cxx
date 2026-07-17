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

#include "vtkSlicerQWidgetImageSource.h"

// SlicerQt includes
#include "qMRMLUtils.h"

// VTK includes
#include <vtkObjectFactory.h>

// Qt includes
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QHash>
#include <QImage>
#include <QWidget>

namespace
{
/// Live sources by widget. Entries are raw pointers -- the registry holds no reference of its
/// own (lifetime is owned by the textures holding smart pointers); each source unregisters
/// itself in its destructor.
QHash<QWidget*, vtkSlicerQWidgetImageSource*> WidgetImageSources;
}

vtkStandardNewMacro(vtkSlicerQWidgetImageSource);

//------------------------------------------------------------------------------
vtkSmartPointer<vtkSlicerQWidgetImageSource> vtkSlicerQWidgetImageSource::GetSourceForWidget(QWidget* w)
{
  if (!w)
  {
    return nullptr;
  }

  auto existingSourceIt = WidgetImageSources.find(w);
  if (existingSourceIt != WidgetImageSources.end())
  {
    return existingSourceIt.value();
  }

  vtkSmartPointer<vtkSlicerQWidgetImageSource> source =
    vtkSmartPointer<vtkSlicerQWidgetImageSource>::Take(vtkSlicerQWidgetImageSource::New());
  source->SetWidget(w);
  WidgetImageSources.insert(w, source);
  return source;
}

//------------------------------------------------------------------------------
vtkSlicerQWidgetImageSource::vtkSlicerQWidgetImageSource()
{
  this->Scene = new QGraphicsScene();

  this->ImageData = vtkSmartPointer<vtkImageData>::New();
  this->TrivialProducer = vtkSmartPointer<vtkTrivialProducer>::New();
  this->TrivialProducer->SetOutput(this->ImageData);

  this->UpdateImageMethod = [this]() {
    if (!this->Widget)
    {
      return;
    }
    QImage grabImage(this->Widget->grab().toImage());
    qMRMLUtils::qImageToVtkImageData(grabImage, this->ImageData.GetPointer());
    this->ImageData->Modified();
    // Notify observers -- one vtkSlicerQWidgetTexture per view showing this widget -- so each
    // view updates its plane geometry and requests its own render.
    this->Modified();
  };

  QObject::connect(this->Scene, &QGraphicsScene::changed, this->UpdateImageMethod);
}

//------------------------------------------------------------------------------
vtkSlicerQWidgetImageSource::~vtkSlicerQWidgetImageSource()
{
  // Sever the scene's changed() connection before touching its contents below: detaching the
  // proxy (setWidget(nullptr), removeItem, delete) can itself trigger a changed() emission as a
  // geometry/visibility side effect, which would otherwise re-enter UpdateImageMethod() -- calling
  // vtkObject::Modified()/InvokeEvent() and QWidget::grab() -- on this object and this widget
  // while both are already mid-teardown.
  this->Scene->disconnect();

  if (this->Widget)
  {
    WidgetImageSources.remove(this->Widget);

    // Detach the widget from its graphics proxy (and delete the proxy). This is required (not
    // just removing it from the scene) because QGraphicsProxyWidget::setWidget() refuses to
    // re-embed a widget that still reports a non-null graphicsProxyWidget(), which would
    // otherwise silently break rendering and event handling for the widget the next time it is
    // shown (e.g. after a hide/show cycle).
    QGraphicsProxyWidget* proxy = this->Widget->graphicsProxyWidget();
    if (proxy)
    {
      // QGraphicsProxyWidget takes ownership of its embedded widget: deleting it without first
      // detaching the widget via setWidget(nullptr) deletes the widget too. This image source
      // does not own the widget (it is created and owned by whoever calls
      // vtkMRMLGUIWidgetNode::SetWidget(), e.g. qSlicerGUIWidgetsModuleWidget), so it must never
      // delete it -- doing so left the MRML node holding a dangling void* Widget pointer,
      // crashing the next time anything touched it.
      proxy->setWidget(nullptr);
      this->Scene->removeItem(proxy);
      delete proxy;
    }

    this->Widget = nullptr;
  }

  delete this->Scene;
  this->Scene = nullptr;
}

//------------------------------------------------------------------------------
vtkAlgorithmOutput* vtkSlicerQWidgetImageSource::GetOutputPort()
{
  return this->TrivialProducer->GetOutputPort();
}

//------------------------------------------------------------------------------
void vtkSlicerQWidgetImageSource::SetWidget(QWidget* w)
{
  if (this->Widget == w)
  {
    return;
  }

  this->Widget = w;
  if (!this->Widget)
  {
    return;
  }

  this->Widget->move(0, 0);
  this->Scene->addWidget(this->Widget);

  this->UpdateImageMethod();
}
