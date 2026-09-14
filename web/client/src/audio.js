// Timer W transitions use the audio clock. Lookahead covers a firmware block
// plus worker delivery jitter, including when playback is slower than 1×.
export async function createFirmwareAudio(){
 const ctx=new AudioContext({latencyHint:'interactive'}),osc=ctx.createOscillator(),gain=ctx.createGain(),high=ctx.createBiquadFilter(),low=ctx.createBiquadFilter();
 osc.type='square';gain.gain.value=0;high.type='highpass';high.frequency.value=180;low.type='lowpass';low.frequency.value=6500;osc.connect(gain).connect(high).connect(low).connect(ctx.destination);osc.start();await ctx.resume();
 let base=null,speed=1,serial=-1,late=0,events=0,latency=90,resyncs=0;
 const reset=()=>{base=null;gain.gain.cancelScheduledValues(ctx.currentTime);osc.frequency.cancelScheduledValues(ctx.currentTime);gain.gain.setTargetAtTime(0,ctx.currentTime,.002);};
 const schedule=(at,period,mode)=>{if(at<ctx.currentTime){late++;at=ctx.currentTime+.002;}osc.frequency.setValueAtTime(Math.min(ctx.sampleRate*.45,period?32768/period*speed:100),at);gain.gain.setTargetAtTime(period?(mode===1?.025:.05):0,at,.0008);events++;};
 return {packet:d=>{
  if(ctx.state!=='running')return;
  // A stalled worker must not leave all later notes on an expired time base.
  if(base!==null&&base+d.from/1e6/speed<ctx.currentTime+.005){reset();resyncs++;}
  if(base===null||d.speed!==speed||d.serial!==serial){reset();speed=d.speed;serial=d.serial;const block=.0625/speed;latency=Math.max(.090,block+.030)*1000;base=ctx.currentTime+latency/1000-(d.from/1e6/speed+block);schedule(base+d.from/1e6/speed,d.initialPeriod,d.initialVolume);}
  for(const [us,period,mode] of d.events)schedule(base+us/1e6/speed,period,mode);
 },reset,close:()=>ctx.close(),metrics:()=>({late,events,latency,resyncs})};
}
