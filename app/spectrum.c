#include "app/spectrum.h"
#include "scanner.h"
#include "driver/backlight.h"
#include "driver/eeprom.h"

#include "ui/helper.h"
#include "common.h"
#include "action.h"
#include "bands.h"
#include "ui/main.h"
//#include "debugging.h"

/*	
          /////////////////////////DEBUG//////////////////////////
          char str[64] = "";sprintf(str, "%d\r\n", Spectrum_state );LogUart(str);
*/

#ifdef ENABLE_SCREENSHOT
  #include "screenshot.h"
#endif

/* --- Add near top of file, po include'ach --- */
extern char _sheap;   /* początek sterty (z linker script) */
extern char _eheap;   /* limit sterty (z linker script) */

static inline uint32_t get_sp(void)
{
    uint32_t sp;
    __asm volatile ("mov %0, sp" : "=r" (sp));
    return sp;
}


static uint32_t free_ram_bytes(void)
{
    uint32_t sp = get_sp();
    uint32_t heap_start = (uint32_t)&_sheap;
    uint32_t heap_limit = (uint32_t)&_eheap;

    if (sp <= heap_start) return 0;
    uint32_t free = sp - heap_start;

    uint32_t max_free = (heap_limit > heap_start) ? (heap_limit - heap_start) : 0;
    if (free > max_free) free = max_free;

    return free;
}

#define MAX_VISIBLE_LINES 6

static volatile bool gSpectrumChangeRequested = false;
static volatile uint8_t gRequestedSpectrumState = 0;

#ifdef ENABLE_EEPROM_512K
  #define HISTORY_SIZE 100
#else
  #define HISTORY_SIZE 500
#endif


static uint16_t historyListIndex = 0;
static uint16_t indexFs = 0;
static int historyScrollOffset = 0;
static uint32_t    HFreqs[HISTORY_SIZE];
static uint8_t     HCount[HISTORY_SIZE];
static bool  HBlacklisted[HISTORY_SIZE];
static bool gHistoryScan = false; // Indicateur de scan de l'historique

/////////////////////////////Parameters://///////////////////////////
//SEE parametersSelectedIndex
// see GetParametersText
static uint8_t DelayRssi = 4;                // case 0       
static uint16_t SpectrumDelay = 0;           // case 1      
static uint16_t MaxListenTime = 0;           // case 2
static uint8_t PttEmission = 0;              // case 3      
static uint32_t gScanRangeStart = 1400000;   // case 4      
static uint32_t gScanRangeStop = 13000000;   // case 5      
//Step                                       // case 6      
//ListenBW                                   // case 7      
//Modulation                                 // case 8      
//ClearSettings                              // case 9      
static bool Backlight_On_Rx = 0;             // case 10        
static bool gCounthistory = 1;               // case 11      
//ClearHistory                               // case 12      
//RAM                                        // case 13     
static uint16_t SpectrumSleepMs = 0;         // case 14
static uint8_t Noislvl_OFF = 68;             // case 15
static uint8_t Noislvl_ON = 58;
static uint16_t osdPopupSetting = 500;       // case 16
static uint16_t UOO_trigger = 25;            // case 17
static uint8_t AUTO_KEYLOCK = AUTOLOCK_OFF;  // case 18
static uint8_t Gain3D_Index = 1;             // case 19 
static bool    Enable2XIF = 0;               // case 20 
static bool    SoundBoost = 0;               // case 21 
#define PARAMETER_COUNT 22
////////////////////////////////////////////////////////////////////

uint8_t  gKeylockCountdown = 0;
bool     gIsKeylocked = false;
static uint16_t osdPopupTimer = 0;
static uint32_t Fmax = 0;
static uint32_t spectrumElapsedCount = 0;
static uint32_t SpectrumPauseCount = 0;
static bool SPECTRUM_PAUSED;
static uint8_t IndexMaxLT = 0;
static const char *labels[] = {"OFF","3s","6s","10s","20s", "1m", "5m", "10m", "20m", "30m"};
static const uint16_t listenSteps[] = {0, 3, 6, 10, 20, 60, 300, 600, 1200, 1800}; //in s
#define LISTEN_STEP_COUNT 9

static uint8_t IndexPS = 0;
static const char *labelsPS[] = {"OFF","200ms","400ms", "1s", "2s", "5s"};
static const uint16_t PS_Steps[] = {0, 20, 40, 100, 200, 500}; //in 10 ms
#define PS_STEP_COUNT 5


static uint32_t lastReceivingFreq = 0;
static bool gIsPeak = false;
static bool historyListActive = false;
static bool gForceModulation = 0;
static bool classic = 1;
static uint8_t SpectrumMonitor = 0;
static uint8_t prevSpectrumMonitor = 0;
static bool Key_1_pressed = 0;
static uint16_t WaitSpectrum = 0; 
static uint32_t Last_Tuned_Freq = 44610000;
#define SQUELCH_OFF_DELAY 10;
static bool StorePtt_Toggle_Mode = 0;
static uint8_t ArrowLine = 1;
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
static void ClearHistory();
static void DrawMeter(int);
static uint8_t scanListSelectedIndex = 0;
static uint8_t scanListScrollOffset = 0;
static uint8_t parametersSelectedIndex = 0;
static uint8_t parametersScrollOffset = 0;
static uint8_t validScanListCount = 0;
static KeyboardState kbd = {KEY_INVALID, KEY_INVALID, 0,0};
struct FrequencyBandInfo {
    uint32_t lower;
    uint32_t upper;
    uint32_t middle;
};
static bool isBlacklistApplied;
static uint32_t cdcssFreq;
static uint16_t ctcssFreq;
//static uint8_t refresh = 0; // СУБТОНО ЗАПРОС ВСЕГДА
#define F_MAX frequencyBandTable[ARRAY_SIZE(frequencyBandTable) - 1].upper
#define Bottom_print 51 //Robby69
static Mode appMode;
#define UHF_NOISE_FLOOR 5
static uint16_t scanChannel[MR_CHANNEL_LAST + 3];
static uint8_t ScanListNumber[MR_CHANNEL_LAST + 3];
static uint16_t scanChannelsCount;
static void ToggleScanList();
static void SaveSettings();
static const uint16_t RSSI_MAX_VALUE = 255;
static uint16_t R30, R37, R3D, R43, R47, R48, R7E, R02, R3F;
static char String[100];
static char StringC[10];
static bool isKnownChannel = false;
static uint16_t  gChannel;
static char channelName[12];
ModulationMode_t  channelModulation;
static BK4819_FilterBandwidth_t channelBandwidth;
static bool isInitialized = false;
static bool isListening = true;
static bool newScanStart = true;
static bool audioState = true; //true
static uint8_t bl;
static State currentState = SPECTRUM, previousState = SPECTRUM;
static uint8_t Spectrum_state; 
static PeakInfo peak;
static ScanInfo scanInfo;
static char     latestScanListName[12];
static bool IsBlacklisted(uint32_t f);

SpectrumSettings settings = {stepsCount: STEPS_128,
                             scanStepIndex: S_STEP_500kHz,
                             frequencyChangeStep: 80000,
                             rssiTriggerLevelUp: 20,
                             bw: BK4819_FILTER_BW_WIDE,
                             listenBw: BK4819_FILTER_BW_WIDE,
                             modulationType: false,
                             dbMin: -128,
                             dbMax: 10,
                             scanList: S_SCAN_LIST_ALL,
                             scanListEnabled: {0},
                             bandEnabled: {0}
                            };

static uint32_t currentFreq, tempFreq;
static uint8_t rssiHistory[128];
static int ShowLines = 1;  // СТРОКА ПО УМОЛЧАНИЮ
static uint8_t freqInputIndex = 0;
static uint8_t freqInputDotIndex = 0;
static KEY_Code_t freqInputArr[10];
char freqInputString[11];
static const bandparameters BParams[32];
static uint8_t nextBandToScanIndex = 0;
static void LookupChannelModulation();

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
  static uint16_t scanListChannels[MR_CHANNEL_LAST+1]; // Array to store Channel indices for selected scanlist
  static uint16_t scanListChannelsCount = 0; // Number of Channels in selected scanlist
  static uint16_t scanListChannelsSelectedIndex = 0;
  static uint16_t scanListChannelsScrollOffset = 0;
  static uint16_t selectedScanListIndex = 0; // Which scanlist we're viewing Channels for
  static void BuildScanListChannels(uint8_t scanListIndex);
  static void RenderScanListChannels();
  static void RenderScanListChannelsDoubleLines(const char* title, uint8_t numItems, uint8_t selectedIndex, uint8_t scrollOffset);
#endif

  #define MAX_VALID_SCANLISTS 15
static uint8_t validScanListIndices[MAX_VALID_SCANLISTS]; // stocke les index valides
      static void MyDrawShortHLine(uint8_t y, uint8_t x_start, uint8_t x_end, uint8_t step, bool white); //ПРОСТОЙ РЕЖИМ ЛИНИИ
static void MyDrawVLine(uint8_t x, uint8_t y_start, uint8_t y_end, uint8_t step); //ПРОСТОЙ РЕЖИМ ЛИНИИ

const RegisterSpec allRegisterSpecs[] = {
 //   {"10_LNAs",  0x10, 8, 0b11,  1},
 //   {"10_LNA",   0x10, 5, 0b111, 1},
 //   {"10_PGA",   0x10, 0, 0b111, 1},
 //   {"10_MIX",   0x10, 3, 0b11,  1},
 //   {"11_LNAs",  0x11, 8, 0b11,  1},
 //   {"11_LNA",   0x11, 5, 0b111, 1},
 //   {"11_PGA",   0x11, 0, 0b111, 1},
 //   {"11_MIX",   0x11, 3, 0b11,  1},
 //   {"12_LNAs",  0x12, 8, 0b11,  1},
 //   {"12_LNA",   0x12, 5, 0b111, 1},
 //   {"12_PGA",   0x12, 0, 0b111, 1},
 //   {"12_MIX",   0x12, 3, 0b11,  1},
    {"13_LNAs",  0x13, 8, 0b11,  1},
    {"13_LNA",   0x13, 5, 0b111, 1},
    {"13_PGA",   0x13, 0, 0b111, 1},
    {"13_MIX",   0x13, 3, 0b11,  1},
 //   {"14_LNAs",  0x14, 8, 0b11,  1},
 //   {"14_LNA",   0x14, 5, 0b111, 1},
 //   {"14_PGA",   0x14, 0, 0b111, 1},
 //   {"14_MIX",   0x14, 3, 0b11,  1},
    {"XTAL F Mode Select", 0x3C, 6, 0b11, 1},
//    {"OFF AF Rx de-emp", 0x2B, 8, 1, 1},
//    {"Gain after FM Demod", 0x43, 2, 1, 1},
    {"RF Tx Deviation", 0x40, 0, 0xFFF, 10},
    {"Compress AF Tx Ratio", 0x29, 14, 0b11, 1},
    {"Compress AF Tx 0 dB", 0x29, 7, 0x7F, 1},
    {"Compress AF Tx noise", 0x29, 0, 0x7F, 1},
    {"MIC AGC Disable", 0x19, 15, 1, 1},
    {"AFC Range Select", 0x73, 11, 0b111, 1},
    {"AFC Disable", 0x73, 4, 1, 1},
    {"AFC Speed", 0x73, 5, 0b111111, 1},
//   {"IF step100x", 0x3D, 0, 0xFFFF, 100},
//   {"IF step1x", 0x3D, 0, 0xFFFF, 1},
//   {"RFfiltBW1.7-4.5khz ", 0x43, 12, 0b111, 1},
//   {"RFfiltBWweak1.7-4.5khz", 0x43, 9, 0b111, 1},
//   {"BW Mode Selection", 0x43, 4, 0b11, 1},
//   {"XTAL F Low-16bits", 0x3B, 0, 0xFFFF, 1},
//   {"XTAL F Low-16bits 100", 0x3B, 0, 0xFFFF, 100},
//   {"XTAL F High-8bits", 0x3C, 8, 0xFF, 1},
//   {"XTAL F reserved flt", 0x3C, 0, 0b111111, 1},
//   {"XTAL Enable", 0x37, 1, 1, 1},
//   {"ANA LDO Selection", 0x37, 11, 1, 1},
//   {"VCO LDO Selection", 0x37, 10, 1, 1},
//   {"RF LDO Selection", 0x37, 9, 1, 1},
//   {"PLL LDO Selection", 0x37, 8, 1, 1},
//   {"ANA LDO Bypass", 0x37, 7, 1, 1},
//   {"VCO LDO Bypass", 0x37, 6, 1, 1},
//   {"RF LDO Bypass", 0x37, 5, 1, 1},
//   {"PLL LDO Bypass", 0x37, 4, 1, 1},
//   {"Freq Scan Indicator", 0x0D, 15, 1, 1},
//   {"F Scan High 16 bits", 0x0D, 0, 0xFFFF, 1},
//   {"F Scan Low 16 bits", 0x0E, 0, 0xFFFF, 1},
//   {"AGC fix", 0x7E, 15, 0b1, 1},
//   {"AGC idx", 0x7E, 12, 0b111, 1},
//   {"49", 0x49, 0, 0xFFFF, 100},
//   {"7B", 0x7B, 0, 0xFFFF, 100},
//   {"rssi_rel", 0x65, 8, 0xFF, 1},
//   {"agc_rssi", 0x62, 8, 0xFF, 1},
//   {"lna_peak_rssi", 0x62, 0, 0xFF, 1},
//   {"rssi_sq", 0x67, 0, 0xFF, 1},
//   {"weak_rssi 1", 0x0C, 7, 1, 1},
//   {"ext_lna_gain set", 0x2C, 0, 0b11111, 1},
//   {"snr_out", 0x61, 8, 0xFF, 1},
//   {"noise sq", 0x65, 0, 0xFF, 1},
//   {"glitch", 0x63, 0, 0xFF, 1},
//   {"soft_mute_en 1", 0x20, 12, 1, 1},
//   {"SNR Threshold SoftMut", 0x20, 0, 0b111111, 1},
//   {"soft_mute_atten", 0x20, 6, 0b11, 1},
//   {"soft_mute_rate", 0x20, 8, 0b11, 1},
//   {"Band Selection Thr", 0x3E, 0, 0xFFFF, 100},
//   {"chip_id", 0x00, 0, 0xFFFF, 1},
//   {"rev_id", 0x01, 0, 0xFFFF, 1},
//   {"aerror_en 0am 1fm", 0x30, 9, 1, 1},
//   {"bypass 1tx 0rx", 0x47, 0, 1, 1},
//   {"bypass tx gain 1", 0x47, 1, 1, 1},
//   {"bps afdac 3tx 9rx ", 0x47, 8, 0b1111, 1},
//   {"bps tx dcc=0 ", 0x7E, 3, 0b111, 1},
//   {"audio_tx_mute1", 0x50, 15, 1, 1},
//   {"audio_tx_limit_bypass1", 0x50, 10, 1, 1},
//  {"audio_tx_limit320", 0x50, 0, 0x3FF, 1},
//   {"audio_tx_limit reserved7", 0x50, 11, 0b1111, 1},
//   {"audio_tx_path_sel", 0x2D, 2, 0b11, 1},
//   {"AFTx Filt Bypass All", 0x47, 0, 1, 1},
   {"3kHz AF Resp K Tx", 0x74, 0, 0xFFFF, 100},
//   {"MIC Sensit Tuning", 0x7D, 0, 0b11111, 1},
//   {"DCFiltBWTxMICIn15-480hz", 0x7E, 3, 0b111, 1},
//   {"04 768", 0x04, 0, 0x0300, 1},
//   {"43 32264", 0x43, 0, 0x7E08, 1},
//   {"4b 58434", 0x4b, 0, 0xE442, 1},
//   {"73 22170", 0x73, 0, 0x569A, 1},
//   {"7E 13342", 0x7E, 0, 0x341E, 1},
//   {"47 26432 24896", 0x47, 0, 0x6740, 1},
//   {"03 49662 49137", 0x30, 0, 0xC1FE, 1},
//   {"Enable Compander", 0x31, 3, 1, 1},
//   {"Band-Gap Enable", 0x37, 0, 1, 1},
//   {"IF step100x", 0x3D, 0, 0xFFFF, 100},
//   {"IF step1x", 0x3D, 0, 0xFFFF, 1},
//   {"Band Selection Thr", 0x3E, 0, 0xFFFF, 1},
//   {"RF filt BW ", 0x43, 12, 0b111, 1},
//   {"RF filt BW weak", 0x43, 9, 0b111, 1},
//   {"BW Mode Selection", 0x43, 4, 0b11, 1},
//   {"AF Output Inverse", 0x47, 13, 1, 1},
//   {"AF ALC Disable", 0x4B, 5, 1, 1},
//   {"AGC Fix Mode", 0x7E, 15, 1, 1},
//   {"AGC Fix Index", 0x7E, 12, 0b111, 1},
//   {"Crystal vReg Bit", 0x1A, 12, 0b1111, 1},
//   {"Crystal iBit", 0x1A, 8, 0b1111, 1},
//   {"PLL CP bit", 0x1F, 0, 0b1111, 1},
//   {"PLL/VCO Enable", 0x30, 4, 0xF, 1},
//   {"Exp AF Rx Ratio", 0x28, 14, 0b11, 1},
//   {"Exp AF Rx 0 dB", 0x28, 7, 0x7F, 1},
//   {"Exp AF Rx noise", 0x28, 0, 0x7F, 1},
//   {"OFF AFRxHPF300 flt", 0x2B, 10, 1, 1},
//   {"OFF AF RxLPF3K flt", 0x2B, 9, 1, 1},
//   {"AF Rx Gain1", 0x48, 10, 0x11, 1},
//   {"AF Rx Gain2", 0x48, 4, 0b111111, 1},
//   {"AF DAC G after G1 G2", 0x48, 0, 0b1111, 1},
     {"300Hz AF Resp K Tx", 0x44, 0, 0xFFFF, 100},
     {"300Hz AF Resp K Tx", 0x45, 0, 0xFFFF, 100},
//   {"DC Filt BW Rx IF In", 0x7E, 0, 0b111, 1},
//   {"OFF AFTxHPF300filter", 0x2B, 2, 1, 1},
//   {"OFF AFTxLPF1filter", 0x2B, 1, 1, 1},
//   {"OFF AFTxpre-emp flt", 0x2B, 0, 1, 1},
//   {"PA Gain Enable", 0x30, 3, 1, 1},
//   {"PA Biasoutput 0~3", 0x36, 8, 0xFF, 1},
//   {"PA Gain1 Tuning", 0x36, 3, 0b111, 1},
//   {"PA Gain2 Tuning", 0x36, 0, 0b111, 1},
//   {"RF TxDeviation ON", 0x40, 12, 1, 1},
//   {"AFTxLPF2fltBW1.7-4.5khz", 0x43, 6, 0b111, 1}, 
     {"300Hz AF Resp K Rx", 0x54, 0, 0xFFFF, 100},
     {"300Hz AF Resp K Rx", 0x55, 0, 0xFFFF, 100},
     {"3kHz AF Resp K Rx", 0x75, 0, 0xFFFF, 100},
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
static char osdPopupText[32] = "";

// 
static void ShowOSDPopup(const char *str)
{   osdPopupTimer = osdPopupSetting;
    strncpy(osdPopupText, str, sizeof(osdPopupText)-1);
    osdPopupText[sizeof(osdPopupText)-1] = '\0';  // Zabezpieczenie przed przepełnieniem
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

static void SetState(State state) {
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
  SYSTEM_DelayMs(5);
    Reg |= (1 << 9);
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}

static void SetF(uint32_t f) {
  if (f < 1400000 || f > 130000000) return;
  if (SPECTRUM_PAUSED) return;
  BK4819_SetFrequency(f);
  BK4819_PickRXFilterPathBasedOnFrequency(f);
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
  BK4819_WriteRegister(BK4819_REG_30, 0);
  BK4819_WriteRegister(BK4819_REG_30, reg);
}

static void ResetInterrupts()
{
  // disable interupts
  BK4819_WriteRegister(BK4819_REG_3F, 0);
  // reset the interrupt
  BK4819_WriteRegister(BK4819_REG_02, 0);
}

// scan step in 0.01khz
static uint32_t GetScanStep() { return scanStepValues[settings.scanStepIndex]; }

static uint16_t GetStepsCount() 
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

static uint32_t GetBW() { return GetStepsCount() * GetScanStep(); }

static uint16_t GetRandomChannelFromRSSI(uint16_t maxChannels) {
  uint32_t rssi = rssiHistory[1]*rssiHistory[maxChannels/2];
  if (maxChannels == 0 || rssi == 0) {
        return 1;  // Fallback to chanel 1 if invalid input
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

/////////////////////////////EEPROM://///////////////////////////

static void TrimTrailingChars(char *str) {
    int len = strlen(str);
    while (len > 0) {
        unsigned char c = str[len - 1];
        if (c == '\0' || c == 0x20 || c == 0xFF)  // fin de chaîne, espace, EEPROM vide
            len--;
        else
            break;
    }
    str[len] = '\0';
}


static void ReadChannelName(uint16_t Channel, char *name) {
    EEPROM_ReadBuffer(ADRESS_NAMES + Channel * 16, (uint8_t *)name, 12);
    TrimTrailingChars(name);
}

// *****************************************************************************
// Fonction : Supprime la fréquence sélectionnée de la liste d'historique en RAM
// *****************************************************************************
static void DeleteHistoryItem(void) {
    // Vérification de base
    if (!historyListActive || indexFs == 0) return;
    if (historyListIndex >= indexFs) {
        // L'index est hors limite, on le corrige (au dernier élément valide)
        historyListIndex = (indexFs > 0) ? indexFs - 1 : 0;
        if (indexFs == 0) return;
    }

    uint16_t indexToDelete = historyListIndex;

    // Décaler tous les éléments suivants d'une position vers le haut
    for (uint16_t i = indexToDelete; i < indexFs - 1; i++) {
        HFreqs[i]       = HFreqs[i + 1];
        HCount[i]       = HCount[i + 1];
        HBlacklisted[i] = HBlacklisted[i + 1];
    }
    
    // Réduire le compteur d'éléments dans la liste
    indexFs--;
    
    // Nettoyer la nouvelle dernière entrée et rétablir le marqueur de fin logique
    HFreqs[indexFs]       = 0;
    HCount[indexFs]       = 0;
    HBlacklisted[indexFs] = 0xFF; // Rétablit le marqueur 0xFF pour la cohérence

    // Ajuster l'index de sélection
    // Si nous avons supprimé le dernier élément, la sélection passe au nouvel élément final.
    if (historyListIndex >= indexFs && indexFs > 0) {
        historyListIndex = indexFs - 1;
    } else if (indexFs == 0) {
        historyListIndex = 0;
    }
    
    // Mettre à jour l'affichage
    
    ShowOSDPopup("Deleted");
    
}


#include "settings.h" // Assurez-vous que ce fichier est inclus pour SETTINGS_SaveChannel

static void SaveHistoryToFreeChannel(void) {
    if (!historyListActive) return;

    uint32_t f = HFreqs[historyListIndex];
    if (f < 1000000) return; // Sécurité fréquence invalide

    char str[32];

    // --- ÉTAPE 1 : VÉRIFIER SI LA FRÉQUENCE EXISTE DÉJÀ ---
    for (int i = 0; i < MR_CHANNEL_LAST; i++) {
        uint32_t freqInMem;
        // Lecture des 4 premiers octets du canal (la fréquence)
        EEPROM_ReadBuffer(ADRESS_FREQ_PARAMS + (i * 16), (uint8_t *)&freqInMem, 4);
        
        // Si le canal n'est pas vide (0xFFFFFFFF) et que la fréquence correspond
        if (freqInMem != 0xFFFFFFFF && freqInMem == f) {
            sprintf(str, "Exist CH %d", i + 1);
            
            ShowOSDPopup(str);
            return; // On arrête ici, pas de sauvegarde
        }
    }

    // --- ÉTAPE 2 : CHERCHER UN EMPLACEMENT LIBRE ---
    int freeCh = -1;
    for (int i = 0; i < MR_CHANNEL_LAST; i++) {
        uint8_t checkByte;
        // On vérifie juste le premier octet pour voir si le slot est libre
        EEPROM_ReadBuffer(ADRESS_FREQ_PARAMS + (i * 16), &checkByte, 1);
        if (checkByte == 0xFF) { 
            freeCh = i;
            break;
        }
    }

    // --- ÉTAPE 3 : SAUVEGARDER ---
    
    if (freeCh != -1) {
        VFO_Info_t tempVFO;
        memset(&tempVFO, 0, sizeof(tempVFO)); 

        // Remplissage des paramètres
        tempVFO.freq_config_RX.Frequency = f;
        tempVFO.freq_config_TX.Frequency = f; 
        tempVFO.TX_OFFSET_FREQUENCY = 0;
        
        tempVFO.Modulation = settings.modulationType;
        tempVFO.CHANNEL_BANDWIDTH = settings.listenBw; 

        tempVFO.OUTPUT_POWER = OUTPUT_POWER_LOW;
        tempVFO.STEP_SETTING = STEP_12_5kHz; 
        // Sauvegarde propre via la fonction système
        SETTINGS_SaveChannel(freeCh, &tempVFO, 2);

        // Rafraichir les listes
        LoadValidMemoryChannels();

        sprintf(str, "SAVED TO CH %d", freeCh + 1);
        ShowOSDPopup(str);
    } else {
        ShowOSDPopup("MEMORY FULL");
    }
}

typedef struct HistoryStruct {
    uint32_t HFreqs;
    uint8_t HCount;
    uint8_t HBlacklisted;
} HistoryStruct;


#ifdef ENABLE_EEPROM_512K
static bool historyLoaded = false; // flaga stanu wczytania histotii spectrum

void ReadHistory(void) {
    HistoryStruct History = {0};
    for (uint16_t position = 0; position < HISTORY_SIZE; position++) {
        EEPROM_ReadBuffer(ADRESS_HISTORY + position * sizeof(HistoryStruct),
                          (uint8_t *)&History, sizeof(HistoryStruct));

        // Stop si marque de fin trouvée
        if (History.HBlacklisted == 0xFF) {
            indexFs = position;
            break;
        }
      if (History.HFreqs){
        HFreqs[position] = History.HFreqs;
        HCount[position] = History.HCount;
        HBlacklisted[position] = History.HBlacklisted;
        indexFs = position + 1;
      }
    }
    
    ShowOSDPopup("HISTORY LOADED");
}


void WriteHistory(void) {
    HistoryStruct History = {0};
    for (uint16_t position = 0; position < indexFs; position++) {
        History.HFreqs = HFreqs[position];
        History.HCount = HCount[position];
        History.HBlacklisted = HBlacklisted[position];
        EEPROM_WriteBuffer(ADRESS_HISTORY + position * sizeof(HistoryStruct),
                           (uint8_t *)&History);
    }

    // Marque de fin (HBlacklisted = 0xFF)
    History.HFreqs = 0;
    History.HCount = 0;
    History.HBlacklisted = 0xFF;
    EEPROM_WriteBuffer(ADRESS_HISTORY + indexFs * sizeof(HistoryStruct),
                       (uint8_t *)&History);
    
    ShowOSDPopup("HISTORY SAVED");
}

#endif

/////////////////////////////EEPROM://///////////////////////////*/

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
          uint16_t i = 0;
          SpectrumDelay = 0; //not compatible with ninja

          while (rssiHistory[randomChannel]> 120) //check chanel availability
            {i++;
            randomChannel++;
            if (randomChannel >scanChannelsCount)randomChannel = 1;
            if (i > MR_CHANNEL_LAST) break;}
          rndfreq = gMR_ChannelFrequencyAttributes[scanChannel[randomChannel]].Frequency;
          SETTINGS_SetVfoFrequency(rndfreq);
          gEeprom.MrChannel     = scanChannel[randomChannel];
			    gEeprom.ScreenChannel = scanChannel[randomChannel];
          gTxVfo->Modulation = MODULATION_FM;
          gTxVfo->STEP_SETTING = STEP_0_01kHz;
          gRequestSaveChannel = 1;
          }
      else 
          if (PttEmission ==2){
          SpectrumDelay = 0; //not compatible
          uint16_t ExitCh = BOARD_gMR_fetchChannel(HFreqs[historyListIndex]);
          if (ExitCh == 0xFFFF) { //Not a known channel
              SETTINGS_SetVfoFrequency(HFreqs[historyListIndex]);
              gTxVfo->STEP_SETTING = STEP_0_01kHz;
              gTxVfo->Modulation = MODULATION_FM;
              gTxVfo->OUTPUT_POWER = OUTPUT_POWER_HIGH;
              COMMON_SwitchToVFOMode();
          }
          else {
            gTxVfo->freq_config_RX.Frequency = HFreqs[historyListIndex];
            gEeprom.ScreenChannel = ExitCh;
            gEeprom.MrChannel = ExitCh;
            COMMON_SwitchToChannelMode();
          }
          gRequestSaveChannel = 1;
          }

      DeInitSpectrum(1);
      break;      
    
    default:
      DeInitSpectrum(0);
      break;
  }
    // Additional delay to debounce keys
    SYSTEM_DelayMs(50); //200
    isInitialized = false;
}

static uint8_t GetBWRegValueForScan() {
  return scanStepBWRegValues[settings.scanStepIndex];
}

static uint16_t GetRssi(void) {
    uint16_t rssi;
    //BK4819_ReadRegister(0x63);
    if (isListening) SYSTICK_DelayUs(12000); 
    else SYSTICK_DelayUs(DelayRssi * 1000);
    rssi = BK4819_GetRSSI();
    if (FREQUENCY_GetBand(scanInfo.f) > BAND4_174MHz) {rssi += UHF_NOISE_FLOOR;}
    BK4819_ReadRegister(0x63);
  return rssi;
}

static void ToggleAudio(bool on) {

  audioState = on;
  if (on)
   { SYSTEM_DelayMs(100);
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  } else {
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  }
}

static void FillfreqHistory(void)
{
    uint32_t f = peak.f;
    if (f == 0 || f < 1400000 || f > 130000000) return;

    for (uint16_t i = 0; i < indexFs; i++) {
        if (HFreqs[i] == f) {
            // Gestion du compteur
            if (gCounthistory) {
                if (lastReceivingFreq != f)
                    HCount[i]++;
            } else {
                HCount[i]++;
            }
            lastReceivingFreq = f;
            historyListIndex = i;
            return;
        }
    }
    uint16_t pos = 0;
    while (pos < indexFs && HFreqs[pos] < f) {pos++;}

    for (uint16_t i = indexFs; i > pos; i--) {
        HFreqs[i]       = HFreqs[i - 1];
        HCount[i]       = HCount[i - 1];
        HBlacklisted[i] = HBlacklisted[i - 1];
    }

    HFreqs[pos]       = f;
    HCount[pos]       = 1;
    HBlacklisted[pos] = 0;
    lastReceivingFreq = f;
    historyListIndex = pos;
    if (indexFs < HISTORY_SIZE) indexFs++;
    
    if (indexFs >= HISTORY_SIZE) {indexFs = 0;}
/*     for (uint8_t i = 0; i < indexFs; i++) {
        char str[64];
        sprintf(str, "%d %d %lu\r\n", i, indexFs, HFreqs[i]);
        LogUart(str);
    } */

} 

static void ToggleRX(bool on) {
    if (SPECTRUM_PAUSED) return;
    if(!on && SpectrumMonitor == 2) {isListening = 1;return;}
    isListening = on;

// ────────────────────────────────────────────────
    // SATCOM BOOST — вставляем здесь, в начале ToggleRX
    // ────────────────────────────────────────────────
    uint32_t Frequency = gCurrentVfo->pRX->Frequency;

    if (gEeprom.SATCOM_ENABLE && Frequency >= 24000000 && Frequency <= 28000000)
    {
        // 2 моргания — для визуального подтверждения в спектре
        for (int i = 0; i < 2; i++) {
            GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
            SYSTEM_DelayMs(30);
            GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
            if (i == 0) SYSTEM_DelayMs(50);
        }

        // 1. Принудительный Band 3 (UHF high)
        uint16_t reg44 = BK4819_ReadRegister(0x44);
        reg44 = (reg44 & ~(7u << 13)) | (2u << 13);
        BK4819_WriteRegister(0x44, reg44);

        // 2. LNA Gain = Max (0xF)
        uint16_t reg13 = BK4819_ReadRegister(0x13);
        reg13 = (reg13 & ~(0xFu << 8)) | (0xFu << 8);
        BK4819_WriteRegister(0x13, reg13);
		

        // 3. Mixer Gain = Max (3)
        uint16_t reg10 = BK4819_ReadRegister(0x10);
        reg10 = (reg10 & ~(3u << 3)) | (3u << 3);
        BK4819_WriteRegister(0x10, reg10);

        // 4. IF Gain Max + аттенюатор OFF
        uint16_t reg12 = BK4819_ReadRegister(0x12);
        reg12 &= ~(1u << 6);          // аттенюатор выкл
        reg12 = (reg12 & 0xFFF0) | 0x000F;  // IF gain = 15
        BK4819_WriteRegister(0x12, reg12);

        // Дополнительно: отключить AGC (чтобы не сбросил gain)
        BK4819_WriteRegister(0x13, reg13 & ~(1u << 15));  // AGC OFF (бит 15)

    }


    if (on && isKnownChannel) {
        if(!gForceModulation) settings.modulationType = channelModulation;
        BK4819_InitAGCSpectrum(settings.modulationType);
    }
    else if(on && appMode == SCAN_BAND_MODE) {
            if (!gForceModulation) settings.modulationType = BParams[bl].modulationType;
            BK4819_InitAGCSpectrum(settings.modulationType);
          }
    
    if (on) { 
        BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
        SYSTEM_DelayMs(20);
        RADIO_SetModulation(settings.modulationType);
        BK4819_SetFilterBandwidth(settings.listenBw, false);
        BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_02_CxCSS_TAIL);

    } else { 
        BK4819_SetFilterBandwidth(BK4819_FILTER_BW_WIDE, false); //Scan in 25K bandwidth
        if(appMode!=CHANNEL_MODE) BK4819_WriteRegister(0x43, GetBWRegValueForScan());
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, 0);
    }
    if (on != audioState) {
         SYSTEM_DelayMs(5);
        ToggleAudio(on);
         SYSTEM_DelayMs(5);
        ToggleAFDAC(on);
         SYSTEM_DelayMs(5);
        ToggleAFBit(on);
    }
    
    // Если выключаем RX — возвращаем дефолт (на всякий случай)
    if (!on && gEeprom.SATCOM_ENABLE)
    {
        // Возврат к нормальному режиму (перезаписываем)
        uint16_t reg13 = BK4819_ReadRegister(0x13);
        reg13 = (reg13 & ~(0xFu << 8)) | (0x08u << 8);   // LNA средний
        reg13 |= (1u << 15);                              // AGC ON
        BK4819_WriteRegister(0x13, reg13);

        uint16_t reg10 = BK4819_ReadRegister(0x10);
        reg10 = (reg10 & ~(3u << 3)) | (0x02u << 3);     // Mixer дефолт
        BK4819_WriteRegister(0x10, reg10);

        uint16_t reg12 = BK4819_ReadRegister(0x12);
        reg12 &= ~(1u << 6);                              // аттенюатор авто
        BK4819_WriteRegister(0x12, reg12);
    }
}


static void ResetScanStats() {
  scanInfo.rssiMax = scanInfo.rssiMin + 20 ; 
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
      uint16_t currentChannel = scanChannel[0];
      scanInfo.f = gMR_ChannelFrequencyAttributes[currentChannel].Frequency; 
    }
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

static void UpdateNoiseOff(){
  if( BK4819_GetExNoiseIndicator() > Noislvl_OFF) {gIsPeak = false;ToggleRX(0);}		
}

static void UpdateNoiseOn(){
	if( BK4819_GetExNoiseIndicator() < Noislvl_ON) {gIsPeak = true;ToggleRX(1);}
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
    if (settings.rssiTriggerLevelUp == 50 && rssi > previousRssi + UOO_trigger) {
      peak.f = scanInfo.f;
      peak.i = scanInfo.i;
      FillfreqHistory();
    }

    if (!gIsPeak && rssi > previousRssi + settings.rssiTriggerLevelUp) {
        SYSTEM_DelayMs(10);
        uint16_t rssi2 = scanInfo.rssi = GetRssi();
        if (rssi2 > rssi+10) {
          peak.f = scanInfo.f;
          peak.i = scanInfo.i;
        }
        if (settings.rssiTriggerLevelUp < 50) {gIsPeak = true;}
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

static void UpdateDBMaxAuto() { //Zoom
  static uint8_t z = 3;
  int newDbMax;
    if (scanInfo.rssiMax > 0) {
        newDbMax = clamp(Rssi2DBm(scanInfo.rssiMax), -100, 0);

        if (newDbMax > settings.dbMax + z) {
            settings.dbMax = settings.dbMax + z;   // montée limitée
        } else if (newDbMax < settings.dbMax - z) {
            settings.dbMax = settings.dbMax - z;   // descente limitée
        } else {
            settings.dbMax = newDbMax;              // suivi normal
        }
    }

    if (scanInfo.rssiMin > 0) {
        settings.dbMin = clamp(Rssi2DBm(scanInfo.rssiMin), -160, -110);
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
  BK4819_InitAGCSpectrum(settings.modulationType);
  gForceModulation = 1;
}

static void ToggleListeningBW(bool inc) {
  settings.listenBw = ACTION_NextBandwidth(settings.listenBw, false, inc);
  BK4819_SetFilterBandwidth(settings.listenBw, false);
  
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
  Key_1_pressed = 1;
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
    for (uint16_t i = 0; i < HISTORY_SIZE; i++) {
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
    for (uint16_t i = 0; i < HISTORY_SIZE; i++) {
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
static uint16_t Rssi2PX(uint16_t rssi, uint16_t pxMin, uint16_t pxMax) {
  const int16_t DB_MIN = settings.dbMin << 1;
  const int16_t DB_MAX = settings.dbMax << 1;
  const int16_t DB_RANGE = DB_MAX - DB_MIN;
  const int16_t PX_RANGE = pxMax - pxMin;
  int dbm = clamp(rssi - (160 << 1), DB_MIN, DB_MAX);
  return ((dbm - DB_MIN) * PX_RANGE + DB_RANGE / 2) / DB_RANGE + pxMin;
}

static int16_t Rssi2Y(uint16_t rssi) {
  int delta = ArrowLine*8;
  return DrawingEndY + delta -Rssi2PX(rssi, delta, DrawingEndY);
}

//**********************ПИКИ СПЕКТРА РИСУЕМ С ОТСТУПОМ.*********************** */
static void DrawSpectrum()
{
                  // === ПИКИ СПЕКТРА С ОТСТУПАМИ ПО 4 ПИКСЕЛЯ СЛЕВА И СПРАВА ===
        const uint8_t left_margin  = 3;   // отступ слева
        const uint8_t right_margin = 3;   // отступ справа
        const uint8_t graph_width  = 128 - left_margin - right_margin;  // 120 пикселей

        for (uint8_t i = 0; i < graph_width; ++i) {
            uint8_t x = left_margin + i;  // реальная позиция на экране
            uint16_t rssi = rssiHistory[i];

            if (rssi != RSSI_MAX_VALUE) {
                DrawVLine(Rssi2Y(rssi), DrawingEndY, x, true);
            }
        }
        // === КОНЕЦ ПИКОВ С ОТСТУПАМИ ===    
}

static void RemoveTrailZeros(char *s) {
    char *p;
    if (strchr(s, '.')) {
        p = s + strlen(s) - 1;
        while (p > s && *p == '0') {
            *p-- = '\0';
        }
        if (*p == '.') {
            *p = '\0';
        }
    }
}

//******************************СТАТУСБАР************** */
static void DrawStatus() {
  int len=0;
  int pos=0;

  /*/ === НАСТРАИВАЕМАЯ ПУНКТИРНАЯ ЛИНИЯ В СТАТУСБАРЕ ===
const uint8_t line_y       = 7;     // ← Y позиция линии (0–7)
const uint8_t left_x       = 1;    // ← начало по X (0–127)
const uint8_t right_x      = 127;   // ← конец по X (0–127)
const uint8_t dash_step    = 2;     // ← шаг пунктира (1 = сплошная, 2 = •◦, 4 = •◦◦◦ и т.д.)
const bool    line_white   = true;  // true = белая линия, false = чёрная (на индикаторах)

// Рисуем пунктирную линию
for (uint8_t x = left_x; x <= right_x; x += dash_step) {
    if (line_white) {
        gStatusLine[x] |=  (1 << line_y);   // белая точка
    } else {
        gStatusLine[x] &= ~(1 << line_y);   // чёрная точка (стирает)
    }
}
// === КОНЕЦ ЛИНИИ ===/*/


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

#ifdef ENABLE_IN_BAND
      len = sprintf(&String[pos],"IN ");
#endif

#ifdef ENABLE_FI_BAND
      len = sprintf(&String[pos],"FI ");
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
  if (settings.rssiTriggerLevelUp == 50) len = sprintf(&String[pos],"UOO ");
  else len = sprintf(&String[pos],"U%d ", settings.rssiTriggerLevelUp);
  pos += len;
  
  len = sprintf(&String[pos],"%dms %s %s ", DelayRssi, gModulationStr[settings.modulationType],bwNames[settings.listenBw]);
  pos += len;
  int16_t afcVal = BK4819_GetAFCValue();
  if (afcVal) {
      len = sprintf(&String[pos],"A%+d ", afcVal);
      pos += len;
  }
  GUI_DisplaySmallest(String, 0, 1, true,true);
  BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[gBatteryCheckCounter++ % 4]);

  uint16_t voltage = (gBatteryVoltages[0] + gBatteryVoltages[1] + gBatteryVoltages[2] +
             gBatteryVoltages[3]) /
            4 * 760 / gBatteryCalibration[3];

  unsigned perc = BATTERY_VoltsToPercent(voltage);
  sprintf(String,"%d%%", perc);
  GUI_DisplaySmallest(String, 112, 1, true,true);
}

// ------------------ Frequency string ------------------
static void FormatFrequency(uint32_t f, char *buf, size_t buflen) {
    snprintf(buf, buflen, "%u.%05u", f / 100000, f % 100000);
    RemoveTrailZeros(buf);
}

// ------------------ CSS detection ------------------
static void UpdateCssDetection(void) {
    // Проверяем только когда есть приём сигнала
    if (!isListening && !gIsPeak) {
        StringCode[0] = '\0';  // очищаем, если нет сигнала
        return;
    }

    // Включаем CxCSS детектор
    BK4819_WriteRegister(BK4819_REG_51,
        BK4819_REG_51_ENABLE_CxCSS |
        BK4819_REG_51_AUTO_CDCSS_BW_ENABLE |
        BK4819_REG_51_AUTO_CTCSS_BW_ENABLE |
        (51u << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));

    BK4819_CssScanResult_t scanResult = BK4819_GetCxCSSScanResult(&cdcssFreq, &ctcssFreq);

    if (scanResult == BK4819_CSS_RESULT_CDCSS) {
        uint8_t code = DCS_GetCdcssCode(cdcssFreq);
        if (code != 0xFF) {
            snprintf(StringCode, sizeof(StringCode), "D%03oN", DCS_Options[code]); //субтон цифра
            return;
        }
    } else if (scanResult == BK4819_CSS_RESULT_CTCSS) {
        uint8_t code = DCS_GetCtcssCode(ctcssFreq);
        if (code < ARRAY_SIZE(CTCSS_Options)) {
            snprintf(StringCode, sizeof(StringCode), "%u.%uHz", // субтон аналог Hz
                     CTCSS_Options[code] / 10, CTCSS_Options[code] % 10);
            return;
        }
    }

    // Если ничего не нашли — очищаем
    StringCode[0] = '\0';
}

static void DrawF(uint32_t f) {
    if ((f == 0) || f < 1400000 || f > 130000000) return;
    char freqStr[18];
    //FormatFrequency(f, freqStr, sizeof(freqStr));
    snprintf(freqStr, sizeof(freqStr), "%u.%05u", f / 100000, f % 100000); //последние нули
    UpdateCssDetection(); // субтон новый
    uint16_t channelFd = BOARD_gMR_fetchChannel(f);
    isKnownChannel = (channelFd != 0xFFFF);
    char line1[19] = "";
    char line1b[19] = "";
    char line2[19] = "";
    char line3[32] = "";
    
    sprintf(line1, "%s", freqStr);
    sprintf(line1b, "%s %s", freqStr, StringCode);
    
    // Обновляем имя канала раз в секунду (как было в старом коде)
    if (gNextTimeslice_1s) {
        ReadChannelName(channelFd, channelName);
        gNextTimeslice_1s = 0;
    }

    // line2 — имя списка/бэнда + имя канала (точно как в старом коде, но безопасно)
    char prefix[9] = "";
    if (appMode == SCAN_BAND_MODE) {
        snprintf(prefix, sizeof(prefix), "B%u ", bl + 1);
        if (isListening && isKnownChannel) {
            snprintf(line2, sizeof(line2), "%-3s%s ", prefix, channelName);
    } else {
            snprintf(line2, sizeof(line2), "%-3s%s", prefix, BParams[bl].BandName);
        }
    } else if (appMode == CHANNEL_MODE) {
        if (ScanListNumber[scanInfo.i] && ScanListNumber[scanInfo.i] < 16) {
            snprintf(prefix, sizeof(prefix), "S%d ", ScanListNumber[scanInfo.i]);
            } else {
            snprintf(prefix, sizeof(prefix), "ALL ");
              }
        // Показываем имя канала, если есть
        if (channelName[0] != '\0') {
            snprintf(line2, sizeof(line2), "%-3s%s ", prefix, channelName);
        } else {
            snprintf(line2, sizeof(line2), "%-3s", prefix);  // только префикс, если нет имени
        }
    } else {
        line2[0] = '\0';
    }

    // line3 — логика ровно по твоему описанию
    line3[0] = '\0';
    int pos = 0;

    // 1. Если есть MaxListenTime → показываем только его + End (если есть)
    if (MaxListenTime > 0) {
        pos += sprintf(&line3[pos], "RX%d|%s", spectrumElapsedCount / 1000, labels[IndexMaxLT]);
        
        if (WaitSpectrum > 0) {
            if (WaitSpectrum < 61000) {
                pos += sprintf(&line3[pos], "%d", WaitSpectrum / 1000);
            } else {
                pos += sprintf(&line3[pos], "End OO");
            }
        }
    }
    // 2. Если MaxListenTime НЕ установлен → показываем Rx + End (если есть)
    else {
        pos += sprintf(&line3[pos], "RX%d", spectrumElapsedCount / 1000);
        
        if (WaitSpectrum > 0) {
            if (WaitSpectrum < 61000) {
                pos += sprintf(&line3[pos], "%d", WaitSpectrum / 1000);
            } else {
                pos += sprintf(&line3[pos], "End OO");
            }
        }
    }
    
   
    if (classic) {
            if (ShowLines == 2) {
                UI_DisplayFrequency(line1, 10, 0, 0);  // BIG FREQUENCY
                GUI_DisplaySmallestDark(StringCode, 80, 17, false, false);  // CSS субтон
                GUI_DisplaySmallestDark(line2,      18, 17, false, true);  // имя канала / бэнд / список
                GUI_DisplaySmallestDark	(">", 8, 17, false, false);
                GUI_DisplaySmallestDark	("<", 118, 17, false, false);   
                ArrowLine = 3;
            }

            if (ShowLines == 1) {
                UI_PrintStringSmall(line1b, 1, LCD_WIDTH - 1, 0, 0);  // F + CSS
                UI_PrintStringSmall(line2,  1, LCD_WIDTH - 1, 1, 0);  // SL or BD + Name
                GUI_DisplaySmallestDark(line3, 18,17, false, true);  // таймеры
                GUI_DisplaySmallestDark	(">", 8, 17, false, false);
                GUI_DisplaySmallestDark	("<", 118, 17, false, false);   
                ArrowLine = 3;
            }
    if (Fmax) 
      {
          FormatFrequency(Fmax, freqStr, sizeof(freqStr));
          GUI_DisplaySmallest(freqStr,  50, Bottom_print, false,true);
      }

    } else { //Not Classic — ПРОСТОЙ РЕЖИМ ЛИНИИ

    DrawMeter(4); // положение бара
    // Горизонтальная линия

    //MyDrawShortHLine(11, 0, 6, 1, false);  // верх кор лев
    MyDrawShortHLine(10, 0, 127, 2, false);  // верх кор лев

    MyDrawShortHLine(35, 0, 5, 1, false);  // верх кор лев
    MyDrawShortHLine(35, 122, 127, 1, false);  // верх кор прав

    MyDrawVLine(0,   35, 57, 1);  // левая вертикальная сплошная
    MyDrawVLine(127, 35, 57, 1);  // правая вертикальная сплошная           
    
    // Текст — ВСЕГДА показываем большую частоту
    UI_DisplayFrequency(line1, 10, 2, 0); // большая частота — теперь без условия!

    UI_PrintString(line2, 5, LCD_WIDTH - 1, 5, 8); // имя список
    GUI_DisplaySmallestDark(">",     2, 22, false, false);
    GUI_DisplaySmallestDark("<", 123, 22, false, false);  
    UI_PrintStringSmall(line3,  0, 0, 0, 0);  //таймер 

    // RSSI числом — точно как в истории (маленький шрифт, справа вверху)
    char rssiText[16];
    sprintf(rssiText, "R:%3d", scanInfo.rssi);
    UI_PrintStringSmall(rssiText, 96, 1, 0, 0);  // x=96, y=0, BSmall

    // Субтон отдельно, правее RSSI, тот же шрифт, та же строка y=0
    if (StringCode[0]) {
        UI_PrintStringSmall(StringCode, 50, 1, 0, 0);  // ← подбери 100–110
    }
}
}

static void LookupChannelInfo() {
    gChannel = BOARD_gMR_fetchChannel(peak.f);
    isKnownChannel = gChannel == 0xFFFF ? false : true;
    if (isKnownChannel){LookupChannelModulation();}
  }

static void LookupChannelModulation() {
	  uint8_t tmp;
		uint8_t data[8];

		EEPROM_ReadBuffer(ADRESS_FREQ_PARAMS + gChannel * 16 + 8, data, sizeof(data));

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

//*********************НАЧАЛО И КОНЕЦ ВНИЗУ******************** */
static void DrawNums() {
  if (appMode==CHANNEL_MODE) 
  {
    sprintf(String, "M:%d", scanChannel[0]+1);
    GUI_DisplaySmallest(String, 2, Bottom_print, false, true);

    sprintf(String, "M:%d", scanChannel[GetStepsCount()-1]+1);
    GUI_DisplaySmallest(String, 108, Bottom_print, false, true);
  }
  if(appMode!=CHANNEL_MODE){
    sprintf(String, "%u.%05u", gScanRangeStart / 100000, gScanRangeStart % 100000); //старт %u.%05u
    GUI_DisplaySmallest(String, 2, Bottom_print, false, true);
 
    sprintf(String, "%u.%05u", gScanRangeStop / 100000, gScanRangeStop % 100000); //стоп %u.%05u
    GUI_DisplaySmallest(String, 90, Bottom_print, false, true);
  }
  
/*   if(isBlacklistApplied){
    sprintf(String, "BL");
    GUI_DisplaySmallest(String, 60, Bottom_print, false, true);
  } */
  
}

static void nextFrequency833() {
    if (scanInfo.i % 3 != 1) {
        scanInfo.f += 833;
    } else {
        scanInfo.f += 834;
    }
}

static void NextScanStep() {
    ++scanInfo.i;  
    spectrumElapsedCount = 0;
    if (appMode==CHANNEL_MODE)
    { 
      int currentChannel = scanChannel[scanInfo.i];
      scanInfo.f =  gMR_ChannelFrequencyAttributes[currentChannel].Frequency;
    } 
    else {
          // frequency mode
          if(scanInfo.scanStep==833) nextFrequency833();
          else scanInfo.f += scanInfo.scanStep;
    }
    

}

static uint16_t CountValidHistoryItems() {
    uint16_t count = 0;
    for (uint16_t i = 0; i < HISTORY_SIZE; i++) {if (HFreqs[i]) count++;}
    return count;
}

static void Skip() {
    WaitSpectrum = 0;
    spectrumElapsedCount = 0;
    gIsPeak = false;
    ToggleRX(false);
    NextScanStep();
}

static void SetTrigger50(){
  char triggerText[32];
  if (settings.rssiTriggerLevelUp == 50) {
      sprintf(triggerText, "TRIGGER: 00");
  }
  else {
      sprintf(triggerText, "TRIGGER: %d", settings.rssiTriggerLevelUp);
  }
  ShowOSDPopup(triggerText);
}
static const uint8_t durations[] = {0, 20, 40, 60};

static void OnKeyDown(uint8_t key) {

    //if (!gBacklightCountdown) {BACKLIGHT_TurnOn(); return;}
    BACKLIGHT_TurnOn();
    if (gIsKeylocked) {
        // Seule la touche F (Function) permet de déverrouiller
        if (key == KEY_F) { 
            gIsKeylocked = false;
            ShowOSDPopup("Unlocked");
            gKeylockCountdown = durations[AUTO_KEYLOCK];
            
        }
        else ShowOSDPopup("Unlock:F");
        return;
    } 
    gKeylockCountdown = durations[AUTO_KEYLOCK];
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
     
    if (historyListActive) {
          gHistoryScan = !gHistoryScan;
          if (gHistoryScan) {
              ShowOSDPopup("SCAN HISTORY ON");
              gIsPeak = false; // Force le redémarrage si on était bloqué
              SpectrumMonitor = 0;
          } else {
              ShowOSDPopup("SCAN HISTORY OFF");
          }
          return;     
    }
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
                SpectrumMonitor = 0;
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
#endif
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
                    gForceModulation = 0; //Kolyan request release modulation
                }
                break;
				
        case KEY_EXIT: // Exit scan list selection
                SpectrumMonitor = 0;
                SetState(SPECTRUM); // Return to scanning mode
                ResetModifiers();
                gForceModulation = 0; //Kolyan request release modulation
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
    case KEY_UP: //SCANLIST DETAILS
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
          case KEY_3:
          case KEY_1:
              bool isKey3 = (key == KEY_3);
              switch(parametersSelectedIndex) {//SEE HERE parametersSelectedIndex
                  case 0: // DelayRssi
                      DelayRssi = isKey3 ? 
                                 (DelayRssi >= 12 ? 0 : DelayRssi + 1) :
                                 (DelayRssi <= 0 ? 12 : DelayRssi - 1);
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
                  
                  case 2: 
                      if (isKey3) {
                          IndexMaxLT++;
                          if (IndexMaxLT > LISTEN_STEP_COUNT) IndexMaxLT = 0;
                      } else {
                          if (IndexMaxLT == 0) IndexMaxLT = LISTEN_STEP_COUNT;
                          else IndexMaxLT--;
                      }
                      MaxListenTime = listenSteps[IndexMaxLT];
                      break;
                  
                  case 3: // PttEmission
                      PttEmission = isKey3 ?
                                (PttEmission >= 2 ? 0 : PttEmission + 1) :
                                (PttEmission <= 0 ? 2 : PttEmission - 1);
                      break;
                    
                  case 4: // FreqInput
                  case 5:
                      if (!isKey3) {
                          appMode = SCAN_RANGE_MODE;
                          FreqInput();
                      }
                      break;

                  case 6: // UpdateScanStep
                      UpdateScanStep(isKey3);
                      break;
                    
                  case 7: // ToggleListeningBW
                  case 8: // ToggleModulation
                      if (isKey3 || key == KEY_1) {
                          if (parametersSelectedIndex == 7) {
                              ToggleListeningBW(isKey3 ? 0 : 1);
                          } else {
                              ToggleModulation();
                          }
                      }
                      break;
                  case 9: 
                        if (isKey3) ClearSettings();
                      break;
                  case 10: 
                        Backlight_On_Rx=!Backlight_On_Rx;
                      break;
                  case 11: // gCounthistory
                        gCounthistory=!gCounthistory;
                      break;
                  case 12: // ClearHistory
                        if (isKey3) ClearHistory();
                      break;

                  case 13: // RAM
                      break;
                  case 14: // SpectrumSleepMs
                        if (isKey3) {
                          IndexPS++;
                          if (IndexPS > PS_STEP_COUNT) IndexPS = 0;
                        } else {
                          if (IndexPS == 0) IndexPS = PS_STEP_COUNT;
                          else IndexPS--;
                        }
                        SpectrumSleepMs = PS_Steps[IndexPS];
                      break;
                  case 15: // Noislvl_OFF
                      Noislvl_OFF = isKey3 ? 
                                 (Noislvl_OFF >= 100 ? 30 : Noislvl_OFF + 2) :
                                 (Noislvl_OFF <= 30 ? 100 : Noislvl_OFF - 2);
                      Noislvl_ON = Noislvl_OFF - 10;                      
                      break;
                  case 16: //osdPopupSetting
                      osdPopupSetting = isKey3 ? 
                                 (osdPopupSetting >= 5000 ? 0 : osdPopupSetting + 500) :
                                 (osdPopupSetting <= 0 ? 5000 : osdPopupSetting - 500);
                      break;
                  case 17: // UOO_trigger
                      UOO_trigger = isKey3 ? 
                                 (UOO_trigger >= 50 ? 0 : UOO_trigger + 5) :
                                 (UOO_trigger <= 0 ? 50 : UOO_trigger - 5);
                      break;
                  case 18: // AUTO_KEYLOCK
                      AUTO_KEYLOCK = isKey3 ? 
                                 (AUTO_KEYLOCK > 2 ? 0 : AUTO_KEYLOCK + 1) :
                                 (AUTO_KEYLOCK <= 0 ? 3 : AUTO_KEYLOCK - 1);
                      gKeylockCountdown = durations[AUTO_KEYLOCK];
                      break;
                  case 19: // Gain3D
                      if (isKey3) {
                          if (Gain3D_Index < 7) Gain3D_Index++;
                      } else {
                          if (Gain3D_Index > 0) Gain3D_Index--;
                      }
                      const uint16_t g3d_table[] = {0, 0x2AAB, 0x4924, 0x6800, 0x871C, 0xA666, 0xC5D1, 0xE555};
                      BK4819_WriteRegister(BK4819_REG_3D, g3d_table[Gain3D_Index]);
                      break;
                  
                  case 20: // 2X IF
                      Enable2XIF = !Enable2XIF;
                      uint16_t reg43 = BK4819_ReadRegister(BK4819_REG_43);
                      if (Enable2XIF) reg43 |= (1 << 5);
                      else reg43 &= ~(1 << 5);
                      BK4819_WriteRegister(BK4819_REG_43, reg43);
                      break;
                  case 21: // AF 300 SoundBoost
                      SoundBoost = !SoundBoost;
                      if(SoundBoost){
                            BK4819_WriteRegister(0x54, 0x90D1);   	//default is 0x9009
                            BK4819_WriteRegister(0x55, 0x3271);		//default is 0x31a9
                              BK4819_WriteRegister(0x75, 0xFC13);		//default is 0xF50B, clear is 0xFC13
                            // BK4819_WriteRegister(0x54, 0x9009);   	//default is 0x9009
                            //  BK4819_WriteRegister(0x55, 0x31a9);		//default is 0x31a9 
                      }
                      else {
                        	 BK4819_WriteRegister(0x54, 0x9009);
                             BK4819_WriteRegister(0x55, 0x31a9);
                           BK4819_WriteRegister(0x75, 0xF50B);		//default is 0xF50B
                      }
                      break;
              }
        break;

        case KEY_7:
          SaveSettings(); 
        break;

        case KEY_EXIT: // Exit parameters menu to previous menu/state
          //SaveSettings();
          SetState(SPECTRUM);
          RelaunchScan();
          ResetModifiers();
          if(Key_1_pressed) {
            //Key_1_pressed = 0;
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
          settings.rssiTriggerLevelUp = (settings.rssiTriggerLevelUp >= 50? 0 : settings.rssiTriggerLevelUp + step);
          SPECTRUM_PAUSED = true;
          if (!SpectrumMonitor) Skip();
          SetTrigger50();
          break;
      }

      case KEY_F: {
          int step = (settings.rssiTriggerLevelUp <= 20) ? 1 : 5;
          settings.rssiTriggerLevelUp = (settings.rssiTriggerLevelUp <= 0? 50 : settings.rssiTriggerLevelUp - step);
          SPECTRUM_PAUSED = true;
          if (!SpectrumMonitor) Skip();
          SetTrigger50();
          break;
      }


      case KEY_3:
        if (historyListActive) { DeleteHistoryItem();}
        else {
          ToggleListeningBW(1);
          char bwText[32];
          sprintf(bwText, "BW: %s", bwNames[settings.listenBw]);
          ShowOSDPopup(bwText);
        }
        break;
     
      case KEY_9:
        ToggleModulation();
		    char modText[32];
        sprintf(modText, "MOD: %s", gModulationStr[settings.modulationType]);
        ShowOSDPopup(modText);
        break;

      case KEY_1: //SKIP OR SAVE
        Skip();
        ShowOSDPopup("SKIPPED");  // ← попап при пропуске
        break;
     
     case KEY_7:

        if (historyListActive) {
        #ifdef ENABLE_EEPROM_512K
          WriteHistory();
        #endif
        }
        else {
          SaveSettings(); 
        }
        break;
     
     case KEY_2:
        if (historyListActive) {
            SaveHistoryToFreeChannel();
        } else {
            classic=!classic;
        }
      break;

    case KEY_8://СМЕНА РЕЖИМА
      if (historyListActive) {
          memset(HFreqs,0,sizeof(HFreqs));
          memset(HCount,0,sizeof(HCount));
          memset(HBlacklisted,0,sizeof(HBlacklisted));
          historyListIndex = 0;
          historyScrollOffset = 0;
          indexFs = 0;
          SpectrumMonitor = 0;
      } else {
         // ShowLines++; 
          //if (ShowLines > 3) ShowLines = 1; // 3 режима
          ShowLines = (ShowLines == 1) ? 2 : 1;  // переключает только между 1 и 2
      }
    break;

      
    case KEY_UP: //History
      if (historyListActive) {
        uint16_t count = CountValidHistoryItems();
        SpectrumMonitor = 1; //Auto FL when moving in history
        if (!count) return;
        if (historyListIndex == 0) {
            historyListIndex = count - 1;
            if (count > MAX_VISIBLE_LINES)
                historyScrollOffset = count - MAX_VISIBLE_LINES;
            else
                historyScrollOffset = 0;
        } else {
            historyListIndex--;
        }

        if (historyListIndex < historyScrollOffset) {
            historyScrollOffset = historyListIndex;
        }
        SetF(HFreqs[historyListIndex]);
      } else {
        if (appMode==SCAN_BAND_MODE) {
            ToggleScanList(bl, 1);
            settings.bandEnabled[bl+1]= true;
            RelaunchScan(); 
            break;
        }
        else if(appMode==FREQUENCY_MODE) {UpdateCurrentFreq(true);}
        //else if (ShowLines > 4) {historyListIndex = (historyListIndex >0 ? historyListIndex-1 : 0);}
        else if(appMode==CHANNEL_MODE){
              BuildValidScanListIndices();
              scanListSelectedIndex = (scanListSelectedIndex < validScanListCount ? scanListSelectedIndex+1 : 0);
              ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
              SetState(SPECTRUM);
              ResetModifiers();
              break;
        }
        else if(appMode==SCAN_RANGE_MODE){
              uint32_t RangeStep = gScanRangeStop - gScanRangeStart;
              gScanRangeStop  += RangeStep;
              gScanRangeStart += RangeStep;
              RelaunchScan();
              break;
      }
    }
    break;
  case KEY_DOWN: //History
      if (historyListActive) {
        uint16_t count = CountValidHistoryItems();
        SpectrumMonitor = 1; //Auto FL when moving in history
        if (!count) return;
        historyListIndex++;
        if (historyListIndex >= count) {
            historyListIndex = 0;
            historyScrollOffset = 0;
        }
        if (historyListIndex >= historyScrollOffset + MAX_VISIBLE_LINES) {
            historyScrollOffset = historyListIndex - MAX_VISIBLE_LINES + 1;
        }
        SetF(HFreqs[historyListIndex]);
    } else {
        if (appMode==SCAN_BAND_MODE) {
            ToggleScanList(bl, 1);
            settings.bandEnabled[bl-1]= true;
            RelaunchScan(); 
            break;
        }
        else if(appMode==FREQUENCY_MODE){UpdateCurrentFreq(false);}
        //else if (ShowLines > 4) {if (historyListIndex < HISTORY_SIZE) historyListIndex++;}
        else if(appMode==CHANNEL_MODE){
            BuildValidScanListIndices();
            scanListSelectedIndex = (scanListSelectedIndex < 1 ? validScanListCount-1:scanListSelectedIndex-1);
            ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
            SetState(SPECTRUM);
            ResetModifiers();
            break;
        }
        else if(appMode==SCAN_RANGE_MODE){
            uint32_t RangeStep = gScanRangeStop - gScanRangeStart;
            gScanRangeStop  -= RangeStep;
            gScanRangeStart -= RangeStep;
            RelaunchScan();
            break;
      }
    }
  break;
  
  case KEY_4:
    if (appMode!=SCAN_RANGE_MODE){ToggleStepsCount();}
    break;

  case KEY_0:
    if (!historyListActive) {
        historyListActive = true;
        historyListIndex = 0;
        historyScrollOffset = 0;
        prevSpectrumMonitor = SpectrumMonitor;
        }
    break;
  
/* next mode poprawione */ //СМЕНА РЕЖИМОВ
     case KEY_6:
        // Смена режимов спектра: 0 = FR, 1 = SL, 2 = BD, 3 = RG (4 режима)
        Spectrum_state++;
        
        // Цикл по 4 режимам (0 → 1 → 2 → 3 → 0)
        if (Spectrum_state > 3) {
            Spectrum_state = 0;
        }

        gRequestedSpectrumState = Spectrum_state;
        gSpectrumChangeRequested = true;

        // Полный сброс состояния спектра при смене режима
        isInitialized = false;
        spectrumElapsedCount = 0;
        WaitSpectrum = 0;
        gIsPeak = false;
        SPECTRUM_PAUSED = false;
        SpectrumPauseCount = 0;
        newScanStart = true;
        ToggleRX(false);  // выкл RX на время смены

          // обновляем индикатор режима
        break;
  
    case KEY_SIDE1:
        if (SPECTRUM_PAUSED) return;
        SpectrumMonitor++;
        if (SpectrumMonitor > 2) SpectrumMonitor = 0; // 0 normal, 1 Freq lock, 2 Monitor
		    char monitorText[32];
        const char* modes[] = {"NORMAL", "FREQ LOCK", "MONITOR"};
        sprintf(monitorText, "MODE: %s", modes[SpectrumMonitor]);
	      ShowOSDPopup(monitorText);
        if(SpectrumMonitor == 2) ToggleRX(1);
    break;

    case KEY_SIDE2:
    if (historyListActive) {
        HBlacklisted[historyListIndex] = !HBlacklisted[historyListIndex];
        if (HBlacklisted[historyListIndex]) {
            ShowOSDPopup("BL ADDED");
        } else {
            ShowOSDPopup("BL REMOVED");
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
    ShowOSDPopup("BL ADD");  // ← попап при добавлении в BL
    break;

  case KEY_PTT:
      ExitAndCopyToVfo();
      break;
  
  case KEY_MENU: //History
      uint16_t validCount = 0;
      for (uint16_t k = 1; k < HISTORY_SIZE; k++) {
          if (HFreqs[k]) {validCount++;}
      }
      if (historyListActive == true) {Last_Tuned_Freq = HFreqs[historyListIndex];}
      SetState(STILL);      
  break;

  case KEY_EXIT: //exit from history
  
    if (historyListActive == true) {
      gHistoryScan = false;
      SetState(SPECTRUM);
      historyListActive = false;
      SpectrumMonitor = prevSpectrumMonitor;
      SetF(scanInfo.f);
      break;
    }

    if (WaitSpectrum) {WaitSpectrum = 0;} //STOP wait
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
  case KEY_EXIT: //EXIT from freq input
    if (freqInputIndex == 0) {
      SetState(previousState);
      WaitSpectrum = 0;
      break;
    }
    UpdateFreqInput(key);
    break;
  case KEY_MENU: //OnKeyDownFreqInput
    if (tempFreq < RX_freq_min() || tempFreq > F_MAX) {
      break;
    }
    SetState(previousState);
    if (currentState == SPECTRUM) {
        currentFreq = tempFreq;
      ResetModifiers();
    }
    if (currentState == PARAMETERS_SELECT && parametersSelectedIndex == 4)
        gScanRangeStart = tempFreq;
    if (currentState == PARAMETERS_SELECT && parametersSelectedIndex == 5)
        gScanRangeStop = tempFreq;
    if(gScanRangeStart > gScanRangeStop)
		    SWAP(gScanRangeStart, gScanRangeStop);
    break;
  default:
    break;
  }
}

static void OnKeyDownStill(KEY_Code_t key) {
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
        //FreqInput();
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
      case KEY_EXIT: //EXIT from regs
        if (stillEditRegs) {
          stillEditRegs = false;
        break;
        }
        SetState(SPECTRUM);
        WaitSpectrum = 0; //Prevent coming back to still directly
        
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

// Преобразование dBm → RSSI (обратное к Rssi2DBm)
static uint16_t DBm2Rssi(int dbm)
{
    // Формула из оригинального кода: dbm ≈ rssi - 160 (примерно)
    // Мы делаем обратное: rssi ≈ dbm + 160
    // Ограничиваем в диапазоне 0..255 (как реальное RSSI от BK4819)
    int rssi = dbm + 160;
    if (rssi < 0) rssi = 0;
    if (rssi > 255) rssi = 255;
    return (uint16_t)rssi;
}

// === УНИВЕРСАЛЬНЫЕ ФУНКЦИИ ДЛЯ ЛИНИЙ (не конфликтуют с оригинальной DrawVLine) ===
static void MyDrawHLine(uint8_t y, bool white)
{
    if (y >= 64) return;
    uint8_t byte_idx = y / 8;
    uint8_t bit_mask = 1U << (y % 8);
    for (uint8_t x = 0; x < 128; x++) {
        if (white) {
            gFrameBuffer[byte_idx][x] &= ~bit_mask;  // белая
        } else {
            gFrameBuffer[byte_idx][x] |= bit_mask;   // чёрная
        }
    }
}

// Короткая горизонтальная пунктирная линия
static void MyDrawShortHLine(uint8_t y, uint8_t x_start, uint8_t x_end, uint8_t step, bool white)
{
    if (y >= 64 || x_start >= x_end || x_end > 127) return;
    uint8_t byte_idx = y / 8;
    uint8_t bit_mask = 1U << (y % 8);

    for (uint8_t x = x_start; x <= x_end; x++) {
        if (step > 1 && (x % step) != 0) continue;  // пунктир

        if (white) {
            gFrameBuffer[byte_idx][x] &= ~bit_mask;  // белая
        } else {
            gFrameBuffer[byte_idx][x] |= bit_mask;   // чёрная
        }
    }
}

static void MyDrawVLine(uint8_t x, uint8_t y_start, uint8_t y_end, uint8_t step)
{
    if (x >= 128) return;
    for (uint8_t y = y_start; y <= y_end && y < 64; y++) {
        if (step > 1 && (y % step) != 0) continue;  // пунктир
        uint8_t byte_idx = y / 8;
        uint8_t bit_mask = 1U << (y % 8);
        gFrameBuffer[byte_idx][x] |= bit_mask;  // чёрная (для белой сделай отдельно или параметр)
    }
}

// === ТВОЯ РАМКА + ДОПОЛНИТЕЛЬНЫЕ ЛИНИИ ===
static void MyDrawFrameLines(void)
{
    // Основная рамка
    MyDrawHLine(50, true);   // белая горизонтальная на y=50
    MyDrawHLine(49, false);  // чёрная горизонтальная на y=49

    MyDrawVLine(0,   21, 49, 1);  // левая вертикальная сплошная
    MyDrawVLine(127, 21, 49, 1);  // правая вертикальная сплошная

    MyDrawVLine(0,   0, 17, 1);  // левая вертикальная сплошная
    MyDrawVLine(127, 0, 17, 1);  // правая вертикальная сплошная

    //MyDrawHLine(19, false);                 
    MyDrawShortHLine(0, 0, 3, 1, false);  // верх кор лев
    MyDrawShortHLine(0, 4, 8, 2, false);  // верх кор лев
    MyDrawShortHLine(0, 124, 127, 1, false);  // верх кор прав
    MyDrawShortHLine(0, 118, 123, 2, false);  // верх кор прав
    MyDrawShortHLine(17, 0, 10, 1, false);  // верх кор лев
    MyDrawShortHLine(21, 0, 10, 1, false);  // верх кор лев
    //MyDrawShortHLine(21, 11, 119, 2, false);  // центр длин
    MyDrawShortHLine(19, 11, 119, 2, false);  // центр длин
    MyDrawShortHLine(21, 120, 127, 1, false);  // кор прав
    MyDrawShortHLine(17, 120, 127, 1, false);  // кор прав
   //MyDrawHLine(30, true);                   // белая горизонтальная на y=30


    //MyDrawShortHLine(10, 20, 108, 4, false);  // Короткая чёрная пунктирная вверху (от x=20 до x=108, шаг 4)
    //MyDrawVLine(64, 0, 63, 1);               // вертикальная в центре сплошная
    //MyDrawVLine(32, 0, 63, 4);               // вертикальная пунктирная слева (шаг 4)
    //MyDrawVLine(96, 0, 63, 3);               // вертикальная пунктирная справа (шаг 3)

    // Добавь свои:
    // MyDrawHLine(15, false);  // новая горизонтальная
    // MyDrawVLine(48, 10, 50, 2);  // новая пунктирная вертикальная
    // === КОНЕЦ ===
}


static void RenderSpectrum()
{
    if (classic) {
        DrawNums();
        UpdateDBMaxAuto();
        DrawSpectrum();

 // === ЛИНИИ С ОТСТУПОМ ПО 4 ПИКСЕЛЯ (только линии, тики от края) ===
const int LEFT_MARGIN  = 4;
const int RIGHT_MARGIN = 4;

const int lineLevels[] = { -60, -40};   // твои уровни (меняй)
const int numLineLevels = ARRAY_SIZE(lineLevels);
const int dashStep = 2;                     // шаг пунктира

const int tickLevels[] = {-20, -50, -70, -100};  // тики (остаются от края)
const int numTickLevels = ARRAY_SIZE(tickLevels);
const int tickLength = 3;

int i, y, x;

// ПУНКТИРНЫЕ ЛИНИИ (с отступом 4 пикселя слева и справа)
for (i = 0; i < numLineLevels; i++) {
    y = Rssi2Y(DBm2Rssi(lineLevels[i]));
    if (y < 8 || y > DrawingEndY) continue;

    // Чёрная пунктирная линия (с отступами)
    for (x = LEFT_MARGIN; x < 128 - RIGHT_MARGIN; x += dashStep) {
        PutPixel(x, y, true);   // чёрный
    }

    // Белая пунктирная линия (сдвиг на 1, с отступами)
    for (x = LEFT_MARGIN + 1; x < 128 - RIGHT_MARGIN; x += dashStep) {
        PutPixel(x, y, false);  // белый
    }
}

// ТИКИ (остаются от самого края — без отступа)
for (i = 0; i < numTickLevels; i++) {
    y = Rssi2Y(DBm2Rssi(tickLevels[i]));
    if (y < 8 || y > DrawingEndY) continue;

    // Левый тик (от x=0)
    for (x = 0; x < tickLength; x++) {
        PutPixel(x, y, true);
    }

    // Правый тик (до x=127)
    for (x = 0; x < tickLength; x++) {
        PutPixel(127 - x, y, true);
    }
}
// === КОНЕЦ СЕТКИ ===
MyDrawFrameLines();

        
  }

    if(isListening) {
      DrawF(peak.f);
    }
    else {
      //if (SpectrumMonitor) DrawF(HFreqs[historyListIndex]);
      //else 
      DrawF(scanInfo.f);
    }
}


// ВЫВОД БАРА В ПРОСТОМ РЕЖИМЕ — теперь высота 6 пикселей (убрали по 1 сверху и снизу)
static void DrawMeter(int line) {
    const uint8_t METER_PAD_LEFT = 7;
    const uint8_t NUM_SQUARES    = 23;          // чуть короче, чтобы точно влез
    const uint8_t SQUARE_SIZE    = 4;
    const uint8_t SQUARE_GAP     = 1;
    const uint8_t Y_START_BIT    = 2;

    settings.dbMax = 30; 
    settings.dbMin = -100;

    uint8_t max_width_px = NUM_SQUARES * (SQUARE_SIZE + SQUARE_GAP) - SQUARE_GAP;
    uint8_t fill_px      = Rssi2PX(scanInfo.rssi, 0, max_width_px);
    uint8_t fill_count   = fill_px / (SQUARE_SIZE + SQUARE_GAP);

    // Очистка строки
    for (uint8_t px = 0; px < 128; px++) {
        gFrameBuffer[line][px] = 0;
    }

    // Рисуем все квадратики с обводкой
    for (uint8_t sq = 0; sq < NUM_SQUARES; sq++) {
        uint8_t x_left  = METER_PAD_LEFT + sq * (SQUARE_SIZE + SQUARE_GAP);
        uint8_t x_right = x_left + SQUARE_SIZE - 1;

        if (x_right >= 128) break;

        // Верх и низ
        for (uint8_t x = x_left; x <= x_right; x++) {
            gFrameBuffer[line][x] |= (1 << Y_START_BIT);
            gFrameBuffer[line][x] |= (1 << (Y_START_BIT + SQUARE_SIZE - 1));
        }

        // Лево и право
        for (uint8_t bit = Y_START_BIT; bit < Y_START_BIT + SQUARE_SIZE; bit++) {
            gFrameBuffer[line][x_left]  |= (1 << bit);
            gFrameBuffer[line][x_right] |= (1 << bit);
        }
    }

    // Заполняем активные квадратики
    for (uint8_t sq = 0; sq < fill_count; sq++) {
        uint8_t x_left  = METER_PAD_LEFT + sq * (SQUARE_SIZE + SQUARE_GAP);
        uint8_t x_right = x_left + SQUARE_SIZE - 1;

        if (x_right >= 128) break;

        for (uint8_t x = x_left; x <= x_right; x++) {
            for (uint8_t bit = Y_START_BIT; bit < Y_START_BIT + SQUARE_SIZE; bit++) {
                gFrameBuffer[line][x] |= (1 << bit);
            }
        }
    }
}
//*******************подробный режим */
static void RenderStill() {
  classic=1;
  char freqStr[18];
  //if (SpectrumMonitor) FormatFrequency(HFreqs[historyListIndex], freqStr, sizeof(freqStr));
  //else
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

static void HandleUserInput(void) {
    kbd.prev = kbd.current;
    kbd.current = GetKey();
    // ---- Anti-rebond + répétition ----
    if (kbd.current != KEY_INVALID && kbd.current == kbd.prev) {
        kbd.counter++;
    } else {
          kbd.counter = 0;
      }

if (kbd.counter == 2 || (kbd.counter > 22 && (kbd.counter % 20 == 0))) {
       
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
            case SCANLIST_CHANNELS:
                OnKeyDown(kbd.current);
                break;
        #endif
        }
    }
}

static void NextHistoryScanStep() {
    uint16_t count = CountValidHistoryItems();
    if (count == 0) return;

    uint16_t start = historyListIndex;
    
    // Boucle pour trouver le prochain élément non blacklisté
    do {
        historyListIndex++;
        if (historyListIndex >= count) historyListIndex = 0;
        
        // Sécurité : si on a fait un tour complet (tout est blacklisté), on s'arrête
        if (historyListIndex == start && HBlacklisted[historyListIndex]) return;
        
    } while (HBlacklisted[historyListIndex]);

    // Mise à jour de l'affichage (scroll) pour suivre le curseur
    if (historyListIndex < historyScrollOffset) {
        historyScrollOffset = historyListIndex;
    } else if (historyListIndex >= historyScrollOffset + 6) { // 6 = MAX_VISIBLE_LINES
        historyScrollOffset = historyListIndex - 6 + 1;
    }

    // Mise à jour de la fréquence pour le prochain cycle de mesure
    scanInfo.f = HFreqs[historyListIndex];
    
    // Reset du compteur de temps pour la logique de pause
    spectrumElapsedCount = 0;
}


static void UpdateScan() {
  if(SPECTRUM_PAUSED || gIsPeak || SpectrumMonitor || WaitSpectrum) return;

  SetF(scanInfo.f);
  Measure();
  
  // Si un signal est trouvé (gIsPeak), la fonction s'arrête ici grâce au return ci-dessus
  // au prochain passage (via UpdateListening).
  if(gIsPeak || SpectrumMonitor || WaitSpectrum) return;

  // --- MODIFICATION ICI ---
  if (gHistoryScan && historyListActive) {
      NextHistoryScanStep();
      return;
  }
  // ------------------------

  if (scanInfo.i < GetStepsCount()) {
    NextScanStep();
    return;
  }
  
  // Scan end
  newScanStart = true; 
  Fmax = peak.f;
  
  if (SpectrumSleepMs) {
      BK4819_Sleep();
      BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);
      SPECTRUM_PAUSED = true;
      SpectrumPauseCount = SpectrumSleepMs;
  }
}


static void UpdateListening(void) { // called every 10ms
    static uint32_t stableFreq = 1;
    static uint16_t stableCount = 0;
    uint16_t rssi = GetRssi();
    scanInfo.rssi = rssi;
    uint16_t count = GetStepsCount() + 1;
    uint16_t i = peak.i;

    // --- Mise à jour du buffer RSSI ---
    if (count > 128) {
        uint16_t pixel = (uint32_t)i * 128 / count;
        rssiHistory[pixel] = rssi;
    } else {
        uint16_t j;
        uint16_t base = 128 / count;
        uint16_t rem  = 128 % count;
        uint16_t startIndex = i * base + (i < rem ? i : rem);
        uint16_t width      = base + (i < rem ? 1 : 0);
        uint16_t endIndex   = startIndex + width;
        for (j = startIndex; j < endIndex; ++j) {
            rssiHistory[j] = rssi;
        }
    }

    // Détection de fréquence stable
    if (peak.f == stableFreq) {
        if (++stableCount >= 15) {  // ~500ms CHANGE 50 TO 15
            if (!SpectrumMonitor) FillfreqHistory();
            stableCount = 0;
            if (gEeprom.BACKLIGHT_MAX > 5)
                BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, 1);
            if(Backlight_On_Rx) BACKLIGHT_TurnOn();
        }
    } else {
        stableFreq = peak.f;
        stableCount = 0;
    }
    
    if (isListening || SpectrumMonitor || WaitSpectrum) {UpdateNoiseOff();}
    UpdateNoiseOn();
        
    spectrumElapsedCount+=10; //in ms
    uint32_t maxCount = (uint32_t)MaxListenTime * 1000;

    if (MaxListenTime && spectrumElapsedCount >= maxCount) {
        // délai max atteint → reset
        ToggleRX(false);
        Skip();
        return;
    }

    // --- Gestion du pic ---
    if (gIsPeak) {
        WaitSpectrum = SpectrumDelay;   // reset timer
        return;
    }

    if (WaitSpectrum > 61000)
        return;

    if (WaitSpectrum > 10) {
        WaitSpectrum -= 10;
        return;
    }
    // timer écoulé
    WaitSpectrum = 0;
    ResetScanStats();
}

static void Tick() {
  if (gNextTimeslice_500ms) {
    if (gBacklightCountdown > 0)
      if (--gBacklightCountdown == 0)	BACKLIGHT_TurnOff();
    gNextTimeslice_500ms = false;
    
    if (gKeylockCountdown > 0) {gKeylockCountdown--;}
    if (AUTO_KEYLOCK && !gKeylockCountdown) {
      if (!gIsKeylocked) ShowOSDPopup("Locked"); 
      gIsKeylocked = true;
	  }
  }

  if (gNextTimeslice_10ms) {
    HandleUserInput();
    gNextTimeslice_10ms = 0;
    if (isListening || SpectrumMonitor || WaitSpectrum) UpdateListening(); 
    if(SpectrumPauseCount) SpectrumPauseCount--;
    if (osdPopupTimer > 0) {
        UI_DisplayPopup(osdPopupText);  // Wyświetl aktualny tekst
        ST7565_BlitFullScreen();
        osdPopupTimer -= 10; //10
        if (osdPopupTimer <= 0) {osdPopupText[0] = '\0';}
        return;
    }

  }

  if (SPECTRUM_PAUSED && (SpectrumPauseCount == 0)) {
      // fin de la pause
      SPECTRUM_PAUSED = false;
      
      BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
      BK4819_RX_TurnOn(); //Wake up
      SYSTEM_DelayMs(10);
  }

  if(!isListening && gIsPeak && !SpectrumMonitor && !SPECTRUM_PAUSED) {
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


void APP_RunSpectrum(uint8_t Spectrum_state)
{
    for (;;) {
        Mode mode;
        if      (Spectrum_state == 4) mode = FREQUENCY_MODE ;
        else if (Spectrum_state == 3) mode = SCAN_RANGE_MODE ;
        else if (Spectrum_state == 2) mode = SCAN_BAND_MODE ;
        else if (Spectrum_state == 1) mode = CHANNEL_MODE ;
        else mode = FREQUENCY_MODE;
        //BK4819_SetFilterBandwidth(BK4819_FILTER_BW_NARROW, false);  // принудительно узкий в спектре ЧИНИМ ВФО
        EEPROM_WriteBuffer(0x1D00, &Spectrum_state);
        if (!Key_1_pressed) LoadSettings(0);
        appMode = mode;
        ResetModifiers();
        if (appMode==CHANNEL_MODE) LoadValidMemoryChannels();
        if (appMode==FREQUENCY_MODE && !Key_1_pressed) {
            currentFreq = gTxVfo->pRX->Frequency;
            gScanRangeStart = currentFreq - (GetBW() >> 1);
            gScanRangeStop  = currentFreq + (GetBW() >> 1);
        }
        Key_1_pressed = 0;
        BackupRegisters();
        uint8_t CodeType = gTxVfo->pRX->CodeType;
        uint8_t Code     = gTxVfo->pRX->Code;
        BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(CodeType, Code));
        ResetInterrupts();
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
        isListening = true;
        newScanStart = true;
        AutoAdjustFreqChangeStep();
        RelaunchScan();
        for (int i = 0; i < 128; ++i) { rssiHistory[i] = 0; }
        isInitialized = true;
        historyListActive = false;

        while (isInitialized) {
            Tick();
        }

        if (gSpectrumChangeRequested) {
            Spectrum_state = gRequestedSpectrumState;
            gSpectrumChangeRequested = false;
            RestoreRegisters(); 
            continue;
        } else {
            RestoreRegisters();
            break;
        }
    } 
}

static void LoadValidMemoryChannels(void)
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
      int16_t channelIndex= -1;
      for(uint16_t i=0; i < listChannelsCount; i++)
      {
        uint16_t nextChannel;
        nextChannel = RADIO_FindNextChannel(channelIndex+1, 1, listsEnabled, CurrentScanList-1);
        
        if (nextChannel == 0xFFFF) {break;}
        else
        {
          channelIndex = nextChannel;
          scanChannel[offset+i]=channelIndex;
          //char str[64] = "";sprintf(str, "%d %d %d %d \r\n", scanChannelsCount,offset,i,channelIndex);LogUart(str);
		
          ScanListNumber[offset+i]=CurrentScanList;
      
        }
      }
    }
  }

static void ToggleScanList(int scanListNumber, int single )
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


#include "index.h"

bool IsVersionMatching(void) {
    uint16_t stored,app_version;
    app_version = APP_VERSION;
    EEPROM_ReadBuffer(0x1D08, &stored, 2);
    if (stored != APP_VERSION) EEPROM_WriteBuffer(0x1D08, &app_version);
    return (stored == APP_VERSION);
}


typedef struct {
    // Block 1 (0x1D10 - 0x1D1F) 240 bytes max
    int ShowLines;
    uint8_t DelayRssi;
    uint8_t PttEmission; 
    uint8_t listenBw;
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
    uint16_t R10;
    uint16_t R11;
    uint16_t R12;
    uint16_t R13;
    uint16_t R14;
    uint16_t R3C;
    uint16_t R43;
    uint16_t R2B;
    uint16_t SpectrumDelay;
    uint8_t IndexMaxLT;
    uint8_t IndexPS;
    uint8_t Noislvl_OFF;
    uint16_t UOO_trigger;
    uint16_t osdPopupSetting;
    bool Backlight_On_Rx;
} SettingsEEPROM;


void LoadSettings(bool LNA)
{
  SettingsEEPROM  eepromData  = {0};
  EEPROM_ReadBuffer(0x1D10, &eepromData, sizeof(eepromData));
  
  BK4819_WriteRegister(BK4819_REG_10, eepromData.R10);
  BK4819_WriteRegister(BK4819_REG_11, eepromData.R11);
  BK4819_WriteRegister(BK4819_REG_12, eepromData.R12);
  BK4819_WriteRegister(BK4819_REG_13, eepromData.R13);
  BK4819_WriteRegister(BK4819_REG_14, eepromData.R14);
  if (LNA) return;
  if(!IsVersionMatching()) ClearSettings();
  for (int i = 0; i < 15; i++) {
    settings.scanListEnabled[i] = (eepromData.scanListFlags >> i) & 0x01;
  }
  settings.rssiTriggerLevelUp = eepromData.Trigger;
  settings.listenBw = eepromData.listenBw;
  BK4819_SetFilterBandwidth(settings.listenBw, false);
  if (eepromData.RangeStart >= 1400000) gScanRangeStart = eepromData.RangeStart;
  if (eepromData.RangeStop >= 1400000) gScanRangeStop = eepromData.RangeStop;
  settings.scanStepIndex = eepromData.scanStepIndex;
  for (int i = 0; i < 32; i++) {
      settings.bandEnabled[i] = (eepromData.bandListFlags >> i) & 0x01;
    }
  DelayRssi = eepromData.DelayRssi;
  if (DelayRssi > 12) DelayRssi =12;
  if (PttEmission > 1) PttEmission =0;
  PttEmission = eepromData.PttEmission;
  validScanListCount = 0;
  ShowLines = eepromData.ShowLines;
  SpectrumDelay = eepromData.SpectrumDelay;
  
  IndexMaxLT = eepromData.IndexMaxLT;
  MaxListenTime = listenSteps[IndexMaxLT];
  
  IndexPS = eepromData.IndexPS;
  SpectrumSleepMs = PS_Steps[IndexPS];
  Noislvl_OFF = eepromData.Noislvl_OFF;
  Noislvl_ON  = Noislvl_OFF-10; 
  UOO_trigger = eepromData.UOO_trigger;
  osdPopupSetting = eepromData.osdPopupSetting;
  Backlight_On_Rx = eepromData.Backlight_On_Rx;
  ChannelAttributes_t att;
  for (int i = 0; i < MR_CHANNEL_LAST+1; i++) {
    att = gMR_ChannelAttributes[i];
    if (att.scanlist > validScanListCount) {validScanListCount = att.scanlist;}
  }
  BK4819_WriteRegister(BK4819_REG_40, eepromData.R40);
  BK4819_WriteRegister(BK4819_REG_29, eepromData.R29);
  BK4819_WriteRegister(BK4819_REG_19, eepromData.R19);
  BK4819_WriteRegister(BK4819_REG_73, eepromData.R73);
  BK4819_WriteRegister(BK4819_REG_3C, eepromData.R3C);
  BK4819_WriteRegister(BK4819_REG_43, eepromData.R43);
  BK4819_WriteRegister(BK4819_REG_2B, eepromData.R2B);
  
  
#ifdef ENABLE_EEPROM_512K
  if (!historyLoaded) {
     ReadHistory();
     historyLoaded = true;
  }
#endif
}

static void SaveSettings() 
{
  SettingsEEPROM  eepromData  = {0};
  for (int i = 0; i < 15; i++) {
    if (settings.scanListEnabled[i]) eepromData.scanListFlags |= (1 << i);
  }
  eepromData.Trigger = settings.rssiTriggerLevelUp;
  eepromData.listenBw = settings.listenBw;
  eepromData.RangeStart = gScanRangeStart;
  eepromData.RangeStop = gScanRangeStop;
  eepromData.DelayRssi = DelayRssi;
  eepromData.PttEmission = PttEmission;
  eepromData.scanStepIndex = settings.scanStepIndex;
  eepromData.ShowLines = ShowLines;
  eepromData.SpectrumDelay = SpectrumDelay;
  eepromData.IndexMaxLT = IndexMaxLT;
  eepromData.IndexPS = IndexPS;
  eepromData.Backlight_On_Rx = Backlight_On_Rx;
  eepromData.Noislvl_OFF = Noislvl_OFF;
  eepromData.UOO_trigger = UOO_trigger;
  eepromData.osdPopupSetting = osdPopupSetting;
  
  for (int i = 0; i < 32; i++) { 
      if (settings.bandEnabled[i]) eepromData.bandListFlags |= (1 << i);
    }
  eepromData.R40 = BK4819_ReadRegister(BK4819_REG_40);
  eepromData.R29 = BK4819_ReadRegister(BK4819_REG_29);
  eepromData.R19 = BK4819_ReadRegister(BK4819_REG_19);
  eepromData.R73 = BK4819_ReadRegister(BK4819_REG_73);
  eepromData.R10 = BK4819_ReadRegister(BK4819_REG_10);
  eepromData.R11 = BK4819_ReadRegister(BK4819_REG_11);
  eepromData.R12 = BK4819_ReadRegister(BK4819_REG_12);
  eepromData.R13 = BK4819_ReadRegister(BK4819_REG_13);
  eepromData.R14 = BK4819_ReadRegister(BK4819_REG_14);
  eepromData.R3C = BK4819_ReadRegister(BK4819_REG_3C);
  eepromData.R43 = BK4819_ReadRegister(BK4819_REG_43);
  eepromData.R2B = BK4819_ReadRegister(BK4819_REG_2B);
  
/*   char str[64] = "";
  sprintf(str, "R40 %d \r\n", eepromData.R40);LogUart(str); //R40 13520
  sprintf(str, "R29 %d \r\n", eepromData.R29);LogUart(str); //R29 43840
  sprintf(str, "R19 %d \r\n", eepromData.R19);LogUart(str); //R19 4161
  sprintf(str, "R73 %d \r\n", eepromData.R73);LogUart(str); //R73 18066
  sprintf(str, "R13 %d \r\n", eepromData.R13);LogUart(str); //R13 958
  sprintf(str, "R3C %d \r\n", eepromData.R3C);LogUart(str); //R3C 20360
  sprintf(str, "R43 %d \r\n", eepromData.R43);LogUart(str); //R43 13896
  sprintf(str, "R2B %d \r\n", eepromData.R2B);LogUart(str); //R2B 49152 */

  
  // Write in 8-byte chunks
  for (uint16_t addr = 0; addr < sizeof(eepromData); addr += 8) 
    EEPROM_WriteBuffer(addr + 0x1D10, ((uint8_t*)&eepromData) + addr);
  
  ShowOSDPopup("PARAMS SAVED");
}

static void ClearHistory() 
{
  memset(HFreqs,0,sizeof(HFreqs));
  memset(HCount,0,sizeof(HCount));
  memset(HBlacklisted,0,sizeof(HBlacklisted));
  historyListIndex = 0;
  historyScrollOffset = 0;
  #ifdef ENABLE_EEPROM_512K
  indexFs = HISTORY_SIZE;
  WriteHistory();
  #endif
  indexFs = 0;
  SaveSettings(); 
}

void ClearSettings() 
{
  for (int i = 1; i < 15; i++) {
    settings.scanListEnabled[i] = 0;
  }
  settings.scanListEnabled[0] = 1;
  settings.rssiTriggerLevelUp = 5;
  settings.listenBw = 1;
  gScanRangeStart = 43000000;
  gScanRangeStop  = 44000000;
  DelayRssi = 3;
  PttEmission = 2;
  settings.scanStepIndex = S_STEP_10_0kHz;
  ShowLines = 1; //СТРОКА ПО УМОЛЧАНИЮ
  SpectrumDelay = 0;
  MaxListenTime = 0;
  IndexMaxLT = 0;
  IndexPS = 0;
  Backlight_On_Rx = 1;
  Noislvl_OFF = 68; 
  Noislvl_ON = 58;  
  UOO_trigger = 25;
  osdPopupSetting = 250;
  settings.bandEnabled[0] = 1;
  BK4819_WriteRegister(BK4819_REG_10, 0x0145);
  BK4819_WriteRegister(BK4819_REG_11, 0x01B5);
  BK4819_WriteRegister(BK4819_REG_12, 0x0393);
  BK4819_WriteRegister(BK4819_REG_13, 0x03BE);
  BK4819_WriteRegister(BK4819_REG_14, 0x0019);
  
  BK4819_WriteRegister(BK4819_REG_40, 13520);
  BK4819_WriteRegister(BK4819_REG_29, 43840);
  BK4819_WriteRegister(BK4819_REG_19, 4161);
  BK4819_WriteRegister(BK4819_REG_73, 18066);
  BK4819_WriteRegister(BK4819_REG_3C, 20360);
  BK4819_WriteRegister(BK4819_REG_43, 13896);
  BK4819_WriteRegister(BK4819_REG_2B, 49152);
  
  ShowOSDPopup("DEFAULT SETTINGS");
  SaveSettings();
}




static bool GetScanListLabel(uint8_t scanListIndex, char* bufferOut) {
    ChannelAttributes_t att;
    char channel_name[12];
    uint16_t first_channel = 0xFFFF;
    uint16_t channel_count = 0;

    for (uint16_t i = 0; i < MR_CHANNEL_LAST+1; i++) {
        att = gMR_ChannelAttributes[i];
        if (att.scanlist == scanListIndex + 1) {
            if (first_channel == 0xFFFF)
                first_channel = i;
            channel_count++;
        }
    }
    if (first_channel == 0xFFFF) return false; 

    SETTINGS_FetchChannelName(channel_name, first_channel);

    char nameOrFreq[13];
    if (channel_name[0] == '\0') {
        uint32_t freq = gMR_ChannelFrequencyAttributes[first_channel].Frequency;
        sprintf(nameOrFreq, "%u.%05u", freq / 100000, freq % 100000);
        RemoveTrailZeros(nameOrFreq);
    } else {
        strncpy(nameOrFreq, channel_name, 12);
        nameOrFreq[12] = '\0';
    }

    // Метка "<" слева от номера
    if (settings.scanListEnabled[scanListIndex]) {
        sprintf(bufferOut, "> %d:%-11s", scanListIndex + 1, nameOrFreq);
    } else {
        sprintf(bufferOut, " %d:%-11s", scanListIndex + 1, nameOrFreq);
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


static void GetFilteredScanListText(uint16_t displayIndex, char* buffer) {
    uint8_t realIndex = validScanListIndices[displayIndex];
    GetScanListLabel(realIndex, buffer);
}

static void GetParametersText(uint16_t index, char *buffer) {
    switch(index) {
        case 0:
            sprintf(buffer, "RSSI Delay: %2d ms", DelayRssi);
            break;
            
        case 1:
            if (SpectrumDelay < 65000) sprintf(buffer, "Spectrum Delay:%2us", SpectrumDelay / 1000);
              else sprintf(buffer, "Spectrum Delay:00");
            break;
        case 2:
            sprintf(buffer, "MaxListenTime:%s", labels[IndexMaxLT]);
            break;

        case 3:
            if(PttEmission == 0)
              sprintf(buffer, "PTT: Last VFO Freq");
            else if (PttEmission == 1)
              sprintf(buffer, "PTT: NINJA MODE");
            else if (PttEmission == 2)
              sprintf(buffer, "PTT: Last Recived");
            break;
            
        case 4: {
            uint32_t start = gScanRangeStart;
            sprintf(buffer, "Fstart: %u.%05u", start / 100000, start % 100000);
            break;
        }
            
        case 5: {
            uint32_t stop = gScanRangeStop;
            sprintf(buffer, "Fstop: %u.%05u", stop / 100000, stop % 100000);
            break;
        }
      
        case 6: {
            uint32_t step = GetScanStep();
            sprintf(buffer, step % 100 ? "Step: %uk%02u" : "Step: %uk", 
                   step / 100, step % 100);
            break;
        }
            
        case 7:
            sprintf(buffer, "Listen BW: %s", bwNames[settings.listenBw]);
            break;
            
        case 8:
            sprintf(buffer, "Modulation: %s", gModulationStr[settings.modulationType]);
            break;
        
        case 9:
            sprintf(buffer, "Reset Dafault: 3");
            break;
        case 10:
            if (Backlight_On_Rx)
            sprintf(buffer, "RX Backlight ON");
            else sprintf(buffer, "RX Backlight OFF");
            break;
        case 11:
            if (gCounthistory) sprintf(buffer, "Freq Counting");
            else sprintf(buffer, "Time Counting");
            break;

        case 12:
            sprintf(buffer, "Clear History: 3");
            break;

        case 13:
            uint32_t free = free_ram_bytes();
            sprintf(buffer, "Free RAM %uB", (unsigned)free);
            break;

        case 14:
            sprintf(buffer, "Power Save: %s", labelsPS[IndexPS]);
            break;
        case 15:
            sprintf(buffer, "Nois LVL OFF: %d", Noislvl_OFF);
            break;
        case 16:
            if (osdPopupSetting) {
                uint8_t seconds = osdPopupSetting / 1000;   //1000
                uint8_t decimals = (osdPopupSetting % 1000) / 100;  //1000/100

                if (decimals) {
                    sprintf(buffer, "Popups: %d.%ds", seconds, decimals);
                } else {
                    sprintf(buffer, "Popups: %ds", seconds);
                }
            } else {
                sprintf(buffer, "No Popups");
            }
            break;
        
            break;
        case 17:
            sprintf(buffer, "UOO Trigger: %d", UOO_trigger);
            break;
        case 18:
         if (AUTO_KEYLOCK) sprintf(buffer, "Keylock: %ds", durations[AUTO_KEYLOCK]/2);
            else  sprintf(buffer, "Key Unlocked");
            break;

        case 19:
            const uint16_t g3d_values[] = {0, 0x2AAB, 0x4924, 0x6800, 0x871C, 0xA666, 0xC5D1, 0xE555};
            sprintf(buffer, "REG_3D:%04X", g3d_values[Gain3D_Index]);
            break;
        case 20:
            sprintf(buffer
                , "2X IF: %s", Enable2XIF ? "ON" : "OFF");
            break;
        case 21:
            sprintf(buffer, "Sound_Clear: %s", SoundBoost ? "ON" : "OFF");
            break;
        
        default:
            // Gestion d'un index inattendu (optionnel)
            buffer[0] = '\0';
            break;
    }
}

static void GetBandItemText(uint16_t index, char* buffer) {
    if (settings.bandEnabled[index]) {
        sprintf(buffer, "> %d: %-11s", index + 1, BParams[index].BandName);
    } else {
        sprintf(buffer, " %d: %-11s", index + 1, BParams[index].BandName);
    }
}

static void GetHistoryItemText(uint16_t index, char* buffer) {
    char freqStr[12];
    char Name[20] = "";  // запас на имя + "..."
    uint8_t dcount;
    uint32_t f = HFreqs[index];
    buffer[0] = '\0';
    if (!f) return;

    // Всегда 5 знаков после точки — нули НЕ обрезаем
    sprintf(freqStr, "%u.%05u", f / 100000, f % 100000);

    uint16_t Hchannel = BOARD_gMR_fetchChannel(f);
    
    if (gCounthistory) {
        dcount = HCount[index];
    } else {
        dcount = HCount[index] / 2;
    }
    
    // Lecture du nom du canal (Argument 1: Index, Argument 2: Buffer)
    if (Hchannel != 0xFFFF) {
        ReadChannelName(Hchannel, Name);
        Name[19] = '\0';  // безопасность
    }
    
    const char *blacklistPrefix = HBlacklisted[index] ? "#" : "";

    // --- 2. Détermination de l'espace nécessaire (Max 18 chars) ---
    const size_t MAX_LINE_CHARS = 18; 
    
    // Construction de la chaîne du compteur (Ex: ":5" ou ":1234")
    char dcountStr[6]; 
    snprintf(dcountStr, sizeof(dcountStr), ":%u", dcount);

    size_t len_prefix = strlen(blacklistPrefix);
    size_t len_freq = strlen(freqStr);
    size_t len_name = strlen(Name);
    size_t len_dcount = strlen(dcountStr);

    // Longueur requise pour la partie non-fréquence : [Prefix] + [Espace] + [Name] + [Dcount]
    size_t critical_len = len_prefix + 1 + len_name + len_dcount; 
    
    size_t space_for_freq = 0;
    
    if (MAX_LINE_CHARS > critical_len) {
        // Cas nominal : Il y a de la place après le Nom et le Compteur.
        space_for_freq = MAX_LINE_CHARS - critical_len;
    } else {
        // Cas critique : Le Nom et le Compteur sont trop longs. On supprime le Nom.
        
        // Recalcul de la longueur critique sans le Nom et l'espace qui le précède.
        critical_len = len_prefix + 1 + len_dcount;
        
        if (MAX_LINE_CHARS > critical_len) {
            space_for_freq = MAX_LINE_CHARS - critical_len;
        } else {
            // Cas très critique : On donne tout l'espace sauf le compteur (très court)
            space_for_freq = MAX_LINE_CHARS - len_dcount; 
        }
        
        // Suppression du nom pour l'affichage final
        Name[0] = '\0';
        len_name = 0;
    }

    // --- 3. Construction de la chaîne finale (avec troncature de la Fréquence) ---
    
    // La longueur finale à afficher pour la fréquence (min(espace_disponible, longueur_réelle))
    size_t final_freq_len = (space_for_freq > len_freq) ? len_freq : space_for_freq;
    
    // Le format : [Prefix][Freq Tronquée][Espace si Nom][Name][Dcount]
    
    // Le snprintf final doit toujours garantir que la taille n'est pas dépassée (19)
    snprintf(buffer, 19, "%s%.*s%s%s%s", 
             blacklistPrefix,
             (int)final_freq_len, // Troncature dynamique de la fréquence
             freqStr, 
             (len_name > 0) ? " " : "", // Espace si le Nom n'est pas vide (géré par la suppression ci-dessus)
             Name, 
             dcountStr);
}


//*******************************СПИСКИ*****************************// */
static void RenderList(const char* title, uint16_t numItems, uint16_t selectedIndex, uint16_t scrollOffset,
                      void (*getItemText)(uint16_t index, char* buffer)) {
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
        uint16_t itemIndex = i + scrollOffset;
        if (itemIndex >= numItems) break;
        
        char itemText[32];
        getItemText(itemIndex, itemText);
        
        uint16_t lineNumber = FIRST_ITEM_LINE + i;
        
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

#ifdef ENABLE_IN_BAND
      static void RenderBandSelect() {RenderList("INT BANDS:", ARRAY_SIZE(BParams),bandListSelectedIndex, bandListScrollOffset, GetBandItemText);}
#endif

#ifdef ENABLE_FI_BAND
      static void RenderBandSelect() {RenderList("FI BANDS:", ARRAY_SIZE(BParams),bandListSelectedIndex, bandListScrollOffset, GetBandItemText);}
#endif

#ifdef ENABLE_BR_BAND
      static void RenderBandSelect() {RenderList("BR BANDS:", ARRAY_SIZE(BParams),bandListSelectedIndex, bandListScrollOffset, GetBandItemText);}
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
      sprintf(headerString, "HISTORY: %d", CountValidHistoryItems());
      UI_PrintStringSmall(headerString, 1, LCD_WIDTH - 1, 0, 0);
    } else DrawMeter(0);
    
    const uint16_t FIRST_ITEM_LINE = 1;
    const uint16_t MAX_LINES = 6;
    
    uint16_t scrollOffset = historyScrollOffset;
    uint16_t selectedIndex = historyListIndex;
    
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
        uint16_t itemIndex = i + scrollOffset;

        if (itemIndex >= CountValidHistoryItems()) break;
        char itemText[32];
        GetHistoryItemText(itemIndex, itemText);
        //if (strcmp(itemText, "") == 0) 
        if (itemText[0] == '\0') 
            {itemIndex--;
             continue;
            }
        uint16_t lineNumber = FIRST_ITEM_LINE + i;
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
}

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
static void BuildScanListChannels(uint8_t scanListIndex) {
    scanListChannelsCount = 0;
    ChannelAttributes_t att;
    
    for (uint16_t i = 0; i < MR_CHANNEL_LAST+1; i++) {
        att = gMR_ChannelAttributes[i];
        if (att.scanlist == scanListIndex + 1) {
            if (scanListChannelsCount < MR_CHANNEL_LAST+1) {
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
    UI_PrintStringSmall(title, 1, 0, 0,1);
    
    const uint8_t MAX_ITEMS_VISIBLE = 3; // 3 kanały x 2 linie = 6 linii
    
    for (uint8_t i = 0; i < MAX_ITEMS_VISIBLE; i++) {
        uint8_t itemIndex = i + scrollOffset;
        if (itemIndex >= numItems) break;
        
        uint16_t channelIndex = scanListChannels[itemIndex];
        char channel_name[12];
        SETTINGS_FetchChannelName(channel_name, channelIndex);
        
        uint32_t freq = gMR_ChannelFrequencyAttributes[channelIndex].Frequency;
        char freqStr[16];
        sprintf(freqStr, " %u.%05u", freq/100000, freq%100000);
        RemoveTrailZeros(freqStr);
        
        uint8_t line1 = 1 + i * 2;
        uint8_t line2 = 2 + i * 2;
        
        char nameText[20], freqText[20];
        if (itemIndex == selectedIndex) {
            sprintf(nameText, "%3d: %s", channelIndex + 1, channel_name);
            sprintf(freqText, "    %s", freqStr);
            UI_PrintStringSmall(nameText, 1, 0, line1,1);
            UI_PrintStringSmall(freqText, 1, 0, line2,1);
        } else {
            sprintf(nameText, "%3d: %s", channelIndex + 1, channel_name);
            sprintf(freqText, "    %s", freqStr);
            UI_PrintStringSmall(nameText, 1, 0, line1,0);
            UI_PrintStringSmall(freqText, 1, 0, line2,0);
        }
        

    }
    
    ST7565_BlitFullScreen();
}
#endif // ENABLE_SCANLIST_SHOW_DETAIL
