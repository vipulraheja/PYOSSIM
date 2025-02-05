//----------------------------------------------------------------------------
//
// License:  See top level LICENSE.txt file.
//
// File: ossim-foo.cpp
//
// Author:  David Burken
//
// Description: Contains application definition "foo" app.
//
// NOTE:  This is supplied for simple quick test.  Makefile links with
//        libossim so you don't have to muck with that.
//        DO NOT checkin your test to the svn repository.  Simply
//        edit foo.cc (Makefile if needed) and run your test.
//        After completion you can do a "svn revert foo.cpp" if you want to
//        keep your working repository up to snuff.  Enjoy!
//
// $Id: ossim-foo.cpp 19751 2011-06-13 15:13:07Z dburken $
//----------------------------------------------------------------------------

#include <ossim/base/ossimArgumentParser.h>
#include <ossim/base/ossimApplicationUsage.h>
#include <ossim/base/ossimConstants.h>  // ossim contants...
#include <ossim/base/ossimException.h>
#include <ossim/base/ossimNotify.h>
#include <ossim/init/ossimInit.h>

// Put your includes here:

#include <iostream>
#include <sstream>
using namespace std;

int main(int argc, char *argv[])
{
   ossimArgumentParser ap(&argc, argv);
   ossimInit::instance()->addOptions(ap);
   ossimInit::instance()->initialize(ap);

   try
   {
      // Put your code here.
      string s1 = "foo you";
      istringstream is(s1);
      
      ossimString s2;
      ossimString s3;
      
      is >> s2.string() >> s3.string();
      
      cout << "s1: " << s1
           << "\ns2: " << s2
           << "\ns3: " << s3
           << endl;
   }
   catch (const ossimException& e)
   {
      ossimNotify(ossimNotifyLevel_WARN) << e.what() << std::endl;
      return 1;
   }
   
   return 0;
}
