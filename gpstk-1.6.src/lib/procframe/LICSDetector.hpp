#pragma ident "$Id: LICSDetector.hpp 1308 2008-07-22 20:01:04Z architest $"

/**
 * @file LICSDetector.hpp
 * This is a class to detect cycle slips using LI observables.
 */

#ifndef LICSDETECTOR_HPP
#define LICSDETECTOR_HPP

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



#include "ProcessingClass.hpp"



namespace gpstk
{

      /** @addtogroup GPSsolutions */
      //@{


      /** This is a class to detect cycle slips using LI observables.
       *
       * This class is meant to be used with the GNSS data structures objects
       * found in "DataStructures" class.
       *
       * A typical way to use this class follows:
       *
       * @code
       *   RinexObsStream rin("ebre0300.02o");
       *
       *   gnssRinex gRin;
       *   ComputeLI getLI;
       *   LICSDetector markCSLI;
       *
       *   while(rin >> gRin)
       *   {
       *      gRin >> getLI >> markCSLI;
       *   }
       * @endcode
       *
       * The "LICSDetector" object will visit every satellite in the GNSS data
       * structure that is "gRin" and will decide if a cycle slip has happened
       * in the given observable.
       *
       * The algorithm will use LI observables, and the LLI1 and LLI2 indexes.
       * The result (a 1 if a cycle slip is found, 0 otherwise) will be stored
       * in the data structure both as the CSL1 and CSL2 indexes.
       *
       * This algorithm will use some values as maximum interval of time
       * between two successive epochs, minimum threshold to declare cycle
       * slip and LI combination limit drift.
       *
       * The default values are usually fine, but you may change them with the
       * appropriate methods. This is of special importance for the maximum
       * interval time, that should be adjusted for your sampling rate. It is
       * 61 seconds by default, which is appropriate for 30 seconds per sample
       * RINEX observation files.
       *
       * When used with the ">>" operator, this class returns the same incoming
       * data structure with the cycle slip indexes inserted along their
       * corresponding satellites. Be warned that if a given satellite does not
       * have the observations required, it will be summarily deleted from the
       * data structure.
       *
       * Be aware that some combinations of cycle slips in L1 and L2 may result
       * in a false negative when using a cycle slip detector based on LI.
       * Therefore, to be on the safe side you should complement this with
       * another kind of detector, such as one based on the Melbourne-Wubbena
       * combination.
       *
       * @sa MWCSDetector.hpp for more information.
       *
       * \warning Cycle slip detectors are objets that store their internal
       * state, so you MUST NOT use the SAME object to process DIFFERENT data
       * streams.
       *
       */
   class LICSDetector : public ProcessingClass
   {
   public:

         /// Default constructor, setting default parameters.
      LICSDetector() : obsType(TypeID::LI), lliType1(TypeID::LLI1),
                       lliType2(TypeID::LLI2), resultType1(TypeID::CSL1),
                       resultType2(TypeID::CSL2), deltaTMax(61.0),
                       minThreshold(0.04), LIDrift(0.002), useLLI(true)
      { setIndex(); };


         /** Common constructor
          *
          * @param mThr    Minimum threshold to declare cycle slip, in meters.
          * @param drift   LI combination limit drift, in meters/second.
          * @param dtMax   Maximum interval of time allowed between two
          *                successive epochs, in seconds.
          */
      LICSDetector( const double& mThr,
                    const double& drift,
                    const double& dtMax = 61.0,
                    const bool& use = true);


         /** Returns a satTypeValueMap object, adding the new data generated
          *  when calling this object.
          *
          * @param epoch     Time of observations.
          * @param gData     Data object holding the data.
          * @param epochflag Epoch flag.
          */
      virtual satTypeValueMap& Process( const DayTime& epoch,
                                        satTypeValueMap& gData,
                                        const short& epochflag = 0 )
         throw(ProcessingException);



         /** Method to get the maximum interval of time allowed between two
          *  successive epochs, in seconds.
          */
      virtual double getDeltaTMax() const
      { return deltaTMax; };


         /** Method to set the maximum interval of time allowed between two
          *  successive epochs.
          *
          * @param maxDelta      Maximum interval of time, in seconds
          */
      virtual LICSDetector& setDeltaTMax(const double& maxDelta);


         /** Method to get the minimum threshold for cycle slip detection, in
          *  meters.
          */
      virtual double getMinThreshold() const
      { return minThreshold; };


         /** Method to set the minimum threshold for cycle slip detection, in
          *  meters.
          *
          * @param mThr    Minimum threshold for cycle slip detection, in
          *                meters.
          */
      virtual LICSDetector& setMinThreshold(const double& mThr);


         /// Method to get the LI combination limit drift, in meters/second
      virtual double getLIDrift() const
      { return LIDrift; };


         /** Method to set the LI combination limit drift, in meters/second
          *
          * @param drift     LI combination limit drift, in meters/second.
          */
      virtual LICSDetector& setLIDrift(const double& drift);


         /// Method to know if the LLI check is enabled or disabled.
      virtual bool getUseLLI() const
      { return useLLI; };


         /** Method to set whether the LLI indexes will be used as an aid
          *  or not.
          *
          * @param use   Boolean value enabling/disabling LLI check
          */
      virtual LICSDetector& setUseLLI(const bool& use)
      { useLLI = use; return (*this); };


         /** Returns a gnnsSatTypeValue object, adding the new data generated
          *  when calling this object.
          *
          * @param gData    Data object holding the data.
          */
      virtual gnssSatTypeValue& Process(gnssSatTypeValue& gData)
         throw(ProcessingException)
      { Process(gData.header.epoch, gData.body); return gData; };


         /** Returns a gnnsRinex object, adding the new data generated when
          *  calling this object.
          *
          * @param gData    Data object holding the data.
          */
      virtual gnssRinex& Process(gnssRinex& gData)
         throw(ProcessingException);


         /// Returns an index identifying this object.
      virtual int getIndex(void) const;


         /// Returns a string identifying this object.
      virtual std::string getClassName(void) const;


         /// Destructor
      virtual ~LICSDetector() {};


   private:


         /// Type of observable.
      TypeID obsType;


         /// Type of LLI1 record.
      TypeID lliType1;


         /// Type of LLI2 record.
      TypeID lliType2;


         /// Type of result #1.
      TypeID resultType1;


         /// Type of result #2.
      TypeID resultType2;


         /// Maximum interval of time allowed between two successive epochs,
         /// in seconds.
      double deltaTMax;


         /// Minimum threshold to declare cycle slip, in meters.
      double minThreshold;


         /// LI combination limit drift, in meters/second.
      double LIDrift;


         /// This field tells whether to use or ignore the LLI indexes as
         /// an aid.
      bool useLLI;


         /// A structure used to store filter data for a SV.
      struct filterData
      {
            // Default constructor initializing the data in the structure
         filterData() : formerEpoch(DayTime::BEGINNING_OF_TIME),
                        windowSize(0), formerLI(0.0), formerBias(0.0),
                        formerDeltaT(1.0)
         {};

         DayTime formerEpoch;    ///< The previous epoch time stamp.
         int windowSize;         ///< Size of current window, in samples.
         double formerLI;        ///< Value of the previous LI observable.
         double formerBias;      ///< Previous bias (LI_1 - LI_0).
         double formerDeltaT;    ///< Previous time difference, in seconds.
      };


         /// Map holding the information regarding every satellite
      std::map<SatID, filterData> LIData;


         /** Method that implements the LI cycle slip detection algorithm
          *
          * @param epoch     Time of observations.
          * @param sat       SatID.
          * @param tvMap     Data structure of TypeID and values.
          * @param epochflag Epoch flag.
          * @param li        Current LI observation value.
          * @param lli1      LLI1 index.
          * @param lli2      LLI2 index.
          */
      virtual double getDetection( const DayTime& epoch,
                                   const SatID& sat,
                                   typeValueMap& tvMap,
                                   const short& epochflag,
                                   const double& li,
                                   const double& lli1,
                                   const double& lli2 );


         /// Initial index assigned to this class.
      static int classIndex;


         /// Index belonging to this object.
      int index;


         /// Sets the index and increment classIndex.
      void setIndex(void)
      { index = classIndex++; };


   }; // End of class 'LICSDetector'

      //@}

}
#endif   // LICSDETECTOR_HPP
