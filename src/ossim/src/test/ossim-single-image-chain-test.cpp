//----------------------------------------------------------------------------
//
// License:  LGPL
// 
// See LICENSE.txt file in the top level directory for more details.
//
// Author:  David Burken
//
// Description: Test application for ossimSingleImageChain class.
// 
//----------------------------------------------------------------------------
// $Id: ossim-single-image-chain-test.cpp 19751 2011-06-13 15:13:07Z dburken $

#include <iostream>
using namespace std;

#include <ossim/base/ossimFilename.h>
#include <ossim/base/ossimKeywordlist.h>
#include <ossim/base/ossimRefPtr.h>
#include <ossim/base/ossimStdOutProgress.h>
#include <ossim/base/ossimTimer.h>
#include <ossim/imaging/ossimImageHandler.h>
#include <ossim/imaging/ossimImageRenderer.h>
#include <ossim/imaging/ossimSingleImageChain.h>
#include <ossim/imaging/ossimTiffWriter.h>
#include <ossim/init/ossimInit.h>

int main(int argc, char* argv[])
{
   ossimTimer::instance()->setStartTick();
   
   ossimInit::instance()->initialize(argc, argv);

   ossimTimer::Timer_t t1 = ossimTimer::instance()->tick();
   
   cout << "elapsed time after initialize(ms): "
        << ossimTimer::instance()->time_s() << "\n";

   if (argc < 2)
   {
      cout << argv[0] << "<image_file> <optional_output_file>"
           << "\nOpens up single image chain and dumps the state to keyword"
           << " list." << endl;
      return 0;
   }

   ossimRefPtr<ossimSingleImageChain> sic1 = new ossimSingleImageChain();
   if ( sic1->open( ossimFilename(argv[1]) ) )
   {
      //---
      // Since we are doing a sequential write disable the end cache
      // which is only good for displays where you revisit tiles.
      //
      // We won't be using a histogram either.
      //---
      // sic1->setAddChainCacheFlag(false);
      // sic1->setAddHistogramFlag(false);
      sic1->createRenderedChain();
      
      sic1->getImageRenderer()->setEnableFlag(false);
      
      if (argc == 3)
      {
#if 0
         ossimRefPtr<ossimImageGeometry> geom = sic1->getImageGeometry();
         if (geom.valid())
         {
            ossimRefPtr<ossimProjection> proj = geom->getProjection();
            if ( proj.valid() )
            {
               ossimRefPtr<ossimMapProjection> mapProj =
                  PTR_CAST( ossimMapProjection, proj.get() );
               if ( mapProj.valid() )
               {
                  
               }
            }
         }
#endif
         
         ossimRefPtr<ossimImageFileWriter> writer = new ossimTiffWriter();
         if ( writer->open( ossimFilename(argv[2]) ) )
         {
            // Add a listener to get percent complete.
            ossimStdOutProgress prog(0, true);
            writer->addListener(&prog);

            writer->connectMyInputTo(0, sic1.get());
            writer->execute();
            ossimTimer::Timer_t t2 = ossimTimer::instance()->tick();
            cout << "elapsed time after write(ms): "
                 << ossimTimer::instance()->time_s() << "\n";

            cout << "write time minus initialize: "
                 << ossimTimer::instance()->delta_s(t1, t2) << "\n";
         }
      }

      ossimRefPtr<ossimImageGeometry> geom = sic1->getImageGeometry();
      if (geom.valid())
      {
         geom->print(cout);
      }

      // Test the load state.
      ossimKeywordlist kwl;
      sic1->saveState(kwl, 0);

      ossimSingleImageChain* sic2 = new ossimSingleImageChain();
      sic2->loadState(kwl, 0);

      kwl.clear();
      sic2->saveState(kwl, 0);

      cout << "\n\nSingle image chain from load state kwl\n" << kwl;
   }

   // Create a normal chain.
   sic1 = new ossimSingleImageChain();
   if ( sic1->open( ossimFilename(argv[1]) ) )
   {
      sic1->createRenderedChain();
      ossimKeywordlist kwl;
      sic1->saveState(kwl, 0);
      cout << "\n\nNormal single image chain kwl\n" << kwl;
   }

   // Create a stripped down chain.
   sic1 = new ossimSingleImageChain();
   if ( sic1->open( ossimFilename(argv[1]) ) )
   {
      sic1->setAddHistogramFlag(false);
      sic1->setAddResamplerCacheFlag(false);
      sic1->setAddChainCacheFlag(false);
      sic1->createRenderedChain();
      ossimKeywordlist kwl;
      sic1->saveState(kwl, 0);
      cout << "\n\nSingle image chain stripped down kwl\n" << kwl;
   }

   // Create a rgb reversed chain.
   sic1 = new ossimSingleImageChain();
   if ( sic1->open( ossimFilename(argv[1]) ) )
   {
      sic1->setThreeBandReverseFlag(true);
      sic1->createRenderedChain();
      ossimKeywordlist kwl;
      sic1->saveState(kwl, 0);
      cout << "\n\nSingle image chain rgb reversed kwl\n" << kwl;
   }

   cout << "constness test:\n";
   ossimRefPtr<const ossimSingleImageChain> consSic = sic1.get();
   ossimRefPtr<const ossimImageHandler> ihConst =  consSic->getImageHandler().get();
   cout << "image handler bands: " << ihConst->getNumberOfOutputBands() << endl;

   return 0;
}
