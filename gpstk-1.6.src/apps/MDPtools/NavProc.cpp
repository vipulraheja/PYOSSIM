#pragma ident "$Id: NavProc.cpp 1895 2009-05-12 19:34:29Z afarris $"

/*
  Think, navdmp for mdp, with bonus output that you get data from all code/carrier
  combos.
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
//  Copyright 2008, The University of Texas at Austin
//
//============================================================================

#include "Geodetic.hpp"
#include "NavProc.hpp"

#include "RinexConverters.hpp"

using namespace std;
using namespace gpstk;
using namespace gpstk::StringUtils;


//-----------------------------------------------------------------------------
MDPNavProcessor::MDPNavProcessor(MDPStream& in, std::ofstream& out)
   : MDPProcessor(in, out),
     firstNav(true), almOut(false), ephOut(false), minimalAlm(false),
     badNavSubframeCount(0), navSubframeCount(0)
{
   timeFormat = "%4Y/%03j/%02H:%02M:%02S";

   binByElevation = true;
   if (binByElevation)
   {
      double binSize=5;
      for (double x=0; x<90; x+=binSize)
         bins.push_back(Histogram::BinRange(x, x+binSize));
   }
   else
   {
      bins.push_back(Histogram::BinRange(0, 30));
      double binSize=3;
      for (double x=30; x<60; x+=binSize)
         bins.push_back(Histogram::BinRange(x, x+binSize));
      bins.push_back(Histogram::BinRange(60, 99));
   }
}


//-----------------------------------------------------------------------------
MDPNavProcessor::~MDPNavProcessor()
{
   using gpstk::RangeCode;
   using gpstk::CarrierCode;
   using gpstk::StringUtils::asString;
   
   out << "Done processing data." << endl << endl;
   if (firstNav)
   {
      out << "  No Navigation Subframe messages processed." << endl;
      return;
   }

   out << endl
       << "Navigation Subframe message summary:" << endl
       << "  navSubframeCount: " << navSubframeCount << endl
       << "  badNavSubframeCount: " << badNavSubframeCount << endl
       << "  percent bad: " << setprecision(3)
       << 100.0 * badNavSubframeCount/navSubframeCount << " %" << endl;

   if (badNavSubframeCount==0)
       return;

   out << "Parity Errors" << endl;
   out << "# elev";
   std::map<RangeCarrierPair, Histogram>::const_iterator peh_itr;
   for (peh_itr = peHist.begin(); peh_itr != peHist.end(); peh_itr++)
   {
      const RangeCarrierPair& rcp=peh_itr->first;
      out << "    " << asString(rcp.second)
           << "-"    << leftJustify(asString(rcp.first), 2);
   }
   out << endl;

   Histogram::BinRangeList::const_iterator brl_itr;
   for (brl_itr = bins.begin(); brl_itr != bins.end(); brl_itr++)
   {
      const Histogram::BinRange& br = *brl_itr ;
      out << setprecision(0)
          << right << setw(2) << br.first << "-"
          << left  << setw(2) << br.second << ":";

      for (peh_itr = peHist.begin(); peh_itr != peHist.end(); peh_itr++)
      {
         const RangeCarrierPair& rcp=peh_itr->first;
         Histogram h=peh_itr->second;
         out << right << setw(9) << h.bins[br];
      }

      out << endl;
   }

   // Whoever would write a reference like this should be shot...
   out << right << setw(2) << peHist.begin()->second.bins.begin()->first.first
        << "-" << left  << setw(2) << peHist.begin()->second.bins.rbegin()->first.second
        << ":";

   for (peh_itr = peHist.begin(); peh_itr != peHist.end(); peh_itr++)
      out << right <<  setw(9) << peh_itr->second.total;
      
   out << endl;
}


//-----------------------------------------------------------------------------
void MDPNavProcessor::process(const MDPNavSubframe& msg)
{
   if (firstNav)
   {
      firstNav = false;
      if (verboseLevel)
         out << msg.time.printf(timeFormat)
             << "  Received first Navigation Subframe message"
             << endl;
   }

   navSubframeCount++;
   RangeCarrierPair rcp(msg.range, msg.carrier);
   NavIndex ni(rcp, msg.prn);

   MDPNavSubframe umsg = msg;

   ostringstream oss;
   oss << umsg.time.printf(timeFormat)
       << "  PRN:" << setw(2) << umsg.prn
       << " " << asString(umsg.carrier)
       << ":" << setw(2) << left << asString(umsg.range)
       << "  ";
   string msgPrefix = oss.str();
   
   umsg.cookSubframe();
   if (verboseLevel>3 && umsg.neededCooking)
      out << msgPrefix << "Subframe required cooking" << endl;

   if (!umsg.parityGood)
   {
      badNavSubframeCount++;
      if (verboseLevel)
         out << msgPrefix << "Parity error"
             << " SNR:" << fixed << setprecision(1) << snr[ni]
             << " EL:" << el[ni]
             << endl;

      if (peHist.find(rcp) == peHist.end())
         peHist[rcp].resetBins(bins);

      if (binByElevation)
         peHist[rcp].addValue(el[ni]);
      else
         peHist[rcp].addValue(snr[ni]);

      return;
   }

   short sfid = umsg.getSFID();
   short svid = umsg.getSVID();
   bool isAlm = sfid > 3;
   long sow = umsg.getHOWTime();
   short page = ((sow-6) / 30) % 25 + 1;

   if (((isAlm && almOut) || (!isAlm && ephOut))
       && verboseLevel>2)
   {
      out << msgPrefix
          << "SOW:" << setw(6) << sow
          << " NC:" << static_cast<int>(umsg.nav)
          << " I:" << umsg.inverted
          << " SFID:" << sfid;
      if (isAlm)
         out << " SVID:" << svid
             << " Page:" << page;
      out << endl;
   }

   // Sanity check on the header time versus the HOW time
   short week = umsg.time.GPSfullweek();
   if (sow <0 || sow>=604800)
   {
      badNavSubframeCount++;
      if (verboseLevel>1)
         out << msgPrefix << "  Bad SOW: " << sow << endl;
      return;
   }
      
   DayTime howTime(week, umsg.getHOWTime());
   if (howTime == umsg.time)
   {
      if (verboseLevel && ! (bugMask & 0x4))
         out << msgPrefix << " Header time==HOW time" << endl;
   }
   else if (howTime != umsg.time+6)
   {
      badNavSubframeCount++;
      if (verboseLevel>1)
         out << msgPrefix << " HOW time != hdr time+6, HOW:"
             << howTime.printf(timeFormat)
             << endl;
      if (verboseLevel>3)
         umsg.dump(out);
      return;
   }

   prev[ni] = curr[ni];
   curr[ni] = umsg;

   if (prev[ni].parityGood && 
       prev[ni].inverted != curr[ni].inverted && 
       curr[ni].time - prev[ni].time <= 12)
   {
      if (verboseLevel)
         out << msgPrefix << "Polarity inversion"
             << " SNR:" << fixed << setprecision(1) << snr[ni]
             << " EL:" << el[ni]
             << endl;
   }      

   if (isAlm && almOut)
   {
      AlmanacPages& almPages = almPageStore[ni];
      EngAlmanac& engAlm = almStore[ni];
      SubframePage sp(sfid, page);
      almPages[sp] = umsg;
      almPages.insert(make_pair(sp, umsg));

      if (makeEngAlmanac(engAlm, almPages, !minimalAlm))
      {
         out << msgPrefix << "Built complete almanac" << endl;
         if (verboseLevel>2)
            dump(out, almPages);
         if (verboseLevel>1)
            engAlm.dump(out);
         almPages.clear();
         engAlm = EngAlmanac();
      }            
   }
   if (!isAlm && ephOut)
   {
      EphemerisPages& ephPages = ephPageStore[ni];
      ephPages[sfid] = umsg;
      EngEphemeris engEph;
      try
      {
         if (makeEngEphemeris(engEph, ephPages))
         {
            out << msgPrefix << "Built complete ephemeris, iocd:0x"
                << hex << setw(3) << engEph.getIODC() << dec
                << endl;
            if (verboseLevel>2)
               dump(out, ephPages);
            if (verboseLevel>1)
               out << engEph;
            ephStore[ni] = engEph;
         }
      }
      catch (Exception& e)
      {
         out << e << endl;
      }
   }

}  // end of process()


void  MDPNavProcessor::process(const MDPObsEpoch& msg)
{
   if (!msg)
      return;

   for (MDPObsEpoch::ObsMap::const_iterator i = msg.obs.begin();
        i != msg.obs.end(); i++)
   {
      const MDPObsEpoch::Observation& obs=i->second;      
      NavIndex ni(RangeCarrierPair(obs.range, obs.carrier), msg.prn);
      snr[ni] = obs.snr;
      el[ni] = msg.elevation;
   }
}
