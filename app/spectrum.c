#include "app/spectrum.h"
#include "scanner.h"
#include "driver/backlight.h"
#include "driver/eeprom.h"   // EEPROM_ReadBuffer()

#include "ui/helper.h"
#include "common.h"
#include "action.h"
#include "bands.h"
#include "ui/main.h"
//#include "debugging.h"


#ifdef ENABLE_SCREENSHOT
  #include "screenshot.h"
#endif

/*	
          /////////////////////////DEBUG//////////////////////////
          char str[64] = "";sprintf(str, "%d\r\n", Spectrum_state );LogUart(str);
*/
#define MAX_VISIBLE_LINES 6
#define HISTORY_SIZE 30
uint32_t HFreqs[HISTORY_SIZE]= {0};
uint8_t HCount[HISTORY_SIZE]= {0};
bool HBlacklisted[HISTORY_SIZE]= {0};

/////////////////////////////Parameters://///////////////////////////
// see parametersSelectedIndex
// see GetParametersText
uint8_t DelayRssi = 4;                          // 1
uint8_t PttEmission = 0;                        // 2
uint16_t SpectrumDelay = 0;                     // 3
uint32_t gScanRangeStart = 1400000;             // 4
uint32_t gScanRangeStop = 13000000;             // 5
//Step                                          // 6
//ListenBW                                      // 7
//Modulation                                    // 8
//ClearSettings                                 // 9
bool gCounthistory = 1;                         // 10
#define PARAMETER_COUNT 10
////////////////////////////////////////////////////////////////////




static uint32_t lastReceivingFreq = 0;
bool gIsPeak = false;
bool historyListActive = false;
bool gForceModulation = 0;
bool classic = 1;
uint8_t SlIndex = 0;
uint8_t SpectrumMonitor = 0;
bool Key_1_pressed = 0;
uint16_t WaitSpectrum = 0; 
uint32_t Last_Tuned_Freq = 44610000;
#define SQUELCH_OFF_DELAY 10;
bool StorePtt_Toggle_Mode = 0;
uint8_t historyListIndex = 0;
uint8_t ArrowLine = 1;
static int historyScrollOffset = 0;
static void LoadValidMemoryChannels(void);
static void ToggleRX(bool on);
static void NextScanStep();
static void BuildValidScanListIndices();
static void RenderHistoryList();
static void RenderScanListSelect();
static void RenderParametersSelect();
static void UpdateScan();
static uint8_t bandListSelectedIndex = 0;
static int bandListScrollOffset = 0;
static void RenderBandSelect();
static void ClearSettings();
void DrawMeter(int);
uint8_t scanListSelectedIndex = 0;
uint8_t scanListScrollOffset = 0;
uint8_t parametersSelectedIndex = 0;
uint8_t parametersScrollOffset = 0;
static uint8_t validScanListCount = 0;
bool inScanListMenu = false;
KeyboardState kbd = {KEY_INVALID, KEY_INVALID, 0,0};
struct FrequencyBandInfo {
    uint32_t lower;
    uint32_t upper;
    uint32_t middle;
};
bool isBlacklistApplied;
bool saved_params= false;
uint32_t cdcssFreq;
uint16_t ctcssFreq;
uint8_t refresh = 0;
#define F_MAX frequencyBandTable[ARRAY_SIZE(frequencyBandTable) - 1].upper
#define Bottom_print 51 //Robby69
Mode appMode;
#define UHF_NOISE_FLOOR 20

uint8_t scanChannel[MR_CHANNEL_LAST + 3];
uint8_t ScanListNumber[MR_CHANNEL_LAST + 3];

//uint8_t scanChannel[MR_CHANNEL_LAST+3];
uint8_t scanChannelsCount;
void ToggleScanList();
static void LoadSettings();
static void SaveSettings();
const uint16_t RSSI_MAX_VALUE = 65535;
static uint16_t R30, R37, R3D, R43, R47, R48, R7E, R02, R3F;
static uint32_t initialFreq;
static char String[100];
static char StringC[10];
bool isKnownChannel = false;
int  channel;
int  latestChannel;
char channelName[12];
char rxChannelName[12];
ModulationMode_t  channelModulation;
BK4819_FilterBandwidth_t channelBandwidth;
void LoadValidMemoryChannels(void);
bool isInitialized = false;
bool isListening = true;
bool newScanStart = true;
bool audioState = true;
uint8_t bl;
uint8_t CurrentScanBand = 1;
State currentState = SPECTRUM, previousState = SPECTRUM;
uint8_t Spectrum_state; 
PeakInfo peak;
ScanInfo scanInfo;
char     latestScanListName[12];
const char *bwOptions[] = {"  25k", "12.5k", "6.25k"};
const char *scanListOptions[] = {"SL1", "SL2", "SL3", "SL4", "SL5", "SL6", 
  "SL7", "SL8", "SL9", "SL10", "SL11", "SL12", "SL13", "SL14", "SL15", "ALL"};
const uint8_t modulationTypeTuneSteps[] = {100, 50, 10};
const uint8_t modTypeReg47Values[] = {1, 7, 5};
uint8_t BPRssiTriggerLevelUp[32]={0};
uint8_t SLRssiTriggerLevelUp[15]={0};
// Deklaracja funkcji pomocniczej
static bool IsBlacklisted(uint32_t f);

SpectrumSettings settings = {stepsCount: STEPS_128,
                             scanStepIndex: S_STEP_500kHz,
                             frequencyChangeStep: 80000,
                             rssiTriggerLevelUp: 20,
							               backlightAlwaysOn: false,
                             bw: BK4819_FILTER_BW_WIDE,
                             listenBw: BK4819_FILTER_BW_NARROWEST,
                             modulationType: false,
                             dbMin: -128,
                             dbMax: 10,
                             scanList: S_SCAN_LIST_ALL,
                             scanListEnabled: {0},
                             bandEnabled: {0}
                            };

uint32_t currentFreq, tempFreq;
uint16_t rssiHistory[128];
uint8_t indexFd = 0;
uint8_t indexFs = 0;
int ShowLines = 3;
uint8_t freqInputIndex = 0;
uint8_t freqInputDotIndex = 0;
KEY_Code_t freqInputArr[10];
char freqInputString[11];
static const bandparameters BParams[32];
static uint8_t nextBandToScanIndex = 0;

uint8_t menuState = 0;

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
  static uint16_t scanListChannels[1000]; // Array to store Channel indices for selected scanlist
  static uint16_t scanListChannelsCount = 0; // Number of Channels in selected scanlist
  static uint16_t scanListChannelsSelectedIndex = 0;
  static uint16_t scanListChannelsScrollOffset = 0;
  static uint16_t selectedScanListIndex = 0; // Which scanlist we're viewing Channels for
  static void BuildScanListChannels(uint8_t scanListIndex);
  static void RenderScanListChannels();
  static void RenderScanListChannelsDoubleLines(const char* title, uint8_t numItems, uint8_t selectedIndex, uint8_t scrollOffset);
  #define MAX_VALID_SCANLISTS 15
#endif
  static uint8_t validScanListIndices[MAX_VALID_SCANLISTS]; // stocke les index valides

const RegisterSpec allRegisterSpecs[] = {
    {"LNAs",  0x13, 8, 0b11,  1},
    {"LNA",   0x13, 5, 0b111, 1},
    {"PGA",   0x13, 0, 0b111, 1},
    {"MIX",   0x13, 3, 0b11,  1},
    {"XTAL F Mode Select", 0x3C, 6, 0b11, 1},
    {"OFF AF Rx de-emp", 0x2B, 8, 1, 1},
    {"Gain after FM Demod", 0x43, 2, 1, 1},
    //Saved values
    {"RF Tx Deviation", 0x40, 0, 0xFFF, 10},
    {"Compress AF Tx Ratio", 0x29, 14, 0b11, 1},
    {"Compress AF Tx 0 dB", 0x29, 7, 0x7F, 1},
    {"Compress AF Tx noise", 0x29, 0, 0x7F, 1},
    {"MIC AGC Disable", 0x19, 15, 1, 1},
    {"AFC Range Select", 0x73, 11, 0b111, 1},
    {"AFC Disable", 0x73, 4, 1, 1},


    /*
    {"IF step100x", 0x3D, 0, 0xFFFF, 100},
    {"IF step1x", 0x3D, 0, 0xFFFF, 1},
    {"RFfiltBW1.7-4.5khz ", 0x43, 12, 0b111, 1},
    {"RFfiltBWweak1.7-4.5khz", 0x43, 9, 0b111, 1},
    {"BW Mode Selection", 0x43, 4, 0b11, 1},
    {"XTAL F Low-16bits", 0x3B, 0, 0xFFFF, 1},
    {"XTAL F Low-16bits 100", 0x3B, 0, 0xFFFF, 100},
    {"XTAL F High-8bits", 0x3C, 8, 0xFF, 1},
    {"XTAL F reserved flt", 0x3C, 0, 0b111111, 1},
    {"XTAL Enable", 0x37, 1, 1, 1},
    {"ANA LDO Selection", 0x37, 11, 1, 1},
    {"VCO LDO Selection", 0x37, 10, 1, 1},
    {"RF LDO Selection", 0x37, 9, 1, 1},
    {"PLL LDO Selection", 0x37, 8, 1, 1},
    {"ANA LDO Bypass", 0x37, 7, 1, 1},
    {"VCO LDO Bypass", 0x37, 6, 1, 1},
    {"RF LDO Bypass", 0x37, 5, 1, 1},
    {"PLL LDO Bypass", 0x37, 4, 1, 1},
    {"Freq Scan Indicator", 0x0D, 15, 1, 1},
    {"F Scan High 16 bits", 0x0D, 0, 0xFFFF, 1},
    {"F Scan Low 16 bits", 0x0E, 0, 0xFFFF, 1},
    {"AGC fix", 0x7E, 15, 0b1, 1},
    {"AGC idx", 0x7E, 12, 0b111, 1},
    {"49", 0x49, 0, 0xFFFF, 100},
    {"7B", 0x7B, 0, 0xFFFF, 100},
    {"rssi_rel", 0x65, 8, 0xFF, 1},
    {"agc_rssi", 0x62, 8, 0xFF, 1},
    {"lna_peak_rssi", 0x62, 0, 0xFF, 1},
    {"rssi_sq", 0x67, 0, 0xFF, 1},
    {"weak_rssi 1", 0x0C, 7, 1, 1},
    {"ext_lna_gain set", 0x2C, 0, 0b11111, 1},
    {"snr_out", 0x61, 8, 0xFF, 1},
    {"noise sq", 0x65, 0, 0xFF, 1},
    {"glitch", 0x63, 0, 0xFF, 1},
    {"soft_mute_en 1", 0x20, 12, 1, 1},
    {"SNR Threshold SoftMut", 0x20, 0, 0b111111, 1},
    {"soft_mute_atten", 0x20, 6, 0b11, 1},
    {"soft_mute_rate", 0x20, 8, 0b11, 1},
    {"Band Selection Thr", 0x3E, 0, 0xFFFF, 100},
    {"chip_id", 0x00, 0, 0xFFFF, 1},
    {"rev_id", 0x01, 0, 0xFFFF, 1},
    {"aerror_en 0am 1fm", 0x30, 9, 1, 1},
    {"bypass 1tx 0rx", 0x47, 0, 1, 1},
    {"bypass tx gain 1", 0x47, 1, 1, 1},
    {"bps afdac 3tx 9rx ", 0x47, 8, 0b1111, 1},
    {"bps tx dcc=0 ", 0x7E, 3, 0b111, 1},
    {"audio_tx_mute1", 0x50, 15, 1, 1},
    {"audio_tx_limit_bypass1", 0x50, 10, 1, 1},
    {"audio_tx_limit320", 0x50, 0, 0x3FF, 1},
    {"audio_tx_limit reserved7", 0x50, 11, 0b1111, 1},
    {"audio_tx_path_sel", 0x2D, 2, 0b11, 1},
    {"AFTx Filt Bypass All", 0x47, 0, 1, 1},
    {"3kHz AF Resp K Tx", 0x74, 0, 0xFFFF, 100},
    {"MIC Sensit Tuning", 0x7D, 0, 0b11111, 1},
    {"DCFiltBWTxMICIn15-480hz", 0x7E, 3, 0b111, 1},
    {"04 768", 0x04, 0, 0x0300, 1},
    {"43 32264", 0x43, 0, 0x7E08, 1},
    {"4b 58434", 0x4b, 0, 0xE442, 1},
    {"73 22170", 0x73, 0, 0x569A, 1},
    {"7E 13342", 0x7E, 0, 0x341E, 1},
    {"47 26432 24896", 0x47, 0, 0x6740, 1},
    {"03 49662 49137", 0x30, 0, 0xC1FE, 1},
    {"Enable Compander", 0x31, 3, 1, 1},
    {"Band-Gap Enable", 0x37, 0, 1, 1},
    {"IF step100x", 0x3D, 0, 0xFFFF, 100},
    {"IF step1x", 0x3D, 0, 0xFFFF, 1},
    {"Band Selection Thr", 0x3E, 0, 0xFFFF, 1},
    {"RF filt BW ", 0x43, 12, 0b111, 1},
    {"RF filt BW weak", 0x43, 9, 0b111, 1},
    {"BW Mode Selection", 0x43, 4, 0b11, 1},
    {"AF Output Inverse", 0x47, 13, 1, 1},
    {"AF ALC Disable", 0x4B, 5, 1, 1},
    {"AGC Fix Mode", 0x7E, 15, 1, 1},
    {"AGC Fix Index", 0x7E, 12, 0b111, 1},
    {"Crystal vReg Bit", 0x1A, 12, 0b1111, 1},
    {"Crystal iBit", 0x1A, 8, 0b1111, 1},
    {"PLL CP bit", 0x1F, 0, 0b1111, 1},
    {"PLL/VCO Enable", 0x30, 4, 0xF, 1},
    {"Exp AF Rx Ratio", 0x28, 14, 0b11, 1},
    {"Exp AF Rx 0 dB", 0x28, 7, 0x7F, 1},
    {"Exp AF Rx noise", 0x28, 0, 0x7F, 1},
    {"OFF AFRxHPF300 flt", 0x2B, 10, 1, 1},
    {"OFF AF RxLPF3K flt", 0x2B, 9, 1, 1},
    
    
    {"AF Rx Gain1", 0x48, 10, 0x11, 1},
    {"AF Rx Gain2", 0x48, 4, 0b111111, 1},
    {"AF DAC G after G1 G2", 0x48, 0, 0b1111, 1},
    {"300Hz AF Resp K Rx", 0x54, 0, 0xFFFF, 100},
    {"300Hz AF Resp K Rx", 0x55, 0, 0xFFFF, 100},
    {"3kHz AF Resp K Rx", 0x75, 0, 0xFFFF, 100},
    {"DC Filt BW Rx IF In", 0x7E, 0, 0b111, 1},
    

    {"OFF AFTxHPF300filter", 0x2B, 2, 1, 1},
    {"OFF AFTxLPF1filter", 0x2B, 1, 1, 1},
    {"OFF AFTxpre-emp flt", 0x2B, 0, 1, 1},
    {"PA Gain Enable", 0x30, 3, 1, 1},
    {"PA Biasoutput 0~3", 0x36, 8, 0xFF, 1},
    {"PA Gain1 Tuning", 0x36, 3, 0b111, 1},
    {"PA Gain2 Tuning", 0x36, 0, 0b111, 1},
    {"RF TxDeviation ON", 0x40, 12, 1, 1},
    
    {"AFTxLPF2fltBW1.7-4.5khz", 0x43, 6, 0b111, 1},
    {"300Hz AF Resp K Tx", 0x44, 0, 0xFFFF, 100},
    {"300Hz AF Resp K Tx", 0x45, 0, 0xFFFF, 100},*/
};

#define STILL_REGS_MAX_LINES 3
static uint8_t stillRegSelected = 0;
static uint8_t stillRegScroll = 0;
static bool stillEditRegs = false; // false = edycja czestotliwosci, true = edycja rejestrow

uint16_t statuslineUpdateTimer = 0;

static void RelaunchScan();
static void ResetInterrupts();
static char StringCode[10] = "";

//
static uint16_t osdPopupTimer = 0;
static char osdPopupText[32] = "";

// 
void ShowOSDPopup(const char *str)
{
    strncpy(osdPopupText, str, sizeof(osdPopupText)-1);
    osdPopupText[sizeof(osdPopupText)-1] = '\0';  // Zabezpieczenie przed przepełnieniem
    osdPopupTimer = 1000;
    UI_DisplayPopup(osdPopupText);
    ST7565_BlitFullScreen();
}


static uint16_t GetRegMenuValue(uint8_t st) {
  RegisterSpec s = allRegisterSpecs[st];
  return (BK4819_ReadRegister(s.num) >> s.offset) & s.mask;
}

static void SetRegMenuValue(uint8_t st, bool add) {
  uint16_t v = GetRegMenuValue(st);
  RegisterSpec s = allRegisterSpecs[st];

  uint16_t reg = BK4819_ReadRegister(s.num);
  if (add && v <= s.mask - s.inc) {
    v += s.inc;
  } else if (!add && v >= 0 + s.inc) {
    v -= s.inc;
  }
  reg &= ~(s.mask << s.offset);
  BK4819_WriteRegister(s.num, reg | (v << s.offset));
  
}

// Utility functions
KEY_Code_t GetKey() {
  KEY_Code_t btn = KEYBOARD_Poll();
  // Gestion PTT existante
  if (btn == KEY_INVALID && !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT)) {
    btn = KEY_PTT;
  }
  return btn;
}

static int clamp(int v, int min, int max) {
  return v <= min ? min : (v >= max ? max : v);
}

void SetState(State state) {
  previousState = currentState;
  currentState = state;
  
  
}

// Radio functions

static void BackupRegisters() {
  R30 = BK4819_ReadRegister(BK4819_REG_30);
  R37 = BK4819_ReadRegister(BK4819_REG_37);
  R3D = BK4819_ReadRegister(BK4819_REG_3D);
  R43 = BK4819_ReadRegister(BK4819_REG_43);
  R47 = BK4819_ReadRegister(BK4819_REG_47);
  R48 = BK4819_ReadRegister(BK4819_REG_48);
  R7E = BK4819_ReadRegister(BK4819_REG_7E);
  R02 = BK4819_ReadRegister(BK4819_REG_02);
  R3F = BK4819_ReadRegister(BK4819_REG_3F);
}

static void RestoreRegisters() {
  BK4819_WriteRegister(BK4819_REG_30, R30);
  BK4819_WriteRegister(BK4819_REG_37, R37);
  BK4819_WriteRegister(BK4819_REG_3D, R3D);
  BK4819_WriteRegister(BK4819_REG_43, R43);
  BK4819_WriteRegister(BK4819_REG_47, R47);
  BK4819_WriteRegister(BK4819_REG_48, R48);
  BK4819_WriteRegister(BK4819_REG_7E, R7E);
  BK4819_WriteRegister(BK4819_REG_02, R02);
  BK4819_WriteRegister(BK4819_REG_3F, R3F);
}

static void ToggleAFBit(bool on) {
  //uint16_t reg = BK4819_ReadRegister(BK4819_REG_47);
  uint32_t reg = regs_cache[BK4819_REG_47]; //KARINA mod
  reg &= ~(1 << 8);
  if (on)
    reg |= on << 8;
  BK4819_WriteRegister(BK4819_REG_47, reg);
}

static void ToggleAFDAC(bool on) {
  //uint32_t Reg = BK4819_ReadRegister(BK4819_REG_30);
  uint32_t Reg = regs_cache[BK4819_REG_30]; //KARINA mod
  Reg &= ~(1 << 9);
  if (on)
    Reg |= (1 << 9);
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}

static void SetF(uint32_t f) {
  if (f < 1400000 || f > 130000000) return;
  BK4819_SetFrequency(f);
  BK4819_PickRXFilterPathBasedOnFrequency(f);
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
  BK4819_WriteRegister(BK4819_REG_30, 0);
  BK4819_WriteRegister(BK4819_REG_30, reg);
}

/* static void SetF(uint32_t f) {
  const uint16_t regrx = 0xBDF1; //afdac bit
  if (f < 1400000 || f > 130000000) return;
  BK4819_PickRXFilterPathBasedOnFrequency(f);
  BK4819_SetFrequency(f);
  uint16_t reg = 0x3FF0; // reset rx-dsp bit <0> and vco bit <15> Precise
  //reg = regrx & ~BK4819_REG_30_ENABLE_VCO_CALIB; //vco calib 
  //uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
  BK4819_WriteRegister(BK4819_REG_30, reg);
  BK4819_WriteRegister(BK4819_REG_30, regrx);
} */

static void ResetInterrupts()
{
  // disable interupts
  BK4819_WriteRegister(BK4819_REG_3F, 0);
  // reset the interrupt
  BK4819_WriteRegister(BK4819_REG_02, 0);
}

// scan step in 0.01khz
uint32_t GetScanStep() { return scanStepValues[settings.scanStepIndex]; }

uint16_t GetStepsCount() 
{ 
  if (appMode==CHANNEL_MODE)
  {
    return scanChannelsCount - 1;
  }
  if(appMode==SCAN_RANGE_MODE) {
    return ((gScanRangeStop - gScanRangeStart) / GetScanStep()); //Robby69
  }
  if (appMode==SCAN_BAND_MODE) {return (gScanRangeStop - gScanRangeStart) / scanInfo.scanStep;}
  
  return 128 >> settings.stepsCount;
}

uint32_t GetBW() { return GetStepsCount() * GetScanStep(); }

uint16_t GetRandomChannelFromRSSI(uint16_t maxChannels) {
  uint32_t rssi = rssiHistory[1]*rssiHistory[maxChannels/2];
  if (maxChannels == 0 || rssi == 0) {
        return 1;  // Fallback to channel 1 if invalid input
    }
    // Scale RSSI to [1, maxChannels]
    return 1 + (rssi % maxChannels);
}

static void DeInitSpectrum(bool ComeBack) {
  
  RestoreRegisters();
  gVfoConfigureMode = VFO_CONFIGURE;
  isInitialized = false;
  SetState(SPECTRUM);
  if(!ComeBack) {
    uint8_t Spectrum_state = 0; //Spectrum Not Active
    EEPROM_WriteBuffer(0x1D00, &Spectrum_state);
    ToggleRX(0);
    SYSTEM_DelayMs(50);
    }
    
  else {
    EEPROM_ReadBuffer(0x1D00, &Spectrum_state, 1);
	  Spectrum_state+=10;
    EEPROM_WriteBuffer(0x1D00, &Spectrum_state);
    StorePtt_Toggle_Mode = Ptt_Toggle_Mode;
    SYSTEM_DelayMs(50);
    Ptt_Toggle_Mode =0;
    }
}

static void ExitAndCopyToVfo() {
  RestoreRegisters();
if (historyListActive == true){
      SETTINGS_SetVfoFrequency(HFreqs[historyListIndex]); 
      gTxVfo->Modulation = MODULATION_FM;
      gRequestSaveChannel = 1;
      DeInitSpectrum(0);
}
  switch (currentState) {
    case SPECTRUM:
      if (PttEmission ==1){
          uint16_t randomChannel = GetRandomChannelFromRSSI(scanChannelsCount);
          static uint32_t rndfreq;
          uint8_t i = 0;
          SpectrumDelay = 0; //not compatible with ninja

          while (rssiHistory[randomChannel]> 120) //check channel availability
            {i++;
            randomChannel++;
            if (randomChannel >scanChannelsCount)randomChannel = 1;
            if (i>200) break;}
          rndfreq = gMR_ChannelFrequencyAttributes[scanChannel[randomChannel]].Frequency;
          SETTINGS_SetVfoFrequency(rndfreq);
          //SETTINGS_UpdateChannel(scanChannel[randomChannel],gTxVfo,1);
          gEeprom.MrChannel[0]     = scanChannel[randomChannel];
			    gEeprom.ScreenChannel[0] = scanChannel[randomChannel];
          //gVfoConfigureMode = VFO_CONFIGURE;
	        //gFlagResetVfos    = true;
          gTxVfo->Modulation = MODULATION_FM;
          gTxVfo->STEP_SETTING = STEP_0_01kHz;
          gRequestSaveChannel = 1;
          }
      else 
          if (PttEmission ==2){
          SpectrumDelay = 0; //not compatible
          SETTINGS_SetVfoFrequency(HFreqs[historyListIndex]);
          COMMON_SwitchToVFOMode();
          //gVfoConfigureMode = VFO_CONFIGURE;
	        //gFlagResetVfos    = true;
          gTxVfo->Modulation = MODULATION_FM;
          gTxVfo->STEP_SETTING = STEP_0_01kHz;
          gTxVfo->OUTPUT_POWER = OUTPUT_POWER_HIGH;
          gRequestSaveChannel = 1;
          }

      DeInitSpectrum(1);
      break;      
    
    default:
      DeInitSpectrum(0);
      break;
  }
    // Additional delay to debounce keys
    SYSTEM_DelayMs(200);
    isInitialized = false;
}

uint8_t GetScanStepFromStepFrequency(uint16_t stepFrequency) 
{
	for(uint8_t i = 0; i < ARRAY_SIZE(scanStepValues); i++)
		if(scanStepValues[i] == stepFrequency)
			return i;
	return S_STEP_25_0kHz;
}

uint8_t GetBWRegValueForScan() {
  return scanStepBWRegValues[settings.scanStepIndex];
}

uint16_t GetRssi(void) {
    uint16_t rssi,test=0;
    
    while (test == 0){
        if (isListening) {
            while ((BK4819_ReadRegister(0x63) & 0xFF) >= 255) {
                SYSTICK_DelayUs(500);
            }
        } else {
            BK4819_ReadRegister(0x63);
            SYSTICK_DelayUs(DelayRssi * 1000);
        }
        test = BK4819_GetRSSI();
        if(test) rssi = test;
  }
    
    if ((appMode == CHANNEL_MODE) && (FREQUENCY_GetBand(scanInfo.f) > BAND4_174MHz)) {
        rssi += UHF_NOISE_FLOOR;
      }
  return rssi;
}

static void ToggleAudio(bool on) {
  if (on == audioState) {
    return;
  }
  audioState = on;
  if (on) {
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  } else {
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  }
}

void FillfreqHistory(void) {
    
    if (peak.f == 0) return;

    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        if (HFreqs[i] == peak.f) {
            if (lastReceivingFreq != peak.f && gCounthistory) {
                lastReceivingFreq = peak.f;
                HCount[i]++;
            }
            else if (!gCounthistory) {
                HCount[i]++;
            }

            historyListIndex = i;
            return;
        }
    }

    HFreqs[indexFs]   = peak.f;
    HCount[indexFs]       = 1;
    HBlacklisted[indexFs] = false;
    historyListIndex = indexFs;
    if (++indexFs >= HISTORY_SIZE) {
      //historyListIndex = 0;
      historyScrollOffset = 0;
      indexFs=0;
    }
    
/*     /////////////////////////DEBUG//////////////////////////
    char str[200] = "";
    for (uint8_t i = 0; i < 3; i++) {
    sprintf(str,"%d iFS %d HLi %d BL %d \r\n",HFreqs[i],indexFs,historyListIndex,HCount[i]);LogUart(str); }
    sprintf(str,"\r\n");LogUart(str); 
    /////////////////////////DEBUG////////////////////////// */
}


static void ToggleRX(bool on) {
    if(!on && SpectrumMonitor == 2) {isListening = 1;return;}
    isListening = on;
    BACKLIGHT_TurnOn();
    // automatically switch modulation & bw if known channel
    if (on && isKnownChannel) {
        settings.modulationType = channelModulation;
        memmove(rxChannelName, channelName, sizeof(rxChannelName));
        RADIO_SetModulation(settings.modulationType);
        BK4819_InitAGC(settings.modulationType);
        
    }
    else if(on && appMode == SCAN_BAND_MODE) {
            if (!gForceModulation) settings.modulationType = BParams[bl].modulationType;
            RADIO_SetModulation(BParams[bl].modulationType);
            BK4819_InitAGC(settings.modulationType);
            
          }
    
    if (on) { 
        //if (StopSpectrum == 0) StopSpectrum = 5000;
        //BK4819_SetFilterBandwidth(settings.listenBw, false);
        BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_02_CxCSS_TAIL);
        gBacklightCountdown = 0;
    } else { 
        if(appMode!=CHANNEL_MODE) BK4819_WriteRegister(0x43, GetBWRegValueForScan());
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, 0);
    }

    ToggleAudio(on);
    ToggleAFDAC(on);
    ToggleAFBit(on);
}


// Scan info
static void ResetScanStats() {
  scanInfo.rssiMax = scanInfo.rssiMin + 20 ; 
}

bool SingleBandCheck(void) {
    int count = 0;
    for (int i = 0; i < 32; i++) {
        if (settings.bandEnabled[i]) {
            if (++count > 1) {
                return false;
            }
        }
    }
    return (count == 1);
}

static bool InitScan() {
    ResetScanStats();
    scanInfo.i = 0;
    
    bool scanInitializedSuccessfully = false;

    if (appMode == SCAN_BAND_MODE) {
        uint8_t checkedBandCount = 0;
        while (checkedBandCount < 32) { 
            if (settings.bandEnabled[nextBandToScanIndex]) {
                bl = nextBandToScanIndex; 
                scanInfo.f = BParams[bl].Startfrequency;
                scanInfo.scanStep = scanStepValues[BParams[bl].scanStep];
                settings.scanStepIndex = BParams[bl].scanStep; 
                if(BParams[bl].Startfrequency>0) gScanRangeStart = BParams[bl].Startfrequency;
                if(BParams[bl].Stopfrequency>0)  gScanRangeStop = BParams[bl].Stopfrequency;
                settings.rssiTriggerLevelUp = BPRssiTriggerLevelUp[bl];
                if (!gForceModulation) settings.modulationType = BParams[bl].modulationType;
                nextBandToScanIndex = (nextBandToScanIndex + 1) % 32;
                scanInitializedSuccessfully = true;
                break;
            }
            nextBandToScanIndex = (nextBandToScanIndex + 1) % 32;
            checkedBandCount++;
                        
        }
    } else {
        scanInfo.f = gScanRangeStart;
        scanInfo.scanStep = GetScanStep();
        scanInitializedSuccessfully = true;
      }
    if (appMode == CHANNEL_MODE) {
      int currentChannel = scanChannel[0];
      scanInfo.f = gMR_ChannelFrequencyAttributes[currentChannel].Frequency; 
    }
    BK4819_SetFilterBandwidth(settings.listenBw, false);
    return scanInitializedSuccessfully;
}

// resets modifiers like blacklist, attenuation, normalization
static void ResetModifiers() {
  memset(StringC, 0, sizeof(StringC)); 
  for (int i = 0; i < 128; ++i) {
    if (rssiHistory[i] == RSSI_MAX_VALUE) rssiHistory[i] = 0;
  }
  if(appMode==CHANNEL_MODE){LoadValidMemoryChannels();}
  RelaunchScan();
}

static void RelaunchScan() {
    InitScan();
    ToggleRX(false);
    scanInfo.rssiMin = RSSI_MAX_VALUE;
    gIsPeak = false;
}

void UpdateNoiseOff(){
	const uint16_t NOISLVL = 69; //65-72
  if( BK4819_GetExNoiseIndicator() > NOISLVL) {gIsPeak = false;}		
}

void UpdateNoiseOn(){
	const uint16_t NOISLVL = 60; //65-72
	if( BK4819_GetExNoiseIndicator() < NOISLVL) {
    gIsPeak = true;
    ToggleRX(true);}

}

static void UpdateScanInfo() {
  if (scanInfo.rssi > scanInfo.rssiMax) {
    scanInfo.rssiMax = scanInfo.rssi;
  }
  if (scanInfo.rssi < scanInfo.rssiMin && scanInfo.rssi > 0) {
    scanInfo.rssiMin = scanInfo.rssi;
  }
}

//static uint8_t my_abs(signed v) { return v > 0 ? v : -v; }

static void Measure() {
    uint16_t j;    
    uint16_t startIndex;
    static int16_t previousRssi = 0;
    static bool isFirst = true;
    uint16_t rssi = scanInfo.rssi = GetRssi();
    UpdateScanInfo();
    if (scanInfo.f % 1300000 == 0 || IsBlacklisted(scanInfo.f)) rssi = scanInfo.rssi = 0;

    if (isFirst) {
        previousRssi = rssi;
        gIsPeak      = false;
        isFirst      = false;
    }

    if (!gIsPeak && rssi > previousRssi + settings.rssiTriggerLevelUp) {
        SYSTEM_DelayMs(5);
        uint16_t rssi2 = scanInfo.rssi = GetRssi();
        if (rssi2 > rssi+10) {
          peak.f = scanInfo.f;
          peak.i = scanInfo.i;
          gIsPeak = true;
        }
    } 
    if (!gIsPeak || !isListening)
        previousRssi = rssi;
    else if (rssi < previousRssi)
        previousRssi = rssi;

    uint16_t count = GetStepsCount()+1;
    uint16_t i = scanInfo.i;
    if (count > 128) {
        uint16_t pixel = (uint32_t) i * 128 / count;
        rssiHistory[pixel] = rssi;
        if(++pixel < 128) rssiHistory[pixel] = 0; //2 blank pixels
        if(++pixel < 128) rssiHistory[pixel] = 0;
        
    } else {
          uint16_t base = 128 / count;
          uint16_t rem  = 128 % count;
          startIndex = i * base + (i < rem ? i : rem);
          uint16_t width      = base + (i < rem ? 1 : 0);
          uint16_t endIndex   = startIndex + width;
          for (j = startIndex; j < endIndex; ++j) {rssiHistory[j] = rssi;}
          for (j = endIndex; j < endIndex + width; ++j) {rssiHistory[j] = 0;}
      }
/////////////////////////DEBUG//////////////////////////
//SYSTEM_DelayMs(200);
/* char str[200] = "";
sprintf(str,"%d %d %d \r\n", startIndex, j-2, rssiHistory[j-2]);
LogUart(str); */
/////////////////////////DEBUG//////////////////////////  
}


static void UpdateDBMaxAuto() {
  static uint8_t z = 3;
    if (scanInfo.rssiMax > 0) {
        int newDbMax = clamp(Rssi2DBm(scanInfo.rssiMax), -60, 60);

        if (newDbMax > settings.dbMax + z) {
            settings.dbMax = settings.dbMax + z;   // montée limitée
        } else if (newDbMax < settings.dbMax - z) {
            settings.dbMax = settings.dbMax - z;   // descente limitée
        } else {
            settings.dbMax = newDbMax;              // suivi normal
        }
    }

    if (scanInfo.rssiMin > 0) {
        settings.dbMin = clamp(Rssi2DBm(scanInfo.rssiMin), -160, 100);
    }
}




static void AutoAdjustFreqChangeStep() {
  settings.frequencyChangeStep = gScanRangeStop - gScanRangeStart;
}

static void UpdateScanStep(bool inc) {
if (inc) {
    settings.scanStepIndex = (settings.scanStepIndex >= S_STEP_500kHz) 
                          ? S_STEP_0_01kHz 
                          : settings.scanStepIndex + 1;
} else {
    settings.scanStepIndex = (settings.scanStepIndex <= S_STEP_0_01kHz) 
                          ? S_STEP_500kHz 
                          : settings.scanStepIndex - 1;
}
  AutoAdjustFreqChangeStep();
  scanInfo.scanStep = settings.scanStepIndex;
}

static void UpdateCurrentFreq(bool inc) {
  if (inc && currentFreq < F_MAX) {
    gScanRangeStart += settings.frequencyChangeStep;
    gScanRangeStop += settings.frequencyChangeStep;
  } else if (!inc && currentFreq > RX_freq_min() && currentFreq > settings.frequencyChangeStep) {
    gScanRangeStart -= settings.frequencyChangeStep;
    gScanRangeStop -= settings.frequencyChangeStep;
  } else {
    return;
  }
  ResetModifiers();
  
}

static void ToggleModulation() {
  if (settings.modulationType < MODULATION_UKNOWN - 1) {
    settings.modulationType++;
  } else {
    settings.modulationType = MODULATION_FM;
  }
  RADIO_SetModulation(settings.modulationType);
  BK4819_InitAGC(settings.modulationType);
  
  if (appMode == SCAN_BAND_MODE)gForceModulation = 1;
}

static void ToggleListeningBW(bool inc) {
  settings.listenBw = ACTION_NextBandwidth(settings.listenBw, false, inc);
  
}

static void ToggleStepsCount() {
  if (settings.stepsCount == STEPS_128) {
    settings.stepsCount = STEPS_16;
  } else {
    settings.stepsCount--;
  }
  AutoAdjustFreqChangeStep();
  ResetModifiers();
  
}

static void ResetFreqInput() {
  tempFreq = 0;
  for (int i = 0; i < 10; ++i) {
    freqInputString[i] = '-';
  }
}

static void FreqInput() {
  freqInputIndex = 0;
  freqInputDotIndex = 0;
  ResetFreqInput();
  SetState(FREQ_INPUT);
}

static void UpdateFreqInput(KEY_Code_t key) {
  if (key != KEY_EXIT && freqInputIndex >= 10) {
    return;
  }
  if (key == KEY_STAR) {
    if (freqInputIndex == 0 || freqInputDotIndex) {
      return;
    }
    freqInputDotIndex = freqInputIndex;
  }
  if (key == KEY_EXIT) {
    freqInputIndex--;
    if(freqInputDotIndex==freqInputIndex)
      freqInputDotIndex = 0;    
  } else {
    freqInputArr[freqInputIndex++] = key;
  }

  ResetFreqInput();

  uint8_t dotIndex =
      freqInputDotIndex == 0 ? freqInputIndex : freqInputDotIndex;

  KEY_Code_t digitKey;
  for (int i = 0; i < 10; ++i) {
    if (i < freqInputIndex) {
      digitKey = freqInputArr[i];
      freqInputString[i] = digitKey <= KEY_9 ? '0' + digitKey-KEY_0 : '.';
    } else {
      freqInputString[i] = '-';
    }
  }

  uint32_t base = 100000; // 1MHz in BK units
  for (int i = dotIndex - 1; i >= 0; --i) {
    tempFreq += (freqInputArr[i]-KEY_0) * base;
    base *= 10;
  }

  base = 10000; // 0.1MHz in BK units
  if (dotIndex < freqInputIndex) {
    for (int i = dotIndex + 1; i < freqInputIndex; ++i) {
      tempFreq += (freqInputArr[i]-KEY_0) * base;
      base /= 10;
    }
  }
  
}

static bool IsBlacklisted(uint32_t f) {
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        if (HFreqs[i] == f && HBlacklisted[i]) {
            return true;
        }
    }
    return false;
}

static void Blacklist() {
    if (peak.f == 0) return;
    gIsPeak = 0;
    ToggleRX(false);
    isBlacklistApplied = true;
    ResetScanStats();
    NextScanStep();
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        if (HFreqs[i] == peak.f) {
            HBlacklisted[i] = true;
            historyListIndex = i;
            gIsPeak = 0;
            return;
        }
    }

    HFreqs[indexFs]   = peak.f;
    HCount[indexFs]       = 1;
    HBlacklisted[indexFs] = true;
    historyListIndex = indexFs;
    if (++indexFs >= HISTORY_SIZE) {
      historyScrollOffset = 0;
      indexFs=0;
    }  
}


// Draw things

// applied x2 to prevent initial rounding
uint16_t Rssi2PX(uint16_t rssi, uint16_t pxMin, uint16_t pxMax) {
  const int16_t DB_MIN = settings.dbMin << 1;
  const int16_t DB_MAX = settings.dbMax << 1;
  const int16_t DB_RANGE = DB_MAX - DB_MIN;
  const int16_t PX_RANGE = pxMax - pxMin;
  int dbm = clamp(rssi - (160 << 1), DB_MIN, DB_MAX);
  return ((dbm - DB_MIN) * PX_RANGE + DB_RANGE / 2) / DB_RANGE + pxMin;
}

int16_t Rssi2Y(uint16_t rssi) {
  int delta = ArrowLine*8;
  return DrawingEndY + delta -Rssi2PX(rssi, delta, DrawingEndY);
}

static void DrawSpectrum()
{
    for (uint8_t i = 0; i < 128; ++i)
    {   
        uint16_t rssi = rssiHistory[i];
        if (rssi != RSSI_MAX_VALUE) DrawVLine(Rssi2Y(rssi), DrawingEndY, i, true);
    }
}

 // Format frequency string (remove trailing zeros)
 void RemoveTrailZeros(char* freqStr){
     
     char *p = freqStr + strlen(freqStr) - 1;
     while (p > freqStr && *p == '0') *p-- = '\0';
    if (*p == '.') *p = '\0';
}

static void DrawStatus() {
  int len=0;
  int pos=0;

   switch(appMode) {
    case FREQUENCY_MODE:
      len = sprintf(&String[pos],"FR ");
      pos += len;
    break;

    case CHANNEL_MODE:
      len = sprintf(&String[pos],"SL ");
      pos += len;
    break;

    case SCAN_RANGE_MODE:
      len = sprintf(&String[pos],"RG ");
      pos += len;
    break;
    
    case SCAN_BAND_MODE:
#ifdef ENABLE_FR_BAND
      len = sprintf(&String[pos],"BD ");
#endif

#ifdef ENABLE_PL_BAND
      len = sprintf(&String[pos],"PL ");
#endif

#ifdef ENABLE_RO_BAND
      len = sprintf(&String[pos],"RO ");
#endif

#ifdef ENABLE_KO_BAND
      len = sprintf(&String[pos],"KO ");
#endif

#ifdef ENABLE_CZ_BAND
      len = sprintf(&String[pos],"CZ ");
#endif

#ifdef ENABLE_TU_BAND
      len = sprintf(&String[pos],"TU ");
#endif

#ifdef ENABLE_RU_BAND
      len = sprintf(&String[pos],"RU ");
#endif
      pos += len;
    break;
  } 
switch(SpectrumMonitor) {
    case 0:
      len = sprintf(&String[pos],"");
      pos += len;
    break;

    case 1:
      len = sprintf(&String[pos],"FL ");
      pos += len;
    break;

    case 2:
      len = sprintf(&String[pos],"M ");
      pos += len;
    break;
  } 


  //if (gAutoTriggerLevel)len = sprintf(&String[pos],"A U%d/D%d %dms %s ", settings.rssiTriggerLevelUp,settings.rssiTriggerLevelDn,DelayRssi, gModulationStr[settings.modulationType]);
  //else 
  len = sprintf(&String[pos],"U%d %dms %s %s ", settings.rssiTriggerLevelUp,DelayRssi, gModulationStr[settings.modulationType],bwNames[settings.listenBw]);
  pos += len;  // Move position forward
  
  if (WaitSpectrum > 0 && WaitSpectrum <61000){len = sprintf(&String[pos],"%d", WaitSpectrum/1000);pos += len;}
  else if(WaitSpectrum > 61000){len = sprintf(&String[pos],"oo");pos += len;} //locked
  
  //if (StopSpectrum>0 && StopSpectrum <61000){len = sprintf(&String[pos],"%d", StopSpectrum/1000);pos += len;}
  //else if(StopSpectrum > 61000){len = sprintf(&String[pos],"oo");pos += len;} //locked
  
  GUI_DisplaySmallest(String, 0, 1, true,true);
  BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[gBatteryCheckCounter++ % 4]);

  uint16_t voltage = (gBatteryVoltages[0] + gBatteryVoltages[1] + gBatteryVoltages[2] +
             gBatteryVoltages[3]) /
            4 * 760 / gBatteryCalibration[3];

  unsigned perc = BATTERY_VoltsToPercent(voltage);
  gStatusLine[116] = 0b00011100;
  gStatusLine[117] = 0b00111110;
  for (int i = 118; i <= 126; i++) {
    gStatusLine[i] = 0b00100010;
  }
  
  for (unsigned i = 127; i >= 118; i--) {
    if (127 - i <= (perc+5)*9/100) {
      gStatusLine[i] = 0b00111110;
    }
  }
}

static void formatHistory(char *buf, uint16_t index, uint16_t channel, uint32_t freq) {
    char freqStr[16];
    snprintf(freqStr, sizeof(freqStr), "%u.%05u", freq/100000, freq%100000);
    RemoveTrailZeros(freqStr);

    if(channel != 0xFFFF) {
        snprintf(buf, 19, "%s(%u)", 
                gMR_ChannelFrequencyAttributes[channel].Name,
                HCount[index]);
                
    } else {
        snprintf(buf, 19, "%s(%u)", 
                freqStr,
                HCount[index]);
        }
}

// ------------------ Popups ------------------
static bool HandlePopup(void) {
    if (saved_params) {
        UI_DisplayPopup("Params Saved");
        saved_params = 0;
        ST7565_BlitFullScreen();
        SYSTEM_DelayMs(200);
        return true;
    }
    return false;
}

// ------------------ Frequency string ------------------
static void FormatFrequency(uint32_t f, char *buf, size_t buflen) {
    snprintf(buf, buflen, "%u.%05u", f / 100000, f % 100000);
    RemoveTrailZeros(buf);
}

// ------------------ CSS detection ------------------
static void UpdateCssDetection(void) {
    if (refresh == 0) {
        BK4819_WriteRegister(BK4819_REG_51,
		        BK4819_REG_51_ENABLE_CxCSS         |
		        BK4819_REG_51_AUTO_CDCSS_BW_ENABLE |
		        BK4819_REG_51_AUTO_CTCSS_BW_ENABLE |
		        (51u << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));
		
        BK4819_CssScanResult_t scanResult =
            BK4819_GetCxCSSScanResult(&cdcssFreq, &ctcssFreq);
        refresh = 30;
        if (scanResult == BK4819_CSS_RESULT_CDCSS) {
            uint8_t code = DCS_GetCdcssCode(cdcssFreq);
            if (code != 0xFF)
                snprintf(StringCode, sizeof(StringCode), "D%03oN", DCS_Options[code]);
        } else if (scanResult == BK4819_CSS_RESULT_CTCSS) {
            uint8_t code = DCS_GetCtcssCode(ctcssFreq);
            snprintf(StringCode, sizeof(StringCode), "%u.%uHz",
                     CTCSS_Options[code] / 10, CTCSS_Options[code] % 10);
        } else {
            StringCode[0] = '\0';
        }
    }
    if (refresh > 0) refresh--; 
    else StringCode[0] = '\0';
}

// ------------------ Enabled scan lists ------------------
static void BuildEnabledScanLists(char *buf, size_t buflen) {
    buf[0] = '\0';
    bool first = true;
    for (int i = 0; i < 15; i++) {
        if (settings.scanListEnabled[i]) {
            if (!first) strncat(buf, ",", buflen - strlen(buf) - 1);
            char listNum[4];
            snprintf(listNum, sizeof(listNum), "%d", i + 1);
            strncat(buf, listNum, buflen - strlen(buf) - 1);
            first = false;
        }
    }
}

static void DrawF(uint32_t f) {
    if (HandlePopup()) return;
    if (f == 0) return;

    // --- Format fréquence ---
    char freqStr[18];
    FormatFrequency(f, freqStr, sizeof(freqStr));

    // --- Mise à jour CTCSS/DCS ---
    UpdateCssDetection();

    // --- Scan lists activées ---
    char enabledLists[64];
    BuildEnabledScanLists(enabledLists, sizeof(enabledLists));
    
    // --- Contexte canal ---
    f = HFreqs[historyListIndex];
    uint16_t channelFd = BOARD_gMR_fetchChannel(f);
    isKnownChannel = (channelFd != 0XFFFF);
    memmove(rxChannelName, channelName, sizeof(rxChannelName));

    // Buffers
    char line1[19] = "";
    char line2[19] = "";
    char line3[19] = "";
    
    if (ShowLines > 0 || !classic)
    strncpy(line1, freqStr, sizeof(line1)-1);
    
    if (ShowLines > 1 || !classic){
      if (StringCode[0]) {
          strncat(line1, " ", sizeof(line1)-strlen(line1)-1);
          strncat(line1, StringCode, sizeof(line1)-strlen(line1)-1);
      }
    }

    char prefix[9] = "";
    if (ShowLines > 2 || !classic){
        if (appMode == SCAN_BAND_MODE) {
            snprintf(prefix, sizeof(prefix), "B%u ", bl + 1);
        } else if (appMode == CHANNEL_MODE) {
            if (enabledLists[0])
                snprintf(prefix, sizeof(prefix), "S%s ", enabledLists);
            else
                snprintf(prefix, sizeof(prefix), "ALL ");
        }

        if (isKnownChannel && channelName[0] && isListening) {
            snprintf(line2, sizeof(line2), "%-3s%s", prefix, channelName);
        } else {
            snprintf(line2, sizeof(line2), "%s", prefix);
        }
        if (appMode == SCAN_BAND_MODE) {
            snprintf(line2, sizeof(line2), "%-3s%s", prefix, BParams[bl].BandName);
        }
    } 

    if (ShowLines > 3 || !classic) {
        if (f > 0 && historyListIndex <HISTORY_SIZE) {
          formatHistory(line3, historyListIndex, channelFd, f);
        }
        else {
            snprintf(line3, sizeof(line3), "0:EMPTY(0)");
          }
    }

    // ------------------------------------------------------------
    // AFFICHAGE
    // ------------------------------------------------------------
    if (classic) {
        if (ShowLines == 0) ArrowLine = 0; //Draw nothing
        if (ShowLines == 1) {UI_DisplayFrequency(line1, 0, 0, 0);ArrowLine = 2;}
        if (ShowLines > 1) {UI_PrintStringSmall(line1, 1, 1, 0,1); ArrowLine = 1;}
        if (ShowLines > 2) { UI_PrintStringSmall(line2, 1, 1, 1,1); ArrowLine = 2; }
        if (ShowLines > 3) { UI_PrintStringSmall(line3, 1, 1, 2,1); ArrowLine = 3; }
    } else {
        DrawMeter(6);
        if (StringCode[0]) {UI_PrintStringSmall(line1, 1, 1, 0,1);}
        else UI_DisplayFrequency(line1, 0, 0, 0);
        UI_PrintString(line2, 1, 1, 2, 8);
        UI_PrintString(line3, 1, 1, 4, 8);
    }
}

void LookupChannelInfo() {
    channel = BOARD_gMR_fetchChannel(peak.f);
    isKnownChannel = channel == -1 ? false : true;
    if (isKnownChannel){
      memmove(channelName, gMR_ChannelFrequencyAttributes[channel].Name, sizeof(channelName));
      LookupChannelModulation();
    }
  }

void LookupChannelModulation() {
	  uint16_t base;
		base = channel * 16;

		uint8_t tmp;
		uint8_t data[8];

		EEPROM_ReadBuffer(base + 8, data, sizeof(data));

		tmp = data[3] >> 4;
		if (tmp >= MODULATION_UKNOWN)
			tmp = MODULATION_FM;
		channelModulation = tmp;

		if (data[4] == 0xFF)
		{
			channelBandwidth = BK4819_FILTER_BW_WIDE;
		}
		else
		{
			const uint8_t d4 = data[4];
			channelBandwidth = !!((d4 >> 1) & 1u);
			if(channelBandwidth != BK4819_FILTER_BW_WIDE)
				channelBandwidth = ((d4 >> 5) & 3u) + 1;
		}	

}


static void DrawNums() {



  if (appMode==CHANNEL_MODE) 
  {
    sprintf(String, "M:%d", scanChannel[0]+1);
    GUI_DisplaySmallest(String, 0, Bottom_print, false, true);

    sprintf(String, "M:%d", scanChannel[GetStepsCount()-1]+1);
    GUI_DisplaySmallest(String, 108, Bottom_print, false, true);
  }
  if(appMode!=CHANNEL_MODE){
    sprintf(String, "%u.%05u", gScanRangeStart / 100000, gScanRangeStart % 100000); //Robby69 was %u.%05u
    GUI_DisplaySmallest(String, 0, Bottom_print, false, true);
 
    sprintf(String, "%u.%05u", gScanRangeStop / 100000, gScanRangeStop % 100000); //Robby69 was %u.%05u
    GUI_DisplaySmallest(String, 90, Bottom_print, false, true);
  }
  
  if(isBlacklistApplied){
    sprintf(String, "BL");
    GUI_DisplaySmallest(String, 60, Bottom_print, false, true);
  }
  
}

void nextFrequency833() {
    if (scanInfo.i % 3 != 1) {
        scanInfo.f += 833;
    } else {
        scanInfo.f += 834;
    }
}

static void NextScanStep() {
    ++scanInfo.i;  
    if (appMode==CHANNEL_MODE)
    { 
      int currentChannel = scanChannel[scanInfo.i];
      settings.rssiTriggerLevelUp = SLRssiTriggerLevelUp[ScanListNumber[scanInfo.i]];
      scanInfo.f =  gMR_ChannelFrequencyAttributes[currentChannel].Frequency;
    } 
    else {
          // frequency mode
          if(scanInfo.scanStep==833) nextFrequency833();
          else scanInfo.f += scanInfo.scanStep;
    }
    

}

static uint8_t CountValidHistoryItems() {
    uint8_t count = 0;
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        if (HFreqs[i] != 0) count++;
    }
    return count;
}

static void Skip() {
    WaitSpectrum = 0;
    gIsPeak = false;
    ToggleRX(false);
    NextScanStep();
}
    
static void OnKeyDown(uint8_t key) {
        BACKLIGHT_TurnOn();
  
  
    // NEW HANDLING: press of '4' key in SCAN_BAND_MODE
    if (appMode == SCAN_BAND_MODE && key == KEY_4 && currentState == SPECTRUM) {
        SetState(BAND_LIST_SELECT);
        bandListSelectedIndex = 0; // Start from the first band
        bandListScrollOffset = 0;  // Reset scrolling
        
        return; // Key handled
    }

    // NEW HANDLING: press of '4' key in CHANNEL_MODE
    if (appMode == CHANNEL_MODE && key == KEY_4 && currentState == SPECTRUM) {
        SetState(SCANLIST_SELECT);
        scanListSelectedIndex = 0;
        scanListScrollOffset = 0;
        
        return; // Key handled
    }
    
	if (key == KEY_5 && currentState == SPECTRUM) {
        SetState(PARAMETERS_SELECT );
        parametersSelectedIndex = 0;
        parametersScrollOffset = 0;
        
        return; // Key handled
    }
    
    // If we're in band selection mode, use dedicated key logic
    if (currentState == BAND_LIST_SELECT) {
        switch (key) {
            case KEY_UP: //Band
                if (bandListSelectedIndex > 0) {
                    bandListSelectedIndex--;
                    if (bandListSelectedIndex < bandListScrollOffset) {
                        bandListScrollOffset = bandListSelectedIndex;
                    }
                }
                else bandListSelectedIndex =  ARRAY_SIZE(BParams) - 1;
                
                break;
            case KEY_DOWN:
                // ARRAY_SIZE(BParams) gives the number of defined bands
                if (bandListSelectedIndex < ARRAY_SIZE(BParams) - 1) {
                    bandListSelectedIndex++;
                    if (bandListSelectedIndex >= bandListScrollOffset + MAX_VISIBLE_LINES) {
                        bandListScrollOffset = bandListSelectedIndex - MAX_VISIBLE_LINES + 1;
                    }
              }
                else bandListSelectedIndex = 0;
                
                break;
            case KEY_4: // Band selection
                if (bandListSelectedIndex < ARRAY_SIZE(BParams)) {
                    // Set the selected band as the only active one for scanning
                    settings.bandEnabled[bandListSelectedIndex] = !settings.bandEnabled[bandListSelectedIndex]; 
                    // Reset nextBandToScanIndex so InitScan starts from the selected one
                    nextBandToScanIndex = bandListSelectedIndex; 
                    bandListSelectedIndex++;
                }
                break;
            case KEY_5: // Band selection
                if (bandListSelectedIndex < ARRAY_SIZE(BParams)) {
                    // Set the selected band as the only active one for scanning
                    memset(settings.bandEnabled, 0, sizeof(settings.bandEnabled)); // Clear all flags
                    settings.bandEnabled[bandListSelectedIndex] = true; // Enable selected band
                    
                    // Reset nextBandToScanIndex so InitScan starts from the selected one
                    nextBandToScanIndex = bandListSelectedIndex; 
                }
                break;
				
				        // NOWA FUNKCJA: Przejście do wybranego zakresu po wciśnięciu MENU
        case KEY_MENU:
            if (bandListSelectedIndex < ARRAY_SIZE(BParams)) {
                memset(settings.bandEnabled, 0, sizeof(settings.bandEnabled));
                settings.bandEnabled[bandListSelectedIndex] = true;
                nextBandToScanIndex = bandListSelectedIndex;
                SetState(SPECTRUM);
                RelaunchScan();
            }
            break;
				
            case KEY_EXIT: // Exit band list
                SetState(SPECTRUM); // Return to band scanning mode
                RelaunchScan(); 
                break;
            default:
                break;
        }
        return; // Finish handling if we were in BAND_LIST_SELECT
    }
// If we're in scanlist selection mode, use dedicated key logic
    if (currentState == SCANLIST_SELECT) {
        switch (key) {

            case KEY_UP://SCANLIST
                if (scanListSelectedIndex > 0) {
                    scanListSelectedIndex--;
                    if (scanListSelectedIndex < scanListScrollOffset) {
                        scanListScrollOffset = scanListSelectedIndex;
                    }
                }
                else scanListSelectedIndex = validScanListCount-1;
                
                break;
            case KEY_DOWN:
                // ARRAY_SIZE(BParams) gives the number of defined bands
                if (scanListSelectedIndex < validScanListCount-1) { 
                    scanListSelectedIndex++;
                    if (scanListSelectedIndex >= scanListScrollOffset + MAX_VISIBLE_LINES) {
                        scanListScrollOffset = scanListSelectedIndex - MAX_VISIBLE_LINES + 1;
                    }    
                }
                else scanListSelectedIndex = 0;
                
                break;
#ifdef ENABLE_SCANLIST_SHOW_DETAIL
            case KEY_STAR: // NOWA OBSŁUGA - Show channels in selected scanlist
                selectedScanListIndex = scanListSelectedIndex;
                BuildScanListChannels(validScanListIndices[selectedScanListIndex]);
                scanListChannelsSelectedIndex = 0;
                scanListChannelsScrollOffset = 0;
                SetState(SCANLIST_CHANNELS);
                
                break;	
#endif // ENABLE_SCANLIST_SHOW_DETAIL
            case KEY_4: // Scan list selection
                //ToggleScanList(scanListSelectedIndex, 0);
				ToggleScanList(validScanListIndices[scanListSelectedIndex], 0);

                if (scanListSelectedIndex < validScanListCount - 1) {
                      scanListSelectedIndex++;
                   }

                
                break;
            case KEY_5: // Scan list selection
                ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
                 
                break;
				
				        
        case KEY_MENU:
            if (scanListSelectedIndex < 15) {
                ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
                SetState(SPECTRUM);
                ResetModifiers();
                RelaunchScan();
            }
            break;
				
        case KEY_EXIT: // Exit scan list selection
                SetState(SPECTRUM); // Return to scanning mode
                ResetModifiers();
                RelaunchScan(); 
                break;
        default:
                break;
        }
        return; // Finish handling if we were in SCAN_LIST_SELECT
      }
      	  
	// If we're in scanlist channels mode, use dedicated key logic
#ifdef ENABLE_SCANLIST_SHOW_DETAIL
  if (currentState == SCANLIST_CHANNELS) {
    switch (key) {
    case KEY_UP:
        if (scanListChannelsSelectedIndex > 0) {
            scanListChannelsSelectedIndex--;
            if (scanListChannelsSelectedIndex < scanListChannelsScrollOffset) {
                scanListChannelsScrollOffset = scanListChannelsSelectedIndex;
            }
            
        }
        break;
    case KEY_DOWN:
        if (scanListChannelsSelectedIndex < scanListChannelsCount - 1) {
            scanListChannelsSelectedIndex++;
            if (scanListChannelsSelectedIndex >= scanListChannelsScrollOffset + 3) { //MAX_VISIBLE_LINES=3
                scanListChannelsScrollOffset = scanListChannelsSelectedIndex - 3 + 1;
            }
            
        }
        break;
    case KEY_EXIT: // Exit scanlist channels back to scanlist selection
        SetState(SCANLIST_SELECT);
        
        break;
    default:
        break;
    }
    return; // Finish handling if we were in SCANLIST_CHANNELS
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
      // If we're in PARAMETERS_SELECT selection mode, use dedicated key logic
    if (currentState == PARAMETERS_SELECT) {
   
      switch (key) {
          case KEY_UP://PARAMETERS
                if (parametersSelectedIndex > 0) {
                    parametersSelectedIndex--;
                    if (parametersSelectedIndex < parametersScrollOffset) {
                        parametersScrollOffset = parametersSelectedIndex;
                    }
                }
                else parametersSelectedIndex = PARAMETER_COUNT-1;
                break;
          case KEY_DOWN:
                if (parametersSelectedIndex < PARAMETER_COUNT-1) { 
                    parametersSelectedIndex++;
                    if (parametersSelectedIndex >= parametersScrollOffset + MAX_VISIBLE_LINES) {
                        parametersScrollOffset = parametersSelectedIndex - MAX_VISIBLE_LINES + 1;
                    }
                }
                else parametersSelectedIndex = 0;
                break;
          case KEY_3: // Scan list selection
          case KEY_1:
          {
              bool isKey3 = (key == KEY_3);
              bool redrawNeeded = false;
          
              switch(parametersSelectedIndex) {//SEE HERE
                  case 0: // DelayRssi
                      DelayRssi = isKey3 ? 
                                 (DelayRssi >= 12 ? 0 : DelayRssi + 1) :
                                 (DelayRssi <= 0 ? 12 : DelayRssi - 1);
                      redrawNeeded = true;
                      break;
              
                  case 1: // SpectrumDelay
                      if (isKey3) {
                          if (SpectrumDelay < 61000) {
                              SpectrumDelay += (SpectrumDelay < 10000) ? 1000 : 5000;
                          }
                      } else if (SpectrumDelay >= 1000) {
                          SpectrumDelay -= (SpectrumDelay < 10000) ? 1000 : 5000;
                      }
                      break;
                    
                  case 2: // PttEmission
                      PttEmission = isKey3 ?
                                (PttEmission >= 2 ? 0 : PttEmission + 1) :
                                (PttEmission <= 0 ? 2 : PttEmission - 1);
                      break;
                    
                  case 3: // FreqInput
                  case 4:
                      if (!isKey3) {
                          appMode = SCAN_RANGE_MODE;
                          FreqInput();
                          Key_1_pressed = 1;
                      }
                      break;

                  case 5: // UpdateScanStep
                      UpdateScanStep(isKey3);
                      break;
                    
                  case 6: // ToggleListeningBW
                  case 7: // ToggleModulation
                      if (isKey3 || key == KEY_1) {
                          if (parametersSelectedIndex == 6) {
                              ToggleListeningBW(isKey3 ? 0 : 1);
                          } else {
                              ToggleModulation();
                          }
                      }
                      break;
                  case 8: // settings.rssiTriggerLevelUp
                        if (isKey3) ClearSettings();
                      break;
                  case 9: // gCounthistory
                        gCounthistory=!gCounthistory;
                      break;
              }
            
              if (isKey3 || redrawNeeded) { //TO REMOVE MAYBE
                  
              }
              break;
          }
        case KEY_EXIT: // Exit parameters menu to previous menu/state
          SaveSettings();
          SetState(SPECTRUM);
          RelaunchScan();
          ResetModifiers();
          if(Key_1_pressed) {
            Key_1_pressed = 0;
            APP_RunSpectrum(3);
          }
          break;

        default:
          break;
      }
            
      return; // Finish handling if we were in PARAMETERS_SELECT
    }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    

  switch (key) {

          
      case KEY_STAR: {
          int step = (settings.rssiTriggerLevelUp >= 20) ? 5 : 1;
          settings.rssiTriggerLevelUp = (settings.rssiTriggerLevelUp >= 80? 0 : settings.rssiTriggerLevelUp + step);
          if(appMode == SCAN_BAND_MODE) BPRssiTriggerLevelUp[bl] = settings.rssiTriggerLevelUp;
          if(appMode == CHANNEL_MODE) SLRssiTriggerLevelUp[ScanListNumber[scanInfo.i]] = settings.rssiTriggerLevelUp;
          if (!SpectrumMonitor) Skip();
          char triggerText[32];
          sprintf(triggerText, "Trigger: %d", settings.rssiTriggerLevelUp);
          ShowOSDPopup(triggerText);
      break;
      }

      case KEY_F: {
          int step = (settings.rssiTriggerLevelUp <= 20) ? 1 : 5;
          settings.rssiTriggerLevelUp = (settings.rssiTriggerLevelUp <= 0? 80 : settings.rssiTriggerLevelUp - step);
          if(appMode == SCAN_BAND_MODE) BPRssiTriggerLevelUp[bl] = settings.rssiTriggerLevelUp;
          if(appMode == CHANNEL_MODE) SLRssiTriggerLevelUp[ScanListNumber[scanInfo.i]] = settings.rssiTriggerLevelUp;
          if (!SpectrumMonitor) Skip();
          char triggerText[32];
          sprintf(triggerText, "Trigger: %d", settings.rssiTriggerLevelUp);
          ShowOSDPopup(triggerText);
    break;
      }


      case KEY_3:
         ToggleListeningBW(1);
		 // NOWE OSD dla zmiany BW
    char bwText[32];
    sprintf(bwText, "BW: %s", bwNames[settings.listenBw]);
    ShowOSDPopup(bwText);

      break;
     
      case KEY_9:
        ToggleModulation();
		 // NOWE OSD dla zmiany modulacji
    char modText[32];
    sprintf(modText, "MOD: %s", gModulationStr[settings.modulationType]);
    ShowOSDPopup(modText);
    break;

      case KEY_1: //SKIP
        Skip();
      break;
     
     case KEY_7:
        SaveSettings(); 
        saved_params= true;
        break;
     
     case KEY_2: //FREE
      classic=!classic;
    /* /////////////////////////DEBUG//////////////////////////
    char str[200] = "";
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
    sprintf(str,"%d %d \r\n",HFreqs[i],historyListIndex);
    LogUart(str); }
    /////////////////////////DEBUG//////////////////////////  */
      break;

    case KEY_8:
      if (historyListActive) {
          memset(HFreqs,0,sizeof(HFreqs));
          memset(HCount,0,sizeof(HCount));
          memset(HBlacklisted,0,sizeof(HBlacklisted));
          historyListIndex = 0;
          historyScrollOffset = 0;
          indexFs = 0;
      } else {
          ShowLines++; 
          if (ShowLines > 4) ShowLines = 0;
      }
    break;

      
    case KEY_UP:
    
    if (historyListActive) {
        if (historyListIndex > 0) {
        historyListIndex--;
        if (historyListIndex < historyScrollOffset) {historyScrollOffset = historyListIndex;}
        if (SpectrumMonitor > 0) SetF(HFreqs[historyListIndex]);
        }
    } else {
        if (appMode==SCAN_BAND_MODE) {
            ToggleScanList(bl, 1);
            settings.bandEnabled[bl+1]= true;
            RelaunchScan(); 
            break;
        }
        else if(appMode==FREQUENCY_MODE) {UpdateCurrentFreq(true);}
        else if (ShowLines > 3) {historyListIndex = (historyListIndex >0 ? historyListIndex-1 : 0);}
        else if(appMode==CHANNEL_MODE){
              BuildValidScanListIndices();
              static bool isFirst = true;
              if (isFirst) {isFirst = false;
                  scanListSelectedIndex = 0;
              }
              else {scanListSelectedIndex = (scanListSelectedIndex < validScanListCount ? scanListSelectedIndex+1 : 0);}
              ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
              SetState(SPECTRUM);
              ResetModifiers();
              RelaunchScan();
              break;
      }
    }
    break;
  case KEY_DOWN: //History
    
    if (historyListActive) {
        if (historyListIndex < CountValidHistoryItems()-1) {
        historyListIndex++;
        if (historyListIndex < historyScrollOffset) {historyScrollOffset = historyListIndex;}
        if (SpectrumMonitor > 0) SetF(HFreqs[historyListIndex]);
        }
    }
    else {
        if (appMode==SCAN_BAND_MODE) {
            ToggleScanList(bl, 1);
            settings.bandEnabled[bl-1]= true;
            RelaunchScan(); 
            break;
        }
        else if(appMode==FREQUENCY_MODE){UpdateCurrentFreq(false);}
        else if (ShowLines > 3) {if (historyListIndex < HISTORY_SIZE) historyListIndex++;}
        else if(appMode==CHANNEL_MODE){
            BuildValidScanListIndices();
            static bool isFirst = true;
            if (isFirst) {isFirst= false;
                scanListSelectedIndex = 0;
            }
              else {scanListSelectedIndex = (scanListSelectedIndex < 1 ? validScanListCount-1:scanListSelectedIndex-1);}
            ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
            SetState(SPECTRUM);
            ResetModifiers();
            RelaunchScan();
            break;
      }
    }
  break;
  
  case KEY_4:
    if (appMode!=SCAN_RANGE_MODE){ToggleStepsCount();}
    break;

  case KEY_0:
    //Free
    break;
  
    case KEY_6:    //Next Mode
    Spectrum_state++;
      if(Spectrum_state > 4) Spectrum_state = 0;
      //SYSTEM_DelayMs(2000);
      APP_RunSpectrum(Spectrum_state);
    break;
  
    case KEY_SIDE1:
        SpectrumMonitor++;
        if (SpectrumMonitor > 2) SpectrumMonitor = 0; // 0 normal, 1 Freq lock, 2 Monitor
		    char monitorText[32];
        const char* modes[] = {"Normal", "Freq Lock", "Monitor"};
        sprintf(monitorText, "Mode: %s", modes[SpectrumMonitor]);
	      ShowOSDPopup(monitorText);
        if (SpectrumMonitor > 0) SetF(HFreqs[historyListIndex]);
        if(SpectrumMonitor == 2) ToggleRX(1);
    break;

    case KEY_SIDE2:
    if (historyListActive) {
        HBlacklisted[historyListIndex] = !HBlacklisted[historyListIndex];
        
        if (HBlacklisted[historyListIndex]) {
            ShowOSDPopup("BL added");
        } else {
            ShowOSDPopup("BL removed");
        }
        RenderHistoryList();
        gIsPeak = 0;
        ToggleRX(false);
        isBlacklistApplied = true;
        ResetScanStats();
        NextScanStep();
        break;
    }
    
    else Blacklist();
    WaitSpectrum = 0;
    break;

  case KEY_PTT:
      ExitAndCopyToVfo();
      break;
  
  case KEY_MENU: //History
      int validCount = 0;
      for (int k = 1; k < HISTORY_SIZE; k++) {
          if (HFreqs[k] != 0) {
              validCount++;
          }
      }
      if (historyListActive == true) {
          Last_Tuned_Freq = HFreqs[historyListIndex];
      } else Last_Tuned_Freq = peak.f;
      currentFreq = Last_Tuned_Freq;
      if(scanInfo.f != Last_Tuned_Freq) SetF(Last_Tuned_Freq);
      scanInfo.f = Last_Tuned_Freq;
      SetState(STILL);
      menuState = 0;
  break;

  case KEY_EXIT:
    if (historyListActive == true) {
      SetState(SPECTRUM);
      historyListActive = false;
      break;
      }
    if (WaitSpectrum) { //STOP wait
      WaitSpectrum = 0; 
      break;
    }

    if (menuState) { menuState = 0;break;}
      DeInitSpectrum(0);
    break;
   
   default:
      break;
  }
}

static void OnKeyDownFreqInput(uint8_t key) {
  BACKLIGHT_TurnOn();
  switch (key) {
  case KEY_0:
  case KEY_1:
  case KEY_2:
  case KEY_3:
  case KEY_4:
  case KEY_5:
  case KEY_6:
  case KEY_7:
  case KEY_8:
  case KEY_9:
  case KEY_STAR:
    UpdateFreqInput(key);
    break;
  case KEY_EXIT:
    if (freqInputIndex == 0) {
      SetState(previousState);
      WaitSpectrum = 0;
      break;
    }
    UpdateFreqInput(key);
    break;
  case KEY_MENU:
    if (tempFreq < RX_freq_min() || tempFreq > F_MAX) {
      break;
    }
    SetState(previousState);
    if (currentState == SPECTRUM) {
        currentFreq = tempFreq;
      ResetModifiers();
    }
    if (currentState == PARAMETERS_SELECT && parametersSelectedIndex == 3)
        gScanRangeStart = tempFreq;
    if (currentState == PARAMETERS_SELECT && parametersSelectedIndex == 4)
        gScanRangeStop = tempFreq;
    if(gScanRangeStart > gScanRangeStop)
		    SWAP(gScanRangeStart, gScanRangeStop);
    break;
  default:
    break;
  }
}

void OnKeyDownStill(KEY_Code_t key) {
  BACKLIGHT_TurnOn();
  switch (key) {
      case KEY_3:
         ToggleListeningBW(1);
      break;
     
      case KEY_9:
        ToggleModulation();
      break;
      case KEY_UP:
          if (stillEditRegs) {
            SetRegMenuValue(stillRegSelected, true);
          }
        break;
      case KEY_DOWN:
          if (stillEditRegs) {
            SetRegMenuValue(stillRegSelected, false);
          }
          break;
      case KEY_2: // przewijanie w górę po liście rejestrów
          if (stillEditRegs && stillRegSelected > 0) {
            stillRegSelected--;
          }
      break;
      case KEY_8: // przewijanie w dół po liście rejestrów
          if (stillEditRegs && stillRegSelected < ARRAY_SIZE(allRegisterSpecs)-1) {
            stillRegSelected++;
          }
      break;
      case KEY_STAR:
      break;
      case KEY_F:
      break;
      case KEY_5:
        FreqInput();
      break;
      case KEY_0:
      break;
      case KEY_6:
      break;
      case KEY_7:
      break;
          
      case KEY_SIDE1: 
          SpectrumMonitor++;
          if (SpectrumMonitor > 2) SpectrumMonitor = 0; // 0 normal, 1 Freq lock, 2 Monitor
          if(SpectrumMonitor == 2) ToggleRX(1);
      break;

      case KEY_SIDE2: 
            Blacklist();
            WaitSpectrum = 0; //don't wait if this frequency not interesting
      break;
      case KEY_PTT:
        ExitAndCopyToVfo();
        break;
      case KEY_MENU:
          stillEditRegs = !stillEditRegs;
      break;
      case KEY_EXIT:
        if (stillEditRegs) {
          stillEditRegs = false;
        break;
        }
        SetState(SPECTRUM);
        SpectrumDelay = 0; //Prevent coming back to still directly
        
    break;
  default:
    break;
  }
}


static void RenderFreqInput() {
  UI_PrintString(freqInputString, 2, 127, 0, 8);
}

static void RenderStatus() {
  memset(gStatusLine, 0, sizeof(gStatusLine));
  DrawStatus();
  ST7565_BlitStatusLine();
}

static void RenderSpectrum() {
  
    if (classic) {
        DrawNums();
        DrawSpectrum();
    }
    if(isListening) {
      DrawF(peak.f);}
    else {
      if (SpectrumMonitor)DrawF(HFreqs[historyListIndex]);
      else DrawF(scanInfo.f);
      UpdateDBMaxAuto();    
    }
}

void DrawMeter(int line) {
  const uint8_t METER_PAD_LEFT = 3;
  settings.dbMax = 30; 
  settings.dbMin = -100;
  uint8_t x = Rssi2PX(scanInfo.rssi, 0, 121);
  for (int i = 0; i < x; ++i) {
    if (i % 5) {
      gFrameBuffer[line][i + METER_PAD_LEFT] |= 0b11111111;
    }
  }
}

static void RenderStill() {
  classic=1;
  char freqStr[18];
  FormatFrequency(scanInfo.f, freqStr, sizeof(freqStr));
  UI_DisplayFrequency(freqStr, 0, 0, 0);
  DrawMeter(2);
  sLevelAttributes sLevelAtt;
  sLevelAtt = GetSLevelAttributes(scanInfo.rssi, scanInfo.f);

  if(sLevelAtt.over > 0)
    sprintf(String, "S%2d+%2d", sLevelAtt.sLevel, sLevelAtt.over);
  else
    sprintf(String, "S%2d", sLevelAtt.sLevel);

  GUI_DisplaySmallest(String, 4, 25, false, true);
  sprintf(String, "%d dBm", sLevelAtt.dBmRssi);
  GUI_DisplaySmallest(String, 40, 25, false, true);



  // --- lista rejestrów
  uint8_t total = ARRAY_SIZE(allRegisterSpecs);
  uint8_t lines = STILL_REGS_MAX_LINES;
  if (total < lines) lines = total;

  // Scroll logic
  if (stillRegSelected >= total) stillRegSelected = total-1;
  if (stillRegSelected < stillRegScroll) stillRegScroll = stillRegSelected;
  if (stillRegSelected >= stillRegScroll + lines) stillRegScroll = stillRegSelected - lines + 1;

  for (uint8_t i = 0; i < lines; ++i) {
    uint8_t idx = i + stillRegScroll;
    RegisterSpec s = allRegisterSpecs[idx];
    uint16_t v = GetRegMenuValue(idx);

    char buf[32];
    // Przygotuj tekst do wyświetlenia
    if (stillEditRegs && idx == stillRegSelected)
      snprintf(buf, sizeof(buf), ">%-18s %6u", s.name, v);
    else
      snprintf(buf, sizeof(buf), " %-18s %6u", s.name, v);

    uint8_t y = 32 + i * 8;
    if (stillEditRegs && idx == stillRegSelected) {
      // Najpierw czarny prostokąt na wysokość linii
      for (uint8_t px = 0; px < 128; ++px)
        for (uint8_t py = y; py < y + 6; ++py) // 6 = wysokość fontu 3x5
          PutPixel(px, py, true); // 
      // Następnie białe litery (fill = true)
      GUI_DisplaySmallest(buf, 0, y, false, false);
    } else {
      // Zwykły tekst: czarne litery na białym
      GUI_DisplaySmallest(buf, 0, y, false, true);
    }
  }
}


static void Render() {
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
  
  switch (currentState) {
  case SPECTRUM:
    if(historyListActive) RenderHistoryList();
    else RenderSpectrum();
    break;
  case FREQ_INPUT:
    RenderFreqInput();
    break;
  case STILL:
    RenderStill();
    break;
  
    case BAND_LIST_SELECT:
      RenderBandSelect();
    break;

    case SCANLIST_SELECT:
      RenderScanListSelect();
    break;
    case PARAMETERS_SELECT:
      RenderParametersSelect();
    break;
#ifdef ENABLE_SCANLIST_SHOW_DETAIL
    case SCANLIST_CHANNELS: // NOWY CASE
      RenderScanListChannels();
      break;
#endif // ENABLE_SCANLIST_SHOW_DETAIL
    
  }
  #ifdef ENABLE_SCREENSHOT
    getScreenShot(1);
  #endif
  ST7565_BlitFullScreen();
}

bool HandleUserInput() {
    kbd.prev = kbd.current;
    kbd.current = GetKey();
    if (kbd.current != KEY_INVALID && kbd.current == kbd.prev) {
        if (kbd.counter < 16)
            kbd.counter++;
        else
            kbd.counter -= 3;
        SYSTEM_DelayMs(20);
    } else {
        kbd.counter = 0;
    }

    if (kbd.counter == 3 || kbd.counter == 16) {
    
        if (kbd.current == KEY_0) {
            if (!historyListActive) {
                historyListActive = true;  
                historyListIndex = 0;
				        historyScrollOffset = 0;
                return true;
            }
    }

        switch (currentState) {
            case SPECTRUM:
                OnKeyDown(kbd.current);
                break;
            case FREQ_INPUT:
                OnKeyDownFreqInput(kbd.current);
                break;
            case STILL:
                OnKeyDownStill(kbd.current);
                break;
			      case BAND_LIST_SELECT: 
                OnKeyDown(kbd.current);
                break;
            case SCANLIST_SELECT:
                OnKeyDown(kbd.current);
                break;
            case PARAMETERS_SELECT:
                OnKeyDown(kbd.current);
                break;
#ifdef ENABLE_SCANLIST_SHOW_DETAIL
            case SCANLIST_CHANNELS: // NOWY CASE
                OnKeyDown(kbd.current);
                break;
#endif // ENABLE_SCANLIST_SHOW_DETAIL
        }
        return true;
    }
    return false;
}

static void UpdateScan() {
  if(gIsPeak || SpectrumMonitor) return;
  SetF(scanInfo.f);
  Measure();
  if(gIsPeak || SpectrumMonitor) return;
  //UpdateScanInfo();
  if (scanInfo.i < GetStepsCount()) {
    NextScanStep();
    return;
  }
  newScanStart = true;
}

static void UpdateListening(void) { // called every 10ms
    static uint32_t stableFreq = 1;
    static uint16_t stableCount = 0;
    uint16_t rssi = GetRssi();
    scanInfo.rssi = rssi;
    uint16_t count = GetStepsCount()+1;
    uint16_t i = peak.i;

    if (count > 128) {
        uint16_t pixel = (uint32_t) i * 128 / count;
        rssiHistory[pixel] = rssi;
    } else {
          uint16_t j;    
          uint16_t base = 128 / count;
          uint16_t rem  = 128 % count;
          uint16_t startIndex = i * base + (i < rem ? i : rem);
          uint16_t width      = base + (i < rem ? 1 : 0);
          uint16_t endIndex   = startIndex + width;
          for (j = startIndex; j < endIndex; ++j) {rssiHistory[j] = rssi;} 
      }
      

    // Détection de fréquence stable
    if (peak.f == stableFreq) {
        if (++stableCount >= 50) {  // ~500ms
            if (!SpectrumMonitor) FillfreqHistory();
            stableCount = 0;
            // turn on green led only if screen brightness is over 7
            BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, 1);
        }
    } else {
        stableFreq = peak.f;
        stableCount = 0;
    }
    if (isListening ||SpectrumMonitor) UpdateNoiseOff();
    UpdateNoiseOn();
    
    if (gIsPeak) {
        WaitSpectrum = SpectrumDelay;   // reset timer
        return;
    }
    if (WaitSpectrum > 61000) {
        return;
    }
    if (WaitSpectrum > 10) {
        WaitSpectrum -= 10;
        return;
    }
    
    // timer écoulé
    WaitSpectrum = 0;
    ToggleRX(false);
    ResetScanStats();
    //NextScanStep();
}


static void Tick() {
  if (gNextTimeslice_500ms) {
    if (gBacklightCountdown > 0)
      if (--gBacklightCountdown == 0)
				if (!settings.backlightAlwaysOn)
					BACKLIGHT_TurnOff();   // turn backlight off
    gNextTimeslice_500ms = false;
  }
  
  if (osdPopupTimer > 0) {
        UI_DisplayPopup(osdPopupText);  // Wyświetl aktualny tekst
        ST7565_BlitFullScreen();
        osdPopupTimer -= 10;  // Tick co 10ms
        if (osdPopupTimer <= 0) {
            osdPopupText[0] = '\0';  // Wyczyść tekst po wygaśnięciu
        }
        return;  // Priorytet dla OSD (?)
    }



  if (gNextTimeslice_10ms) {
    HandleUserInput();
    gNextTimeslice_10ms = 0;
    if (isListening || SpectrumMonitor) UpdateListening(); 
  }

  if(!isListening && gIsPeak) {
     LookupChannelInfo();
     SetF(peak.f);
     ToggleRX(true);
     return;
  }

  if (newScanStart) {
    newScanStart = false;
    InitScan();
  }

  if (!isListening) {UpdateScan();}
  
  if (gNextTimeslice_display) {
    gNextTimeslice_display = 0;
    latestScanListName[0] = '\0';
    RenderStatus();
    Render();
  } 
}

void APP_RunSpectrum(uint8_t Spectrum_state) {
  Mode mode;
  if (StorePtt_Toggle_Mode) Ptt_Toggle_Mode = StorePtt_Toggle_Mode;
  // Spectrum_state 1: MR channel, 2: band scan, 3: range scan, 4: basic spectrum, 5:new scan range 0: no spectrum
  if (Spectrum_state == 4) mode = FREQUENCY_MODE ;
  if (Spectrum_state == 3) mode = SCAN_RANGE_MODE ;
  if (Spectrum_state == 2) mode = SCAN_BAND_MODE ;
  if (Spectrum_state == 1) mode = CHANNEL_MODE ;
  EEPROM_WriteBuffer(0x1D00, &Spectrum_state);
  if (!Key_1_pressed)LoadSettings();
  appMode = mode;
  ResetModifiers();
  if (appMode==CHANNEL_MODE)LoadValidMemoryChannels();
  if (appMode==FREQUENCY_MODE) {
    currentFreq = initialFreq = gTxVfo->pRX->Frequency;
      gScanRangeStart = currentFreq - (GetBW() >> 1);
      gScanRangeStop  = currentFreq + (GetBW() >> 1);
  }
  BackupRegisters();
  uint8_t CodeType = gTxVfo->pRX->CodeType;
	uint8_t Code     = gTxVfo->pRX->Code;
  BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(CodeType, Code));
  ResetInterrupts();
  // turn of GREEN LED if spectrum was started during active RX
  BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
  isListening = true; // to turn off RX later
  newScanStart = true;
  RADIO_SetModulation(settings.modulationType = MODULATION_FM);
  BK4819_SetFilterBandwidth(settings.listenBw, false);
  AutoAdjustFreqChangeStep();
  RelaunchScan();
  for (int i = 0; i < 128; ++i) {rssiHistory[i] = 0;}
  isInitialized = true;
  historyListActive = false;  
  while (isInitialized) {
    Tick();
  }
}

void LoadValidMemoryChannels(void)
  {
    memset(scanChannel,0,sizeof(scanChannel));
    scanChannelsCount = 0;
    bool listsEnabled = false;
    
    // loop through all scanlists
    for (int CurrentScanList=1; CurrentScanList <= 16; CurrentScanList++) {
      // skip disabled scanlist
      if (CurrentScanList <= 15 && !settings.scanListEnabled[CurrentScanList-1])
        continue;

      // valid scanlist is enabled
      if (CurrentScanList <= 15 && settings.scanListEnabled[CurrentScanList-1])
        listsEnabled = true;
      
      // break if some lists were enabled, else scan all channels
      if (CurrentScanList > 15 && listsEnabled)
        break;

      uint16_t offset = scanChannelsCount;
      uint16_t listChannelsCount = RADIO_ValidMemoryChannelsCount(listsEnabled, CurrentScanList-1);
      scanChannelsCount += listChannelsCount;
      uint16_t channelIndex = 0xFFFF;
      for(uint16_t i=0; i < listChannelsCount; i++)
      {
        uint16_t nextChannel;
        nextChannel = RADIO_FindNextChannel((channelIndex)+1, 1, listsEnabled, CurrentScanList-1);

        if (nextChannel == 0xFFFF)
        {	// no valid channel found
          break;
        }
        else
        {
          channelIndex = nextChannel;
          scanChannel[offset+i]=channelIndex;
          ScanListNumber[offset+i]=CurrentScanList;
        }
      }
    }
  }

  void ToggleScanList(int scanListNumber, int single )
  {
    if (appMode == SCAN_BAND_MODE)
      {
      if (single) memset(settings.bandEnabled, 0, sizeof(settings.bandEnabled));
        else settings.bandEnabled[scanListNumber-1] = !settings.bandEnabled[scanListNumber-1];
      }
    if (appMode == CHANNEL_MODE) {
        if (single) {memset(settings.scanListEnabled, 0, sizeof(settings.scanListEnabled));}
        settings.scanListEnabled[scanListNumber] = !settings.scanListEnabled[scanListNumber];
      }
  }

typedef struct {
    // Block 1 (0x1D10 - 0x1D1F)
    int ShowLines;
    uint8_t DelayRssi;
    uint8_t PttEmission; 
    uint8_t listenBw;
    uint16_t BPRssiTriggerLevelUp[32]; // 32 bytes of settings.rssiTriggerLevelUp levels
    uint8_t SLRssiTriggerLevelUp[15];
    uint32_t bandListFlags;            // Bits 0-31: bandEnabled[0..31]
    uint16_t scanListFlags;            // Bits 0-14: scanListEnabled[0..14]
    int16_t Trigger;
    uint32_t RangeStart;
    uint32_t RangeStop;
    ScanStep scanStepIndex;
    uint16_t R40;                      // RF TX Deviation
    uint16_t R29;                      // AF TX noise compressor, AF TX 0dB compressor, AF TX compression ratio
    uint16_t R19;                      // Disable MIC AGC
    uint16_t R73;                      // AFC range select
} SettingsEEPROM;


static void LoadSettings()
{
  SettingsEEPROM  eepromData  = {0};
  
  // Lecture de toutes les données
  EEPROM_ReadBuffer(0x1D10, &eepromData, sizeof(eepromData));
  for (int i = 0; i < 15; i++) {
    settings.scanListEnabled[i] = (eepromData.scanListFlags >> i) & 0x01;
    SLRssiTriggerLevelUp[i] = eepromData.SLRssiTriggerLevelUp[i];
  }
  settings.rssiTriggerLevelUp = eepromData.Trigger;
  if (eepromData.listenBw >0 && eepromData.listenBw <7) settings.listenBw = eepromData.listenBw;

  if (eepromData.RangeStart > 1400000) gScanRangeStart = eepromData.RangeStart;
  if (eepromData.RangeStop > 1400000) gScanRangeStop = eepromData.RangeStop;
  settings.scanStepIndex = eepromData.scanStepIndex;
  for (int i = 0; i < 32; i++) {
      BPRssiTriggerLevelUp[i] = eepromData.BPRssiTriggerLevelUp[i];
      settings.bandEnabled[i] = (eepromData.bandListFlags >> i) & 0x01;
    }
  DelayRssi = eepromData.DelayRssi;
  if (DelayRssi > 12) DelayRssi =12;
  if (PttEmission > 1) PttEmission =0;
  PttEmission = eepromData.PttEmission;
  validScanListCount = 0;
  ShowLines = eepromData.ShowLines;
  ChannelAttributes_t att;
  for (int i = 0; i < 200; i++) {
    att = gMR_ChannelAttributes[i];
    if (att.scanlist > validScanListCount) {validScanListCount = att.scanlist;}
  }
  BK4819_WriteRegister(BK4819_REG_40, eepromData.R40);
  BK4819_WriteRegister(BK4819_REG_29, eepromData.R29);
  BK4819_WriteRegister(BK4819_REG_19, eepromData.R19);
  BK4819_WriteRegister(BK4819_REG_73, eepromData.R73);

  }

static void SaveSettings() 
{
  SettingsEEPROM  eepromData  = {0};
  for (int i = 0; i < 15; i++) {
    if (settings.scanListEnabled[i]) eepromData.scanListFlags |= (1 << i);
    eepromData.SLRssiTriggerLevelUp[i] = SLRssiTriggerLevelUp[i];
  }
  eepromData.Trigger = settings.rssiTriggerLevelUp;
  eepromData.listenBw = settings.listenBw;
  eepromData.RangeStart = gScanRangeStart;
  eepromData.RangeStop = gScanRangeStop;
  eepromData.DelayRssi = DelayRssi;
  eepromData.PttEmission = PttEmission;
  eepromData.scanStepIndex = settings.scanStepIndex;
  eepromData.ShowLines = ShowLines;
  for (int i = 0; i < 32; i++) { 
      eepromData.BPRssiTriggerLevelUp[i] = BPRssiTriggerLevelUp[i];
      if (settings.bandEnabled[i]) eepromData.bandListFlags |= (1 << i);
    }
  eepromData.R40 = BK4819_ReadRegister(BK4819_REG_40);
  eepromData.R29 = BK4819_ReadRegister(BK4819_REG_29);
  eepromData.R19 = BK4819_ReadRegister(BK4819_REG_19);
  eepromData.R73 = BK4819_ReadRegister(BK4819_REG_73);

  //char str[64] = "";sprintf(str, "R40:%d R29:%d R19:%d R73:%d \r\n", eepromData.R40, eepromData.R29, eepromData.R19, eepromData.R73 );LogUart(str);
  
  
  //R40:13520 R29:43840 R19:4161 R73:18066

  // Write in 8-byte chunks
  for (uint16_t addr = 0; addr < sizeof(eepromData); addr += 8) 
    EEPROM_WriteBuffer(addr + 0x1D10, ((uint8_t*)&eepromData) + addr);
  
}

static void ClearSettings() 
{
  for (int i = 1; i < 15; i++) {
    settings.scanListEnabled[i] = 0;
    SLRssiTriggerLevelUp[i] = 5;
  }
  settings.scanListEnabled[1] = 1;
  settings.rssiTriggerLevelUp = 5;
  settings.listenBw = 1;
  gScanRangeStart = 1400000;
  gScanRangeStop = 13000000;
  DelayRssi = 2;
  PttEmission = 2;
  settings.scanStepIndex = S_STEP_500kHz;
  ShowLines = 2;
  for (int i = 0; i < 32; i++) { 
      BPRssiTriggerLevelUp[i] = 5;
      settings.bandEnabled[i] = 0;
    }
  settings.bandEnabled[1] = 1;
  
  BK4819_WriteRegister(BK4819_REG_40, 0x34D0);
  BK4819_WriteRegister(BK4819_REG_29, 0xAB40);
  BK4819_WriteRegister(BK4819_REG_19, 0x1041);
  BK4819_WriteRegister(BK4819_REG_73, 0x4692);
  SaveSettings(); 
}

static bool GetScanListLabel(uint8_t scanListIndex, char* bufferOut) {
    ChannelAttributes_t att;
    char channel_name[12];
    uint16_t first_channel = 0XFFFF;
    uint16_t channel_count = 0;

    // Szukaj kanału należącego do tej scanlisty i licz kanały
    for (uint16_t i = 0; i < MR_CHANNEL_LAST; i++) {
      att = gMR_ChannelAttributes[i];
        if (att.scanlist == scanListIndex + 1) {
            if (first_channel == 0XFFFF)
                first_channel = i;
            channel_count++;
        }
    }
    if (first_channel == 0XFFFF)
        return false; // Brak kanałów

    SETTINGS_FetchChannelName(channel_name, first_channel);
    if (channel_name[0] == '\0') {
        uint32_t freq = gMR_ChannelFrequencyAttributes[first_channel].Frequency;
        char freqStr[12];
        sprintf(freqStr, "%u.%05u", freq / 100000, freq % 100000);
        RemoveTrailZeros(freqStr);
        sprintf(bufferOut, "%2d:%s%s", scanListIndex + 1, freqStr, settings.scanListEnabled[scanListIndex] ? " *" : "");
    } else {
        sprintf(bufferOut, "%2d:%s%s", scanListIndex + 1, channel_name, settings.scanListEnabled[scanListIndex] ? " *" : "");
    }
    return true;
}

static void BuildValidScanListIndices() {
    uint8_t ScanListCount = 0;
    for (uint8_t i = 0; i < 15; i++) {
        char tempName[17];
        if (GetScanListLabel(i, tempName)) {
            validScanListIndices[ScanListCount++] = i;
        }
    }
    validScanListCount = ScanListCount; // <-- KLUCZOWE!
}


static void GetFilteredScanListText(uint8_t displayIndex, char* buffer) {
    uint8_t realIndex = validScanListIndices[displayIndex];
    GetScanListLabel(realIndex, buffer);
}
static void GetParametersText(uint8_t index, char *buffer) {
    switch(index) {
        case 0:
            sprintf(buffer, "Rssi Delay: %2d ms", DelayRssi);
            break;
            
        case 1:
            if (SpectrumDelay < 65000) sprintf(buffer, "SpectrumDelay:%2us", SpectrumDelay / 1000);
              else sprintf(buffer, "SpectrumDelay:oo");
            break;
            
        case 2:
            if(PttEmission == 0)
              sprintf(buffer, "PTT: LAST VFO FREQ");
            else if (PttEmission == 1)
              sprintf(buffer, "PTT: NINJA MODE");
            else if (PttEmission == 2)
              sprintf(buffer, "PTT: LAST RECEIVED");
            break;
            
        case 3: {
            uint32_t start = gScanRangeStart;
            sprintf(buffer, "FStart: %u.%05u", start / 100000, start % 100000);
            break;
        }
            
        case 4: {
            uint32_t stop = gScanRangeStop;
            sprintf(buffer, "FStop: %u.%05u", stop / 100000, stop % 100000);
            break;
        }
      
        case 5: {
            uint32_t step = GetScanStep();
            sprintf(buffer, step % 100 ? "STEP: %uk%02u" : "STEP: %uk", 
                   step / 100, step % 100);
            break;
        }
            
        case 6:
            sprintf(buffer, "ListenBw: %s", bwNames[settings.listenBw]);
            break;
            
        case 7:
            sprintf(buffer, "Modulation: %s", gModulationStr[settings.modulationType]);
            break;
        
        case 8:
            sprintf(buffer, "DEFAULT PARAMS: 3");
            break;
        case 9:
            if (gCounthistory) sprintf(buffer, "Freq Counting");
            else sprintf(buffer, "Time Counting");
            break;
        default:
            // Gestion d'un index inattendu (optionnel)
            buffer[0] = '\0';
            break;
    }
}

 static void GetBandItemText(uint8_t index, char* buffer) {
    
    sprintf(buffer, "%d:%-12s%s", 
            index + 1, 
            BParams[index].BandName,
            settings.bandEnabled[index] ? "*" : "");
}

static void GetHistoryItemText(uint8_t index, char* buffer) {
        
    char freqStr[16];
    sprintf(freqStr, "%u.%05u", HFreqs[index] / 100000, HFreqs[index] % 100000);
    RemoveTrailZeros(freqStr);
    
    uint16_t channel = BOARD_gMR_fetchChannel(HFreqs[index]);
    
    if (channel != 0XFFFF) {
        sprintf(buffer, "%s%s:%d", 
                HBlacklisted[index] ? "#" : "",
                gMR_ChannelFrequencyAttributes[channel].Name,
                //freqStr,
                HCount[index]);
    } else {
        sprintf(buffer, "%s%s:%d", 
                HBlacklisted[index] ? "#" : "",
                freqStr,
                HCount[index]);
    }
}

static void RenderList(const char* title, uint8_t numItems, uint8_t selectedIndex, uint8_t scrollOffset,
                      void (*getItemText)(uint8_t index, char* buffer)) {
    // Clear display buffer
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
    
    // Draw title - wyrównany do lewej dla maksymalnego wykorzystania miejsca
    if (!SpectrumMonitor) UI_PrintStringSmall(title, 1, LCD_WIDTH - 1, 0,0);
    // List parameters for UI_PrintStringSmall (lines 1-7 available)
    const uint8_t FIRST_ITEM_LINE = 1;  // Start from line 1 (line 0 is title)
    const uint8_t MAX_LINES = 6;        // Lines 1-7 available for items
    
    // Adjust scroll offset if needed
    if (numItems <= MAX_LINES) {
        scrollOffset = 0;
    } else if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    } else if (selectedIndex >= scrollOffset + MAX_LINES) {
        scrollOffset = selectedIndex - MAX_LINES + 1;
    }
    
    // Maksymalna liczba znaków na linię (128 pikseli / 7 pikseli na znak = ~18)
    const uint8_t MAX_CHARS_PER_LINE = 18;
    // Draw visible items
    for (uint8_t i = 0; i < MAX_LINES; i++) {
        uint8_t itemIndex = i + scrollOffset;
        if (itemIndex >= numItems) break;
        
        char itemText[32];
        getItemText(itemIndex, itemText);
        
        uint8_t lineNumber = FIRST_ITEM_LINE + i;
        
        // Wyrównanie maksymalnie do lewej
        if (itemIndex == selectedIndex) {
        char displayText[MAX_CHARS_PER_LINE + 1];
        strcpy(displayText, itemText);
        char selectedText[MAX_CHARS_PER_LINE + 2];
        snprintf(selectedText, sizeof(selectedText), "%s", displayText);
        UI_PrintStringSmall(selectedText, 1, 0, lineNumber,1);
        } else {
            UI_PrintStringSmall(itemText, 1, 0, lineNumber,0); // Minimalne wcięcie
          }
          
    }
    if (historyListActive && SpectrumMonitor > 0) DrawMeter(0);
    ST7565_BlitFullScreen();
}



// Wrapper functions for original calls

// Fonction pour afficher le menu ScanList
static void RenderScanListSelect() {
    BuildValidScanListIndices(); 
    RenderList("SCANLISTS:", validScanListCount,scanListSelectedIndex, scanListScrollOffset, GetFilteredScanListText);
}

static void RenderParametersSelect() {
  RenderList("PARAMETERS:", PARAMETER_COUNT,parametersSelectedIndex, parametersScrollOffset, GetParametersText);
}


#ifdef ENABLE_FR_BAND
      static void RenderBandSelect() {RenderList("FRA BANDS:", ARRAY_SIZE(BParams),bandListSelectedIndex, bandListScrollOffset, GetBandItemText);}
#endif

#ifdef ENABLE_PL_BAND
      static void RenderBandSelect() {RenderList("POL BANDS:", ARRAY_SIZE(BParams),bandListSelectedIndex, bandListScrollOffset, GetBandItemText);}
#endif

#ifdef ENABLE_RO_BAND
      static void RenderBandSelect() {RenderList("ROM BANDS:", ARRAY_SIZE(BParams),bandListSelectedIndex, bandListScrollOffset, GetBandItemText);}
#endif

#ifdef ENABLE_KO_BAND
      static void RenderBandSelect() {RenderList("KOL BANDS:", ARRAY_SIZE(BParams),bandListSelectedIndex, bandListScrollOffset, GetBandItemText);}
#endif

#ifdef ENABLE_CZ_BAND
      static void RenderBandSelect() {RenderList("CZ BANDS:", ARRAY_SIZE(BParams),bandListSelectedIndex, bandListScrollOffset, GetBandItemText);}
#endif

#ifdef ENABLE_TU_BAND
      static void RenderBandSelect() {RenderList("TU BANDS:", ARRAY_SIZE(BParams),bandListSelectedIndex, bandListScrollOffset, GetBandItemText);}
#endif

#ifdef ENABLE_RU_BAND
      static void RenderBandSelect() {RenderList("RUS BANDS:", ARRAY_SIZE(BParams),bandListSelectedIndex, bandListScrollOffset, GetBandItemText);}
#endif

static void RenderHistoryList() {
    char headerString[24];
    
    
    // Clear display buffer
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
    
    if (!SpectrumMonitor) {
      sprintf(headerString, "HISTORY: %d", indexFs);
      UI_PrintStringSmall(headerString, 1, LCD_WIDTH - 1, 0, 0);
    } else DrawMeter(0);
    
    const uint8_t FIRST_ITEM_LINE = 1;
    const uint8_t MAX_LINES = 6;
    
    uint8_t scrollOffset = historyScrollOffset;
    uint8_t selectedIndex = historyListIndex;
    
    // Adjust scroll offset if needed
    if (indexFs <= MAX_LINES) {
        scrollOffset = 0;
    } else if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    } else if (selectedIndex >= scrollOffset + MAX_LINES) {
        scrollOffset = selectedIndex - MAX_LINES + 1;
    }
    
    // Draw visible items
    for (uint8_t i = 0; i < MAX_LINES; i++) {
        uint8_t itemIndex = i + scrollOffset;

        if (itemIndex >= CountValidHistoryItems()) break;
        char itemText[32];
        GetHistoryItemText(itemIndex, itemText);
        uint8_t lineNumber = FIRST_ITEM_LINE + i;
        
        if (itemIndex == selectedIndex) {
            for (uint8_t x = 0; x < LCD_WIDTH; x++) {
                for (uint8_t y = lineNumber * 8; y < (lineNumber + 1) * 8; y++) {
                        PutPixel(x, y, true);
                    }
            }
            UI_PrintStringSmall(itemText, 1, 0, lineNumber, 1);
            }
        
        else {UI_PrintStringSmall(itemText, 1, 0, lineNumber, 0);}
        } 
    
    ST7565_BlitFullScreen();
}

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
static void BuildScanListChannels(uint8_t scanListIndex) {
    scanListChannelsCount = 0;
    ChannelAttributes_t att;
    
    for (int i = 0; i < 200; i++) {
        att = gMR_ChannelAttributes[i];
        if (att.scanlist == scanListIndex + 1) {
            if (scanListChannelsCount < 200) {
                scanListChannels[scanListChannelsCount++] = i;
            }
        }
    }
}

static void RenderScanListChannels() {
    char headerString[24];
    uint8_t realScanListIndex = validScanListIndices[selectedScanListIndex];
    sprintf(headerString, "SL %d CHANNELS:", realScanListIndex + 1);
    
    // Specjalna obsługa dwulinijkowa
    RenderScanListChannelsDoubleLines(headerString, scanListChannelsCount, 
                                     scanListChannelsSelectedIndex,
                                     scanListChannelsScrollOffset);
}

static void RenderScanListChannelsDoubleLines(const char* title, uint8_t numItems, 
                                             uint8_t selectedIndex, uint8_t scrollOffset) {
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
    UI_PrintStringSmall(title, 1, LCD_WIDTH - 1, 0,1);
    
    const uint8_t MAX_ITEMS_VISIBLE = 3; // 3 kanały x 2 linie = 6 linii
    
    for (uint8_t i = 0; i < MAX_ITEMS_VISIBLE; i++) {
        uint8_t itemIndex = i + scrollOffset;
        if (itemIndex >= numItems) break;
        
        uint16_t channelIndex = scanListChannels[itemIndex];
        char channel_name[12];
        SETTINGS_FetchChannelName(channel_name, channelIndex);
        
        uint32_t freq = gMR_ChannelFrequencyAttributes[channelIndex].Frequency;
        char freqStr[16];
        sprintf(freqStr, "... %u.%05u", freq/100000, freq%100000);
        RemoveTrailZeros(freqStr);
        
        uint8_t line1 = 1 + i * 2;
        uint8_t line2 = 2 + i * 2;
        
        char nameText[20], freqText[20];
        if (itemIndex == selectedIndex) {
            sprintf(nameText, ">%3d: %s", channelIndex + 1, channel_name);
            sprintf(freqText, " %s", freqStr);
        } else {
            sprintf(nameText, " %3d: %s", channelIndex + 1, channel_name);
            sprintf(freqText, " %s", freqStr);
        }
        
        UI_PrintStringSmall(nameText, 1, 0, line1,0);
        UI_PrintStringSmall(freqText, 1, 0, line2,0);
    }
    
    ST7565_BlitFullScreen();
}
#endif // ENABLE_SCANLIST_SHOW_DETAIL
