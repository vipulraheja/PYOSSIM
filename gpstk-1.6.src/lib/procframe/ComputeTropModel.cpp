#pragma ident "$Id: ComputeTropModel.cpp 1325 2008-07-29 14:33:43Z architest $"

/**
 * @file ComputeTropModel.cpp
 * This is a class to compute the main values related to a given
 * GNSS tropospheric model.
 */

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
//  Dagoberto Salazar - gAGE ( http://www.gage.es ). 2007, 2008
//
//============================================================================


#include "ComputeTropModel.hpp"


namespace gpstk
{


      // Index initially assigned to this class
   int ComputeTropModel::classIndex = 5000000;


      // Returns an index identifying this object.
   int ComputeTropModel::getIndex() const
   { return index; }


      // Returns a string identifying this object.
   std::string ComputeTropModel::getClassName() const
   { return "ComputeTropModel"; }



      /* Returns a satTypeValueMap object, adding the new data generated when
       * calling a modeling object.
       *
       * @param time      Epoch.
       * @param gData     Data object holding the data.
       */
   satTypeValueMap& ComputeTropModel::Process( const DayTime& time,
                                         satTypeValueMap& gData )
      throw(ProcessingException)
   {

      try
      {

         SatIDSet satRejectedSet;

            // Loop through all the satellites
         satTypeValueMap::iterator stv;
         for(stv = gData.begin(); stv != gData.end(); ++stv) 
         {

            // First check if TropModel was set
            if(pTropModel==NULL)
            {
                  // If TropModel is missing, then remove all satellites
               satRejectedSet.insert( (*stv).first );
               continue;
            }

               // If satellite elevation is missing, remove satellite
            if( (*stv).second.find(TypeID::elevation) == (*stv).second.end() )
            {
               satRejectedSet.insert( (*stv).first );
               continue;
            }
            else
            {

                  // Scalar to hold satellite elevation
               double elevation( (*stv).second(TypeID::elevation) );
               double tropoCorr(0.0), dryZDelay(0.0), wetZDelay(0.0);
               double dryMap(0.0), wetMap(0.0);

               try
               {
                     // Compute tropospheric slant correction
                  tropoCorr = pTropModel->correction(elevation);
                  dryZDelay = pTropModel->dry_zenith_delay();
                  wetZDelay = pTropModel->wet_zenith_delay();
                  dryMap = pTropModel->dry_mapping_function(elevation);
                  wetMap = pTropModel->wet_mapping_function(elevation);

                     // Check validity
                  if( !(pTropModel->isValid()) )
                  {
                     tropoCorr = 0.0;
                     dryZDelay = 0.0;
                     wetZDelay = 0.0;
                     dryMap    = 0.0;
                     wetMap    = 0.0;
                  }

               }
               catch(TropModel::InvalidTropModel& e)
               {
                     // If some problem appears, then schedule this
                     // satellite for removal
                  satRejectedSet.insert( (*stv).first );
                  continue;    // Skip this SV if problems arise
               };

                  // Now we have to add the new values to the data structure
               (*stv).second[TypeID::tropoSlant] = tropoCorr;
               (*stv).second[TypeID::dryTropo] = dryZDelay;
               (*stv).second[TypeID::wetTropo] = wetZDelay;
               (*stv).second[TypeID::dryMap] = dryMap;
               (*stv).second[TypeID::wetMap] = wetMap;

            }

         }  // End of loop 'for(stv = gData.begin()...'

            // Remove satellites with missing data
         gData.removeSatID(satRejectedSet);

         return gData;

      }   // End of try...
      catch(Exception& u)
      {
            // Throw an exception if something unexpected happens
         ProcessingException e( getClassName() + ":"
                                + StringUtils::asString( getIndex() ) + ":"
                                + u.what() );

         GPSTK_THROW(e);

      }

   } // End ComputeTropModel::Process()



} // End of namespace gpstk
