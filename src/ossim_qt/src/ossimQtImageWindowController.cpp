//*******************************************************************
//
// License:  LGPL
// 
// See LICENSE.txt file in the top level directory for more details.
//
// Author: Garrett Potts
//
//*************************************************************************
// $Id: ossimQtImageWindowController.cpp 19688 2011-05-31 14:55:39Z dburken $
#include <sstream>
#include <qevent.h>
#include <qaction.h>
#include <qapplication.h>
#include <qtoolbutton.h>
#include <qfiledialog.h>
#include <qtextedit.h>
#include <qcombobox.h>
#include <qstatusbar.h>
#include <qmessagebox.h>
#include "ossimQtInformationDialog.h"
#include "ossimQtPositionQualityDialog.h"
#include "ossimDataManager.h"
#include "ossimQtDisplayUtility.h"
#include "ossimQtEditorWidgetManager.h"
#include <ossim/elevation/ossimElevManager.h>
#include <ossim/base/ossimException.h>
#include <ossim/base/ossimIpt.h>
#include <ossim/base/ossimKeywordlist.h>
#include <ossim/base/ossimObjectFactoryRegistry.h>
#include <ossim/base/ossimHsiVector.h>
#include <ossim/base/ossimUsgsQuad.h>
#include <ossim/base/ossimRefreshEvent.h>
#include "ossimQtImageWindowController.h"
#include "ossimQtImageWindowControllerEventFilter.h"
#include "ossimQtApplicationUtility.h"
#include "ossimQtImageWindow.h"
#include "ossimQtSwipeScrollingImageWidget.h"
#include "ossimQtBandSelectorDialog.h"
#include "ossimQtHistogramDialog.h"
#include "ossimQtHsiRemapperDialog.h"
#include "ossimQtIgenDialog.h"
#include "ossimQtIgenController.h"
#include "ossimQtCombinerEditorDialog.h"
#include "ossimQtCombinerEditorDialogController.h"
#include "ossimQtImageChainEditorDialog.h"
#include "ossimQtImageInfoDialog.h"
#include "ossimQtOverviewBuilderDialog.h"
#include "ossimQtLayerChooser.h"
#include <ossim/projection/ossimImageProjectionModel.h>
#include <ossim/projection/ossimProjectionFactoryRegistry.h>
#include <ossim/projection/ossimMapProjection.h>
#include <ossim/projection/ossimImageViewTransform.h>
#include <ossim/imaging/ossimImageSource.h>
#include <ossim/imaging/ossimImageChain.h>
#include <ossim/imaging/ossimImageHandler.h>
#include <ossim/imaging/ossimFilterResampler.h>
#include <ossim/imaging/ossimBandSelector.h>
#include <ossim/imaging/ossimBrightnessContrastSource.h>
#include <ossim/imaging/ossimCacheTileSource.h>
#include <ossim/imaging/ossimImageRenderer.h>
#include <ossim/imaging/ossimHistogramRemapper.h>
#include <ossim/imaging/ossimHsiRemapper.h>
#include <ossim/imaging/ossimImageCombiner.h>
#include <ossim/imaging/ossimRLevelFilter.h>
#include <ossim/imaging/ossimTopographicCorrectionFilter.h>
#include <ossim/base/ossimDms.h>
#include <ossim/base/ossimKeywordNames.h>
#include <ossim/base/ossimAdjustableParameterInterface.h>
#include <ossim/base/ossimNotifyContext.h>
#include <ossim/projection/ossimImageViewProjectionTransform.h>
#include <ossimQtViewDialog.h>
#include <ossimQtViewDialogController.h>
#include <ossimQtEvent.h>
#include <ossimQtLayerChooser.h>
#include <ossimQtAdjustableParameterDialog.h>
#include <ossimQtAdjustableParameterDialogController.h>
#include <ossimQtVectorEditorDialog.h>
#include <ossimQtVectorEditorDialogController.h>
#include <ossimQWidgetFactoryRegistry.h>
#include <ossim/projection/ossimUtmProjection.h>
#include <ossimQtHistogramBuilderDialog.h>


ossimQtImageWindowController::ossimQtImageWindowController(ossimQtImageWindow* window)
: theImageWindow(0),
theZoomInFlag(true),
theZoomFilter(0),
thePanFilter(0),
theEventCapture(0),
theTrackingCursorFilter(0),
theView(0),
theIgenDialog(0),
thePositionInformationDialog(0),
thePositionQualityDialog(0),
theMainWindowController(0),
thePropagateFlag(false), // ???
theFullRes(),
theCurrentCenter(),
theStatusTextString(),
theRgbVector(),
theRawPixelValues(),
theAutoDeleteIfNoInputFlag(),
theIdentityModeFlag(false),
theImageModel(0)
{
   theImageWindow = window;
   theZoomFilter = new ossimQtImageControllerZoomEventFilter(this);
   theZoomFilter->disableAnnotator();
   thePanFilter  = new ossimQtImageControllerPanEventFilter(this);
   theEventCapture = 0;
   theImageWindow->theImageWidget->setFocusPolicy(QWidget::StrongFocus);
   
   // tmp...
   // Disable random caching scheme.
   theImageWindow->theImageWidget->setRandomPriorityQueueEnabledFlag(false);
   
   
   theZoomInFlag = true;
   connect(theImageWindow->theImageWidget,
           SIGNAL(scrollingImageWidgetMouseEvent(ossimQtMouseEvent*)),
           this,
           SLOT(trackScrollingImageWidget(ossimQtMouseEvent*)));
   theFullRes.makeNan();
   theAutoDeleteIfNoInputFlag = true;
   thePositionInformationDialog = 0;
}    

ossimQtImageWindowController::~ossimQtImageWindowController()
{
   if(thePositionInformationDialog)
   {
      thePositionInformationDialog->close(true);
      thePositionInformationDialog = 0;
   }
   theView = 0;
   if(theImageWindow)
   {
      theImageWindow->removeEventFilter(theZoomFilter);
      theImageWindow->theImageWidget->disconnectAllInputs();
   }
   if(theEventCapture)
   {
      delete theEventCapture;
      theEventCapture = 0;
   }
   if(theZoomFilter)
   {
      delete theZoomFilter;
      theZoomFilter = 0;
   }
   if(thePanFilter)
   {
      delete thePanFilter;
      thePanFilter = 0;
   }
   theImageWindow = 0;
   
   if (theImageModel.valid())
   {
      theImageModel = 0;
   }
}

void ossimQtImageWindowController::enterEvent(QEvent* /*evt*/)
{
   if(theImageWindow&&theImageWindow->getImageWidget())
   {
      theImageWindow->getImageWidget()->setTrackingFlag(false);
   }
}

void ossimQtImageWindowController::leaveEvent(QEvent* /*evt*/)
{
   if(theImageWindow&&theImageWindow->getImageWidget())
   {
      theImageWindow->getImageWidget()->setTrackingFlag(true);
   }
}

void ossimQtImageWindowController::forceExit()
{
   theImageWindow->close(true);
}

void ossimQtImageWindowController::fileExit()
{
   theImageWindow->close(true);
}

void ossimQtImageWindowController::fileSaveAs()
{
}

void ossimQtImageWindowController::fileSaveTiePoints()
{
   QString filename = QFileDialog::getSaveFileName(QString::null, "Image tie points (*.txt)");
   if(filename != QString::null)
   {
      ossimConnectableObject* obj = theEventCapture->
      findObjectOfType("ossimImageHandler",
                       ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                       true);
      if(obj)
      {
         ossimImageHandler* ih = PTR_CAST(ossimImageHandler, obj);
         if(ih)
         {
            ofstream out(filename.ascii());
            if(out.good())
            {
               ossimIrect bounds = ih->getBoundingRect();
               ossimGpt gpt;
               double maxGridSample = 4.0;
               double spacing = (1.0/maxGridSample);
               double halfSpacing = spacing*.5;
               ossimIpt pt = bounds.ul() + ossimDpt(bounds.width()*halfSpacing,
                                                    bounds.height()*halfSpacing);
               ossim_uint32 dx, dy;
               ossim_uint32 id = 0;
               for(dy = 0; dy < 4; ++dy)
               {
                  for(dx = 0; dx < 4; ++dx)
                  {
                     ossimIpt origin = pt + ossimDpt(pt.x + (dx*spacing*bounds.width()),
                                                     pt.y + (dy*spacing*bounds.height()));
                     gpt = imageCoordinateToGround(origin);
                     double mslH = ossimElevManager::instance()->getHeightAboveMSL(gpt);
                     if(ossim::isnan(mslH))
                     {
                        mslH = 0.0;
                     }
                     out << setprecision(20) << id << " " << origin.x << " " << origin.y << " " << setprecision(20)<<gpt.lond() << " " << gpt.latd() << " " << mslH << std::endl; 
                     ++id;
                  }
               }
            }
         }
      }
   }
}

void ossimQtImageWindowController::filePrint()
{
}

void ossimQtImageWindowController::viewTool()
{
   ossimString name = "ossimQtViewDialog";
   
   // First see if we already have a dialog open for this.
   QWidget* w = ossimQtEditorWidgetManager::instance()->
   getFromList(name, getImageWidget());
   if (w)
   {
      w->show();
      w->raise();
      return;
   }
   
   ossimQtViewDialog* viewDialog = new ossimQtViewDialog(theImageWindow,
                                                         name.c_str(),
                                                         false,//modal
                                                         WDestructiveClose);
   ossimQtViewDialogController* controller = viewDialog->controller();
   
   controller->connect(controller,
                       SIGNAL(applyView(const ossimObject*)),
                       this,
                       SLOT(applyView(const ossimObject*)));
   
   controller->connect(controller,
                       SIGNAL(propagateView()),
                       this,
                       SLOT(propagateView()));
   
   controller->connect(controller,
                       SIGNAL(setIdentityMode(bool)),
                       this,
                       SLOT(setIdentityMode(bool)));   
   
   controller->setView(theView.get());
   
   if ( isSingleImageDisplay() == false )
   {
      controller->hideIdentityButton(true);
   }
   else if (theIdentityModeFlag)
   {
      controller->setIdentityButtonChecked(true);
   }
   
   // Add to the list so we don't make a duplicate dialog.
   ossimQtEditorWidgetManager::instance()->addToList(viewDialog,
                                                     getImageWidget());
   
   viewDialog->show();
}

void ossimQtImageWindowController::showPositionInformation()
{
   if(thePositionInformationDialog)
   {
      thePositionInformationDialog->show();
      thePositionInformationDialog->raise();
   }
   else
   {
      thePositionInformationDialog = new ossimQtInformationDialog(theImageWindow, false, WStyle_StaysOnTop|WDestructiveClose);
      connect(thePositionInformationDialog,
              SIGNAL(destroyed()),
              this,
              SLOT(informationDialogDestroyed()));
      
      thePositionInformationDialog->textEditor()->setReadOnly(true);
      thePositionInformationDialog->show();
      thePositionInformationDialog->resize(300,400);
      QString tempString;
      buildStatusText(tempString);
   }
}

void ossimQtImageWindowController::showPositionQuality()
{
   if ( !theIdentityModeFlag )
   {
      if(!thePositionQualityDialog)
      {
         thePositionQualityDialog =
         new ossimQtPositionQualityDialog(theImageWindow);
         connect(thePositionQualityDialog,
                 SIGNAL(destroyed()),
                 this,
                 SLOT(positionQualityDialogDestroyed()));
         thePositionQualityDialog->setImageWidget(getImageWidget());
         thePositionQualityDialog->setController(this);
      }
      
      thePositionQualityDialog->show();
      thePositionQualityDialog->raise();
   }
}

void ossimQtImageWindowController::imageGeometry()
{
}

void ossimQtImageWindowController::vectorSource()
{
   ossimConnectableObject* obj = theEventCapture->
   findObjectOfType("ossimVpfTileSource",
                    ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                    true);
   
   if(obj)
   {
      // First see if we already have a dialog open for this.
      QWidget* w = ossimQtEditorWidgetManager::instance()->getFromList(obj);
      if (w)
      {
         w->show();
         w->raise();
         return;
      }
      
      ossimQtVectorEditorDialog* dialog =
      new ossimQtVectorEditorDialog(theImageWindow,
                                    "ossimQtVectorEditorDialog",
                                    false,
                                    WDestructiveClose);
      
      dialog->controller()->setObject(obj);
      
      w = (QWidget*)dialog;
      
      // Add to the list so we don't make a duplicate dialog.
      ossimQtEditorWidgetManager::instance()->addToList(w, obj);
      
      dialog->show();
   }
}


bool ossimQtImageWindowController::addInput(ossimConnectableObject* obj, bool refresh)
{
   bool result = false;
   if(theImageWindow&&theImageWindow->getImageWidget()&&obj)
   {
      if(!theEventCapture)
      {
         theEventCapture = new ossimQtImageControllerOssimEventCapture(this);
      }
      if(theEventCapture->getNumberOfInputs() <2)
      {
         theEventCapture->connectMyInputTo(obj); 
         theImageWindow->getImageWidget()->connectMyInputTo(obj);
         
         if(theEventCapture->getInput())
         {
            theImageWindow->setCaption(theEventCapture->getInput()->getDescription().c_str());
            initializePointers();
            initializeMenus();
         }
         
         setViewToInputs();
         
         setHistogramStretch();
         
         if(refresh)
         {
            theImageWindow->getImageWidget()->refresh();
         }
         
         setFullResInputValue();
         result = true;
      }
   }
   
   return result;
}

bool ossimQtImageWindowController::removeInput(ossimConnectableObject* obj, bool refresh)
{
   bool result = false;
   
   if(theImageWindow&&theImageWindow->getImageWidget()&&obj)
   {
      if(theEventCapture)
      {
         ossimConnectableObject* connectable = (ossimConnectableObject*)theImageWindow->theImageWidget;
         connectable->disconnect(obj);
         theEventCapture->disconnect(obj);
      }
      setFullResInputValue();
      if(theImageWindow->getImageWidget()->getInput())
      {
         initializePointers();
         initializeMenus();
      }
      if(refresh)
      {
         theImageWindow->getImageWidget()->refresh();
      }
      result = true;
   }
   return result;
}

void ossimQtImageWindowController::disconnectAllInputs()
{
   if(theImageWindow&&theImageWindow->getImageWidget())
   {
      ossimConnectableObject* connectable = (ossimConnectableObject*)theImageWindow->theImageWidget;
      connectable->disconnect();
      if(theEventCapture)
      {
         theEventCapture->disconnect();
      }
      initializePointers();
      initializeMenus();
      theImageWindow->getImageWidget()->refresh();
   }
}

void ossimQtImageWindowController::closeEvent(QCloseEvent* evt)
{
   evt->accept();
   forceExit();
}

void ossimQtImageWindowController::bandSelector()
{
   // Find the band selector in the chain...
   ossimConnectableObject* input = 0;
   if (theImageWindow)
   {
      input = theEventCapture->getInput();
   }
   if (!input)
   {
      ossimNotify(ossimNotifyLevel_FATAL)
      << "FATAL ossimQtImageWindowController::bandSelector :"
      << "No input available.  The window has no input image!"
      << std::endl;
      return;
   }
   
   ossimRefPtr<ossimConnectableObject> con_obj = input->
   findInputObjectOfType(ossimString("ossimBandSelector"));
   
   if (!con_obj)
   {
      // Add to the end of the chain.
      ossimConnectableObject* icObj = input->
      findInputObjectOfType(ossimString("ossimImageChain"));
      if (icObj)
      {
         ossimImageChain* ic = PTR_CAST(ossimImageChain, icObj);
         if (ic)
         {
            ossimRefPtr<ossimConnectableObject> bs = new ossimBandSelector();
            
            //---
            // See if the end of the chain is a cache.  If so, insert the
            // ossimBrightnessContrastSource to the left of the cache; else
            // put it on the end of the chain.
            //---
            ossimConnectableObject* endIcObj =
            (ossimConnectableObject*)ic->getFirstObject();
            if (PTR_CAST(ossimCacheTileSource, endIcObj))
            {
               // Add to the left of the cache.
               if (ic->insertLeft(bs.get(), endIcObj) == true)
               {
                  con_obj = bs;
               }
            }
            else
            {
               // Add to the end of the chain.
               if (ic->addFirst(bs.get()) == true)
               {
                  con_obj = bs;
               }
            }
         }
      }
      
      if (!con_obj)
      {
         return;
      }
   }
   
   ossimObject* obj = PTR_CAST(ossimObject, con_obj.get());
   
   // First see if we already have a dialog open for this.
   QWidget* w = ossimQtEditorWidgetManager::instance()->getFromList(obj);
   if (w)
   {
      w->show();
      w->raise();
      return;
   }
   
   w = ossimQWidgetFactoryRegistry::instance()->
   createEditor(obj,
                theImageWindow,
                WStyle_Customize |
                WStyle_NormalBorder | 
                WStyle_Title |
                WStyle_SysMenu|
                WDestructiveClose);
   
   if (w)
   {
      ossimQtEditorWidgetManager::instance()->addToList(w, obj);
      w->show();
   }
}

void ossimQtImageWindowController::brightnessContrast()
{
   // Find the band selector in the chain...
   ossimRefPtr<ossimConnectableObject> input = 0;
   if (theImageWindow)
   {
      input = theEventCapture->getInput();
   }
   if (!input)
   {
      ossimNotify(ossimNotifyLevel_FATAL)
      << "FATAL ossimQtImageWindowController::brightnessContrast :"
      << "No input available.  The window has no input image!"
      << std::endl;
      return;
   }
   
   ossimRefPtr<ossimConnectableObject> con_obj = input->
   findInputObjectOfType(ossimString("ossimBrightnessContrastSource"));
   
   if (!con_obj)
   {
      // Add to the end of the chain.
      ossimRefPtr<ossimConnectableObject> icObj = input->
      findInputObjectOfType(ossimString("ossimImageChain"));
      if (icObj.valid())
      {
         ossimImageChain* ic = PTR_CAST(ossimImageChain, icObj.get());
         if (ic)
         {
            ossimRefPtr<ossimConnectableObject> bc = new ossimBrightnessContrastSource();
            
            //---
            // See if the end of the chain is a cache.  If so, insert the
            // ossimBrightnessContrastSource to the left of the cache; else
            // put it on the end of the chain.
            //---
            ossimConnectableObject* endIcObj =
            (ossimConnectableObject*)ic->getFirstObject();
            if (PTR_CAST(ossimCacheTileSource, endIcObj))
            {
               if (ic->insertLeft(bc.get(), endIcObj) == true)
               {
                  con_obj = bc.get();
               }
            }
            else
            {
               if (ic->addFirst(bc.get()) == true)
               {
                  con_obj = bc;
               }
            }
            
         }
      }
      
      if (!con_obj)
      {
         return;
      }
   }
   
   ossimObject* obj = PTR_CAST(ossimObject, con_obj.get());
   
   // First see if we already have a dialog open for this.
   QWidget* w = ossimQtEditorWidgetManager::instance()->getFromList(obj);
   if (w)
   {
      w->show();
      w->raise();
      return;
   }
   
   w = ossimQWidgetFactoryRegistry::instance()->
   createEditor(obj,
                theImageWindow,
                WStyle_Customize |
                WStyle_NormalBorder | 
                WStyle_Title |
                WStyle_SysMenu|
                WDestructiveClose);
   
   if (w)
   {
      ossimQtEditorWidgetManager::instance()->addToList(w, obj);
      w->show();
   }
}

void ossimQtImageWindowController::histogramOperation()
{
   ossimConnectableObject* input = 0;
   if (theImageWindow)
   {
      input = theImageWindow->getImageWidget()->getInput();
      
      if (input)
      {
         ossimHistogramRemapper* remapper = getHistogramRemapper();
         if (remapper)
         {
            //---
            // See if the remapper has a histogram yet, if not, call the
            // buildHistogram method which will asks the user if he/she
            // wants to build.
            //---
            bool hasHistogram = true;
            ossimRefPtr<ossimHistogram> h = remapper->getHistogram(0);
            if( !h.valid() )
            {
               hasHistogram = buildHistogram();
            }
            
            if (hasHistogram)
            {
               //---
               // NOTE: Removed adding to the ossimQtEditorWidgetManager as the
               // dialog has a "destructive close" so adding to the manager
               // will not do anything.
               //---
               ossimObject* obj = PTR_CAST(ossimObject, remapper);
               
               QWidget* w = (QWidget*)ossimQWidgetFactoryRegistry::instance()->
               createEditor(obj,
                            theImageWindow,
                            WStyle_Customize |
                            WStyle_NormalBorder | 
                            WStyle_Title |
                            WStyle_SysMenu |
                            WDestructiveClose);
               if (w)
               {
                  w->show();
               }
            }
            
         } // matches:  if (remapper)
      }
      else // matches: if (input)
      {
         ossimNotify(ossimNotifyLevel_FATAL)
         << "FATAL ossimQtImageWindowController::histogramOperation:"
         << " Window not connected!" << std::endl;
      }
      
   } // matches: if (theImageWindow)
   
} // End of: void ossimQtImageWindowController::histogramOperation()

bool ossimQtImageWindowController::isSingleImageDisplay() const
{
   bool result = false;
   ossimConnectableObject::ConnectableObjectList imageHandlerList;
   theEventCapture->findAllInputsOfType(
                                        imageHandlerList,
                                        STATIC_TYPE_INFO(ossimImageHandler),
                                        true,
                                        true);
   if (imageHandlerList.size() == 1)
   {
      result = true;
   }
   return result;
}

void ossimQtImageWindowController::hsiAdjustments()
{
   ossimConnectableObject* input = 0;
   if (theImageWindow)
   {
      input = theImageWindow->getImageWidget()->getInput();
   }
   if (!input)
   {
      ossimNotify(ossimNotifyLevel_FATAL)
      << "FATAL ossimQtImageWindowController::hsiAdjustment:"
      << " Window not connected!" << std::endl;
      return;
   }
   
   ossimRefPtr<ossimConnectableObject> con_obj = input->
   findInputObjectOfType(ossimString("ossimHsiRemapper"));
   
   if (!con_obj)
   {
      // Add to the end of the chain.
      ossimRefPtr<ossimConnectableObject> icObj = input->
      findInputObjectOfType(ossimString("ossimImageChain"));
      if (icObj.valid())
      {
         ossimImageChain* ic = PTR_CAST(ossimImageChain, icObj.get());
         if (ic)
         {
            ossimRefPtr<ossimConnectableObject> hr = new ossimHsiRemapper();
            
            //---
            // See if the end of the chain is a cache.  If so, insert the
            // ossimBrightnessContrastSource to the left of the cache; else
            // put it on the end of the chain.
            //---
            ossimConnectableObject* endIcObj =
            (ossimConnectableObject*)ic->getFirstObject();
            if (PTR_CAST(ossimCacheTileSource, endIcObj))
            {
               if (ic->insertLeft(hr.get(), endIcObj) == true)
               {
                  con_obj = hr.get();
               }
            }
            else
            {
               if (ic->addFirst(hr.get()) == true)
               {
                  con_obj = hr.get();
               }
            }
            
            if (!con_obj)
            {
               hr = 0;
            }
         }
      }
      
      if (!con_obj)
      {
         return;
      }
   }
   
   ossimObject* obj = PTR_CAST(ossimObject, con_obj.get());
   
   // First see if we already have a dialog open for this.
   QWidget* w = ossimQtEditorWidgetManager::instance()->getFromList(obj);
   if (w)
   {
      w->show();
      w->raise();
      return;
   }
   
   w = (QWidget*)ossimQWidgetFactoryRegistry::instance()->
   createEditor(obj,
                theImageWindow,
                WStyle_Customize |
                WStyle_NormalBorder | 
                WStyle_Title |
                WStyle_SysMenu|
                WDestructiveClose);
   if (w)
   {
      ossimQtEditorWidgetManager::instance()->addToList(w, obj);
      w->show();
   }
}

void ossimQtImageWindowController::fusion()
{
   ossimConnectableObject* obj = theEventCapture->findObjectOfType("ossimFusionCombiner",
                                                                   ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                                                                   true);
   if(obj)
   {
      QWidget* widget =ossimQWidgetFactoryRegistry::instance()->
      createEditor(obj,
                   theImageWindow,
                   WStyle_Customize |
                   WStyle_NormalBorder | 
                   WStyle_Title |
                   WStyle_SysMenu|
                   WDestructiveClose);
      
      if (widget)
      {
         widget->show();
      }
   }   
}

void ossimQtImageWindowController::topographicCorrection()
{
   ossimConnectableObject* obj = theEventCapture->
   findObjectOfType("ossimTopographicCorrectionFilter",
                    ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                    true);
   if(obj)
   {
      // First see if we already have a dialog open for this.
      QWidget* w = ossimQtEditorWidgetManager::instance()->
      getFromList(obj);
      if (w)
      {
         w->show();
         w->raise();
         return;
      }
      
      w = ossimQWidgetFactoryRegistry::instance()->
      createEditor(obj,
                   theImageWindow,
                   WStyle_Customize |
                   WStyle_NormalBorder | 
                   WStyle_Title |
                   WStyle_SysMenu|
                   WDestructiveClose);
      
      if (w)
      {
         // Add to the list so we don't make a duplicate dialog.
         ossimQtEditorWidgetManager::instance()->addToList(w, obj);
         
         w->show();
      }
   }
}

void ossimQtImageWindowController::editImageChain()
{
   ossimString name = "ossimQtImageChainEditorDialog";
   
   // First see if we already have a dialog open for this.
   QWidget* w = ossimQtEditorWidgetManager::instance()->
   getFromList(name, getImageWidget());
   if (w)
   {
      w->show();
      w->raise();
      return;
   }
   
   ossimQtImageChainEditorDialog* chainEditor =
   new ossimQtImageChainEditorDialog(theImageWindow,
                                     name.c_str(),
                                     false,
                                     WDestructiveClose);
   chainEditor->setImageWidget(getImageWidget());
   
   // Add to the list so we don't make a duplicate dialog.
   ossimQtEditorWidgetManager::instance()->addToList(chainEditor,
                                                     getImageWidget());
   
   chainEditor->show();
}

void ossimQtImageWindowController::editImageInfo()
{
   ossimString name = "ossimQtImageInfoDialog";
   
   // First see if we already have a dialog open for this.
   QWidget* w = ossimQtEditorWidgetManager::instance()->
   getFromList(name, getImageWidget());
   if (w)
   {
      w->show();
      w->raise();
      return;
   }
   
   ossimQtImageInfoDialog* imageInfo =
   new ossimQtImageInfoDialog(theImageWindow,
                              name.c_str(),
                              false,
                              WDestructiveClose);
   imageInfo->setImageWidget(getImageWidget());
   
   // Add to the list so we don't make a duplicate dialog.
   ossimQtEditorWidgetManager::instance()->addToList(imageInfo,
                                                     getImageWidget());
   
   imageInfo->show();
}

void ossimQtImageWindowController::editLayers()
{
   ossimQtCombinerEditorDialog* combinerEditor =
   new ossimQtCombinerEditorDialog(theImageWindow,
                                   "ossimQtCombinerEditorDialog",
                                   false,
                                   WDestructiveClose);
   
   ossimConnectableObject* obj = theEventCapture->findObjectOfType("ossimImageCombiner",
                                                                   ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                                                                   true);
   if(obj)
   {
      combinerEditor->controller()->setObject(obj);
      
      combinerEditor->show();
   }
}

void ossimQtImageWindowController::geometryAdjustment()
{
   ossimImageRenderer* obj = getImageRenderer();
   if(obj)
   {
      ossimImageViewProjectionTransform* trans = PTR_CAST(ossimImageViewProjectionTransform, obj->getImageViewTransform());
      if(trans)
      {
         ossimRefPtr<ossimImageGeometry> geom = trans->getImageGeometry();
         ossimAdjustableParameterInterface* adjInterface = 0;
         if(geom.valid()&&geom->getProjection())
         {
            adjInterface = PTR_CAST(ossimAdjustableParameterInterface,
                                    geom->getProjection());
         }
         if(adjInterface)
         {
            ossimString name = "ossimQtAdjustableParameterDialog";
            
            // First see if we already have a dialog open for this.
            QWidget* w = ossimQtEditorWidgetManager::instance()->
            getFromList(name, getImageWidget());
            if (w)
            {
               w->show();
               w->raise();
               return;
            }
            
            ossimQtAdjustableParameterDialog* adjParamDialog =
            new ossimQtAdjustableParameterDialog(theImageWindow,
                                                 name.c_str(),
                                                 false,
                                                 WDestructiveClose);
            
            adjParamDialog->controller()->setInformation(obj, adjInterface);
            
            // Add to the list so we don't make a duplicate dialog.
            ossimQtEditorWidgetManager::instance()->addToList(adjParamDialog,
                                                              getImageWidget());
            
            adjParamDialog->show();
         }
      }
   }
}

void ossimQtImageWindowController::igenExport()
{
   if (!theIgenDialog)
   {
      theIgenDialog = new ossimQtIgenDialog(theImageWindow,
                                            "image_generator", 
                                            false, 
                                            WStyle_Customize |
                                            WStyle_NormalBorder | 
                                            WStyle_Title |
                                            WStyle_SysMenu|
                                            WDestructiveClose);
      theIgenDialog->setImageWidget(theImageWindow->getImageWidget());
      
      connect( theIgenDialog,
              SIGNAL(destroyed()),
              this,
              SLOT(igenDialogDestroyed()) );
      connect(this,
              SIGNAL(viewChanged(const ossimObject*)),
              theIgenDialog->controller(),
              SLOT(viewChanged(const ossimObject*)));
   }
   
   theIgenDialog->setView(theView.get());
   theIgenDialog->show();
}

void ossimQtImageWindowController::zoomToolButtonToggled(bool value)
{
   if(value)
   {
      if(theImageWindow->thePanToToolButton->state() == QButton::On)
      {
         theImageWindow->thePanToToolButton->toggle();
      }
      
      theZoomFilter->connect(theImageWindow->theImageWidget,
                             SIGNAL(scrollingImageWidgetMouseEvent(ossimQtMouseEvent*)),
                             theZoomFilter, SLOT(scrollingImageWidgetMouseEvent(ossimQtMouseEvent*)));
      theZoomFilter->enableAnnotator();
      theImageWindow->installEventFilter(theZoomFilter);
   }
   else
   {
      theZoomFilter->disconnect(theImageWindow->theImageWidget,
                                SIGNAL(scrollingImageWidgetMouseEvent(ossimQtMouseEvent*)),
                                theZoomFilter, SLOT(scrollingImageWidgetMouseEvent(ossimQtMouseEvent*)));
      theZoomFilter->disableAnnotator();
      theImageWindow->removeEventFilter(theZoomFilter);
   }
}

void ossimQtImageWindowController::panToToolButtonToggled(bool value)
{
   if(value)
   {
      if(theImageWindow->theZoomToolButton->state() == QButton::On)
      {
         theImageWindow->theZoomToolButton->toggle();
      }
      
      thePanFilter->connect(theImageWindow->theImageWidget,
                            SIGNAL(scrollingImageWidgetMouseEvent(ossimQtMouseEvent*)),
                            thePanFilter, SLOT(scrollingImageWidgetMouseEvent(ossimQtMouseEvent*)));
   }
   else
   {
      thePanFilter->disconnect(theImageWindow->theImageWidget,
                               SIGNAL(scrollingImageWidgetMouseEvent(ossimQtMouseEvent*)),
                               thePanFilter, SLOT(scrollingImageWidgetMouseEvent(ossimQtMouseEvent*)));
   }
}


ossimQtImageWindow* ossimQtImageWindowController::getImageWindow()
{
   return theImageWindow;
}

void ossimQtImageWindowController::setZoomModeOut()
{
   theZoomInFlag = false;
}

void ossimQtImageWindowController::setZoomModeIn()
{
   theZoomInFlag = true;
}

void ossimQtImageWindowController::zoomTo(const ossimIpt& ulPt,
                                          const ossimIpt& lrPt)
{
   vector<ossimIpt> points(2);
   points[0] = ulPt;
   points[1] = lrPt;
   ossimIrect rect(points);
   ossimIrect viewportRect = getImageWidget()->getAbsoluteViewportRect();
   
   rect = rect.clipToRect(viewportRect);
   ossimIpt center = rect.midPoint();
   
   if ( !theIdentityModeFlag )
   {
      ossimIpt viewportWH(viewportRect.width(), viewportRect.height());
      ossimIpt zoomRectWH(rect.width(), rect.height());
      
      ossim_int32 length1 = ossim::max(zoomRectWH.x,zoomRectWH.y);
      ossim_int32 length2 = ossim::max(viewportWH.x, viewportWH.y);
      ossimDpt scale;      
      
      if(theZoomInFlag)
      {
         scale = ossimDpt(.5, .5);
         
         if(length1 > 5)
         {
            scale = ossimDpt((double)length1/(double)length2,
                             (double)length1/(double)length2);
         }
      }
      else
      {
         scale = ossimDpt(2.0, 2.0);
         
         if(length1 > 5)
         {
            scale = ossimDpt((double)length2/(double)length1,
                             (double)length2/(double)length1);
         }
      }
      
      zoomTo(center, scale);
   }
   else // identity mode
   {
      ossimRefPtr<ossimRLevelFilter> rf = getRLevelFilter();
      if ( rf.valid() && theImageModel.valid() )
      {
         ossim_uint32 numberOfRrds = rf->getNumberOfDecimationLevels();
         if (numberOfRrds > 1)
         {
            ossim_uint32 currentRrds = rf->getCurrentRLevel();
            
            // Get the new rrds number.
            ossim_uint32 newRrds = 0;
            if ( theZoomInFlag )
            {
               if ( currentRrds > 0 )
               {
                  newRrds = currentRrds - 1;
               }
            }
            else // zooming out...
            {
               if ( currentRrds <  (numberOfRrds-1) )
               {
                  newRrds = currentRrds + 1;
               }
            }
            
            if (newRrds != currentRrds) // Need to change rrds level.
            {
               // Get the center point for new rrds level.
               
               // Tranform to r0 point.
               ossimDpt r0Pt;
               if (currentRrds) // Not at rrds 0 already.
               {
                  // Tranform to rn point to r0 point.
                  ossimDpt rnPt = center;
                  rnToR0(rnPt, r0Pt);
               }
               else
               {
                  r0Pt = center; // At rrds 0 already.
               }
               
               if (newRrds == 0)
               {
                  center = r0Pt; // Going to r0.
               }
               else // Going to some other rrds.
               {
                  // Transform r0 point to new rrds level.
                  ossimDpt rnPt;
                  try
                  {
                     theImageModel->r0ToRn(newRrds, r0Pt, rnPt);
                  }
                  catch (const ossimException& e)
                  {
                     ossimNotify(ossimNotifyLevel_WARN)
                     << e.what() << std::endl;;
                  }
                  center = rnPt;
               }
               
               // Change rrds level.
               rf->setCurrentRLevel(newRrds);
               
               // Recenter.
               getImageWidget()->refresh(center);
               
               theCurrentCenter = imageToGround(center);
               
            } // End of: if (newRrds != currentRrds)
            
         } // End of: if (numberOfRrds > 1)
         
      } // End of: if ( rf && theImageModel )
      
   } // End of "identity mode" block.
}

void ossimQtImageWindowController::zoomTo(const ossimIpt& center,
                                          const ossimDpt& scale)
{
   ossimMapProjection* mapProj = PTR_CAST(ossimMapProjection, theView.get());
   if(!mapProj)
   {
      return;
   }
   ossimDpt centerDpt;
   ossimGpt centerGround;
   
   mapProj->lineSampleToWorld(center, centerGround);
   
   mapProj->applyScale(scale, true);
   setViewToInputs();
   
   mapProj->worldToLineSample(centerGround, centerDpt);
   ossimIpt newCenter = centerDpt;
   mapProj->lineSampleToWorld(centerDpt, theCurrentCenter);
   getImageWidget()->refresh(newCenter);
   
   emit viewChanged(theView.get());
}

void ossimQtImageWindowController::zoomToCenter(const ossimDpt& scale)
{
   ossimMapProjection* mapProj = PTR_CAST(ossimMapProjection, theView.get());
   if (mapProj && theImageWindow)
   {
      if (theImageWindow->getImageWidget())
      {
         // Get the center ground point
         ossimIrect vRect = theImageWindow->getImageWidget()->
         getSceneBoundingRect();
         ossimDpt centerDpt = vRect.midPoint();
         mapProj->lineSampleToWorld(centerDpt, theCurrentCenter);
         
         // Change the view resolution and propagate.
         mapProj->applyScale(scale, true);
         setViewToInputs();
         
         // Get the center view point.
         mapProj->worldToLineSample(theCurrentCenter, centerDpt);
         ossimIpt newCenter = centerDpt;
         
         getImageWidget()->refresh(newCenter);
         
         emit viewChanged(theView.get());
      }
   }
}

void ossimQtImageWindowController::panTo(const ossimIpt& pt)
{
   theCurrentCenter = imageToGround(pt);
   getImageWidget()->panTo(pt);
}

void ossimQtImageWindowController::setResampler(const QString& typeName)
{
   setResampler(typeName, typeName);
}

ossimDpt ossimQtImageWindowController::viewCoordinateToImageCoordinate(const ossimDpt& viewPt)
{
   ossimDpt imgPt;
   imgPt.makeNan();
   ossimGpt worldPt;
   ossimConnectableObject* obj = theEventCapture->getInput();
   if (!obj)
   {
      return imgPt;
   }
   
   ossimImageRenderer* renderer =
   (ossimImageRenderer*)(obj->findObjectOfType(
                                               "ossimImageRenderer",
                                               ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT));
   if(renderer)
   {
      ossimImageViewProjectionTransform* trans =
      PTR_CAST(ossimImageViewProjectionTransform,
               renderer->getImageViewTransform());
      if(trans)
      {
         trans->viewToImage(viewPt, imgPt);
      }
   }
   else
   {
      imgPt = viewPt;
   }
   
   return imgPt;
}

ossimGpt ossimQtImageWindowController::imageCoordinateToGround(const ossimDpt& imgPt)
{
   ossimGpt worldPt;
   ossimConnectableObject* obj = theEventCapture->getInput();
   worldPt.makeNan();
   if (!obj)
   {
      return worldPt;
   }
   
   ossimImageRenderer* renderer =
   (ossimImageRenderer*)(obj->findObjectOfType(
                                               "ossimImageRenderer",
                                               ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT));
   if(renderer)
   {
      ossimImageViewProjectionTransform* trans =
      PTR_CAST(ossimImageViewProjectionTransform,
               renderer->getImageViewTransform());
      if(trans)
      {
         ossimRefPtr<ossimImageGeometry> geom = trans->getImageGeometry();
         if(geom.valid())
         {
            geom->localToWorld(imgPt, worldPt);
         }
      }
   }
   
   return worldPt;
}

void ossimQtImageWindowController::setResampler(const QString& minifyType,
                                                const QString& magnifyType)
{
   if(theEventCapture)
   {
      ossimConnectableObject::ConnectableObjectList rendererList;
      theEventCapture->findAllInputsOfType(rendererList,
                                           STATIC_TYPE_INFO(ossimImageRenderer),
                                           true,
                                           true);
      std::vector<QObject*> displayList;
      
      for(int i = 0; i < (int)rendererList.size(); ++i)
      {
         ossimImageRenderer* renderer = PTR_CAST(ossimImageRenderer, rendererList[i].get());
         
         if(renderer&&renderer->getResampler())
         {
            renderer->getResampler()->setFilterType(minifyType.ascii(),
                                                    magnifyType.ascii());
            ossimPropertyEvent propEvt(rendererList[0].get());
            rendererList[i]->fireEvent(propEvt);
            rendererList[i]->propagateEventToOutputs(propEvt);
         }
      }
      
      getImageWidget()->flushCache(true);
      
      theImageWindow->theResamplerSelection->setCurrentText(magnifyType);
   }
   //   theImageWindow->theResamplerSelection->update();
}

void ossimQtImageWindowController::setViewToInputs()
{
   if(theImageWindow&&theImageWindow->getImageWidget()&&theView.valid())
   {
      ossimConnectableObject::ConnectableObjectList viewList;
      theEventCapture->findAllInputsOfType(viewList,
                                           STATIC_TYPE_INFO(ossimViewInterface),
                                           true,
                                           true);
      for(int i = 0; i < (int)viewList.size(); ++i)
      {
         ossimViewInterface* viewInterface = PTR_CAST(ossimViewInterface, viewList[i].get());
         if(viewInterface && theView.valid())
         {
            ossimRefPtr<ossimObject> viewDup = theView->dup();
            viewInterface->setView(viewDup.get());
            ossimPropertyEvent propEvt(viewList[i].get());
            viewList[i]->fireEvent(propEvt);
            viewList[i]->propagateEventToOutputs(propEvt);
         }
      }
   }
}

ossimQtSwipeScrollingImageWidget* ossimQtImageWindowController::getImageWidget()
{
   return theImageWindow->getImageWidget();
}

void ossimQtImageWindowController::initializePointers()
{
   theView = 0;
   
   if(theImageWindow&&theImageWindow->getImageWidget())
   {
      ossimConnectableObject* input = theImageWindow->getImageWidget()->getInput();
      ossimImageSource* inter = PTR_CAST(ossimImageSource, input);
      if(input&&inter)
      {
         ossimRefPtr<ossimImageGeometry> geom = inter->getImageGeometry();
         
         if(geom.valid()&&geom->getProjection())
         {
            theView = (ossimProjection*)geom->getProjection()->dup();
         }
      }
   }
}

void ossimQtImageWindowController::initializeMenus()
{
   if(theEventCapture&&theImageWindow)
   {
      if(theEventCapture->getNumberOfInputs() > 1)
      {
         theImageWindow->swipeAddLayerAction->setEnabled(false);
         theImageWindow->swipeRemoveLayerAction->setEnabled(true);
         
         theImageWindow->swipeHorizontalAction->setEnabled(true);
         theImageWindow->swipeVerticalAction->setEnabled(true);
         theImageWindow->swipeBoxAction->setEnabled(false);
         theImageWindow->swipeCircleAction->setEnabled(false);
         
         if(!theImageWindow->swipeHorizontalAction->isOn()&&
            !theImageWindow->swipeVerticalAction->isOn()&&
            !theImageWindow->swipeBoxAction->isOn()&&
            !theImageWindow->swipeCircleAction->isOn())
         {
            theImageWindow->swipeHorizontalAction->setOn(true);
            getImageWidget()->setSwipeType(ossimQtSwipeScrollingImageWidget::ossimQtSwipeType_HORIZONTAL);
         }
      }
      else
      {
         theImageWindow->swipeAddLayerAction->setEnabled(true);
         theImageWindow->swipeRemoveLayerAction->setEnabled(false);
         
         theImageWindow->swipeHorizontalAction->setEnabled(false);
         theImageWindow->swipeVerticalAction->setEnabled(false);
         theImageWindow->swipeBoxAction->setEnabled(false);
         theImageWindow->swipeCircleAction->setEnabled(false);
      }
      
      //---
      // Enable the "Layers" button if a combiner.
      //---
      if ( (theEventCapture->findObjectOfType("ossimImageCombiner",
                                              ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT, true) ) )
      {
         theImageWindow->editLayersAction->setEnabled(true);
         theImageWindow->enhancementsHistogramOperationsAction->setEnabled(false);
      }
      else
      { 
         theImageWindow->editLayersAction->setEnabled(false);
      }
      
      theImageWindow->editVectorSourceAction->setEnabled(((theEventCapture->findObjectOfType("ossimVpfTileSource",
                                                                                             ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                                                                                             true))!=0));
      
      theImageWindow->enhancementsHsiAction->setEnabled(true);
      theImageWindow->enhancementsBandSelectorAction->setEnabled(true);
      theImageWindow->enhancementsBrightnessContrastAction->setEnabled(true);
      
      ossimObject* tempObj = theEventCapture->findObjectOfType("ossimFusionCombiner",
                                                               ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                                                               true);
      theImageWindow->enhancementsFusionAction->setEnabled(PTR_CAST(ossimAdjustableParameterInterface, tempObj)!=0);
      
      ossimConnectableObject* obj = theEventCapture->findObjectOfType("ossimImageToPlaneNormalFilter",
                                                                      ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                                                                      true);
      if(obj)
      {
         
      }
      else
      {
         ossimConnectableObject* ortho = theEventCapture->findObjectOfType("ossimOrthoImageMosaic",
                                                                           ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                                                                           true);
         if(ortho)
         {
            if(((ossimImageSource*)ortho)->getNumberOfOutputBands() == 1)
            {
               
            }
         }
      }
      
      ossimImageRenderer* renderer = getImageRenderer();
      if(renderer)
      {
         ossimFilterResampler* resampler = renderer->getResampler();
         
         if(resampler)
         {
            std::vector<ossimString> filterTypes;
            ossim_uint32 idx = 0;
            theImageWindow->theResamplerSelection->clear();
            resampler->getFilterTypes(filterTypes);
            for(idx = 0; idx < filterTypes.size(); ++idx)
            {
               theImageWindow->theResamplerSelection->insertItem(filterTypes[idx].c_str());
            }
            theImageWindow->theResamplerSelection->setCurrentText(resampler->getMagnifyFilterTypeAsString().c_str());
         } 
      }
   }
}


ossimGpt ossimQtImageWindowController::getCenterGround()const
{
   ossimGpt centerGround;
   
   centerGround.makeNan();
   
   ossimProjection* proj = PTR_CAST(ossimProjection, theView.get());
   ossimIrect vRect = theImageWindow->getImageWidget()->getAbsoluteViewportRect();
   ossimDpt pt = vRect.midPoint();
   if(proj)
   {
      proj->lineSampleToWorld(pt, centerGround);
   }
   
   return centerGround;
}

ossimGpt ossimQtImageWindowController::getBoundingCenterGround()const
{
   ossimGpt centerGround;
   
   centerGround.makeNan();
   
   ossimProjection* proj = PTR_CAST(ossimProjection, theView.get());
   ossimIrect vRect = theImageWindow->getImageWidget()->getBoundingRect();
   ossimDpt pt = vRect.midPoint();
   if(proj)
   {
      proj->lineSampleToWorld(pt, centerGround);
   }
   
   return centerGround;
}


void ossimQtImageWindowController::applyViewWithCenter(const ossimObject* view,
                                                       const ossimGpt& gpt)
{
   if(!view) return;
   ossimGpt centerGround = gpt;
   
   if(view)
   {
      if(centerGround.isLatNan()||centerGround.isLonNan())
      {
         ossimProjection* proj = PTR_CAST(ossimProjection, theView.get());
         ossimIrect vRect = theImageWindow->getImageWidget()->getAbsoluteViewportRect();
         ossimDpt pt = vRect.midPoint();
         if(proj)
         {
            proj->lineSampleToWorld(pt, centerGround);
         }
      }
      if(view != theView.get())
      {
         theView = 0;
         
         theView = view->dup();
      }
      setViewToInputs();
      ossimDpt center;
      
      center = groundToImage(centerGround);
      
      if(!theImageWindow->isMinimized()&&
         !theImageWindow->isHidden()&&
         theImageWindow->isVisible())
      {
         getImageWidget()->refresh(center);
      }
      else
      {
         getImageWidget()->refresh(center, false);
      }
   }
   
   emit viewChanged(theView.get());
}

void ossimQtImageWindowController::applyView(const ossimObject* view)
{
   if(!view) return;
   
   applyViewWithCenter(view, theCurrentCenter);
}


void ossimQtImageWindowController::propagateView()
{
   if(!theView) return;
   QObject* root = ossimQtApplicationUtility::getRoot(theImageWindow);
   if(root)
   {
      ossimQtDisplayGeoViewEvent *evt =
         new ossimQtDisplayGeoViewEvent(theView.get(), theCurrentCenter);
      evt->setEventObject(theImageWindow);
      
      QApplication::postEvent(root, evt);
   }
}

void ossimQtImageWindowController::propagateResampler()
{
   QObject* root = ossimQtApplicationUtility::getRoot(theImageWindow);
   if(root)
   {
      ossimString minify, magnify;
      
      getInputResampler(minify, magnify);   
      ossimQtResamplerEvent* evt = new ossimQtResamplerEvent(minify, magnify);
      evt->setEventObject(theImageWindow);
      evt->setPropagating();
      QApplication::postEvent(root, evt);
   }
}

void ossimQtImageWindowController::fitToWindow()
{
   
   if(theImageWindow && theImageWindow->getImageWidget() )
   {
      ossimIrect currentBoundingRect = getImageWidget()->getBoundingRect();
      ossimIrect vRect = getImageWidget()->getViewportRect();
      
      if( !vRect.hasNans() && !currentBoundingRect.hasNans() )
      {
         if ( !theIdentityModeFlag )
         {
            
            double length1 = ossim::max(currentBoundingRect.width(),
                                        currentBoundingRect.height());
            double length2 = ossim::min(vRect.height(),
                                        vRect.width());         
            
            ossimDpt ratio(length1/length2, length1/length2);
            if( (ratio.x > (1.0+FLT_EPSILON))||
               (ratio.y > (1.0+FLT_EPSILON))||
               (ratio.x < (1.0-FLT_EPSILON))||
               (ratio.x < (1.0-FLT_EPSILON)))
            {
               ossim_uint32 rLevels =
               (ossim_uint32)std::ceil(std::log(ratio.x)/std::log(2.0));
               
               ossim_uint32 inputRLevels = getNumberOfRLevels();
               
               if (inputRLevels < rLevels)
               {
                  if ( buildOverviews() )
                  {
                     inputRLevels = getNumberOfRLevels();
                  }
               }
               
               if((rLevels<=inputRLevels))    
               {
                  // zoomTo(vRect.midPoint(), ratio);
                  zoomToCenter(ratio);
               }
               else
               {
                  ossimNotify(ossimNotifyLevel_WARN)
                  << "WARNING ossimQtImageWindowController::fitToWindow:\n"
                  << "Will not fit image to window, "
                  << "not enough RLevels for the scale" << std::endl;
                  
                  // zoomTo(vRect.midPoint(), ossimDpt(1.0,1.0));
                  zoomToCenter(ossimDpt(1.0,1.0));
               }
            }
            
         } // End of "if ( !theIdentityModeFlag )"
         else
         {
            ossimRLevelFilter* rf = getRLevelFilter();
            if (rf)
            {
               ossim_uint32 numberOfRsets = rf->getNumberOfDecimationLevels();
               if ( numberOfRsets )
               {
                  ossim_uint32 rset = 0;
                  ossimIrect rect;
                  
                  do
                  {
                     rf->setCurrentRLevel(rset); 
                     rect =  rf->getBoundingRect(rset);
                     if ( (rect.width()  < vRect.width()) &&
                         (rect.height() < vRect.height()) )
                     {
                        break;
                     }
                     
                     ++rset;
                     
                  } while (rset < numberOfRsets);
                  
                  // rf->setCurrentRLevel(rset);
                  getImageWidget()->refresh(rect.midPoint());
               }
            }            
            
         } // End of identity mode block.
         
      } // End of "if ( !vRect.hasNans() && !curr..."
   }
}

void ossimQtImageWindowController::refreshWindow()
{
   ossimIpt center = groundToImage(theCurrentCenter);
   ossim_uint32 numInputs = theEventCapture->getNumberOfInputs();
   ossim_uint32 idx = 0;
   ossimRefreshEvent event;
   
   for(idx = 0; idx < numInputs; ++idx)
   {
      if(theEventCapture->getInput(idx))
      {
         theEventCapture->propagateEventToInputs(event);
      }
   }
   getImageWidget()->refresh(center, true);
}

void ossimQtImageWindowController::fullRes()
{
   if ( !theIdentityModeFlag )
   {
      if(theFullRes.hasNans()) return;
      
      zoomToMetersPerPixel(theFullRes);
   }
   else // In identity mode.
   {
      ossimRefPtr<ossimRLevelFilter> rf = getRLevelFilter();
      if ( rf.valid() && theImageModel.valid() )
      {
         ossim_uint32 currentRrds = rf->getCurrentRLevel();
         
         if (currentRrds) // Not at r0.
         {
            ossimDpt rnPt = groundToImage(theCurrentCenter);
            ossimDpt r0Pt;
            rnToR0(rnPt, r0Pt);
            
            // Change rrds level.
            rf->setCurrentRLevel(0);
            
            // Recenter.
            ossimIpt center = r0Pt;
            getImageWidget()->refresh(center);
         }
      }
   }
}

void ossimQtImageWindowController::zoomToMetersPerPixel(const ossimDpt& metersPerPixel)
{
   ossimGpt centerGround = theCurrentCenter;
   ossimMapProjection* mapProj = PTR_CAST(ossimMapProjection, theView.get());
   ossimDpt tempGsd = metersPerPixel;
   if(mapProj)
   {
      //     double average = (metersPerPixel.x+metersPerPixel.y)/2.0;
      //      if(mapProj->isGeographic())
      //        {
      // 	 ossimGpt origin = mapProj->origin();
      
      
      // 	 if( (origin.latd() == 0.0)&&
      // 	     (origin.lond() == 0.0))
      // 	   {
      
      // 	     tempGsd = ossimDpt(average, average);
      
      // 	   }
      // 	 else // scaled geographic
      // 	   {
      // 	     tempGsd = ossimDpt(cosd(origin.latd())*average,
      // 				average);
      // 	   }
      //       }
      //       else if(mapProj->getMetersPerPixel() != metersPerPixel)
      //       {
      
      //          tempGsd = ossimDpt(average, average);
      //       }
      //       else
      //       {
      //          return;
      //       }
      
      mapProj->setMetersPerPixel(tempGsd);
      
      setViewToInputs();
      
      getImageWidget()->refresh(false);
      ossimIpt center = groundToImage(centerGround);
      getImageWidget()->panTo(center, true);
      
      emit viewChanged(theView.get());
   }
}

ossimDpt ossimQtImageWindowController::groundToImage(const ossimGpt& gpt)const
{
   ossimDpt result;
   result.makeNan();
   
   const ossimProjection* proj = 0;
   
   if ( !theIdentityModeFlag )
   {
      proj = PTR_CAST(ossimProjection, theView.get());
      proj->worldToLineSample(gpt, result);
   }
   else if (theImageModel.valid())
   {
      proj = theImageModel->getProjection();
      if (proj)
      {
         ossimDpt r0Pt;
         proj->worldToLineSample(gpt, r0Pt);
         r0ToRn(r0Pt, result);
      }
   }
   
   return result;
}

ossimGpt ossimQtImageWindowController::imageToGround(const ossimIpt& ipt)const
{
   ossimDpt dpt = ipt;
   return imageToGround(dpt);
}

ossimGpt ossimQtImageWindowController::imageToGround(const ossimDpt& dpt)const
{
   ossimGpt result;
   result.makeNan();
   
   const ossimProjection* proj = 0;
   
   if ( !theIdentityModeFlag )
   {
      proj = PTR_CAST(ossimProjection, theView.get());
      if (proj)
      {
         proj->lineSampleToWorld(dpt, result);
      }
      else
      {
         ossimNotify(ossimNotifyLevel_WARN)
         << "ossimQtImageWindowController::imageToGroundend "
         << "null projection!" << endl;
      }
   }
   else if (theImageModel.valid())
   {
      proj = theImageModel->getProjection();
      if (proj)
      {
         // Tranform to rn point to r0 point.
         ossimDpt r0Pt;
         rnToR0(dpt, r0Pt);
         proj->lineSampleToWorld(r0Pt, result);
      }
   }
   
   return result;
}

ossimDpt ossimQtImageWindowController::imageToEastingNorthing(
                                                              const ossimDpt& dpt)const
{
   ossimDpt result;
   result.makeNan();
   
   const ossimMapProjection* mapProj = 0;
   
   if ( !theIdentityModeFlag )
   {
      mapProj = PTR_CAST(ossimMapProjection, theView.get());
   }
   else if (theImageModel.valid())
   {
      const ossimProjection* proj = theImageModel->getProjection();
      mapProj = PTR_CAST(ossimMapProjection, proj);
   }
   
   if(mapProj)
   {
      if(!mapProj->isGeographic())
      {
         mapProj->lineSampleToEastingNorthing(dpt, result);
      }
   }
   return result;
}

ossimDpt ossimQtImageWindowController::imageToEastingNorthing(const ossimIpt& ipt)const
{
   ossimDpt dpt = ipt;
   return imageToEastingNorthing(dpt);
}

void ossimQtImageWindowController::setFullResInputValue()
{
   theFullRes.makeNan();
   
   if(theImageWindow&&theImageWindow->getImageWidget())
   {
      ossimConnectableObject::ConnectableObjectList viewList;
      theEventCapture->findAllInputsOfType(viewList,
                                           STATIC_TYPE_INFO(ossimImageRenderer),
                                           true,
                                           true);
      if(viewList.size())
      {
         ossimImageRenderer* renderer = PTR_CAST(ossimImageRenderer, viewList[0].get());
         
         if(renderer&&renderer->getImageViewTransform())
         {
            theFullRes = renderer->getImageViewTransform()->getInputMetersPerPixel();
         }
      }
   }
}

ossim_uint32 ossimQtImageWindowController::getNumberOfRLevels()
{
   if(!theEventCapture->getInput()) return 0;
   
   ossimConnectableObject* obj = getImageRenderer();
   ossimImageSource* inter=0;
   if(!obj)
   {
      inter = PTR_CAST(ossimImageSource, theEventCapture->getInput());
   }
   else
   {
      inter = PTR_CAST(ossimImageSource, obj->getInput());
   }
   ossim_uint32 result = 1;
   
   if(inter)
   {
      result = inter->getNumberOfDecimationLevels();
   }
   return result;
}

void ossimQtImageWindowController::getInputResampler(ossimString& minify,
                                                     ossimString& magnify)
{
   ossimImageRenderer* renderer = getImageRenderer();
   if(renderer&&renderer->getResampler())
   {
      minify  = renderer->getResampler()->getMinifyFilterTypeAsString();
      magnify = renderer->getResampler()->getMagnifyFilterTypeAsString();
   }
}

void ossimQtImageWindowController::customEvent(QCustomEvent* event)
{
   switch(event->type())
   {
      case OSSIM_QT_EVENT_DISPLAY_GEO_VIEW_ID:
      {
         ossimQtDisplayGeoViewEvent* evt = (ossimQtDisplayGeoViewEvent*)event;
         thePropagateFlag = evt->isPropagating();
         if(evt->getEventObject() != theImageWindow)
         {
            theCurrentCenter = evt->getViewCenter();
			   
            applyView(evt->getView());
         }
         break;
      }  
      case OSSIM_QT_EVENT_RESAMPLER_ID:
      {
         ossimQtResamplerEvent* evt = (ossimQtResamplerEvent*)event;
         thePropagateFlag = evt->isPropagating();
         if(evt->getEventObject() != theImageWindow)
         {
            setResampler(evt->getMinify().c_str(), evt->getMagnify().c_str());
         }
         
         break;
      }
      case OSSIM_QT_EVENT_REFRESH_DISPLAY_ID:
      {
         if(getImageWidget())
         {
            getImageWidget()->refresh(false);
            getImageWidget()->panTo(groundToImage(theCurrentCenter), true);
         }
         break;
      }
      case OSSIM_QT_EVENT_FLUSH_DISPLAY_ID:
      {
         if(getImageWidget())
         {
            getImageWidget()->flushCache(true);
         }
         break;
      }
      case OSSIM_QT_EVENT_TRACK_POSITION_ID:
      {
         if(getImageWidget())
         {
            ossimQtDisplayTrackPositionEvent* evt = (ossimQtDisplayTrackPositionEvent*)event;
            if((!evt->getGround().isLatNan())&&(!evt->getGround().isLonNan()))
            {
               ossimProjection* output =
                  PTR_CAST(ossimProjection, theView.get());
               ossimIpt imagePt = groundToImage(evt->getGround());
               if(output)
               {
                  // need to track the position
                  //
                  getImageWidget()->trackCursor(imagePt);
               }
               if( (evt->getState()  == Qt::LeftButton) ||
                  (evt->getButton() == Qt::LeftButton) )
               {
                  ossim_uint8 r;
                  ossim_uint8 g;
                  ossim_uint8 b;
                  getImageWidget()->getRgb(imagePt,
                                           r, g, b);
                  getImageWidget()->getRaw(imagePt,
                                           theRawPixelValues);
                  theRgbVector = ossimRgbVector(r, g, b);
                  buildStatusText(theStatusTextString);
                  setStatusText();
                  theCurrentCenter = imageToGround(groundToImage(evt->getGround()));
               }
            }
         }
         break;
      }
      case OSSIM_QT_EVENT_STATE_EVENT:
      {
         ossimQtStateEvent* stateEvent = (ossimQtStateEvent*)event;
         
         if(stateEvent)
         {
            
            ossimKeywordlist& kwl = stateEvent->getKeywordlist();
            if(stateEvent->getStateType() == ossimQtStateEvent::ossimQtStateEventType_SAVE)
            {
               saveState(kwl, stateEvent->getPrefix());
            }
            else
            {
               loadState(kwl, stateEvent->getPrefix());
            }
         }
         break;
      }
      default:
      {
         break;
      }
   }
   thePropagateFlag = false;
}

void ossimQtImageWindowController::buildStatusText(QString& statusText)
{
   ossimDms latDms(theCurrentCenter.latd());
   ossimDms lonDms(theCurrentCenter.lond(), false);
   
   double mslH = ossimElevManager::instance()->getHeightAboveMSL(theCurrentCenter);
   double ellipseHeight = ossimElevManager::instance()->getHeightAboveEllipsoid(theCurrentCenter);
   
   ossimDpt imgPt = groundToImage(theCurrentCenter);
   ossimDpt rawImgPt = viewCoordinateToImageCoordinate(imgPt);
   // If in identity mode we can get the image line and sample.
   ossimDpt r0Pt;
   if ( theIdentityModeFlag )
   {
      rnToR0(imgPt, r0Pt);
   }
   
   if(thePositionInformationDialog)
   {
      ossimUsgsQuad quadName(theCurrentCenter);
      std::ostringstream informationFormat;
      
      ossimDpt eastingNorthing = imageToEastingNorthing(imgPt);
      informationFormat << setiosflags(ios::left) << setw(20)
      << "Lat:"
      << latDms.toString("dd@mm'ss.ssss\" C").c_str() << "\n"
      << setiosflags(ios::left) << setw(20)
      << "Lon:"
      << lonDms.toString("ddd@mm'ss.ssss\" C").c_str() << "\n";
      
      informationFormat << setprecision(17) << setiosflags(ios::left) << setw(20)
      << "Lat:" << theCurrentCenter.latd()<< "\n"
      << setiosflags(ios::left) << setw(20)
      << "Lon:" << theCurrentCenter.lond() << "\n";
      
      
      
      informationFormat << "USGS Q.Q. name:     " << quadName.quarterQuadSegName() << "\n";
      
      if ( theIdentityModeFlag )
      {
         informationFormat << setiosflags(ios::left) << setw(20) << "x, y:";
         r0Pt.print(informationFormat, 0);
         informationFormat << "\n";
      }
      else
      {
         informationFormat << setiosflags(ios::left) << setw(20) << "x, y:" << rawImgPt.toString() << std::endl;
      }
      if(ossim::isnan(ellipseHeight) == false)
      {
         informationFormat << setiosflags(ios::left) << setw(20) << "Height:" << theCurrentCenter.height() << "\n";
      }
      else
      {
         informationFormat << setiosflags(ios::left) << setw(20) << "Height:" << "nan" << "\n";
      }
      if(ossim::isnan(mslH) == false)
      {
         informationFormat << setiosflags(ios::left) << setw(20) << "MSL Height:" << mslH << " meters" <<"\n";
      }
      else
      {
         informationFormat << setiosflags(ios::left) << setw(20) << "MSL Height: " << "nan" << "\n";
      }
      
      if(!eastingNorthing.hasNans())
      {
         ossim_int32 zone;
         char hem[2];
         hem[1]='\0';
         getZoneHemisphere(zone, hem[0]);
         if (zone)
         {
            informationFormat << setiosflags(ios::left) << setw(20)
            << "Utm zone:" << zone << " " << hem
            << "\nEasting Northing: "
            <<eastingNorthing.toString(2) << "\n";
         }
      }
      
      informationFormat << setiosflags(ios::left) << setw(20) << "rgb:" << (int)theRgbVector.getR() <<" " 
      << (int)theRgbVector.getG() <<" " 
      << (int)theRgbVector.getB() << "\n";
      
      ossimHsiVector hsi = theRgbVector;
      informationFormat << setiosflags(ios::left) << setw(20) << "hue:" << hsi.getH() << "\n"
      << setiosflags(ios::left) << setw(20) << "Sat:" << hsi.getS()*255 <<"\n" 
      << setiosflags(ios::left) << setw(20) << "Int:" << hsi.getI()*255 <<"\n";
      
      if(theRawPixelValues.size())
      {
         ossim_uint32 bidx = 0;
         for(bidx = 0; bidx < theRawPixelValues.size(); ++bidx)
         {
            informationFormat << setiosflags(ios::left) << setw(20) << (ossimString("band ")+ossimString::toString(bidx)+":").c_str() << theRawPixelValues[bidx] << "\n";
         }
      }
      
      informationFormat << std::ends;
      
      thePositionInformationDialog->setText(informationFormat.str());
   }
   
   // Make the string for lower status bar.
   std::ostringstream out;
   
   out << latDms.toString("dd@mm'ss.ssss\" C").c_str() << " "
   << lonDms.toString("ddd@mm'ss.ssss\" C").c_str() << " ";
   if(ossim::isnan(mslH))
   {
      out << " nan m  ";
   }
   else
   {
      out << mslH << " m  ";
   }
   out << "rgb: " << (int)theRgbVector.getR() <<", " 
   << (int)theRgbVector.getG() <<", " 
   << (int)theRgbVector.getB();
   if ( theIdentityModeFlag && theImageModel.valid() )
   {
      out << " x,y: ";
      r0Pt.print(out, 0);
      out << " Mode:Identity";
   }
   else
   {
      out << " Mode:Projected";
   }
   out << std::ends;
   
   statusText = out.str().c_str();
}

void ossimQtImageWindowController::setStatusText()
{
   if(theImageWindow&&theImageWindow->statusBar())
   {
      theImageWindow->statusBar()->message(theStatusTextString);
   }
}

ossimImageHandler* ossimQtImageWindowController::getImageHandler() const
{
   ossimImageHandler* result = 0;
   if ( isSingleImageDisplay() && theEventCapture )
   {
      ossimConnectableObject* obj = theEventCapture->findObjectOfType(
                                                                      "ossimImageHandler",
                                                                      ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                                                                      true);
      if (obj)
      {
         result = PTR_CAST(ossimImageHandler, obj);
      }
   }
   return result; 
}

ossimHistogramRemapper* ossimQtImageWindowController::getHistogramRemapper()
const
{
   ossimHistogramRemapper* result = 0;
   if ( isSingleImageDisplay() && theEventCapture )
   {
      ossimConnectableObject* obj = theEventCapture->findObjectOfType(
                                                                      "ossimHistogramRemapper",
                                                                      ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                                                                      true);
      if (obj)
      {
         result = PTR_CAST(ossimHistogramRemapper, obj);
      }
   }
   return result; 
}

ossimImageRenderer* ossimQtImageWindowController::getImageRenderer() const
{
   ossimImageRenderer* result = 0;
   if ( isSingleImageDisplay() && theEventCapture)
   {
      ossimConnectableObject* obj = theEventCapture->findObjectOfType(
                                                                      "ossimImageRenderer",
                                                                      ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                                                                      true);
      if (obj)
      {
         result = PTR_CAST(ossimImageRenderer, obj);
      }
   }
   return result; 
}

ossimRLevelFilter* ossimQtImageWindowController::getRLevelFilter() const
{
   ossimRLevelFilter* result = 0;
   if ( isSingleImageDisplay() && theEventCapture )
   {
      ossimConnectableObject* obj = theEventCapture->findObjectOfType(
                                                                      "ossimRLevelFilter",
                                                                      ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                                                                      true);
      if (obj)
      {
         result = PTR_CAST(ossimRLevelFilter, obj);
      }
   }
   return result;
}

void ossimQtImageWindowController::addRLevelFilter() const
{
   ossimConnectableObject* obj = theEventCapture->findObjectOfType(
                                                                   "ossimImageChain",
                                                                   ossimConnectableObject::CONNECTABLE_DIRECTION_INPUT,
                                                                   true);
   if (obj)
   {
      ossimImageChain* ic = PTR_CAST(ossimImageChain, obj);
      if (ic)
      {
         //---
         // Add to the end of the chain.
         //
         // Notes:
         // 1) addFirst places on the right hand side of the chain.
         // 2) The ossimImageChain will own this now so this is not a leak.
         //---
         ossimRLevelFilter* rf = new ossimRLevelFilter();
         ic->addFirst(rf);
      }
   }
}

void ossimQtImageWindowController::rnToR0(const ossimDpt& rnPt,
                                          ossimDpt& r0Pt) const
{
   if ( theIdentityModeFlag && theImageModel.valid() )
   {
      ossimRLevelFilter* rf = getRLevelFilter();
      if ( rf )
      {
         ossim_uint32 currentRrds = rf->getCurrentRLevel();
         if (currentRrds != 0)
         {
            // Tranform to rn point to r0 point.
            try
            {
               theImageModel->rnToR0(currentRrds, rnPt, r0Pt);
            }
            catch (const ossimException& e)
            {
               ossimNotify(ossimNotifyLevel_WARN)
               << e.what() << std::endl;;
            }
         }
         else
         {
            r0Pt = rnPt;
         }
      }
   }
}

void ossimQtImageWindowController::r0ToRn(const ossimDpt& r0Pt,
                                          ossimDpt& rnPt) const
{
   if ( theIdentityModeFlag && theImageModel.valid() )
   {
      ossimRLevelFilter* rf = getRLevelFilter();
      if ( rf )
      {
         ossim_uint32 currentRrds = rf->getCurrentRLevel();
         if (currentRrds != 0)
         {
            // Tranform to rn point to r0 point.
            try
            {
               theImageModel->r0ToRn(currentRrds, r0Pt, rnPt);
            }
            catch (const ossimException& e)
            {
               ossimNotify(ossimNotifyLevel_WARN)
               << e.what() << std::endl;;
            }
         }
         else
         {
            rnPt = r0Pt;
         }
      }
   }
}

void ossimQtImageWindowController::getZoneHemisphere(ossim_int32& zone,
                                                     char& hemisphere) const
{
   ossimUtmProjection* utmProj =  PTR_CAST(ossimUtmProjection, theView.get());
   if (utmProj)
   {
      zone       = utmProj->getZone();
      hemisphere = utmProj->getHemisphere();
   }
   else if (!theCurrentCenter.hasNans())
   {
      ossimUtmProjection utm;
      zone = utm.computeZone(theCurrentCenter);
      if (theCurrentCenter.latd() < 0.0)
      {
         hemisphere = 'S';
      }
      else
      {
         hemisphere = 'N';
      }
   }
   else
   {
      zone = 0;
   }
}

void ossimQtImageWindowController::setHistogramStretch()
{
   // getHistogramRemapper() will only give us one if single image display.
   ossimHistogramRemapper* remapper = getHistogramRemapper();
   if (remapper)
   {
      // We will only auto stretch
      ossimScalarType scalar = remapper->getOutputScalarType();
      if ( (scalar == OSSIM_UINT16) || (scalar == OSSIM_SINT16) )
      {
         bool hasHistogram = true;
         ossimRefPtr<ossimHistogram> h = remapper->getHistogram(0);
         if( !h.valid() )
         {
            hasHistogram = buildHistogram();
         }
         
         if (hasHistogram)
         {
            remapper->setStretchMode(ossimHistogramRemapper::LINEAR_AUTO_MIN_MAX, false);
            remapper->setEnableFlag(true);
         }
      }
   }
}

void ossimQtImageWindowController::trackScrollingImageWidget(ossimQtMouseEvent* event)
{
   switch(event->getQtEventType())
   {
      case QEvent::MouseButtonPress:
      case QEvent::MouseButtonRelease: 
      case QEvent::MouseButtonDblClick:
      case QEvent::MouseMove:
      {
         ossimQtDisplayTrackPositionEvent* evt = new ossimQtDisplayTrackPositionEvent;
         ossimGpt gpt;
         
         gpt = imageToGround(event->getShiftPoint());
         evt->setGround(gpt);
         evt->setButton(event->getButton());
         evt->setState(event->getState());
         evt->setStateAfter(event->getStateAfter());
         evt->setQtEventType(event->getQtEventType());
         ossimQtApplicationUtility::postEventToRoot(theImageWindow, evt);
         break;
      }
      default:
      {
         break;
      }
   }
}

void ossimQtImageWindowController::swipeAddLayer()
{
   ossimQtLayerChooser* layerChooser =
   new ossimQtLayerChooser(theImageWindow,
                           "Choose layer to swipe",
                           false,
                           WDestructiveClose);
   
   connect(layerChooser, SIGNAL(applySelected(ossimConnectableObject::ConnectableObjectList&)),
           this, SLOT(swipeAddLayerApply(ossimConnectableObject::ConnectableObjectList&)));
   connect(this, SIGNAL(closeSwipe()), layerChooser, SLOT(close()));
   layerChooser->show();
}

void ossimQtImageWindowController::swipeAddLayerApply(ossimConnectableObject::ConnectableObjectList& layerObjList)
{
   if(layerObjList.size())
   {
      if((theEventCapture->getNumberOfInputs() < 2)&&
         (layerObjList[0] != theEventCapture->getInput()))
      {
         addInput(layerObjList[0].get());
         
         ossimConnectableObject* connectable = (ossimConnectableObject*)(theImageWindow->getImageWidget());
         
         if(theEventCapture->getInput(1))
         {
            connectable->disconnect(theEventCapture->getInput(1));
            theEventCapture->disconnect(theEventCapture->getInput(1));
         }
         
         addInput(layerObjList[0].get(), false);
         setViewToInputs();
         ossimQtRefreshDisplayEvent* refreshDisplayEvent = new ossimQtRefreshDisplayEvent;
         
         QApplication::postEvent(theImageWindow, refreshDisplayEvent);
         emit closeSwipe();
      }
      else
      {
         if(layerObjList[0] == theEventCapture->getInput())
         {
            QMessageBox message("Error", "Can't swipe the same image", QMessageBox::Warning, QMessageBox::Ok,
                                QMessageBox::NoButton,  QMessageBox::NoButton, theImageWindow);
            message.exec();
         }
      }
   }
}

void ossimQtImageWindowController::swipeRemoveLayer()
{
   if(theEventCapture->getInput(1))
   {
      removeInput(theEventCapture->getInput(1), true);
   }
   //   if(theEventCapture->getInput(1))
   //     {
   //        ossimConnectableObject* connectable = (ossimConnectableObject*)(theImageWindow->getImageWidget());
   //        if(theEventCapture->getInput(1))
   //        {
   //           connectable->disconnect(theEventCapture->getInput(1));
   //           theEventCapture->disconnect(theEventCapture->getInput(1));
   //           initializePointers();
   //           initializeMenus();
   //           theImageWindow->getImageWidget()->refresh(true);
   //           getImageWidget()->panTo(groundToImage(theCurrentCenter), true);
   //           setFullResInputValue();
   //        }
   //     }
}

void ossimQtImageWindowController::swipeVertical()
{
   theImageWindow->swipeBoxAction->setOn(false);
   theImageWindow->swipeCircleAction->setOn(false);
   theImageWindow->swipeVerticalAction->setOn(true);
   theImageWindow->swipeHorizontalAction->setOn(false);
   
   getImageWidget()->setSwipeType(ossimQtSwipeScrollingImageWidget::ossimQtSwipeType_VERTICAL);
}

void ossimQtImageWindowController::swipeHorizontal()
{
   theImageWindow->swipeBoxAction->setOn(false);
   theImageWindow->swipeCircleAction->setOn(false);
   theImageWindow->swipeVerticalAction->setOn(false);
   theImageWindow->swipeHorizontalAction->setOn(true);
   getImageWidget()->setSwipeType(ossimQtSwipeScrollingImageWidget::ossimQtSwipeType_HORIZONTAL);
}

void ossimQtImageWindowController::swipeBox()
{
   theImageWindow->swipeBoxAction->setOn(true);
   theImageWindow->swipeCircleAction->setOn(false);
   theImageWindow->swipeVerticalAction->setOn(false);
   theImageWindow->swipeHorizontalAction->setOn(false);
   
   getImageWidget()->setSwipeType(ossimQtSwipeScrollingImageWidget::ossimQtSwipeType_BOX);
}

void ossimQtImageWindowController::swipeCircle()
{
   theImageWindow->swipeBoxAction->setOn(false);
   theImageWindow->swipeCircleAction->setOn(true);
   theImageWindow->swipeVerticalAction->setOn(false);
   theImageWindow->swipeHorizontalAction->setOn(false);
   
   getImageWidget()->setSwipeType(ossimQtSwipeScrollingImageWidget::ossimQtSwipeType_CIRCLE);
}

void ossimQtImageWindowController::swipeNone()
{
}

bool ossimQtImageWindowController::saveState(ossimKeywordlist& kwl,
                                             const ossimString& prefix)const
{
   if(theView.valid())
   {
      theView->saveState(kwl, prefix + "view.");
   }
   
   if(theCurrentCenter.isLatNan())
   {
      kwl.add(prefix,
              "center_ground",
              "nan nan nan",
              true);
   }
   else
   {
      kwl.add(prefix,
              "center_ground",
              ossimString::toString(theCurrentCenter.latd())+ " " +ossimString::toString(theCurrentCenter.lond()),
              true);
   }
   
   QWidget* widget = theImageWindow->parentWidget(true);
   QPoint pt(widget->pos());
   
   kwl.add(prefix,
           "position",
           ossimString::toString((int)pt.x()) + " " + ossimString::toString((int)pt.y()));
   
   kwl.add(prefix,
           "size",
           ossimString::toString((int)theImageWindow->width()) + " " + ossimString::toString((int)theImageWindow->height()));
   
   ossimString showState = "shown";
   if(theImageWindow->isMinimized())
   {
      showState = "minimized";
   }
   else if(theImageWindow->isMaximized())
   {
      showState = "maximized";
   }
   else if(theImageWindow->isHidden())
   {
      showState = "hidden";
   }
   
   kwl.add(prefix,
           "show_state",
           showState,
           true);
   
   theEventCapture->saveState(kwl, prefix);
   
   kwl.add(prefix,
           ossimKeywordNames::TYPE_KW,
           "ossimQtImageWindow",
           true);
   
   return true;
   
}

bool ossimQtImageWindowController::loadState(const ossimKeywordlist& kwl,
                                             const ossimString& prefix)
{
   theView = 0;
   
   if(!theEventCapture)
   {
      theEventCapture = new ossimQtImageControllerOssimEventCapture(this);
   }
   
   theView = ossimProjectionFactoryRegistry::instance()->createProjection(kwl,
                                                                          prefix + "view.");
   
   ossimString currentCenter = kwl.find(prefix, "center_ground");
   istringstream iCenter(currentCenter.c_str());
   double lat, lon;
   iCenter >> lat >> lon;
   
   theCurrentCenter.latd(lat);
   theCurrentCenter.lond(lon);
   
   ossimString position = kwl.find(prefix,
                                   "position");
   
   if(position == "")
   {
      position = "0 0";
   }
   ossimString size = kwl.find(prefix,
                               "size");
   if(size == "")
   {
      size = "512 512";
   }
   
   ossimString dimensions = position + " " + size;
   int x, y, w, h;
   
   istringstream iDimensions(dimensions.c_str());
   iDimensions >> x >> y >> w >> h;
   
   ossimString showState = kwl.find(prefix, "show_state");
   
   // need to get data manager and add the connections
   
   ossimQtGetDataManagerEvent event;
   
   ossimQtApplicationUtility::sendEventToRoot(theImageWindow,
                                              &event);
   
   ossimDataManager* dataManager = event.getDataManager();
   
   if(dataManager)
   {
      ossimString regExpr( prefix+"input_connection[0-9]+" );
      
      vector<ossimString> splitArray;
      vector<ossimString> keys = kwl.getSubstringKeyList( regExpr );
      
      if(keys.size() >0)
      {
         ossimString id = kwl.find(keys[0]);
         ossimConnectableObject* obj = dataManager->getObject(ossimId(id.toInt()));
         theEventCapture->connectMyInputTo(0, obj);
         theImageWindow->getImageWidget()->connectMyInputTo(0, obj);
         if(keys.size() >1)
         {
            id = kwl.find(keys[1]);
            obj = dataManager->getObject(ossimId(id.toInt()));
            theEventCapture->connectMyInputTo(1, obj);
            theImageWindow->getImageWidget()->connectMyInputTo(1, obj);
         }
      }
   }
   if(!theEventCapture->getInput())
   {
      QApplication::postEvent(theImageWindow,
                              new QCloseEvent);
      return false;
   }
   else
   {
      theImageWindow->setGeometry(x,y,w,h);
      if(theEventCapture->getInput()->getDescription()!="")
      {
         theImageWindow->setCaption(theEventCapture->getInput()->getDescription().c_str());
      }
      
      initializePointers();
      initializeMenus();
      theImageWindow->getImageWidget()->refresh(false);
      getImageWidget()->panTo(groundToImage(theCurrentCenter), false);
      setFullResInputValue();
      
      // now do the visibility thing
      if(showState == "minimized")
      {
         theImageWindow->showMinimized();
      }
      else if(showState == "maximized")
      {
         theImageWindow->showMaximized();
      }
      else if(showState == "hidden")
      {
         theImageWindow->hide();
      }
      else
      {
         theImageWindow->show();
      }
   }
   return true;
}

void ossimQtImageWindowController::igenDialogDestroyed()
{
   theIgenDialog = 0;
}

void ossimQtImageWindowController::informationDialogDestroyed()
{
   thePositionInformationDialog = 0;
}

void ossimQtImageWindowController::positionQualityDialogDestroyed()
{
   thePositionQualityDialog = 0;
}

void ossimQtImageWindowController::setIdentityMode(bool flag)
{
   if ( ( flag && !theIdentityModeFlag ) || ( !flag && theIdentityModeFlag ) )
   {
      ossimImageRenderer* renderer = getImageRenderer();
      if (renderer)
      {
         if ( flag )
         {
            renderer->disableSource();
            theIdentityModeFlag = true;
            
            // Disable things that shouldn't be used in "Identity" mode.
            if (theImageWindow)
            {
               theImageWindow->fileSaveAsAction->setEnabled(false);
               theImageWindow->editPositionQualityAction->setEnabled(false);
               theImageWindow->swipeAddLayerAction->setEnabled(false);
               theImageWindow->resamplerPropagateAction->setEnabled(false);
            }
            
            if ( !getRLevelFilter() )
            {
               addRLevelFilter();
            }
            ossimRLevelFilter* rlf = getRLevelFilter();
            if (rlf)
            {
               rlf->enableSource();
            }
            
            if (!theImageModel)
            {
               ossimImageHandler* ih = getImageHandler();
               if (ih)
               {
                  theImageModel = new ossimImageProjectionModel();
                  theImageModel->initialize(*ih);
               }
            }
         }
         else
         {
            ossimRLevelFilter* rlf = getRLevelFilter();
            if (rlf)
            {
               rlf->disableSource();
            }
            renderer->enableSource();
            theIdentityModeFlag = false;
            
            // Re-enable things turned off in "Identity" mode.
            if (theImageWindow)
            {
               theImageWindow->fileSaveAsAction->setEnabled(true);
               theImageWindow->editPositionQualityAction->setEnabled(true);
               theImageWindow->swipeAddLayerAction->setEnabled(true);
               theImageWindow->resamplerPropagateAction->setEnabled(true);
            }
            
            if (theImageModel.valid())
            {
               theImageModel = 0;
            }
         }
         fitToWindow();
      }
   }
}

bool ossimQtImageWindowController::getAutoDeleteIfNoInputFlag()const
{
   return theAutoDeleteIfNoInputFlag;
}

void ossimQtImageWindowController::setAutoDeleteIfNoInputFlag(bool flag)
{
   theAutoDeleteIfNoInputFlag = flag;
}

bool ossimQtImageWindowController::buildOverviews() const
{
   bool result = false;
   
   if (theImageWindow)
   {
      ossimImageHandler* ih = getImageHandler();
      if (ih)
      {
         QString caption("Question:");
         QString text = "Not enough overviews to fit to window.\n";
         text += "Would you like to build reduced resolution data sets?\n";
         text += "Note:\n";
         text +=
         "This can take some time depending on the size of your image.\n";
         text += "Alternatively use the command line application:  \"img2rr\"";
         
         int answer = QMessageBox::question( theImageWindow,
                                            caption,
                                            text,
                                            QMessageBox::Yes,
                                            QMessageBox::No);
         if (answer == QMessageBox::Yes)
         {
            ossimQtOverviewBuilderDialog* builder =
            new ossimQtOverviewBuilderDialog(theImageWindow, ih);
            builder->buildClicked();
            delete builder;
            
            // Update the image handler.
            ih->openOverview();
            result = true;
         }
      }
   }
   
   return result;
}

bool ossimQtImageWindowController::buildHistogram() const
{
   bool result = true;
   
   if (theImageWindow)
   {
      ossimImageHandler* ih = getImageHandler();
      if (ih)
      {
         ossimFilename file =
         ih->getFilenameWithThisExtension(ossimString(".his"));
         if ( !file.exists() || (file.fileSize() == 0) )
         {
            QString caption("Question:");
            QString text = "Would you like to compute a histogram?\n";
            text += "Alternatively use command line application:";
            text += " \"create_histo\"";
            int answer = QMessageBox::question( theImageWindow,
                                               caption,
                                               text,
                                               QMessageBox::Yes,
                                               QMessageBox::No);
            if (answer == QMessageBox::Yes)
            {
               
               ossimQtHistogramBuilderDialog* w =
               new ossimQtHistogramBuilderDialog(theImageWindow, ih);
               w->buildClicked();
               
               //---
               // Note: Dialog not constructed with destructive close so we
               // must delete.
               //---
               delete w; 
               w = 0;
               
               // Check to the file again.
               if ( !file.exists() || (file.fileSize() == 0) )
               {
                  result = false;
                  
                  QString caption = "Warning:";
                  QString text = "Compute histogram failed...";
                  QMessageBox::information( theImageWindow,
                                           caption,
                                           text,
                                           QMessageBox::Ok );
               }
            }
            else
            {
               result = false;
            }
         }
         
         //---
         // If not result flag user may of opted to not build histo or
         // build failed.
         //---
         if (result)
         {
            ossimHistogramRemapper* remapper = getHistogramRemapper();
            if (remapper)
            {
               result = remapper->openHistogram(file);
            }
         }
         
      } // matches: if (ih)
      
   } // matches: if (theImageWindow)
   
   return result;
}
