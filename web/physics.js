// SI units. This is a configurable input model, not a calibrated BMA150 model.
// The physical integrator runs at 256 Hz; C consumes one sample at 16 Hz.
export const DT=1/256;
export function createPhysics(){return {theta:0,omega:0,phi:0,phiV:0,x:0,y:0,z:0,vx:0,vy:0,vz:0,t:0,mode:'still',tether:'pendulum',length:.06,mass:.021,diameter:.048,damping:1.5,response:1,wind:0,direction:0,gust:.55,fan:.5,cadence:2,drag:null,accel:[0,32,0]};}
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
export function stepPhysics(p){
 const dt=DT,g=9.80665,L=Math.max(.025,p.length),r=p.diameter/2,floor=(p.ground??-.10)+r;
 p.t+=dt;
 const wind=p.mode==='fan'?(p.wind||p.fan*16):p.wind;
 const angle=p.direction*Math.PI/180;
 const air=.5*1.225*1.1*Math.PI*r*r*wind*wind/p.mass*(1+p.gust*(.65*Math.sin(p.t*7)+.35*Math.sin(p.t*13.3)));
 let ax=air*Math.cos(angle),ay=0,az=air*Math.sin(angle);
 if(p.tether==='free'){
  ay=-g;
  if(p.drag){ax+=(p.drag[0]-p.x)*300*p.response-p.vx*18;ay+=(p.drag[1]-p.y)*300*p.response-p.vy*18+g;}
  ax-=p.vx*p.damping;az-=p.vz*p.damping;
  p.vx=clamp(p.vx+ax*dt,-3,3);p.vy=clamp(p.vy+ay*dt,-3,3);p.vz=clamp(p.vz+az*dt,-3,3);
  p.x+=p.vx*dt;p.y+=p.vy*dt;p.z+=p.vz*dt;
  if(p.y<floor){p.y=floor;if(p.vy<-.03){ay+=-1.35*p.vy/dt;p.vy=-p.vy*.35;}else{p.vy=0;ay=0;}p.vx*=.98;p.vz*=.98;}
  // A bounded test area prevents an unattended fan from losing the device.
  for(const [pos,vel] of [['x','vx'],['z','vz']])if(Math.abs(p[pos])>.19){p[pos]=Math.sign(p[pos])*.19;p[vel]*=-.35;}
  p.theta+=(-p.theta*8+p.vx*3-p.omega*3)*dt;p.phi+=(-p.phi*8+p.vz*3)*dt;
 }else if(p.tether==='fixed'){
  p.x=p.y=p.z=p.theta=p.phi=p.omega=p.phiV=0;ax=ay=az=0;
 }else{
  const target=p.drag?Math.atan2(p.drag[0],L-p.drag[1]):0;
  const a=-g/L*Math.sin(p.theta)-p.damping*p.omega+ax/L*Math.cos(p.theta)+(p.drag?(target-p.theta)*180*p.response-p.omega*10:0);
  const b=-g/L*Math.sin(p.phi)-p.damping*p.phiV+az/L*Math.cos(p.phi);
  p.omega=clamp(p.omega+a*dt,-24,24);p.theta=clamp(p.theta+p.omega*dt,-2.7,2.7);
  p.phiV=clamp(p.phiV+b*dt,-20,20);p.phi=clamp(p.phi+p.phiV*dt,-1.3,1.3);
  ax=L*(a*Math.cos(p.theta)-p.omega*p.omega*Math.sin(p.theta));
  ay=L*(a*Math.sin(p.theta)+p.omega*p.omega*Math.cos(p.theta));az=L*b;
  p.x=L*Math.sin(p.theta);p.y=L*(1-Math.cos(p.theta));p.z=L*Math.sin(p.phi);
 }
 if(p.mode==='walk'){
  const w=p.cadence*2*Math.PI;
  ay+=.036*w*w*Math.sin(w*p.t);
 }
 const wx=ax,wy=g+ay,wz=az,ct=Math.cos(p.theta),st=Math.sin(p.theta),cp=Math.cos(p.phi),sp=Math.sin(p.phi);
 const lx=ct*wx+st*wy,ly=-st*wx+ct*wy;
 p.accel=[lx,cp*ly+sp*wz,-sp*ly+cp*wz].map(v=>clamp(Math.round(v/g*32),-128,127));
 return p.accel;
}
export function advancePhysics(p){for(let i=0;i<16;i++)stepPhysics(p);return p.accel;}
export function pose(p){return {theta:p.theta,phi:p.phi,x:p.x,y:p.y+(p.mode==='walk'?.009*Math.sin(p.cadence*2*Math.PI*p.t):0),z:p.z,t:p.t,fan:p.mode==='fan'?(p.wind||p.fan*16):p.wind,tether:p.tether,length:p.length,diameter:p.diameter,direction:p.direction};}
