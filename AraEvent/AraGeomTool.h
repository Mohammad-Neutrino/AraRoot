//////////////////////////////////////////////////////////////////////////////
/////  AraGeomTool.h       ARA Geometry tool                             /////
/////                                                                    /////
/////  Description:                                                      /////
/////     The Ara class working out what is where                        /////
/////  Author: Ryan Nichol (rjn@hep.ucl.ac.uk)                           /////
//////////////////////////////////////////////////////////////////////////////

#ifndef ARAGEOMTOOL_H
#define ARAGEOMTOOL_H

//Includes
#include <TObject.h>
#include <TMath.h>
#include "araIcrrStructures.h"
#include "araIcrrDefines.h"
#include "araAtriStructures.h"
#include "AraAntennaInfo.h"
#include "AraStationInfo.h"

#define MAX_ARA_STATIONS ICRR_NO_STATIONS+ATRI_NO_STATIONS

//! Part of AraEvent library. Loads and stores information about each station's geometry as well as information about the antennae (filters, positions, channels etc...).
/*!
  The Ara geometry and numbering tool
  \ingroup rootclasses
*/
class AraGeomTool
{
 public:
   AraGeomTool(); ///< Default constructor
   ~AraGeomTool(); ///< Destructor

   //   AraAntennaInfo *getAntByRfChan(int chan);//FIXME
   //   AraAntennaInfo *getAntByPolAndAnt(AraAntPol::AraAntPol_t antPol, int antNum);//FIXME

   int getChanIndex(AraLabChip::AraLabChip_t chip, int chan) {return chip*CHANNELS_PER_LAB3 +chan;}

   AraLabChip::AraLabChip_t getLabChipForChan(int rfChan, AraStationId_t stationId) {return getStationInfo(stationId)->fAntInfo[rfChan].labChip;}

   int getNumLabChansForChan(int rfChan, AraStationId_t stationId) { return getStationInfo(stationId)->fAntInfo[rfChan].numLabChans;}
   int getFirstLabChanForChan(int rfChan, AraStationId_t stationId) { return getStationInfo(stationId)->fAntInfo[rfChan].labChans[0];}
   int getSecondLabChanForChan(int rfChan, AraStationId_t stationId) { return getStationInfo(stationId)->fAntInfo[rfChan].labChans[1];}


   int getFirstLabChanIndexForChan(int rfChan, AraStationId_t stationId) { return getChanIndex(getLabChipForChan(rfChan, stationId),getFirstLabChanForChan(rfChan, stationId));}


   int getSecondLabChanIndexForChan(int rfChan, AraStationId_t stationId) { return getChanIndex(getLabChipForChan(rfChan, stationId),getSecondLabChanForChan(rfChan, stationId));}

   int isDiplexed(int rfChan, AraStationId_t stationId) {return getStationInfo(stationId)->fAntInfo[rfChan].isDiplexed;}

   Double_t getLowPassFilter(int rfChan, AraStationId_t stationId) { return getStationInfo(stationId)->fAntInfo[rfChan].lowPassFilterMhz; }

   Double_t getHighPassFilter(int rfChan, AraStationId_t stationId) { return getStationInfo(stationId)->fAntInfo[rfChan].highPassFilterMhz; }


   //This is the new version this function
   int getRFChanByPolAndAnt(AraAntPol::AraAntPol_t antPol, int antNum, AraStationId_t stationId);

   //FIXME -- Only used by web-plotter. Should be fixed when the Web-Plotter is up and running
   int getRFChanByPolAndAnt(AraAntPol::AraAntPol_t antPol, int antNum);//FIXME

   AraAntPol::AraAntPol_t getPolByRFChan(int rfChan, AraStationId_t stationId);
   Int_t getAntNumByRFChan(int rfChan, AraStationId_t stationId);
   Int_t getElecChanFromRFChan(int rfChan, AraStationId_t stationId) {return getStationInfo(stationId)->getElecChanFromRFChan(rfChan);}

   
   Double_t calcDeltaTInfinity(Double_t ant1[3], Double_t ant2[3],Double_t phiWave, Double_t thetaWave);
   Double_t calcDeltaTR(Double_t ant1[3], Double_t ant2[3], Double_t phiWave, Double_t thetaWave,Double_t R);
   
   Double_t calcDeltaTInfinity(Int_t rfChan1, Int_t rfChan2,Double_t phiWave, Double_t thetaWave, AraStationId_t stationId);
   Double_t calcDeltaTR(Int_t rfChan1, Int_t rfChan2, Double_t phiWave, Double_t thetaWave,Double_t R, AraStationId_t stationId);

   Double_t calcDeltaTSimple(Double_t ant1[3], Double_t ant2[3], Double_t source[3]);

   AraStationInfo *getStationInfo(AraStationId_t stationId); ///< Would like to make this const but for now this is fine
   

   //Utility functions
   static bool isIcrrStation(AraStationId_t stationId); ///< Returns TRUE if the station is an ICRR station and false otherwise
   static bool isAtriStation(AraStationId_t stationId); ///< Returns TRUE if the station is an ATRIA station and false otherwise
   static Int_t getStationCalibIndex(AraStationId_t stationId); ///< Used by the calibrator. This function returns the calibration and pedestal index for a stationId (TESTBED==0, STATION1==1, STATION1A==0, STATION2==1, STATION3==2...)
   static Int_t getStationNumber(AraStationId_t stationId);
   static void printStationName(AraStationId_t stationId); ///< Print to stdout the station Name
   static const char *getStationName(AraStationId_t stationId); ///< Return char* with the station Name from the stationId
   static AraStationId_t getAtriStationId(int stationNumber); ///< Simple utility function



   //Instance generator
   static AraGeomTool*  Instance();
   
   AraStationInfo *fStationInfoICRR[ICRR_NO_STATIONS]; //station info contains the antenna info and station information
   AraStationInfo *fStationInfoATRI[ATRI_NO_STATIONS]; //station info contains the antenna info and station information

   
   //Some variables to do with ice properties
   static Double_t nTopOfIce;

  
 protected:
   static AraGeomTool *fgInstance;  
   // protect against multiple instances


};


#endif //ARAGEOMTOOL_H
