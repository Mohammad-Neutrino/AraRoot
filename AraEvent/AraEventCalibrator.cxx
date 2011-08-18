//////////////////////////////////////////////////////////////////////////////
/////  AraEventCalibrator.h        Calibrator                            /////
/////                                                                    /////
/////  Description:                                                      /////
/////     The Ara class for calibrating events                           /////
/////  Author: Ryan Nichol (rjn@hep.ucl.ac.uk)                           /////
//////////////////////////////////////////////////////////////////////////////

#include "AraEventCalibrator.h"
#include "UsefulAraStationEvent.h"
#include "AraGeomTool.h"
#include "TMath.h"
#include "TGraph.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <zlib.h>
#include <cstdlib>


Bool_t AraCalType::hasCableDelays(AraCalType::AraCalType_t calType)
{ 
  if(calType==kFirstCalibPlusCables || calType==kSecondCalibPlusCables)
    return kTRUE;
  return kFALSE;
}

Bool_t AraCalType::hasInterleaveCalib(AraCalType::AraCalType_t calType)
{ 
  if(calType==kFirstCalibPlusCables || calType==kSecondCalibPlusCables ||
     calType==kFirstCalib || calType==kSecondCalib)
    return kTRUE;
  return kFALSE;
}

Bool_t AraCalType::hasBinWidthCalib(AraCalType::AraCalType_t calType)
{ 
  if(calType==kFirstCalibPlusCables || calType==kSecondCalibPlusCables ||
     calType==kFirstCalib || calType==kSecondCalib)
    return kTRUE;
  return kFALSE;
}


Bool_t AraCalType::hasClockAlignment(AraCalType::AraCalType_t calType)
{ 
  if(calType==kSecondCalibPlusCables ||
     calType==kSecondCalib)
    return kTRUE;
  return kFALSE;
}

ClassImp(AraEventCalibrator);

AraEventCalibrator * AraEventCalibrator::fgInstance=0;


AraEventCalibrator::AraEventCalibrator() 
{
  gotPedFile=0;
  loadCalib();
  //Default Constructor
}

AraEventCalibrator::~AraEventCalibrator() {
  //Default Destructor
}

//______________________________________________________________________________
AraEventCalibrator*  AraEventCalibrator::Instance()
{
  //static function
  if(fgInstance)
    return fgInstance;

  fgInstance = new AraEventCalibrator();
  return fgInstance;
}


void AraEventCalibrator::setPedFile(char fileName[])
{
  strncpy(pedFile,fileName,FILENAME_MAX);
  gotPedFile=1;
  loadPedestals();
}

void AraEventCalibrator::loadPedestals()
{
  if(!gotPedFile) {
    char *pedFileEnv = getenv( "ARA_PEDESTAL_FILE" );
    if ( pedFileEnv == NULL ) {
      char calibDir[FILENAME_MAX];
      char *calibEnv=getenv("ARA_CALIB_DIR");
      if(!calibEnv) {
	char *utilEnv=getenv("ARA_UTIL_INSTALL_DIR");
	if(!utilEnv) {
	  sprintf(calibDir,"calib");
	  fprintf(stdout,"AraEventCalibrator::loadPedestals(): INFO - Pedestal file [from ./calib]");
	} else {
	  sprintf(calibDir,"%s/share/araCalib",utilEnv);
	  fprintf(stdout,"AraEventCalibrator::loadPedestals(): INFO - Pedestal file [from ARA_UTIL_INSTALL_DIR/share/calib]");
	}
      }
      else {
	strncpy(calibDir,calibEnv,FILENAME_MAX);
	fprintf(stdout,"AraEventCalibrator::loadPedestals(): INFO - Pedestal file [from ARA_CALIB_DIR]");
      }
      sprintf(pedFile,"%s/peds_1294924296.869787.run001202.dat",calibDir);
      fprintf(stdout," = %s\n",pedFile);
    } // end of IF-block for pedestal file not specified by environment variable
    else {
      strncpy(pedFile,pedFileEnv,FILENAME_MAX);
      fprintf(stdout,"AraEventCalibrator::loadPedestals(): INFO - Pedestal file [from ARA_PEDESTAL_FILE] = %s\n",pedFile);
    } // end of IF-block for pedestal file specified by environment variable
  }

  FullLabChipPedStruct_t peds;
  gzFile inPed = gzopen(pedFile,"r");
  if( !inPed ){
    fprintf(stderr,"ERROR - Failed to open pedestal file %s.\n",pedFile);
    return;
  }

  int nRead = gzread(inPed,&peds,sizeof(FullLabChipPedStruct_t));
  if( nRead != sizeof(FullLabChipPedStruct_t)){
    int numErr;
    fprintf(stderr,"ERROR - Error reading pedestal file %s; %s\n",pedFile,gzerror(inPed,&numErr));
    gzclose(inPed);
    return;
  }

  int chip,chan,samp;
  for(chip=0;chip<LAB3_PER_TESTBED;++chip) {
    for(chan=0;chan<CHANNELS_PER_LAB3;++chan) {
      int chanIndex = chip*CHANNELS_PER_LAB3+chan;
      for(samp=0;samp<MAX_NUMBER_SAMPLES_LAB3;++samp) {
	pedestalData[chip][chan][samp]=peds.chan[chanIndex].pedMean[samp];
      }
    }
  }
  gzclose(inPed);
}

int AraEventCalibrator::doBinCalibration(UsefulAraStationEvent *theEvent, int chanIndex,int overrideRCO)
{
  int nChip=theEvent->chan[chanIndex].chanId/CHANNELS_PER_LAB3;
  int nChan=theEvent->chan[chanIndex].chanId%CHANNELS_PER_LAB3;
  int rco=overrideRCO;
  if(overrideRCO!=0 && overrideRCO!=1) 
    rco=theEvent->getRCO(chanIndex);
  int hbwrap = theEvent->chan[chanIndex].chipIdFlag&0x08;
  char hbextra = (theEvent->chan[chanIndex].chipIdFlag&0xf0)>>4;
  short hbstart=theEvent->chan[chanIndex].firstHitbus;
  short hbend=theEvent->chan[chanIndex].lastHitbus+hbextra;
    


  double calTime=0;
  for(int samp=0;samp<MAX_NUMBER_SAMPLES_LAB3;++samp){
    rawadc[samp]=theEvent->chan[chanIndex].data[samp];
    if(theEvent->chan[chanIndex].data[samp]==0){
      calwv[samp]=0;
      pedsubadc[samp]=0;
    }else{
      pedsubadc[samp]=rawadc[samp]-pedestalData[nChip][nChan][samp];
      calwv[samp]=pedsubadc[samp]*ADCMV;
      if(calwv[samp]>SATURATION) calwv[samp]=SATURATION;
      if(calwv[samp]<-1*SATURATION) calwv[samp]=-1*SATURATION;
    }
  }
    

  if(nChan==0 || (overrideRCO==0 || overrideRCO==1)) {
    //Do the calibration for each chip
    calTime=0;
    int ir=0;
    if(hbwrap){ // Wrapped hitbus
      for(int samp=hbstart+1;samp<hbend;++samp) {
	tempTimeNums[ir++]=calTime;
	calTime+=binWidths[nChip][rco][samp];
      }
    }
    else{
      for(int samp=hbend+1;samp<MAX_NUMBER_SAMPLES_LAB3;++samp) {
	tempTimeNums[ir++]=calTime;
	calTime+=binWidths[nChip][1-rco][samp];
      }
      //Now add epsilon
      calTime+=epsilonVals[nChip][rco];  //Need to check if this is rco or 1-rco
      for(int samp=0;samp<hbstart;++samp) {
	tempTimeNums[ir++]=calTime;
	calTime+=binWidths[nChip][rco][samp];
      }
    }
    for(int samp=ir;samp<MAX_NUMBER_SAMPLES_LAB3;++samp) {
      tempTimeNums[samp]=calTime;
      calTime+=1;
    }    
    TMath::Sort(MAX_NUMBER_SAMPLES_LAB3,tempTimeNums,indexNums,kFALSE);
  }
  // ... and rotate
  int ir=0;
  int numValid=0;
  if(hbwrap){ // Wrapped hitbus
    for(int samp=hbstart+1;samp<hbend;++samp)
      v[ir++]=calwv[samp];
  }else{
    for(int samp=hbend+1;samp<MAX_NUMBER_SAMPLES_LAB3;++samp) 
      v[ir++]=calwv[samp];	
    for(int samp=0;samp<hbstart;++samp)
      v[ir++]=calwv[samp];
  }
  numValid=ir;
  // Fill in remaining bins with zeros
  for(int samp=ir;samp<MAX_NUMBER_SAMPLES_LAB3;++samp)
    v[samp]=0;
  
  //Now we just have to make sure the times are monotonically increasing
  for(int i=0;i<MAX_NUMBER_SAMPLES_LAB3;i++) {
    calTimeNums[i]=tempTimeNums[indexNums[i]];
    calVoltNums[i]=v[indexNums[i]];
    //      std::cout << i << "\t" << indexNums[i] << "\t" << calTimeNums[i] << "\t" << calVoltNums[i] << "\n";
  }
  return numValid;
}


void AraEventCalibrator::calibrateEvent(UsefulAraStationEvent *theEvent, AraCalType::AraCalType_t calType) 
{
  
  static int gotPeds=0;
  if(!gotPeds)  
    loadPedestals();
  gotPeds=1;
  if(AraCalType::hasBinWidthCalib(calType))
    theEvent->guessRCO(0); //Forces the calculation of the RCO phase from the clock


  // Calibrate waveforms
  for(int samp=0;samp<MAX_NUMBER_SAMPLES_LAB3;++samp){
    sampNums[samp]=samp;
    timeNums[samp]=samp*NSPERSAMP;
  }
  
  for(int  chanIndex = 0; chanIndex < NUM_DIGITIZED_TESTBED_CHANNELS; ++chanIndex ){
    int numValid=doBinCalibration(theEvent,chanIndex);



    //Now we stuff it back into the UsefulAraStationEvent object
         
    if(calType==AraCalType::kNoCalib) {
      theEvent->fNumPoints[chanIndex]=MAX_NUMBER_SAMPLES_LAB3;
      for(int samp=0;samp<MAX_NUMBER_SAMPLES_LAB3;samp++) {
	theEvent->fVolts[chanIndex][samp]=rawadc[samp];
	theEvent->fTimes[chanIndex][samp]=sampNums[samp];
      }
    }
    if(calType==AraCalType::kJustUnwrap || calType==AraCalType::kADC) {
      theEvent->fNumPoints[chanIndex]=numValid;
      for(int samp=0;samp<MAX_NUMBER_SAMPLES_LAB3;samp++) {
	theEvent->fTimes[chanIndex][samp]=sampNums[samp];
	if(samp<numValid) {
	  theEvent->fVolts[chanIndex][samp]=v[samp];
	}
	else {
	  theEvent->fVolts[chanIndex][samp]=0;
	}
      }
    }
    if(calType==AraCalType::kVoltageTime) {
      theEvent->fNumPoints[chanIndex]=numValid;
      for(int samp=0;samp<MAX_NUMBER_SAMPLES_LAB3;samp++) {
	theEvent->fTimes[chanIndex][samp]=timeNums[samp];
	if(samp<numValid) {
	  theEvent->fVolts[chanIndex][samp]=v[samp];
	}
	else {
	  theEvent->fVolts[chanIndex][samp]=0;
	}
      }
    }    
    if(calType==AraCalType::kJustPed) {
      theEvent->fNumPoints[chanIndex]=MAX_NUMBER_SAMPLES_LAB3;
      for(int samp=0;samp<MAX_NUMBER_SAMPLES_LAB3;samp++) {
	theEvent->fVolts[chanIndex][samp]=pedsubadc[samp];
	theEvent->fTimes[chanIndex][samp]=sampNums[samp];
      }
    }
    if(AraCalType::hasBinWidthCalib(calType)) {
      //Almost always want this
      theEvent->fNumPoints[chanIndex]=numValid;
      for(int samp=0;samp<MAX_NUMBER_SAMPLES_LAB3;samp++) {
	theEvent->fTimes[chanIndex][samp]=calTimeNums[samp];
	if(samp<numValid) {
	  theEvent->fVolts[chanIndex][samp]=calVoltNums[samp];
	}
	else {
	  theEvent->fVolts[chanIndex][samp]=0;
	}
      }
    }    
  }

  //Now we have done the initial bin-by-bin and epsilon calibrations
  //Next up is to do the clock alignment
  if(AraCalType::hasClockAlignment(calType)) {
    //All of the higher calibrations do some form of clock alignment
    calcClockAlignVals(theEvent,calType);    
    for(int  chanIndex = 0; chanIndex < NUM_DIGITIZED_TESTBED_CHANNELS; ++chanIndex ){
      int nChip=theEvent->chan[chanIndex].chanId/CHANNELS_PER_LAB3;
      for(int samp=0;samp<MAX_NUMBER_SAMPLES_LAB3;samp++) {
	theEvent->fTimes[chanIndex][samp]+=clockAlignVals[nChip];
      }
    }
  }
  



  //For now we just have the one calibration type for interleaving
  AraGeomTool *tempGeom = AraGeomTool::Instance();
  for(int  rfchan = 0; rfchan < RFCHANS_PER_TESTBED; ++rfchan ){
    memset(theEvent->fVoltsRF[rfchan],0,sizeof(Double_t)*2*MAX_NUMBER_SAMPLES_LAB3);
    memset(theEvent->fTimesRF[rfchan],0,sizeof(Double_t)*2*MAX_NUMBER_SAMPLES_LAB3);
    if(tempGeom->getNumLabChansForChan(rfchan)==2) {
      //      std::cout << chan << "\t"
      //		<< tempGeom->getFirstLabChanIndexForChan(rfchan) << "\t"
      //		<< tempGeom->getSecondLabChanIndexForChan(rfchan) << "\n";
      int ci1=tempGeom->getFirstLabChanIndexForChan(rfchan);
      int ci2=tempGeom->getSecondLabChanIndexForChan(rfchan);
      theEvent->fNumPointsRF[rfchan]=theEvent->fNumPoints[ci1]+theEvent->fNumPoints[ci2];
      //Need to zero mean, maybe should do this in each half seperately?
      //RJN hack 13/01/11
      double mean=0;
      for(int i=0;i<theEvent->fNumPoints[ci1];i++) {
	mean+=theEvent->fVolts[ci1][i];
      }
      for(int i=0;i<theEvent->fNumPoints[ci2];i++) {
	mean+=theEvent->fVolts[ci2][i];
      }
      mean/=theEvent->fNumPointsRF[rfchan];

      //Only the interleaved types have any special calib here
      if(AraCalType::hasInterleaveCalib(calType)) {
	int i1=0;
	int i2=0;
	for(int i=0;i<theEvent->fNumPointsRF[rfchan];i++) {
	  if(i1<theEvent->fNumPoints[ci1] && i2<theEvent->fNumPoints[ci2]) {
	    //Both in play
	    if(theEvent->fTimes[ci1][i1]<(theEvent->fTimes[ci2][i2]+interleaveVals[rfchan])) {
	      theEvent->fTimesRF[rfchan][i]=theEvent->fTimes[ci1][i1];
	      theEvent->fVoltsRF[rfchan][i]=theEvent->fVolts[ci1][i1]-mean;
	      //	      std::cout << "A: " << i << "\t" << theEvent->fTimesRF[rfchan][i] << "\n";

	      i1++;
	      continue;
	    }
	    else {
	      theEvent->fTimesRF[rfchan][i]=theEvent->fTimes[ci2][i2]+interleaveVals[rfchan];
	      theEvent->fVoltsRF[rfchan][i]=theEvent->fVolts[ci2][i2]-mean;
	      //	      std::cout << "B: " << i << "\t" << theEvent->fTimesRF[rfchan][i] << "\n";
	      i2++;
	      continue;
	    }
	  }
	  else if(i1<theEvent->fNumPoints[ci1]) {
	    theEvent->fTimesRF[rfchan][i]=theEvent->fTimes[ci1][i1];
	    theEvent->fVoltsRF[rfchan][i]=theEvent->fVolts[ci1][i1]-mean;
	    i1++;
	    //	      std::cout << "C: " << i << "\t" << theEvent->fTimesRF[rfchan][i] << "\n";
	    continue;
	  }
	  else if(i2<theEvent->fNumPoints[ci2]) {

	    theEvent->fTimesRF[rfchan][i]=theEvent->fTimes[ci2][i2]+interleaveVals[rfchan];
	    theEvent->fVoltsRF[rfchan][i]=theEvent->fVolts[ci2][i2]-mean;
	    //	    std::cout << "D: " << i << "\t" << theEvent->fTimesRF[rfchan][i] << "\n";
	    i2++;
	    continue;
	  }
	}
      }
      
      else {	
	for(int i=0;i<theEvent->fNumPointsRF[rfchan];i++) {
	  if(i%2==0) {
	    //ci2 
	    theEvent->fVoltsRF[rfchan][i]=theEvent->fVolts[ci2][i/2];
	    theEvent->fTimesRF[rfchan][i]=theEvent->fTimes[ci2][i/2];	  
	  }
	  else {
	    //ci1 
	    theEvent->fVoltsRF[rfchan][i]=theEvent->fVolts[ci1][i/2];
	    theEvent->fTimesRF[rfchan][i]=theEvent->fTimes[ci1][i/2]+0.5*NSPERSAMP;	  
	  }
	  
	}
      }
      
    }
    else {
      //      std::cout << rfchan << "\t"
      //      << tempGeom->getFirstLabChanIndexForChan(rfchan) << "\n";
      int ci=tempGeom->getFirstLabChanIndexForChan(rfchan);
      theEvent->fNumPointsRF[rfchan]=theEvent->fNumPoints[ci];   

      //Need to zero mean the data
      double mean=0;
      for(int i=0;i<theEvent->fNumPoints[ci];i++) {
	mean+=theEvent->fVolts[ci][i];
      }
      mean/=theEvent->fNumPoints[ci];

      for(int i=0;i<theEvent->fNumPointsRF[rfchan];i++) {
	theEvent->fVoltsRF[rfchan][i]=theEvent->fVolts[ci][i]-mean;
	theEvent->fTimesRF[rfchan][i]=theEvent->fTimes[ci][i];
	
      }
    }
   
    if(AraCalType::hasCableDelays(calType)) {
      Double_t delay=tempGeom->fAntInfo[rfchan].cableDelay;
      //      delay-=180; //Just an arbitrary offset
      for(int i=0;i<theEvent->fNumPointsRF[rfchan];i++) {
	theEvent->fTimesRF[rfchan][i]-=delay;
      }
    }
      
 
  }
  
}


void AraEventCalibrator::loadCalib()
{
  char calibFile[FILENAME_MAX];
  char calibDir[FILENAME_MAX];
  char *calibEnv=getenv("ARA_CALIB_DIR");
  if(!calibEnv) {
    char *utilEnv=getenv("ARA_UTIL_INSTALL_DIR");
    if(!utilEnv)
      sprintf(calibDir,"calib");
    else
      sprintf(calibDir,"%s/share/araCalib",utilEnv);
  }
  else {
    strncpy(calibDir,calibEnv,FILENAME_MAX);
  }  
  int chip,rco,chan;
  //Bin Width Calib
  sprintf(calibFile,"%s/binWidths.txt",calibDir);
  std::ifstream BinFile(calibFile);
  double width;
  while(BinFile >> chip >> rco) {
    for(int samp=0;samp<MAX_NUMBER_SAMPLES_LAB3;samp++) {
      BinFile >> width;
      binWidths[chip][rco][samp]=width;
    }
  }
  //Epsilon Calib
  sprintf(calibFile,"%s/epsilonFile.txt",calibDir);
  std::ifstream EpsFile(calibFile);
  double epsilon;
  while(EpsFile >> chip >> rco >> epsilon) {
    epsilonVals[chip][rco]=epsilon;    
  }
  //Interleave Calib
  sprintf(calibFile,"%s/interleaveFile.txt",calibDir);
  std::ifstream IntFile(calibFile);
  double interleave;
  while(IntFile >> chip >> chan >> interleave) {
    interleaveVals[chan+4*chip]=interleave;    
  }
}



void AraEventCalibrator::calcClockAlignVals(UsefulAraStationEvent *theEvent, AraCalType::AraCalType_t calType)
{
  if(!AraCalType::hasClockAlignment(calType)) return;
  TGraph *grClock[LAB3_PER_TESTBED]={0};
  Double_t lag[LAB3_PER_TESTBED]={0};
  for(int chip=0;chip<LAB3_PER_TESTBED;chip++) {
    clockAlignVals[chip]=0;    
    int chanIndex=TESTBED1_CLOCK_CHANNEL+CHANNELS_PER_LAB3*chip; 
    grClock[chip]=theEvent->getGraphFromElecChan(chanIndex);
    lag[chip]=estimateClockLag(grClock[chip]);
    delete grClock[chip];

    if(chip>0) {
      //Then can actually do some alignment
      clockAlignVals[chip]=lag[0]-lag[chip];
      //The below fudge factors were "tuned" using pulser data 
      // to try and remove period ambiguities resulting from wrong cycle lag
      if(lag[chip]<8 && lag[0]>9) 
	clockAlignVals[chip]-=25;
      if(lag[chip]>9 && lag[0]<7) 
	clockAlignVals[chip]+=25;
      //      std::cout << "clockAlignVals[ " << chip << "] = " << clockAlignVals[chip] << "\n";
    }
    
  }  
}


Double_t AraEventCalibrator::estimateClockLag(TGraph *grClock)
{
  // This funciton estimates the clock lag (i.e. phase but expressed in terms of a deltaT between 0 and 1 period) by just using all the negative-positive zero crossing

  Double_t period=TESTBED1_CLOCK_PERIOD;
  Double_t mean=grClock->GetMean(2);
  Int_t numPoints=grClock->GetN();
  if(numPoints<3) return 0;
  Double_t *tVals=grClock->GetX();
  Double_t *vVals=grClock->GetY();

  Double_t zc[1000]={0}; 
  Double_t rawZc[1000]={0}; 
  int countZC=0;
  for(int i=2;i<numPoints;i++) {
    if(vVals[i-1]<0 && vVals[i]>0) {
      Double_t x1=tVals[i-1];
      Double_t x2=tVals[i];
      Double_t y1=vVals[i-1]-mean;
      Double_t y2=vVals[i]-mean;      
      //      std::cout << i << "\t" << y2 << "\t" << y1 << "\t" << (y2-y1) << "\n";
      zc[countZC]=(((0-y1)/(y2-y1))*(x2-x1))+x1;
      rawZc[countZC]=zc[countZC];
      countZC++;
      //      if(countZC==1)
      //      break;
    }
       
  }

  Double_t firstZC=zc[0];
  while(firstZC>period) firstZC-=period;
  Double_t meanZC=0;
  Double_t meanZC2=0;
  for(int i=0;i<countZC;i++) {
    while((zc[i])>period) zc[i]-=period;
    if(TMath::Abs((zc[i]-period)-firstZC)<TMath::Abs(zc[i]-firstZC))
      zc[i]-=period;
    if(TMath::Abs((zc[i]+period)-firstZC)<TMath::Abs(zc[i]-firstZC))
      zc[i]+=period;
    meanZC+=zc[i];
    meanZC2+=zc[i]*zc[i];
    //     std::cout << i << "\t" << zc[i] << "\t" << rawZc[i] << "\n";     
  }
  meanZC/=countZC;
  meanZC2/=countZC;
  //  Double_t rms=TMath::Sqrt(meanZC2-meanZC*meanZC);
  //  std::cout << meanZC << "\t" << rms << "\n";
  return meanZC;

}

void AraEventCalibrator::fillRCOGuessArray(UsefulAraStationEvent *theEvent, int rcoGuess[LAB3_PER_TESTBED])
{

  for(int chip=0;chip<LAB3_PER_TESTBED;chip++) {
    int chanIndex=TESTBED1_CLOCK_CHANNEL+CHANNELS_PER_LAB3*chip;
    rcoGuess[chip]=theEvent->getRawRCO(chanIndex);
    
    Double_t period[2]={0};
    Double_t rms[2]={0};
    for(int rcoTest=0;rcoTest<2;rcoTest++) {
      int numValid=doBinCalibration(theEvent,chanIndex,rcoTest);
      period[rcoTest]=estimateClockPeriod(numValid,rms[rcoTest]);
    }
    
    Double_t periodTest=(TMath::Abs(period[0]-TESTBED1_CLOCK_PERIOD)-TMath::Abs(period[1]-TESTBED1_CLOCK_PERIOD));
    Double_t rmsTest=(rms[0]-rms[1]);
    if(periodTest<0) {
      rcoGuess[chip]=0;
      if(TMath::Abs(periodTest)<0.5 && rmsTest>0) 
	rcoGuess[chip]=1;
    }    
    else {
      rcoGuess[chip]=1;
      if(TMath::Abs(periodTest)<0.5 && rmsTest<0) {
	rcoGuess[chip]=0;	
      }
    }
    if(rms[rcoGuess[chip]]>4)  {
      rcoGuess[chip]=theEvent->getRawRCO(chanIndex);
    }

    //    std::cout << "AraEventCalibrator:\t" << period[0] << "\t" << period[1] << "\n";
    //    std::cout << "AraEventCalibrator:\t" << periodTest << "\t" << rmsTest << "\t" << rcoGuess[chip] << "\n";
  }
}



Double_t AraEventCalibrator::estimateClockPeriod(Int_t numPoints, Double_t &rms)
{
  // This funciton estimates the period by just using all the negative-positive zero crossing
  if(numPoints<3) return 0;
  Double_t mean=0;
  Double_t vVals[MAX_NUMBER_SAMPLES_LAB3]={0};
  for(int i=0;i<numPoints;i++) {
    mean+=calVoltNums[i];
  }
  mean/=numPoints;
  for(int i=0;i<numPoints;i++) {
    vVals[i]=calVoltNums[i]-mean;
  }


  Double_t zc[1000]={0};
  Double_t periods[1000]={0};
  int countZC=0;
  for(int i=2;i<numPoints;i++) {
    if(vVals[i-1]<0 && vVals[i]>0) {
      Double_t x1=calTimeNums[i-1];
      Double_t x2=calTimeNums[i];
      Double_t y1=vVals[i-1];
      Double_t y2=vVals[i];      
      //      std::cout << i << "\t" << y2 << "\t" << y1 << "\t" << (y2-y1) << "\n";
      Double_t zcTime=(((0-y1)/(y2-y1))*(x2-x1))+x1;
      if(countZC>0) {
	if((zcTime-zc[countZC-1])<10)
	  continue;
      }
      zc[countZC]=zcTime;
      countZC++;
      //      if(countZC==1)
      //      break;
    }    
  }

  if(countZC<2) return 0;
  
  
  int countPeriods=0;
  Double_t meanPeriod=0;
  Double_t meanPeriodSq=0;
  for(int i=1;i<countZC;i++) {
    periods[countPeriods]=zc[i]-zc[i-1];
    meanPeriod+=periods[countPeriods];
    meanPeriodSq+=periods[countPeriods]*periods[countPeriods];
    countPeriods++;
  }
  meanPeriod/=countPeriods;
  meanPeriodSq/=countPeriods;
  rms=TMath::Sqrt(meanPeriodSq-meanPeriod*meanPeriod);
  
  return meanPeriod;
}
