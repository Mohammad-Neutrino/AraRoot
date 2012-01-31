//////////////////////////////////////////////////////////////////////////////
/////  RawAraGenericHeader.cxx        ARA Generic Header Class        /////
/////                                                                    /////
/////  Description:                                                      /////
/////     A simple class that stores AraGenericHeader_t stuff            ///// 
/////   calibrated time and voltage stuff                                /////
/////  Author: Ryan Nichol (rjn@hep.ucl.ac.uk)                           /////
//////////////////////////////////////////////////////////////////////////////

#include "RawAraGenericHeader.h"
#include "AraRootVersion.h"
#include <iostream>
#include <fstream>
#include <cstring>
ClassImp(RawAraGenericHeader);

RawAraGenericHeader::RawAraGenericHeader()   
  :softVerMajor(ARA_ROOT_MAJOR),softVerMinor(ARA_ROOT_MINOR)
{  
  //Default Constructor
}

RawAraGenericHeader::~RawAraGenericHeader() {
   //Default Destructor
}


RawAraGenericHeader::RawAraGenericHeader(AraGenericHeader_t *gHdr) ///< Assignment constructor
  :softVerMajor(ARA_ROOT_MAJOR),softVerMinor(ARA_ROOT_MINOR)
{
  typeId=gHdr->typeId;
  verId=gHdr->verId;
  subVerId=gHdr->subVerId;
  stationId=gHdr->stationId;
  reserved=gHdr->reserved;
  numBytes=gHdr->numBytes;
  checksum=gHdr->checksum;
}
RawAraGenericHeader::RawAraGenericHeader(UInt_t stationId) ///< Assignment constructor for ICRR type
  :softVerMajor(ARA_ROOT_MAJOR),softVerMinor(ARA_ROOT_MINOR)
{
  if(stationId==0||stationId==1){ //ICRR stationId==0 TestBed stationId==1 for Ara1
    typeId=ARA_ICRR_EVENT_TYPE;
    verId=0;
    subVerId=0;
    reserved=0;
    numBytes=0;
    checksum=0;
  }
}