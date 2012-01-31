//////////////////////////////////////////////////////////////////////////////
/////  AraEventCorrelator.h        Correlator                            /////
/////                                                                    /////
/////  Description:                                                      /////
/////     The Ara class for calibrating events                           /////
/////  Author: Ryan Nichol (rjn@hep.ucl.ac.uk)                           /////
//////////////////////////////////////////////////////////////////////////////

#ifndef ARAEVENTCORRELATOR_H
#define ARAEVENTCORRELATOR_H

//Includes
#include <TObject.h>
#include "AraAntennaInfo.h"


#define NUM_PHI_BINS 360
#define NUM_THETA_BINS 180



class UsefulIcrrStationEvent;
class TH2D;
class TGraph;

//!  AraCorrelatorType -- The Calibration Type
/*!
  There are a number of calibration options available to create a UsefulIcrrStationEvent.
  \ingroup rootclasses
*/

namespace AraCorrelatorType {
  typedef enum EAraCorrelatorType {
    kPlaneWave                        = 0, ///<Assumes the source is at infinite distance
    kSphericalDist40,                  ///<Assumes the source is at 40m
    kNotACorType
  } AraCorrelatorType_t;
} 


//!  AraEventCorrelator -- The Ara Event Correlator
/*!
  The Ara Event Correlator
  \ingroup rootclasses
*/
class AraEventCorrelator : public TObject
{
 public:
  AraEventCorrelator(Int_t numAnts=4); ///< Default constructor
   ~AraEventCorrelator(); ///< Destructor
   
   TH2D *getInterferometricMap(UsefulIcrrStationEvent *evPtr, AraAntPol::AraAntPol_t polType, AraCorrelatorType::AraCorrelatorType_t corType=AraCorrelatorType::kPlaneWave);
   void fillAntennaPositions();
   void fillDeltaTArrays(AraCorrelatorType::AraCorrelatorType_t corType);
   void setupDeltaTInfinity();
   void setupDeltaT40m();
   void getPairIndices(int pair, int &ant1, int &ant2);
   Double_t calcDeltaTInfinity(Double_t ant1[3],Double_t rho1, Double_t phi1, Double_t ant2[3],Double_t rho2, Double_t phi2, Double_t phiWave, Double_t thetaWave);
   Double_t calcDeltaTR(Double_t ant1[3],Double_t rho1, Double_t phi1, Double_t ant2[3],Double_t rho2, Double_t phi2, Double_t phiWave, Double_t thetaWave,Double_t R);

   //Instance generator
   static AraEventCorrelator*  Instance(Int_t numAnts=5);
   
   static AraEventCorrelator *fgInstance;  
   // protect against multiple instances

   static TGraph* getNormalisedGraph(TGraph *grIn);
   static Double_t fastEvalForEvenSampling(TGraph *grIn, Double_t xvalue);

   //Start of by just considering the Bicone and Bowtie-slotted ants
   //So only four antennas
   //So only six pairs, eg. v1-v2,v1-v3,v1-v4,v2-v3,v2-v4,v3-v4
   //Sod this go to seven antennas
   
   Int_t fDebugMode;
   Double_t fPhiWaveDeg[NUM_PHI_BINS];
   Double_t fThetaWaveDeg[NUM_PHI_BINS];
   Double_t fPhiWave[NUM_PHI_BINS];
   Double_t fThetaWave[NUM_PHI_BINS];
   //21 is the maximum if you use 7 antennas in each pol
   Int_t fNumAnts;
   Int_t fNumPairs;
   Int_t fFirstAnt[21];
   Int_t fSecondAnt[21];
   Double_t fVPolDeltaT[21][NUM_PHI_BINS][NUM_THETA_BINS];
   Double_t fHPolDeltaT[21][NUM_PHI_BINS][NUM_THETA_BINS];
   Double_t fVPolDeltaTInfinity[21][NUM_PHI_BINS][NUM_THETA_BINS];
   Double_t fHPolDeltaTInfinity[21][NUM_PHI_BINS][NUM_THETA_BINS];
   Double_t fVPolDeltaT40m[21][NUM_PHI_BINS][NUM_THETA_BINS];
   Double_t fHPolDeltaT40m[21][NUM_PHI_BINS][NUM_THETA_BINS];
   Int_t fRfChanVPol[7];
   Int_t fRfChanHPol[7];
   Double_t fVPolPos[7][3];
   Double_t fHPolPos[7][3];
   Double_t fVPolRho[7];
   Double_t fHPolRho[7];
   Double_t fVPolPhi[7];
   Double_t fHPolPhi[7];
   


   ClassDef(AraEventCorrelator,1);

};


#endif //ARAEVENTCORRELATOR_H
