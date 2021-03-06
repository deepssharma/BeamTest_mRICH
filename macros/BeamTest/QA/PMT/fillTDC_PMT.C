#include <string>
#include <TH1D.h>
#include <TH2D.h>
#include <TH3D.h>
#include <TFile.h>
#include <TTree.h>
#include <TCanvas.h>
#include <TMath.h>
#include <iostream>

#define MAXEDGE 100000

// #define H13700MAP "h13700.txt"
#define H13700MAP "../H13700_180degree_v2.txt"

//MAROC configuration polarity (from ssptest_TDCAll.c)
#define MAROCPOLARITY 1

// global variables for display
int x_mRICH[256];
int y_mRICH[256];
int xp_mRICH[4]; //  PMT coordinates X coordinate of pixel 1 of each mapmt
int yp_mRICH[4]; // Y coordinate of pixel 1 of each mapmt

// translation map MAROC/hamamatsu
int maroc2anode[] = {60,58,59,57,52,50,51,49,44,42,43,41,36,34,35,33,28,26,27,25,20,18,19,17,12,10,11,9,4,2,3,1,5,7,6,8,13,15,14,16,21,23,22,24,29,31,30,32,37,39,38,40,45,47,46,48,53,55,54,56,61,63,62,64};
int maroc2h13700[384];

unsigned int tTrigNum;
double tTrigTime;//long unsigned int tTimestamp;
unsigned int tNedge;

unsigned int tPolarity[MAXEDGE];
unsigned int tChannel[MAXEDGE];
unsigned int tTime[MAXEDGE];
unsigned int tSlot[MAXEDGE];
unsigned int tFiber[MAXEDGE];

void InitDisplay_mRICH();
void ResetEventData();
int GetPMT_mRICH(int slot,int fiber,int asic);
void GenCoord_mRICH(int ipmt, int x1, int y1);
int GetPixel_mRICH(int fiber, int asic, int maroc_channel); // return pixel number
float findPixelCoord(int pixel); // return pixel position

void fillTDC_PMT(const int runID = 182)
{
  int debug = 1;
  string mode = "Calibration";
  if(runID < 172) mode = "PositionScan";
  if(runID >= 196 && runID <= 215) mode = "AngleRun";
  if(runID >= 266 && runID <= 316) mode = "ThresholdScan";
  if(runID >= 344 && runID <= 380) mode = "MesonRun";

  float tdc_Start = 1949.5;
  float tdc_Stop  = 2149.5;
  if(runID > 343 && runID < 381) // meson run 344-380
  {
    tdc_Start = 490.0;
    tdc_Stop  = 590.0;
  }

  InitDisplay_mRICH();

  int const NumOfPixel = 33;
  unsigned int pol = MAROCPOLARITY; // 1 falling, 0 rising

  TH2D *h_mRingImage = new TH2D("h_mRingImage","h_mRingImage",NumOfPixel,-0.5,32.5,NumOfPixel,-0.5,32.5);
  float tdc[NumOfPixel][NumOfPixel][2]; // 0 for raising edge | 1 for falling edge
  TH2D *h_mTDC[NumOfPixel][NumOfPixel];
  for(int i_pixel_x = 0; i_pixel_x < NumOfPixel; ++i_pixel_x)
  {
    for(int i_pixel_y = 0; i_pixel_y < NumOfPixel; ++i_pixel_y)
    {
      tdc[i_pixel_x][i_pixel_y][0] = -999.9;
      tdc[i_pixel_x][i_pixel_y][1] = -999.9;
      string HistName = Form("h_mTDC_pixelX_%d_pixelY_%d",i_pixel_x,i_pixel_y);
      h_mTDC[i_pixel_x][i_pixel_y] = new TH2D(HistName.c_str(),HistName.c_str(),200,1949.5,2149.5,150,-0.5,149.5);
    }
  }

  string inputfile = Form("/Users/xusun/WorkSpace/EICPID/Data/BeamTest_mRICH/tdc/richTDC_run%d/sspRich.root",runID);
  TFile *File_InPut = TFile::Open(inputfile.c_str());

  TTree * tree_mRICH = (TTree*)File_InPut->Get("data");
  tree_mRICH->SetBranchAddress("evt",&tTrigNum);
  tree_mRICH->SetBranchAddress("trigtime",&tTrigTime);
  tree_mRICH->SetBranchAddress("nedge",&tNedge);
  tree_mRICH->SetBranchAddress("slot",tSlot);
  tree_mRICH->SetBranchAddress("fiber",tFiber);
  tree_mRICH->SetBranchAddress("ch",tChannel);
  tree_mRICH->SetBranchAddress("pol",tPolarity);
  tree_mRICH->SetBranchAddress("time",tTime);

  int NumOfEvents = tree_mRICH->GetEntries();
  if(NumOfEvents > 50000) NumOfEvents = 50000; // for test only
  printf("NEntries %d\n",NumOfEvents);

  tree_mRICH->GetEntry(0);
  for(int i_event = 0; i_event < NumOfEvents; ++i_event)
  {
    if(NumOfEvents>20)if(i_event%(NumOfEvents/10)==0)printf("Processing Event %6d\n",i_event);
    ResetEventData();
    tree_mRICH->GetEntry(i_event);

    if(tNedge>MAXEDGE){
      printf("Event to big: %u edges vs %u array size...skip\n",tNedge,MAXEDGE);
      continue;
    }

    int NumOfPhotons = 0;
    for(unsigned int i_photon = 0; i_photon < tNedge; ++i_photon)
    {
      int slot = tSlot[i_photon];
      int fiber = tFiber[i_photon];
      int channel = tChannel[i_photon];
      int asic = channel/64;
      int pin = channel%64;

      if(tSlot[i_photon] < 3 || tSlot[i_photon] > 7){printf("%s EVT %d Data Error: bad slot %d \n",__FUNCTION__,i_event,slot);continue;}
      if(tFiber[i_photon] > 31){printf("%s EVT %d Data Error: bad fiber %d \n",__FUNCTION__,i_event,fiber);continue;}
      if(tChannel[i_photon] > 191){printf("%s EVT %d Data Error: bad channel %d \n",__FUNCTION__,i_photon,channel); continue;}

      int pmt = GetPMT_mRICH(slot,fiber,asic);
      GenCoord_mRICH(pmt, xp_mRICH[pmt-1], yp_mRICH[pmt-1]);
      int pixel = GetPixel_mRICH(fiber, asic, pin);
      int pixel_x = x_mRICH[pixel-1];
      int pixel_y = y_mRICH[pixel-1];

      // fill tdc of falling edge for each pixel
      // if(tPolarity[i_photon] == pol) h_mTDC[pixel_x][pixel_y]->Fill(tTime[i_photon]);

      int polarity = tPolarity[i_photon];
      if(tdc[pixel_x][pixel_y][polarity] < 0) // cut off second late pulse
      {
	if(polarity == 1)
	{
	  tdc[pixel_x][pixel_y][polarity] = tTime[i_photon]; // save tdc of falling edge first
	}
	if(polarity == 0 && tdc[pixel_x][pixel_y][1] > 0)
	{
	  tdc[pixel_x][pixel_y][polarity] = tTime[i_photon]; // save corresponding tdc of raising edge
	}
      }

      if(tPolarity[i_photon] == pol && tTime[i_photon] > tdc_Start && tTime[i_photon] < tdc_Stop)
      {
	h_mRingImage->Fill(pixel_x,pixel_y);
      }
    }

    for(int i_pixel_x = 0; i_pixel_x < NumOfPixel; ++i_pixel_x) // fill time duration
    {
      for(int i_pixel_y = 0; i_pixel_y < NumOfPixel; ++i_pixel_y)
      {
	if(tdc[i_pixel_x][i_pixel_y][1] > 0)
	{
	  float time_duration = tdc[i_pixel_x][i_pixel_y][0] - tdc[i_pixel_x][i_pixel_y][1];
	  h_mTDC[i_pixel_x][i_pixel_y]->Fill(tdc[i_pixel_x][i_pixel_y][1],time_duration); // falling edge vs. time

	  tdc[i_pixel_x][i_pixel_y][0] = -999.9; // reset tdc array
	  tdc[i_pixel_x][i_pixel_y][1] = -999.9;
	}
      }
    }
  }
  printf("Processed events %d\n",NumOfEvents);

  string outputfile = Form("/Users/xusun/WorkSpace/EICPID/OutPut/BeamTest/PMT/TDC/%s/richTDC_run%d.root",mode.c_str(),runID);
  TFile *File_OutPut = new TFile(outputfile.c_str(),"RECREATE");
  File_OutPut->cd();
  h_mRingImage->Write();
  for(int i_pixel_x = 0; i_pixel_x < NumOfPixel; ++i_pixel_x)
  {
    for(int i_pixel_y = 0; i_pixel_y < NumOfPixel; ++i_pixel_y)
    {
      h_mTDC[i_pixel_x][i_pixel_y]->Write();
    }
  }
  File_OutPut->Close();
}

//----------------------------------
void InitDisplay_mRICH()
{
  int debug = 1;
  double var[1];
  const char * hname = H13700MAP;
  int anode, asic, pin, channel;

  //Right PMT side (front view)
  xp_mRICH[0]=32;
  yp_mRICH[0]=0;
  xp_mRICH[1]=32;
  yp_mRICH[1]=17;

  //Left PMT side (front view)
  xp_mRICH[2]=0;
  yp_mRICH[2]=32;
  xp_mRICH[3]=0;
  yp_mRICH[3]=15;

  FILE* fin = fopen(hname,"r");
  if(!fin) return ;
  while(fscanf(fin,"%lf",var)!=EOF){
    anode   = (int)var[0];
    fscanf(fin,"%lf",var);
    asic   = (int)var[0];
    fscanf(fin,"%lf",var);
    pin    = (int)var[0];
    int tmp;
    if(asic==2)tmp=1;
    if(asic==1)tmp=2;

    if(anode<=128){
      channel = asic*64 + pin;
    }else{
      channel = 192+asic*64 + pin;
      // channel = 191+asic*64 + pin;
    }

    if(channel<384){ maroc2h13700[channel]=anode;
      if(debug==1)printf("H13700 anode %4d  asic %2d  pin %4d  -->  ch %4d maroc %4d \n",anode, asic, pin, channel, maroc2h13700[channel]);
      // if(channel == 128) cout << "maroc2h13700 = " << maroc2h13700[channel] << endl;
    }
  }
}

//----------------------------------
void ResetEventData()
{
  tTrigNum=0;
  tTrigTime=0;
  tNedge=0;
  for(unsigned int j=0;j<MAXEDGE;j++)
  {
    tPolarity[j]=-1;
    tChannel[j]=-1;
    tTime[j]=-1;
    tSlot[j]=-1;
    tFiber[j]=-1;
  }
}

//------------------------------
int GetPMT_mRICH(int slot,int fiber,int asic)
{
  if(fiber==0 || fiber==1)return 1;
  if(fiber==2 || fiber==3)return 2;
  if(fiber==4 || fiber==5)return 3;
  if(fiber==6 || fiber==7)return 4;
}

//------------------------------
int GetPixel_mRICH(int fiber, int asic, int maroc_channel)
{
 int k=0;
 if(fiber==1 || fiber==3 || fiber==5 || fiber==7)k=1;
 int i = k*192 + asic*64 + maroc_channel;
//  if(maroc2h13700[i]==0)printf("getpixel fiber %d  asic %d ch %d  -->  ii  %d  %d \n",fiber,asic,maroc_channel,i, maroc2h13700[i]);
 return maroc2h13700[i];
}

//------------------------------
void GenCoord_mRICH(int ipmt, int x1, int y1)
{

  int j;
  int debug=1;
  int rw; // row
  int cm; // column
  for(j=0;j<256;j++)x_mRICH[j]=0;
  for(j=0;j<256;j++)y_mRICH[j]=0;

  for(j=0;j<256;j++){
    rw=(int) j/16.;
    cm=j%16;
    if(ipmt<3){
      x_mRICH[j]=x1-cm; // PMT
      y_mRICH[j]=y1+rw;
    }else{
      x_mRICH[j]=x1+cm; // PMT
      y_mRICH[j]=y1-rw;
    }
    // if(debug)if(j==0||j==255)printf("PMT %2d  Pixel %2d  -->  rw %3d  cm  %3d  X %3d Y %3d\n",ipmt, j+1,rw, cm,x_mRICH[j],y_mRICH[j]);
  }
}

float findPixelCoord(int pixel)
{
  float out_coord = -999.9;

  const int mNumOfPixels = 33; // 16*2 3mm-pixels + 1 2*2mm-glasswindow+1mm-gap
  const double mPixels[mNumOfPixels+1] = {-50.5,-47.5,-44.5,-41.5,-38.5,-35.5,-32.5,-29.5,-26.5,-23.5,-20.5,-17.5,-14.5,-11.5,-8.5,-5.5,-2.5,2.5,5.5,8.5,11.5,14.5,17.5,20.5,23.5,26.5,29.5,32.5,35.5,38.5,41.5,44.5,47.5,50.5};

  if(pixel < 0 || pixel > 32) return -999.9; // out of photon sensor

  float out_low = mPixels[pixel];
  float out_high = mPixels[pixel+1];

  out_coord = 0.5*(out_low+out_high);

  return out_coord;
}
