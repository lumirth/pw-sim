import {World,Body,Vec3,Quaternion,Cylinder,Box,Plane,Material,ContactMaterial,Constraint,ContactEquation} from './vendor/cannon-es.js';
export const DT=1/256;
import {defaults} from './physics-defaults.js';
export {defaults};
export function createPhysics(){return {...defaults,x:0,y:0,z:0,vx:0,vy:0,vz:0,q:[0,0,0,1],av:[0,0,0],theta:0,phi:0,t:0,drag:null,accel:[0,32,0],contacts:0,kinetic:0,tension:0,twist:0,twistAngle:0,revision:0};}
const cache=new WeakMap();
const vec=a=>new Vec3(...a),arr=v=>[v.x,v.y,v.z];
// A massless, inextensible rope: tension only. Slack produces no constraint.
class Rope extends Constraint{
 constructor(anchor,body,point,length){super(anchor,body,{collideConnected:false});this.local=point;this.length=length;this.eq=new ContactEquation(anchor,body);this.eq.minForce=-100;this.eq.maxForce=0;this.eq.setSpookParams(1e8,4,DT);this.equations=[this.eq];}
 update(){const a=this.bodyA,b=this.bodyB,e=this.eq;b.quaternion.vmult(this.local,e.rj);b.position.vadd(e.rj,e.ni);e.ni.vsub(a.position,e.ni);const distance=e.ni.length();e.ni.normalize();e.ni.scale(this.length,e.ri);const v=new Vec3();b.angularVelocity.cross(e.rj,v);v.vadd(b.velocity,v);const radial=v.dot(e.ni);e.enabled=distance>=this.length-.00001||distance+radial*DT>=this.length;this.distance=distance;}
}
function system(p){const key=[p.tether,p.mass,p.diameter,p.thickness,p.clip,p.lockFacing,p.length,p.ground,p.friction,p.restitution,p.revision].join('|');let s=cache.get(p);if(s?.key===key)return s;
 const world=new World({gravity:new Vec3(0,-p.gravity,0),allowSleep:false});world.solver.iterations=32;world.solver.tolerance=1e-10;
 const plastic=new Material('plastic'),floorMaterial=new Material('surface');world.addContactMaterial(new ContactMaterial(plastic,floorMaterial,{friction:p.friction,restitution:p.restitution,contactEquationStiffness:1e8,contactEquationRelaxation:4,frictionEquationStiffness:1e8}));
 const body=new Body({mass:p.mass,material:plastic,position:new Vec3(p.x,p.y,p.z),velocity:new Vec3(p.vx,p.vy,p.vz),angularVelocity:vec(p.av),quaternion:new Quaternion(...p.q),allowSleep:false});
 const turn=new Quaternion();turn.setFromAxisAngle(new Vec3(1,0,0),Math.PI/2);body.addShape(new Cylinder(p.diameter/2,p.diameter/2,p.thickness,32),new Vec3(),turn);
 if(p.clip)body.addShape(new Box(new Vec3(.012,.016,.00345)),new Vec3(0,0,-p.thickness/2-.00345));
 // Allocate the specified mass between shell and clip. The uniform shapes
 // approximate the unknown internal distribution; include the clip offset.
 const r=p.diameter/2,h=p.thickness,clipMass=p.clip?p.mass*(.0013/.0219):0,shellMass=p.mass-clipMass,clipZ=h/2+.00345;
 body.inertia.set(shellMass*(3*r*r+h*h)/12+clipMass*((.032**2+.0069**2)/12+clipZ**2),shellMass*(3*r*r+h*h)/12+clipMass*((.024**2+.0069**2)/12+clipZ**2),shellMass*r*r/2+clipMass*(.024**2+.032**2)/12);
 body.invInertia.set(1/body.inertia.x,1/body.inertia.y,1/body.inertia.z);body.updateInertiaWorld(true);
 if(p.lockFacing){body.angularFactor.set(0,0,1);const angle=2*Math.atan2(body.quaternion.z,body.quaternion.w);body.quaternion.setFromAxisAngle(new Vec3(0,0,1),angle);body.angularVelocity.x=body.angularVelocity.y=0;}
 if(p.tether==='fixed'){body.type=Body.KINEMATIC;body.velocity.setZero();body.angularVelocity.setZero();body.updateMassProperties();}
 world.addBody(body);const floor=new Body({mass:0,material:floorMaterial,shape:new Plane(),position:new Vec3(0,p.ground,0)});floor.quaternion.setFromAxisAngle(new Vec3(1,0,0),-Math.PI/2);world.addBody(floor);
 let rope=null;if(p.tether==='pendulum'){const anchor=new Body({mass:0,position:new Vec3(0,p.length+r,0)});world.addBody(anchor);rope=new Rope(anchor,body,new Vec3(0,r,0),p.length);world.addConstraint(rope);}
 s={world,body,rope,key};cache.set(p,s);return s;
}
export function resetPhysics(p,action='position'){
 if(action==='twist'){p.twist+=Math.PI*2;return;}
 p.twist=p.twistAngle=0;p.drag=null;p.vx=p.vy=p.vz=0;p.av=[0,0,0];
 if(action==='flip'){p.q=[Math.sin(-Math.PI/4),0,0,Math.cos(-Math.PI/4)];p.y=Math.max(p.y,p.ground+p.thickness/2+.012);}
 else{p.q=p.lockFacing?[0,0,0,1]:[.075,.035,0,Math.sqrt(1-.075**2-.035**2)];if(action==='position')p.x=p.y=p.z=0;else p.y=Math.max(p.y,p.ground+p.diameter/2+.003);}
 p.revision++;cache.delete(p);
}
export function fanDirection(p){return p.direction+(p.oscillate?p.sweep*Math.sin(p.t*p.oscillationHz*2*Math.PI):0);}
export function stepPhysics(p){const {world,body:b,rope}=system(p);p.t+=DT;world.gravity.set(0,-p.gravity,0);b.linearDamping=1-Math.exp(-p.damping);b.angularDamping=1-Math.exp(-p.damping*1.5);
 const previous=b.velocity.clone();
 // Four surface regions integrate orientation-dependent aerodynamic drag.
 // Force acts at each region, so gusts and off-centre flow also produce torque.
 const angle=fanDirection(p)*Math.PI/180,base=p.direction*Math.PI/180,dir=new Vec3(Math.cos(angle),0,Math.sin(angle)),origin=new Vec3(-.10*Math.cos(base),p.fanHeight,-.10*Math.sin(base));
 const speed=p.mode==='fan'?(p.wind||8):p.wind,normal=b.quaternion.vmult(new Vec3(0,0,1)),r=p.diameter/2;
 for(const [x,y] of [[-.5,-.5],[-.5,.5],[.5,-.5],[.5,.5]]){
  const point=b.quaternion.vmult(new Vec3(x*r,y*r,0)),position=b.position.vadd(point),cross=new Vec3();b.angularVelocity.cross(point,cross);const velocity=b.velocity.vadd(cross);
  const fromFan=position.vsub(origin),along=fromFan.dot(dir),perpendicular=-Math.sin(angle)*fromFan.x+Math.cos(angle)*fromFan.z;
  const stream=along>=0?Math.exp(-((position.y-p.fanHeight)**2+perpendicular**2)/(.035**2))/(1+Math.max(0,along-.10)*4):0;
  const gust=1+p.gust*(.6*Math.sin(p.t*7+position.y*8)+.4*Math.sin(p.t*13.3+position.z*11));
  const relative=dir.scale(speed*gust*stream).vsub(velocity),v=relative.length();if(v<1e-6)continue;
  const alignment=Math.abs(normal.dot(relative)/v),area=(Math.PI*r*r*alignment+p.diameter*p.thickness*Math.sqrt(Math.max(0,1-alignment*alignment)))/4;
  b.applyForce(relative.scale(.5*1.225*1.17*area*v),point);
 }
 // The pointer is a damped hand target, not a position constraint. A firm
 // grip supports the centre and holds the orientation captured at pickup.
 // A point grip applies force at the selected shell point and permits rotation.
 if(p.drag){
  const d=Array.isArray(p.drag)?{target:[p.drag[0],p.drag[1],p.drag[2]||0],local:[0,0,0]}:p.drag;
  const offset=b.quaternion.vmult(vec(d.local)),at=b.position.vadd(offset),velocity=new Vec3();b.angularVelocity.cross(offset,velocity);velocity.vadd(b.velocity,velocity);
  const firm=p.grip==='firm',omega=Math.sqrt(500*p.response),k=p.mass*omega*omega,damp=2*p.mass*omega;
  const force=vec(d.target).vsub(at).scale(k).vsub(velocity.scale(damp));force.y+=p.mass*p.gravity;
  const maximum=p.mass*45;if(force.length()>maximum)force.scale(maximum/force.length(),force);
  b.applyForce(force,firm?new Vec3():offset);
  const angularLocal=b.quaternion.conjugate().vmult(b.angularVelocity),torqueLocal=new Vec3();
  if(firm&&d.orientation){
   const error=b.quaternion.conjugate().mult(new Quaternion(...d.orientation));if(error.w<0){error.x*=-1;error.y*=-1;error.z*=-1;error.w*=-1;}
   const sin=Math.hypot(error.x,error.y,error.z),angle=2*Math.atan2(sin,Math.max(0,error.w)),scale=sin>1e-8?angle/sin:2;
   torqueLocal.set(b.inertia.x*(400*error.x*scale-40*angularLocal.x),b.inertia.y*(400*error.y*scale-40*angularLocal.y),b.inertia.z*(400*error.z*scale-40*angularLocal.z));
  }else torqueLocal.set(-b.inertia.x*5*angularLocal.x,-b.inertia.y*5*angularLocal.y,-b.inertia.z*5*angularLocal.z);
  b.torque.vadd(b.quaternion.vmult(torqueLocal),b.torque);
 }
 // Track twist independently of quaternion wrap. The cord exerts a small
 // torsional spring and damping torque about its current attachment axis.
 if(rope&&!p.lockFacing){
  const attachment=b.quaternion.vmult(new Vec3(0,r,0)).vadd(b.position),axis=rope.bodyA.position.vsub(attachment);axis.normalize();
  const swing=new Quaternion();swing.setFromVectors(new Vec3(0,1,0),axis);
  const relative=swing.conjugate().mult(b.quaternion),angle=2*Math.atan2(relative.y,relative.w);
  p.twist+=Math.atan2(Math.sin(angle-p.twistAngle),Math.cos(angle-p.twistAngle));p.twistAngle=angle;
  const taut=attachment.distanceTo(rope.bodyA.position)>=p.length-.0001;
  const stiffness=p.twistStiffness*1e-6*(.10/p.length)*(p.cordDiameter/.001)**4,damping=3e-6*(taut?1:.1),torque=-(stiffness*p.twist+damping*b.angularVelocity.dot(axis))*(taut?1:.1);
  b.torque.vadd(axis.scale(torque),b.torque);
 }
 if(p.tether==='fixed'){b.position.set(0,0,0);b.quaternion.set(0,0,0,1);b.velocity.setZero();}
 world.step(DT);
 let specific=b.velocity.vsub(previous).scale(1/DT).vsub(world.gravity);
 if(p.tether==='fixed')specific=new Vec3(0,p.gravity,0);
 if(p.mode==='walk'){const w=p.cadence*2*Math.PI;specific.y+=.036*w*w*Math.sin(w*p.t);}
 const local=b.quaternion.conjugate().vmult(specific);p.accel=arr(local).map(v=>Math.max(-128,Math.min(127,Math.round(v/9.80665*32))));
 [p.x,p.y,p.z]=arr(b.position);[p.vx,p.vy,p.vz]=arr(b.velocity);p.q=[b.quaternion.x,b.quaternion.y,b.quaternion.z,b.quaternion.w];p.av=arr(b.angularVelocity);p.theta=-2*Math.atan2(p.q[2],p.q[3]);p.phi=2*Math.atan2(p.q[1],p.q[3]);p.contacts=world.contacts.length;p.tension=rope?.eq.enabled?Math.abs(rope.eq.multiplier):0;
 p.kinetic=.5*p.mass*b.velocity.lengthSquared()+.5*(b.inertia.x*p.av[0]**2+b.inertia.y*p.av[1]**2+b.inertia.z*p.av[2]**2);
 return p.accel;
}
export function advancePhysics(p){for(let i=0;i<16;i++)stepPhysics(p);return p.accel;}
export function pose(p){return {environment:p.environment,x:p.x,y:p.y+(p.mode==='walk'?-.036*Math.sin(p.cadence*2*Math.PI*p.t):0),z:p.z,q:p.q,av:p.av,theta:p.theta,phi:p.phi,t:p.t,fan:p.mode==='fan'?(p.wind||8):p.wind,fanHeight:p.fanHeight,tether:p.tether,length:p.length,cordDiameter:p.cordDiameter,diameter:p.diameter,clip:p.clip,direction:p.direction,fanDirection:fanDirection(p),twist:p.twist,contacts:p.contacts,kinetic:p.kinetic,tension:p.tension};}
