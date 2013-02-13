//////////////////////////////////////////////////////////////////////////////
/////  AraGeomTool.h       ARA Geometry tool                             /////
/////                                                                    /////
/////  Description:                                                      /////
/////     The Ara class working out what is where                        /////
/////  Author: Ryan Nichol (rjn@hep.ucl.ac.uk)                           /////
/////          & Jonathan Davies (jdavies@hep.ucl.ac.uk)                 /////
//////////////////////////////////////////////////////////////////////////////

#include "AraGeomTool.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <zlib.h>
#include <sqlite3.h>
#include <cstdlib>


//sqlite includes
#include <sqlite3.h>

#include "TString.h"
#include "TObjArray.h"
#include "TObjString.h"

AraGeomTool * AraGeomTool::fgInstance=0;
Double_t AraGeomTool::nTopOfIce=1.48;

AraGeomTool::AraGeomTool() 
{
   //Default Constructor

  //Read in the channel maps for the TestBed and Station1
  readChannelMapDb(0);
  readChannelMapDb(1);

}

AraGeomTool::~AraGeomTool() {
   //Default Destructor
}

//______________________________________________________________________________
AraGeomTool*  AraGeomTool::Instance()
{
  //static function
  if(fgInstance)
    return fgInstance;

  fgInstance = new AraGeomTool();
  return fgInstance;
}

int AraGeomTool::getRFChanByPolAndAnt(AraAntPol::AraAntPol_t antPol, int antNum, AraStationId_t stationId)
{
  if(antNum<8 && antNum>=0)
    return fAntLookupTable[stationId][antPol][antNum];
  return -1;
}


AraAntPol::AraAntPol_t AraGeomTool::getPolByRFChan(int rfChan, AraStationId_t stationId)
{
  //RJN Should add a check here for safety
  return fStationInfo[stationId].fAntInfo[rfChan].polType;
 

}

Int_t AraGeomTool::getAntNumByRFChan(int rfChan, AraStationId_t stationId)
{
  //RJN Should add a check here for safety
  return fStationInfo[stationId].fAntInfo[rfChan].antPolNum;
  
}


//FIXME //jpd this is most definitely a hack to make AraCanvasMaker work -> this will only
//return the testbed lookup stuff not station1
int AraGeomTool::getRFChanByPolAndAnt(AraAntPol::AraAntPol_t antPol, int antNum)
{
  
  if(antNum<8 && antNum>=0)
    return fAntLookupTable[0][antPol][antNum];
  return -1;
}//FIXME -- this just loads the stuff for station1



Double_t AraGeomTool::calcDeltaTInfinity(Double_t ant1[3], Double_t ant2[3],Double_t phiWave, Double_t thetaWave)
{
  //Calc some cylindrical coordinates
  Double_t rho1=TMath::Sqrt(ant1[0]*ant1[0]+ant1[1]*ant1[1]);
  Double_t phi1=TMath::ATan2(ant1[1],ant1[0]);
  Double_t rho2=TMath::Sqrt(ant2[0]*ant2[0]+ant2[1]*ant2[1]);
  Double_t phi2=TMath::ATan2(ant2[1],ant2[0]);
  Double_t d1=TMath::Cos(thetaWave)*(ant1[2]*TMath::Tan(thetaWave)+rho1*TMath::Cos(phi1-phiWave));
  Double_t d2=TMath::Cos(thetaWave)*(ant2[2]*TMath::Tan(thetaWave)+rho2*TMath::Cos(phi2-phiWave));
  Double_t t1t2=(d2-d1)*nTopOfIce/TMath::C();
  t1t2*=1e9;
  return t1t2;

}

//jd
Double_t AraGeomTool::calcDeltaTInfinity(Int_t chan1, Int_t chan2,Double_t phiWave, Double_t thetaWave, AraStationId_t stationId)
{
  if(chan1<0 || chan1>=TOTAL_ANTS_PER_ICRR)
    return 0;
  if(chan2<0 || chan2>=TOTAL_ANTS_PER_ICRR)
    return 0;
  return calcDeltaTInfinity(fStationInfo[stationId].fAntInfo[chan1].antLocation,fStationInfo[stationId].fAntInfo[chan2].antLocation,phiWave,thetaWave);            
}

Double_t AraGeomTool::calcDeltaTR(Double_t ant1[3], Double_t ant2[3], Double_t phiWave, Double_t thetaWave,Double_t R)
{

  Double_t xs=R*TMath::Cos(thetaWave)*TMath::Cos(phiWave);
  Double_t ys=R*TMath::Cos(thetaWave)*TMath::Sin(phiWave);
  Double_t zs=R*TMath::Sin(thetaWave);

  
  Double_t d1=TMath::Sqrt((xs-ant1[0])*(xs-ant1[0])+(ys-ant1[1])*(ys-ant1[1])+(zs-ant1[2])*(zs-ant1[2]));
  Double_t d2=TMath::Sqrt((xs-ant2[0])*(xs-ant2[0])+(ys-ant2[1])*(ys-ant2[1])+(zs-ant2[2])*(zs-ant2[2]));
         
  Double_t t1t2=(d1-d2)*nTopOfIce/TMath::C();
  t1t2*=1e9;
  return t1t2;

}

//jd
Double_t AraGeomTool::calcDeltaTR(Int_t chan1, Int_t chan2, Double_t phiWave, Double_t thetaWave,Double_t R, AraStationId_t stationId)
{
  if(chan1<0 || chan1>=TOTAL_ANTS_PER_ICRR)
    return 0;
  if(chan2<0 || chan2>=TOTAL_ANTS_PER_ICRR)
    return 0;
  return calcDeltaTR(fStationInfo[stationId].fAntInfo[chan1].antLocation,fStationInfo[stationId].fAntInfo[chan2].antLocation,phiWave,thetaWave,R);   

}

//______________________________________________________________________________

void AraGeomTool::readChannelMapDb(AraStationId_t stationId){
  sqlite3 *db;
  char *zErrMsg = 0;
  sqlite3_stmt *stmt;

  char calibDir[FILENAME_MAX];
  char fileName[FILENAME_MAX];
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
  sprintf(fileName, "%s/AntennaInfo.sqlite", calibDir);

  //open the database
  //  int rc = sqlite3_open_v2(fileName, &db, SQLITE_OPEN_READONLY, NULL);
  int rc = sqlite3_open(fileName, &db);;
  if(rc!=SQLITE_OK){
    printf("AraGeomTool::readChannelMapDb(AraStationId_t stationId) - Can't open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return;
  }

  const char *query;

  //This is where we decide which table to access in the database
  if(stationId==ARA_TESTBED) query = "select * from TestBed";
  if(stationId==ARA_STATION1) query = "select * from Station1";

  //prepare an sql statment which will be used to obtain information from the data base
  //  rc=sqlite3_prepare_v2(db, query, strlen(query)+1, &stmt, NULL);
  rc=sqlite3_prepare(db, query, strlen(query)+1, &stmt, NULL);

  if(rc!=SQLITE_OK){
    printf("statement not prepared OK\n");
    //should close the data base and exit the function
    sqlite3_close(db);
    return;
  }
  int row=0;
  while(1){
    //printf("row number %i\n", row);
    rc=sqlite3_step(stmt);
    if(rc==SQLITE_DONE) break;
    int nColumns=sqlite3_column_count(stmt);


    for(int column=0;column<nColumns;column++){

      const char* temp;    

      switch(column){  
      case 0: //primary key - stationId+labChip+channel

	break;
      case 1: //antDir

	temp = (const char*)sqlite3_column_text(stmt, column);

        if(strcmp (temp,"kReceiver")==0){
	  fStationInfo[stationId].fAntInfo[row].antDir=AraAntDir::kReceiver; 
	  //printf("fStationInfo[%i].fAntInfo[%i].antDir %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].antDir);
	}

	break;
      case 2: //chanNum

	fStationInfo[stationId].fAntInfo[row].chanNum=sqlite3_column_int(stmt, column);


	//printf("fStationInfo[%i].fAntInfo[%i].chanNum %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].chanNum);
	
	break;
      case 3: //daqChanType

	temp = (const char*)sqlite3_column_text(stmt, column);
        if(strcmp (temp,"kDisconeChan")==0) fStationInfo[stationId].fAntInfo[row].daqChanType=AraDaqChanType::kDisconeChan;
        if(strcmp (temp,"kBatwingChan")==0) fStationInfo[stationId].fAntInfo[row].daqChanType=AraDaqChanType::kBatwingChan;

	//printf("fStationInfo[%i].fAntInfo[%i].daqChanType %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].daqChanType);

	break;
      case 4: //daqChanNum

	fStationInfo[stationId].fAntInfo[row].daqChanNum=sqlite3_column_int(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].daqChanNum %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].daqChanNum);
	
	break;
      case 5: //highPassFilterMhz

	fStationInfo[stationId].fAntInfo[row].highPassFilterMhz=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].highPassFilterMhz %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].highPassFilterMhz);

	break;
      case 6: //lowPassFilterMhz
	fStationInfo[stationId].fAntInfo[row].lowPassFilterMhz=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].lowPassFilterMhz %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].lowPassFilterMhz);

	break;
      case 7: //daqTrigChan
	fStationInfo[stationId].fAntInfo[row].daqTrigChan=sqlite3_column_int(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].daqTrigChan %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].daqTrigChan);

	break;
      case 8: //numLabChans

	fStationInfo[stationId].fAntInfo[row].numLabChans=sqlite3_column_int(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].numLabChans %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].numLabChans);

	break;
      case 9: //labChip

	temp = (const char*)sqlite3_column_text(stmt, column);
        if(strcmp (temp,"kA")==0) fStationInfo[stationId].fAntInfo[row].labChip=AraLabChip::kA;
        if(strcmp (temp,"kB")==0) fStationInfo[stationId].fAntInfo[row].labChip=AraLabChip::kB;
        if(strcmp (temp,"kC")==0) fStationInfo[stationId].fAntInfo[row].labChip=AraLabChip::kC;

	//printf("fStationInfo[%i].fAntInfo[%i].labChip %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].labChip);

	break;
      case 10: //labChans[0]

	fStationInfo[stationId].fAntInfo[row].labChans[0]=sqlite3_column_int(stmt, column)-1;
	//printf("fStationInfo[%i].fAntInfo[%i].labChans[0] %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].labChans[0]);

	break;
      case 11: //labChans[1]

	fStationInfo[stationId].fAntInfo[row].labChans[1]=sqlite3_column_int(stmt, column)-1;
	//printf("fStationInfo[%i].fAntInfo[%i].labChans[1] %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].labChans[1]);

	break;
      case 12: //isDiplexed

	fStationInfo[stationId].fAntInfo[row].isDiplexed=sqlite3_column_int(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].isDiplexed %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].isDiplexed);

	break;
      case 13: //diplexedChans[0]

	fStationInfo[stationId].fAntInfo[row].diplexedChans[0]=sqlite3_column_int(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].diplexedChans[0] %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].diplexedChans[0]);

	break;
      case 14: //diplexedChans[1]

	fStationInfo[stationId].fAntInfo[row].diplexedChans[1]=sqlite3_column_int(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].diplexedChans[1] %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].diplexedChans[1]);

	break;
      case 15: //preAmpNum

	fStationInfo[stationId].fAntInfo[row].preAmpNum=sqlite3_column_int(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].preAmpNum %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].preAmpNum);

	break;
      case 16: //avgNoiseFigure

	fStationInfo[stationId].fAntInfo[row].avgNoiseFigure=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].avgNoiseFigure %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].avgNoiseFigure);

	break;
      case 17: //rcvrNum

	fStationInfo[stationId].fAntInfo[row].rcvrNum=sqlite3_column_int(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].rcvrNum %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].rcvrNum);

	break;
      case 18: //designator

	temp = (const char*)sqlite3_column_text(stmt, column);
	strncpy(fStationInfo[stationId].fAntInfo[row].designator, temp, 3);
	//printf("fStationInfo[%i].fAntInfo[%i].designator %s\n", stationId, row, fStationInfo[stationId].fAntInfo[row].designator);

	break;
      case 19: //antPolNum

	fStationInfo[stationId].fAntInfo[row].antPolNum=sqlite3_column_int(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].antPolNum %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].antPolNum);

	break;
      case 20: //antType

	temp = (const char*)sqlite3_column_text(stmt, column);
        if(strcmp (temp,"kBicone")==0) fStationInfo[stationId].fAntInfo[row].antType=AraAntType::kBicone;
        if(strcmp (temp,"kBowtieSlot")==0) fStationInfo[stationId].fAntInfo[row].antType=AraAntType::kBowtieSlot;
        if(strcmp (temp,"kDiscone")==0) fStationInfo[stationId].fAntInfo[row].antType=AraAntType::kDiscone;
        if(strcmp (temp,"kBatwing")==0) fStationInfo[stationId].fAntInfo[row].antType=AraAntType::kBatwing;
        if(strcmp (temp,"kFatDipole")==0) fStationInfo[stationId].fAntInfo[row].antType=AraAntType::kFatDipole;
        if(strcmp (temp,"kQuadSlot")==0) fStationInfo[stationId].fAntInfo[row].antType=AraAntType::kQuadSlot;

	//printf("fStationInfo[%i].fAntInfo[%i].antType %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].antType);

	break;
      case 21: //polType

	temp = (const char*)sqlite3_column_text(stmt, column);
        if(strcmp (temp,"kVertical")==0) fStationInfo[stationId].fAntInfo[row].polType=AraAntPol::kVertical;
        if(strcmp (temp,"kHorizontal")==0) fStationInfo[stationId].fAntInfo[row].polType=AraAntPol::kHorizontal;
        if(strcmp (temp,"kSurface")==0) fStationInfo[stationId].fAntInfo[row].polType=AraAntPol::kSurface;

	//printf("fStationInfo[%i].fAntInfo[%i].AraAntPol %i\n", stationId, row, fStationInfo[stationId].fAntInfo[row].polType);

	break;
      case 22: //locationName

	temp = (const char*)sqlite3_column_text(stmt, column);
	strncpy(fStationInfo[stationId].fAntInfo[row].locationName, temp, 4);
	//printf("fStationInfo[%i].fAntInfo[%i].locationName %s\n", stationId, row, fStationInfo[stationId].fAntInfo[row].locationName);


	break;
      case 23: //antLocation[0]

	fStationInfo[stationId].fAntInfo[row].antLocation[0]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].antLocation[0] %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].antLocation[0]);

	break;
      case 24: //antLocation[1]

	fStationInfo[stationId].fAntInfo[row].antLocation[1]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].antLocation[1] %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].antLocation[1]);

	break;
      case 25: //antLocation[2]

	fStationInfo[stationId].fAntInfo[row].antLocation[2]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].antLocation[2] %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].antLocation[2]);

	break;
      case 26: //cableDelay

	fStationInfo[stationId].fAntInfo[row].cableDelay=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].cableDelay %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].cableDelay);

	break;
      case 27: //debugHolePosition[0]

	fStationInfo[stationId].fAntInfo[row].debugHolePosition[0]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugHolePosition[0] %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugHolePosition[0]);

	break;
      case 28: //debugHolePosition[1]

	fStationInfo[stationId].fAntInfo[row].debugHolePosition[1]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugHolePosition[1] %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugHolePosition[1]);

	break;
      case 29: //debugHolePosition[2]

	fStationInfo[stationId].fAntInfo[row].debugHolePosition[2]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugHolePosition[2] %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugHolePosition[2]);

	break;
      case 30: //debugPreAmpDz

	fStationInfo[stationId].fAntInfo[row].debugPreAmpDz=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugPreAmpDz %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugPreAmpDz);

	break;
      case 31: //debugHolePositionZft

	fStationInfo[stationId].fAntInfo[row].debugHolePositionZft=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugHolePositionZft %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugHolePositionZft);

	break;
      case 32: //debugHolePositionZm

	fStationInfo[stationId].fAntInfo[row].debugHolePositionZm=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugHolePositionZm %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugHolePositionZm);

	break;
      case 33: //debugTrueAsBuiltPosition[0]

	fStationInfo[stationId].fAntInfo[row].debugTrueAsBuiltPositon[0]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugTrueAsBuiltPositon[0] %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugTrueAsBuiltPositon[0]);

	break;
      case 34: //debugTrueAsBuiltPosition[1]

	fStationInfo[stationId].fAntInfo[row].debugTrueAsBuiltPositon[1]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugTrueAsBuiltPositon[1] %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugTrueAsBuiltPositon[1]);

	break;
      case 35: //debugTrueAsBuiltPosition[2]

	fStationInfo[stationId].fAntInfo[row].debugTrueAsBuiltPositon[2]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugTrueAsBuiltPositon[2] %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugTrueAsBuiltPositon[2]);

	break;
      case 36: //debugCableDelay2

	fStationInfo[stationId].fAntInfo[row].debugCableDelay2=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugCableDelay2 %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugCableDelay2);

	break;
      case 37: //debugFeedPointDelay

	fStationInfo[stationId].fAntInfo[row].debugFeedPointDelay=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugFeedPointDelay %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugFeedPointDelay);

	break;
      case 38: //debugTotalCableDelay

	fStationInfo[stationId].fAntInfo[row].debugTotalCableDelay=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].debugTotalCableDelay %f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].debugTotalCableDelay);

	break;
      case 40: //antOrient[0]

	fStationInfo[stationId].fAntInfo[row].antOrient[0]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].antOrient[0] %1.10f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].antOrient[0]);

	break;

      case 41: //antOrient[1]

	fStationInfo[stationId].fAntInfo[row].antOrient[1]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].antOrient[1] %1.10f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].antOrient[1]);

	break;

      case 42: //antOrient[2]

	fStationInfo[stationId].fAntInfo[row].antOrient[2]=sqlite3_column_double(stmt, column);
	//printf("fStationInfo[%i].fAntInfo[%i].antOrient[2] %1.10f\n", stationId, row, fStationInfo[stationId].fAntInfo[row].antOrient[2]);

	break;


      default:

	break;

      }//switch(column)

    }//column

    row++;    


  }//while(1)
  //now insert the no of rfchannels

  if(stationId==ARA_TESTBED)  fStationInfo[0].numberRFChans=RFCHANS_TESTBED;
  if(stationId==ARA_STATION1)  fStationInfo[1].numberRFChans=RFCHANS_STATION1;

  //now need to destroy the sqls statement prepared earlier
  rc = sqlite3_finalize(stmt);
  if(rc!=SQLITE_OK) printf("error finlizing sql statement\n");
  //  printf("sqlite3_finalize(stmt) = %i\n", rc);

  //now close the connection to the database
  rc = sqlite3_close(db);
  if(rc!=SQLITE_OK) printf("error closing db\n");


  //Now check that we read it in OK

  for(int ant=0;ant<fStationInfo[stationId].numberRFChans;++ant){
    //    fStationInfo[stationId].fAntInfo[ant].printAntennaInfo();
  }

  //Now let's populate the antenna lookups
  //fAntLookUpTable[stationId][AraAntPol][antPolNum]=chanNum-1

  for(int ant=0;ant<fStationInfo[stationId].numberRFChans;++ant){
    //printf("ant %i\t", ant);//FIXME//DEBUG
    switch(fStationInfo[stationId].fAntInfo[ant].polType){
    case AraAntPol::kVertical:
      fAntLookupTable[stationId][0][fStationInfo[stationId].fAntInfo[ant].antPolNum]=fStationInfo[stationId].fAntInfo[ant].chanNum-1;
      //printf("antPolNum %i\t chanNum %i\t kVertical\n", fStationInfo[stationId].fAntInfo[ant].antPolNum,  fStationInfo[stationId].fAntInfo[ant].chanNum-1);//FIXME//DEBUG
      break;
    case AraAntPol::kHorizontal:
      fAntLookupTable[stationId][1][fStationInfo[stationId].fAntInfo[ant].antPolNum]=fStationInfo[stationId].fAntInfo[ant].chanNum-1;
      //printf("antPolNum %i\t chanNum %i\t kHorizontal\n", fStationInfo[stationId].fAntInfo[ant].antPolNum,  fStationInfo[stationId].fAntInfo[ant].chanNum-1);//FIXME//DEBUG
      break;
    case AraAntPol::kSurface:
      fAntLookupTable[stationId][2][fStationInfo[stationId].fAntInfo[ant].antPolNum]=fStationInfo[stationId].fAntInfo[ant].chanNum-1;
      //printf("antPolNum %i\t chanNum %i\t kSurface\n", fStationInfo[stationId].fAntInfo[ant].antPolNum,  fStationInfo[stationId].fAntInfo[ant].chanNum-1);//FIXME//DEBUG
      break;
    default:
      std::cerr << "Unknown AraPolType\n";

    }//switch polType
  }//ant



}

bool AraGeomTool::isIcrrStation(AraStationId_t stationId){ 
  
  if(stationId==ARA_TESTBED||stationId==ARA_STATION1) return true; 
  else return false;
  
}

bool AraGeomTool::isAtriStation(AraStationId_t stationId){

  if(isIcrrStation(stationId)) return false;
  else return true;

}

Int_t AraGeomTool::getStationCalibIndex(AraStationId_t stationId){

  switch(stationId){

  case ARA_TESTBED:
    return 0;
    break;
  case ARA_STATION1:
    return 1;
    break;
  case ARA_STATION1B:
    return 0;
    break;
  case ARA_STATION2:
    return 1;
    break;
  case ARA_STATION3:
    return 2;
    break;
  case ARA_STATION4:
    return 3;
    break;
  case ARA_STATION5:
    return 4;
    break;
  case ARA_STATION6:
    return 5;
    break;
  case ARA_STATION7:
    return 6;
    break;
  case ARA_STATION8:
    return 7;
    break;
  case ARA_STATION9:
    return 8;
    break;
  case ARA_STATION10:
    return 9;
    break;
  case ARA_STATION11:
    return 10;
    break;
  case ARA_STATION12:
    return 11;
    break;
  case ARA_STATION13:
    return 12;
    break;
  case ARA_STATION14:
    return 13;
    break;
  case ARA_STATION15:
    return 14;
    break;
  case ARA_STATION16:
    return 15;
    break;
  case ARA_STATION17:
    return 16;
    break;
  case ARA_STATION18:
    return 17;
    break;
  case ARA_STATION19:
    return 18;
    break;
  case ARA_STATION20:
    return 19;
    break;
  case ARA_STATION21:
    return 20;
    break;
  case ARA_STATION22:
    return 21;
    break;
  case ARA_STATION23:
    return 22;
    break;
  case ARA_STATION24:
    return 23;
    break;
  case ARA_STATION25:
    return 24;
    break;
  case ARA_STATION26:
    return 25;
    break;
  case ARA_STATION27:
    return 26;
    break;
  case ARA_STATION28:
    return 27;
    break;
  case ARA_STATION29:
    return 28;
    break;
  case ARA_STATION30:
    return 29;
    break;
  case ARA_STATION31:
    return 30;
    break;
  case ARA_STATION32:
    return 31;
    break;
  case ARA_STATION33:
    return 32;
    break;
  case ARA_STATION34:
    return 33;
    break;
  case ARA_STATION35:
    return 34;
    break;
  case ARA_STATION36:
    return 35;
    break;
  case ARA_STATION37:
    return 36;
    break;
  default:
    fprintf(stderr, "AraGeomTool::getStationCalibIndex -- Error - Unknown stationId %i\n", stationId);
    return -1;
  }
}

void AraGeomTool::printStationName(AraStationId_t stationId){

  switch(stationId){

  case ARA_TESTBED:
    std::cout << "TESTBED\n";
    break;
  case ARA_STATION1:
    std::cout << "STATION1\n";
    break;
  case ARA_STATION1B:
    std::cout << "STATION1B\n";
    break;
  case ARA_STATION2:
    std::cout << "STATION2\n";
    break;
  case ARA_STATION3:
    std::cout << "STATION3\n";
    break;
  case ARA_STATION4:
    std::cout << "STATION4\n";
    break;
  case ARA_STATION5:
    std::cout << "STATION5\n";
    break;
  case ARA_STATION6:
    std::cout << "STATION6\n";
    break;
  case ARA_STATION7:
    std::cout << "STATION7\n";
    break;
  case ARA_STATION8:
    std::cout << "STATION8\n";
    break;
  case ARA_STATION9:
    std::cout << "STATION9\n";
    break;
  case ARA_STATION10:
    std::cout << "STATION10\n";
    break;
  case ARA_STATION11:
    std::cout << "STATION11\n";
    break;
  case ARA_STATION12:
    std::cout << "STATION12\n";
    break;
  case ARA_STATION13:
    std::cout << "STATION13\n";
    break;
  case ARA_STATION14:
    std::cout << "STATION14\n";
    break;
  case ARA_STATION15:
    std::cout << "STATION15\n";
    break;
  case ARA_STATION16:
    std::cout << "STATION16\n";
    break;
  case ARA_STATION17:
    std::cout << "STATION17\n";
    break;
  case ARA_STATION18:
    std::cout << "STATION18\n";
    break;
  case ARA_STATION19:
    std::cout << "STATION19\n";
    break;
  case ARA_STATION20:
    std::cout << "STATION20\n";
    break;
  case ARA_STATION21:
    std::cout << "STATION21\n";
    break;
  case ARA_STATION22:
    std::cout << "STATION22\n";
    break;
  case ARA_STATION23:
    std::cout << "STATION23\n";
    break;
  case ARA_STATION24:
    std::cout << "STATION24\n";
    break;
  case ARA_STATION25:
    std::cout << "STATION25\n";
    break;
  case ARA_STATION26:
    std::cout << "STATION26\n";
    break;
  case ARA_STATION27:
    std::cout << "STATION27\n";
    break;
  case ARA_STATION28:
    std::cout << "STATION28\n";
    break;
  case ARA_STATION29:
    std::cout << "STATION29\n";
    break;
  case ARA_STATION30:
    std::cout << "STATION30\n";
    break;
  case ARA_STATION31:
    std::cout << "STATION31\n";
    break;
  case ARA_STATION32:
    std::cout << "STATION32\n";
    break;
  case ARA_STATION33:
    std::cout << "STATION33\n";
    break;
  case ARA_STATION34:
    std::cout << "STATION34\n";
    break;
  case ARA_STATION35:
    std::cout << "STATION35\n";
    break;
  case ARA_STATION36:
    std::cout << "STATION36\n";
    break;
  case ARA_STATION37:
    std::cout << "STATION37\n";
    break;
  default:
    std::cout <<"Unkown StationId\n";
    break;
  }

}
char* AraGeomTool::getStationName(AraStationId_t stationId){

  switch(stationId){

  case ARA_TESTBED:
    return "TESTBED";
    break;
  case ARA_STATION1:
    return "STATION1";
    break;
  case ARA_STATION1B:
    return "STATION1B";
    break;
  case ARA_STATION2:
    return "STATION2";
    break;
  case ARA_STATION3:
    return "STATION3";
    break;
  case ARA_STATION4:
    return "STATION4";
    break;
  case ARA_STATION5:
    return "STATION5";
    break;
  case ARA_STATION6:
    return "STATION6";
    break;
  case ARA_STATION7:
    return "STATION7";
    break;
  case ARA_STATION8:
    return "STATION8";
    break;
  case ARA_STATION9:
    return "STATION9";
    break;
  case ARA_STATION10:
    return "STATION10";
    break;
  case ARA_STATION11:
    return "STATION11";
    break;
  case ARA_STATION12:
    return "STATION12";
    break;
  case ARA_STATION13:
    return "STATION13";
    break;
  case ARA_STATION14:
    return "STATION14";
    break;
  case ARA_STATION15:
    return "STATION15";
    break;
  case ARA_STATION16:
    return "STATION16";
    break;
  case ARA_STATION17:
    return "STATION17";
    break;
  case ARA_STATION18:
    return "STATION18";
    break;
  case ARA_STATION19:
    return "STATION19";
    break;
  case ARA_STATION20:
    return "STATION20";
    break;
  case ARA_STATION21:
    return "STATION21";
    break;
  case ARA_STATION22:
    return "STATION22";
    break;
  case ARA_STATION23:
    return "STATION23";
    break;
  case ARA_STATION24:
    return "STATION24";
    break;
  case ARA_STATION25:
    return "STATION25";
    break;
  case ARA_STATION26:
    return "STATION26";
    break;
  case ARA_STATION27:
    return "STATION27";
    break;
  case ARA_STATION28:
    return "STATION28";
    break;
  case ARA_STATION29:
    return "STATION29";
    break;
  case ARA_STATION30:
    return "STATION30";
    break;
  case ARA_STATION31:
    return "STATION31";
    break;
  case ARA_STATION32:
    return "STATION32";
    break;
  case ARA_STATION33:
    return "STATION33";
    break;
  case ARA_STATION34:
    return "STATION34";
    break;
  case ARA_STATION35:
    return "STATION35";
    break;
  case ARA_STATION36:
    return "STATION36";
    break;
  case ARA_STATION37:
    return "STATION37";
    break;
  default:
    return "Unkown StationId";
    break;
  }

}
