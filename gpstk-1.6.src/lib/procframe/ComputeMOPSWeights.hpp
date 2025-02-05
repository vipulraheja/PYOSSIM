#pragma ident "$Id: ComputeMOPSWeights.hpp 1379 2008-08-29 17:04:02Z architest $"

/**
 * @file ComputeMOPSWeights.hpp
 * This class computes satellites weights based on the Appendix J of MOPS C,
 * and is meant to be used with GNSS data structures.
 */

#ifndef COMPUTEMOPSWEIGHTS_HPP
#define COMPUTEMOPSWEIGHTS_HPP

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
//  Dagoberto Salazar - gAGE ( http://www.gage.es ). 2006, 2007, 2008
//
//============================================================================



#include "DayTime.hpp"
#include "DataStructures.hpp"
#include "WeightBase.hpp"
#include "EngEphemeris.hpp"
#include "TabularEphemerisStore.hpp"
#include "GPSEphemerisStore.hpp"
#include "ComputeIURAWeights.hpp"
#include "TropModel.hpp"
#include "geometry.hpp"             // DEG_TO_RAD


namespace gpstk
{

      /** @addtogroup DataStructures */
      //@{


      /** This class computes satellites weights based on the Appendix J
       *  of MOPS C.
       *
       * It is meant to be used with the GNSS data structures objects
       * found in "DataStructures" class.
       *
       * A typical way to use this class follows:
       *
       * @code
       *   RinexObsStream rin("ebre0300.02o");
       *   RinexNavStream rnavin("brdc0300.02n");
       *   RinexNavData rNavData;
       *   GPSEphemerisStore bceStore;
       *   while (rnavin >> rNavData) bceStore.addEphemeris(rNavData);
       *   bceStore.SearchPast();  // This is the default
       *
       *   RinexNavHeader rNavHeader;
       *   IonoModelStore ionoStore;
       *   IonoModel ioModel;
       *   rnavin >> rNavHeader;
       *   ioModel.setModel(rNavHeader.ionAlpha, rNavHeader.ionBeta);
       *   ionoStore.addIonoModel(DayTime::BEGINNING_OF_TIME, ioModel);
       *
       *   Position nominalPos(4833520.2269, 41537.00768, 4147461.489);
       *
       *   MOPSTropModel mopsTM;
       *   mopsTM.setReceiverHeight(nominalPos.getAltitude());
       *   mopsTM.setReceiverLatitude(nominalPos.getGeodeticLatitude());
       *   mopsTM.setDayOfYear(30);    // Day of the year
       *
       *   ModeledPR modelRef( nominalPos,
       *                       ionoStore,
       *                       mopsTM,
       *                       bceStore,
       *                       TypeID::C1,
       *                       true );
       *
       *      // Declare a GDS object
       *   gnssRinex gRin;
       *
       *      // Create a 'ComputeMOPSWeights' object
       *   ComputeMOPSWeights mopsW(nominalPos, bceStore);
       *
       *   while(rin >> gRin)
       *   {
       *      gRin >> modelRef >> mopsW;
       *   }
       * @endcode
       *
       * The "ComputeMOPSWeights" object will visit every satellite in the
       * GNSS data structure that is "gRin" and will try to compute its weight
       * based on the MOPS algorithm.
       *
       * It is very important to note that MOPS algorithm demands a proper
       * modeling  of the observable before starting, otherwise it won't work.
       * That is the reason of the long initialization phase, where the
       * ionospheric model (ionoStore), the MOPS tropospheric model (mopsTM)
       * and the general model (modelRef) objects are set up.
       *
       * When used with the ">>" operator, this class returns the same incoming
       * data structure with the weights inserted along their corresponding
       * satellites. Be warned that if it is not possible to compute the
       * weight for a given satellite, it will be summarily deleted from
       * the data structure.
       *
       * @sa ComputeIURAWeights.hpp.
       */
   class ComputeMOPSWeights : public ComputeIURAWeights
   {
   public:

         /// Default constructor. Generates an invalid object.
      ComputeMOPSWeights() : receiverClass(2), defaultIono(TypeID::ionoL1)
      { pBCEphemeris = NULL; pTabEphemeris = NULL; setIndex(); }


         /** Common constructor
          *
          * @param pos       Reference position.
          * @param bcephem   GPSEphemerisStore object holding the ephemeris.
          * @param rxClass   Receiver class. By default, it is 2.
          */
      ComputeMOPSWeights( const Position& pos,
                          GPSEphemerisStore& bcephem,
                          int rxClass = 2 )
         : receiverClass(rxClass), nominalPos(pos), defaultIono(TypeID::ionoL1)
      { setDefaultEphemeris(bcephem); setIndex(); };


         /** Common constructor
          *
          * @param pos       Reference position.
          * @param tabephem  TabularEphemerisStore object holding the
          *                  ephemeris.
          * @param rxClass   Receiver class. By default, it is 2.
          */
      ComputeMOPSWeights( const Position& pos,
                          TabularEphemerisStore& tabephem,
                          int rxClass = 2 )
         : receiverClass(rxClass), nominalPos(pos), defaultIono(TypeID::ionoL1)
      { setDefaultEphemeris(tabephem); setIndex(); };


         /** Returns a satTypeValueMap object, adding the new data
          *  generated when calling this object.
          *
          * @param time      Epoch corresponding to the data.
          * @param gData     Data object holding the data.
          */
      virtual satTypeValueMap& Process( const DayTime& time,
                                        satTypeValueMap& gData )
         throw(ProcessingException);


         /** Returns a gnnsSatTypeValue object, adding the new data
          *  generated when calling this object.
          *
          * @param gData    Data object holding the data.
          */
      virtual gnssSatTypeValue& Process(gnssSatTypeValue& gData)
         throw(ProcessingException);


         /** Returns a gnnsRinex object, adding the new data generated
          *  when calling this object.
          *
          * @param gData    Data object holding the data.
          */
      virtual gnssRinex& Process(gnssRinex& gData)
         throw(ProcessingException);


         /** Method to set the default ephemeris to be used with
          *  GNSS data structures.
          *
          * @param ephem     TabularEphemerisStore object to be used
          */
      virtual ComputeMOPSWeights& setPosition(const Position& pos)
      { nominalPos = pos; return (*this); };


         /** Method to set the default ephemeris to be used with GNSS
          *  data structures.
          *
          * @param ephem     EphemerisStore object to be used
          */
      virtual ComputeMOPSWeights& setDefaultEphemeris(XvtStore<SatID>& ephem);


         /** Method to set the default ephemeris to be used with GNSS
          *  data structures.
          *
          * @param ephem     GPSEphemerisStore object to be used
          */
      virtual ComputeMOPSWeights& setDefaultEphemeris(GPSEphemerisStore& ephem)
      { pBCEphemeris = &ephem; pTabEphemeris = NULL; return (*this); };


         /** Method to set the default ephemeris to be used with GNSS
          *  data structures.
          *
          * @param ephem     TabularEphemerisStore object to be used
          */
      virtual ComputeMOPSWeights& setDefaultEphemeris(
                                             TabularEphemerisStore& ephem )
      { pBCEphemeris = NULL; pTabEphemeris = &ephem; return (*this); };


         /// Method to get the default ionospheric TypeID value to be used.
      virtual TypeID getDefaultIono() const
      { return defaultIono; };


         /** Method to set the default ionospheric TypeID value to be used.
          *
          * @param type      TypeID of ionospheric values to be used by default
          */
      virtual ComputeMOPSWeights& setDefaultIono(const TypeID& type)
      { defaultIono = type; return (*this); };


         /// Returns an index identifying this object.
      virtual int getIndex(void) const;


         /// Returns a string identifying this object.
      virtual std::string getClassName(void) const;


         /// Destructor
      virtual ~ComputeMOPSWeights() {};


   private:


         /// Default receiver class (the usual value is 2).
      int receiverClass;


         /// Nominal position used for computing weights.
      Position nominalPos;


         /// Default ionospheric values to be used.
      TypeID defaultIono;


         /** Method to really get the MOPS weight of a given satellite.
          *
          * @param sat           Satellite
          *
          */
      virtual double getWeight( const SatID& sat,
                                typeValueMap& tvMap )
         throw(InvalidWeights);


         // Compute ionospheric sigma^2 according to Appendix J.2.3
         // and Appendix A.4.4.10.4 in MOPS-C
      double sigma2iono( const double& ionoCorrection,
                         const double& elevation,
                         const double& azimuth,
                         const Position& rxPosition )
         throw(InvalidWeights);


         /// Initial index assigned to this class.
      static int classIndex;

         /// Index belonging to this object.
      int index;

         /// Sets the index and increment classIndex.
      void setIndex(void)
      { index = classIndex++; };


   }; // End of class 'ComputeMOPSWeights'

      //@}

}  // End of namespace gpstk
#endif   // COMPUTEMOPSWEIGHTS_HPP
