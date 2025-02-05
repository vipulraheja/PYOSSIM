#pragma ident "$Id: CommandOptionWithTimeArg.cpp 274 2006-10-27 14:24:35Z rickmach $"



//============================================================================
//
//  This file is part of GPSTk, the GPS Toolkit.
//
//  The GPSTk is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation; either version 2.1 of the License, or
//  any later version.
//
//  The GPSTk is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with GPSTk; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  
//  Copyright 2004, The University of Texas at Austin
//
//============================================================================

//============================================================================
//
//This software developed by Applied Research Laboratories at the University of
//Texas at Austin, under contract to an agency or agencies within the U.S. 
//Department of Defense. The U.S. Government retains all rights to use,
//duplicate, distribute, disclose, or release this software. 
//
//Pursuant to DoD Directive 523024 
//
// DISTRIBUTION STATEMENT A: This software has been approved for public 
//                           release, distribution is unlimited.
//
//=============================================================================






/**
 * @file CommandOptionWithTimeArg.cpp
 * Command line options with time (class DayTime) arguments
 */

#include "StringUtils.hpp"
#include "CommandOptionWithTimeArg.hpp"

using namespace std;

namespace gpstk
{
   string CommandOptionWithTimeArg :: checkArguments()
   {
      string errstr = CommandOptionWithAnyArg::checkArguments();

      if (errstr != string())
         return errstr;

      vector<string>::size_type vecindex;
      for(vecindex = 0; vecindex < value.size(); vecindex++)
      {
         string thisTimeSpec = getTimeSpec(vecindex);
         if (thisTimeSpec != string())
         {
            try {
               DayTime dt;
               dt.setToString(value[vecindex], thisTimeSpec);
               times.push_back(dt);
            }
            catch (...)
            {
               errstr += "\"" + value[vecindex] + "\" is not a valid time.";
            }
         }
         else
            errstr += "\"" + value[vecindex] + "\" is not a valid time.";
      }

      return errstr;
   }

   string CommandOptionWithSimpleTimeArg :: getTimeSpec
   (vector<string>::size_type index) const
   {
      int numwords = gpstk::StringUtils::numWords(value[index]);
      string thisTimeSpec;
      switch (numwords)
      {
         case 1:
            thisTimeSpec = "%m/%d/%Y";
            break;
         case 2:
            thisTimeSpec = "%Y %j";
            break;
         case 3:
            thisTimeSpec = "%Y %j %s";
            break;
      }

      return thisTimeSpec;
   }

} // namespace gpstk
