#include "browser.h"
#include "project.h"
#include "iodefine.h"
#include "eeprom.h"
#include "display.h"
#include "graphics.h"
#include "beep.h"
#include "common.h"
#include "home.h"
#include "interface.h"
#include "motion.h"
#include "power.h"
#include "rtc.h"
#include "pad.h"
#include "user.h"
#include "ir.h"
#include "startup.h"
#include <stddef.h>
#include "function_names.h"

Workspace g_work;
void PowerOnReset(void);
void RtcQuarterSecondInterrupt(void);
void RtcSecondInterrupt(void);
void RtcMinuteInterrupt(void);
void RtcHourInterrupt(void);
void TimerWInterrupt(void);
void BeepTick(void);
void ViewRender(void);
void FftAccumulate(const volatile s8 *samples);
s32 MotionEstimate(u16 *spectrum);
void IRQ0Interrupt(void);

static u8 eeprom[65536];
static u16 endianMap[65536];
static u8 lcd[2][1536];
static u8 pixels[96*64];
static u8 lcdX,lcdPage,lcdPlane,lcdBank,lcdArgument,lcdSleeping;
static u32 lcdFrames,eepromReads,eepromWrites,mainTicks,rtcTicks,fftEpochs;
static u32 ticks,traceCount[512],traceRing[2048],traceHead;
static u32 readHeat[512],writeHeat[512];
static s8 sensor[3];
static s32 lastCadence;
static u16 lastSpectrum[32];
static s8 lastSamples[3][64];
static u32 ioFaults,scratchHigh;
static unsigned long long clockUs,quarterDeadline,secondDeadline,mainDeadline,beepDeadline;
static unsigned long long irDeadline;
static u8 keys;

void *memcpy(void *dst,const void *src,size_t n){u8 *d=dst;const u8*s=src;while(n--)*d++=*s++;return dst;}
void *memset(void *dst,int v,size_t n){u8*d=dst;while(n--)*d++=(u8)v;return dst;}
void *memmove(void *dst,const void *src,size_t n){u8*d=dst;const u8*s=src;if(d<s){for(size_t i=0;i<n;i++)d[i]=s[i];}else{while(n--)d[n]=s[n];}return dst;}

void BrowserTrace(u16 id){if(id<512)traceCount[id]++;traceRing[traceHead++&2047]=id;if(g_state.scratchUsed>scratchHigh)scratchHigh=g_state.scratchUsed;}
void BrowserMotionResult(s32 cadence){lastCadence=cadence;fftEpochs++;memcpy(lastSpectrum,g_work.motion.fftAccumulator,64);memcpy(lastSamples,(const void*)g_work.motion.x,192);}

void BrowserLcdByte(u8 value){
 if(IO.PDR1.BIT.B1){
   if(lcdX<96&&lcdPage<16)lcd[lcdPage/8][(lcdPage%8)*192+lcdX*2+lcdPlane]=value;
   lcdPlane^=1;if(!lcdPlane)lcdX++;
 }else if(lcdArgument){
   if(lcdArgument==0x40){lcdBank=(value>>6)&1;lcdFrames++;}
   lcdArgument=0;
 }else if(value==0x40||value==0x81){lcdArgument=value;}
 else if((value&0xf0)==0xb0){lcdPage=value&15;lcdPlane=0;}
 else if((value&0xf0)==0x10){lcdX=(lcdX&15)|((value&7)<<4);lcdPlane=0;}
 else if((value&0xf0)==0){lcdX=(lcdX&0x70)|(value&15);lcdPlane=0;}
 else if(value==0xa9)lcdSleeping=1;
 else if(value==0xe1)lcdSleeping=0;
}
static void mapWord(u32 at,u32 size){if(at+size<=65536)for(u32 i=0;i<size;i++)endianMap[at+i]=(u16)(at+size-1-i);}
#define MAP16(base,T,f) mapWord((base)+offsetof(T,f),2)
#define MAP32(base,T,f) mapWord((base)+offsetof(T,f),4)
#include "endian_map.h"
static void initEndian(void){for(u32 i=0;i<65536;i++)endianMap[i]=i;MapEepromRecords();}
void EepromRead(u16 a,void *destination,u16 n){u8*d=destination;eepromReads++;for(u32 i=0;i<n;i++){u16 p=a+i;d[i]=eeprom[endianMap[p]];readHeat[p/128]++;}}
void EepromWrite(u16 a,void *source,u16 n){const u8*s=source;eepromWrites++;for(u32 i=0;i<n;i++){u16 p=a+i;eeprom[endianMap[p]]=s[i];writeHeat[p/128]++;}}
u8 EepromReadByte(u16 a){eepromReads++;readHeat[a/128]++;return eeprom[a];}
void EepromWriteByte(u16 a,u8 v){eeprom[a]=v;eepromWrites++;writeHeat[a/128]++;}
void EepromFill(u16 a,u16 n,u8 v){for(u32 i=0;i<n;i++)EepromWriteByte((u16)(a+i),v);}
void EepromFillPage(u16 a,u8 v){EepromFill(a,128,v);}
void EepromWritePage(uint a,u8*s){EepromWrite(a,s,128);}
void EepromConfigure(void){}
void EepromIdle(void){}
u8 EepromReceive(void){return 0;}
u8 AccelRead(u8 reg,u8*d,u8 n){for(u8 i=0;i<n;i++)d[i]=(i&1)?sensor[(i/2)%3]:0;return 1;}
void AccelWrite(u8 reg,u8 value){}
u8 AccelInit(void){return 1;}
uint BatterySample(void){return 768;}
u8 BatteryLow(u16 scale){return 0;}
void BatteryUpdate(void){g_state.events.bits.batteryLow=0;g_state.events.bits.batteryCheckPending=0;}
u16 BatteryProtect(uint value){return value;}
u8 BatteryVerify(u16 value){return 1;}
void _INITSCT(void){}
void IrInit(void){}
u8 *IrPayload(void){return g_work.irc.packet+8;}
void IrBegin(void){irDeadline=clockUs+3000000;InstallTask(IrProtocolTick);}
void IrProtocolTick(void){if(clockUs>=irDeadline){g_state.irResult=1;g_work.irc.work.completionAction=0;IrComplete();}}

u32 pw_eeprom(void){return (u32)eeprom;}
u32 pw_pixels(void){for(u32 y=0;y<64;y++)for(u32 x=0;x<96;x++){u32 a=(y/8)*192+x*2;pixels[y*96+x]=lcdSleeping?0:(((lcd[lcdBank][a]>>(y&7))&1)*2+((lcd[lcdBank][a+1]>>(y&7))&1));}return(u32)pixels;}
u32 pw_lcd(void){return(u32)lcd;}
u32 pw_runtime(void){return(u32)&g_state;}
u32 pw_workspace(void){return(u32)&g_work;}
u32 pw_ui(void){return(u32)&g_ui;}
u32 pw_counts(void){return(u32)traceCount;}
u32 pw_trace(void){return(u32)traceRing;}
u32 pw_trace_head(void){return traceHead;}
u32 pw_read_heat(void){return(u32)readHeat;}
u32 pw_write_heat(void){return(u32)writeHeat;}
u32 pw_spectrum(void){return(u32)lastSpectrum;}
u32 pw_samples(void){return(u32)lastSamples;}
u32 pw_ring_samples(void){return(u32)g_work.motion.x;}
u32 pw_view_bytes(void){return(u32)&g_ui.view;}
static u32 stats[40];
u32 pw_stats(void){
 stats[0]=(u32)(clockUs/1000);stats[1]=mainTicks;stats[2]=rtcTicks;stats[3]=lcdFrames;stats[4]=fftEpochs;stats[5]=g_state.view;stats[6]=g_state.save.watts;stats[7]=g_state.dailySteps;stats[8]=g_state.save.totalSteps;stats[9]=g_state.hourSteps;stats[10]=g_state.flags.byte;stats[11]=g_state.events.byte;stats[12]=g_state.sampleIndex;stats[13]=g_state.scratchUsed;stats[14]=lastCadence;stats[15]=eepromReads;stats[16]=eepromWrites;stats[17]=g_state.randomState;stats[18]=g_state.menuSelection;stats[19]=g_state.save.rtcSeconds;stats[20]=g_work.motion.batch.stepFractionQ9;stats[21]=g_state.stepPacing.batchSteps;stats[22]=g_state.stepPacing.stepsEmitted;stats[23]=g_work.motion.batch.lastRejected;stats[24]=g_state.save.stepsTowardNextWatt;stats[25]=g_state.save.days;stats[26]=lcdSleeping;stats[27]=g_task==BeepTick?1:g_task==IrProtocolTick?2:0;stats[28]=TW.GRA;stats[29]=g_ui.periodsRemaining;stats[30]=g_state.idleSeconds[0];stats[31]=ioFaults;stats[32]=scratchHigh;stats[33]=g_state.save.volume;return(u32)stats;
}
void pw_buttons(u32 value){u8 rising=(u8)value&~keys;keys=value;IO.PDRB.BIT.B0=(value&2)!=0;IO.PDRB.BIT.B2=(value&4)!=0;IO.PDRB.BIT.B4=(value&8)!=0;if(rising&2)IRQ0Interrupt();}
void pw_sensor(s32 x,s32 y,s32 z){sensor[0]=x;sensor[1]=y;sensor[2]=z;}
void pw_boot(void){
 memset(&g_state,0,sizeof(g_state));memset(&g_ui,0,sizeof(g_ui));memset(&g_work,0,sizeof(g_work));memset(lcd,0,sizeof(lcd));memset(traceCount,0,sizeof(traceCount));memset(readHeat,0,sizeof(readHeat));memset(writeHeat,0,sizeof(writeHeat));
 memset(lastSpectrum,0,sizeof(lastSpectrum));memset(lastSamples,0,sizeof(lastSamples));
 clockUs=0;mainTicks=rtcTicks=lcdFrames=fftEpochs=eepromReads=eepromWrites=traceHead=scratchHigh=0;keys=0;lastCadence=0;lcdArgument=lcdX=lcdPage=lcdPlane=lcdBank=lcdSleeping=0;g_task=0;g_note=0;
 initEndian();SSU.SSSR.BIT.TDRE=1;SSU.SSSR.BIT.TEND=1;SSU.SSSR.BIT.RDRF=1;SSU.SSRDR=0;IO.PDRB.BYTE=0;
 PowerOnReset();
 quarterDeadline=250000;secondDeadline=1000000;mainDeadline=62500;beepDeadline=0;
 DisplayClear(64);ViewRender();DisplayToggleBank();
}
void pw_blank(void){memset(eeprom,255,sizeof(eeprom));pw_boot();}
static u8 bcd(u32 value){return(value/10)*16+value%10;}
void pw_tick(void){
 unsigned long long target=clockUs+62500;
 while(clockUs<target){
   unsigned long long next=target;
   if(quarterDeadline<next)next=quarterDeadline;
   if(secondDeadline<next)next=secondDeadline;
   if(g_task==BeepTick&&beepDeadline<next)next=beepDeadline>clockUs?beepDeadline:clockUs;
   clockUs=next;
   if(clockUs>=secondDeadline){u32 s=g_state.save.rtcSeconds+1;RTC.RSECDR.BYTE=bcd(s%60);RTC.RMINDR.BYTE=bcd((s/60)%60);RTC.RHRDR.BYTE=bcd((s/3600)%24);RtcSecondInterrupt();rtcTicks++;if(s%60==0)RtcMinuteInterrupt();if(s%3600==0)RtcHourInterrupt();secondDeadline+=1000000;}
   if(clockUs>=quarterDeadline){if(RTC.RTCCR2.BIT._025SEIE)RtcQuarterSecondInterrupt();quarterDeadline+=250000;}
   if(g_task==BeepTick&&clockUs>=beepDeadline){TimerWInterrupt();BeepTick();beepDeadline=clockUs+((u32)TW.GRA+1)*1000000/32768;if(beepDeadline<=clockUs)beepDeadline=clockUs+31;}
   if(clockUs>=mainDeadline){if(g_task!=BeepTick&&g_task){g_task();mainTicks++;if(g_task==BeepTick)beepDeadline=clockUs+31;}mainDeadline+=62500;}
 }
 ticks++;
}
void pw_save(void){EepromMirrorWrite(EEPROM_SAVE_PRIMARY,EEPROM_SAVE_BACKUP,(u8*)&g_state.save,sizeof(SaveData));}
u32 pw_probe_fft(u32 axis){if(axis>2)return 0;memset(g_work.motion.fftAccumulator,0,64);FftAccumulate((volatile s8*)lastSamples+axis*64);return(u32)g_work.motion.fftAccumulator;}
void pw_set_watts(u32 watts){g_state.save.watts=watts>9999?9999:watts;}
void pw_set_seed(u32 seed){g_state.randomState=seed;}
