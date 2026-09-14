import assert from 'node:assert/strict';
import {createPhysics,stepPhysics} from './physics.js';
function run(p,seconds){let max=0;for(let i=0;i<seconds*256;i++){stepPhysics(p);max=Math.max(max,Math.abs(p.x));assert.ok(p.accel.every(Number.isFinite));}return max;}
const standard=createPhysics();assert.equal(standard.clip,true);assert.equal(standard.mass,.0219);assert.equal(standard.grip,'point');assert.equal(standard.length,.10);assert.equal(standard.cordDiameter,.001);
const free=createPhysics();free.tether='free';free.drag=[.12,.075];run(free,1);assert.ok(free.x>.10&&free.y>.05,'Free motion follows a distant pointer.');free.drag=null;run(free,4);assert.ok(Math.abs(free.y-(-.1+.024))<.001,'Released device lands on the ground.');
const low=createPhysics(),high=createPhysics();low.mass=.010;high.mass=.050;low.wind=high.wind=6;const a=run(low,8),b=run(high,8);assert.ok(a>b*1.3,'Mass changes wind response.');
const fixed=createPhysics();fixed.tether='fixed';fixed.wind=24;run(fixed,3);assert.equal(fixed.x,0);
console.log(JSON.stringify({freeDragAndRelease:'passed',lightDisplacementM:a,heavyDisplacementM:b,fixedConstraint:'passed'}));
{
 const p=createPhysics();p.tether='free';p.lockFacing=false;p.q=[.075,.035,0,Math.sqrt(1-.075**2-.035**2)];run(p,3);
 const normalY=2*(p.q[1]*p.q[2]-p.q[0]*p.q[3]);assert.ok(Math.abs(normalY)>.98,'A tilted device topples onto a face.');assert.ok(Math.abs(p.y-(-.1+p.thickness/2+(p.clip&&normalY>0?.0069:0)))<.001,'Ground contact includes the clip thickness.');
 const snapshot=structuredClone(p);run(p,1);run(snapshot,1);for(const key of ['x','y','z'])assert.ok(Math.abs(p[key]-snapshot[key])<1e-8,'Rigid-body restore remains deterministic.');
 console.log('Three-axis toppling, face contact and restored rigid-body state: passed.');
}
{
 const p=createPhysics();p.tether='pendulum';p.lockFacing=false;p.y=.03;p.q=[0,0,0,1];stepPhysics(p);assert.equal(p.tension,0,'A slack string cannot push the device.');run(p,1);assert.ok(p.tension>0,'A taut string carries tension.');
 console.log('Unilateral string constraint: slack and tension passed.');
}
{
 const held=[];
 for(const grip of ['firm','point']){const p=createPhysics();p.tether='free';p.grip=grip;p.drag={target:[0,.02,.007],local:[.014,.012,.007],orientation:[0,0,0,1]};run(p,3);held.push(p);}
 assert.ok(held[0].q.slice(0,3).every(v=>Math.abs(v)<1e-4),'Firm grip holds the pickup orientation.');
 assert.ok(Math.hypot(...held[1].q.slice(0,3))>.2,'Off-centre point grip permits rotation.');
 assert.ok(Math.hypot(...held[1].av)<.1,'A stationary point grip settles instead of shaking.');
 const {resetPhysics,fanDirection}=await import('./physics.js');const twist=createPhysics();resetPhysics(twist,'twist');run(twist,2);assert.ok(twist.av[1]<-1,'Stored twist produces untwisting torque.');run(twist,18);assert.ok(Math.abs(twist.twist)<.03,'Twist decays with cord damping.');
 const f=createPhysics();f.oscillate=true;f.direction=15;f.sweep=35;f.oscillationHz=.25;f.t=1;assert.equal(fanDirection(f),50);f.t=3;assert.equal(fanDirection(f),-20);
 console.log('Firm and point grips, settled rotation, cord torsion and fan sweep: passed.');
}
