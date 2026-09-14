import * as T from './vendor/three.module.js';
import {GLTFLoader} from './vendor/loaders/GLTFLoader.js';
export function mountModel(host,lcd,onDrag,onButton){
 const scene=new T.Scene();
 const camera=new T.PerspectiveCamera(35,1,.1,100);camera.position.set(.0,.45,6.3);camera.lookAt(0,.05,0);
 const renderer=new T.WebGLRenderer({antialias:true,alpha:true});renderer.setPixelRatio(Math.min(devicePixelRatio,2));renderer.shadowMap.enabled=true;renderer.shadowMap.type=T.PCFShadowMap;renderer.setClearColor(0x000000,0);renderer.outputColorSpace=T.SRGBColorSpace;host.appendChild(renderer.domElement);
 scene.add(new T.HemisphereLight(0xf9f3df,0x354f43,3));const key=new T.DirectionalLight(0xffffff,4);key.position.set(-3,5,5);key.castShadow=true;key.shadow.mapSize.set(1024,1024);scene.add(key);
 const rim=new T.DirectionalLight(0x8ef5bf,2);rim.position.set(4,1,-2);scene.add(rim);
 const walker=new T.Group();scene.add(walker);walker.position.y=.12;
 const material=(color,metalness=0,roughness=.35)=>new T.MeshStandardMaterial({color,metalness,roughness});
 const red=material(0xce4337,.12,.27),white=material(0xe2dfd3,.12,.3),dark=material(0x303833,.15,.5),silver=material(0xb8b7af,.65,.3);
 for(const [start,color] of [[0,red],[Math.PI/2,white]]){const body=new T.Mesh(new T.SphereGeometry(1.65,64,32,0,Math.PI*2,start,Math.PI/2),color);body.scale.set(1,1,.36);body.castShadow=true;walker.add(body);}
 const seam=new T.Mesh(new T.TorusGeometry(1.65,.023,8,96),dark);seam.rotation.x=Math.PI/2;seam.scale.y=.36;walker.add(seam);
 function roundRect(w,h,r){const s=new T.Shape();s.moveTo(-w/2+r,-h/2);s.lineTo(w/2-r,-h/2);s.quadraticCurveTo(w/2,-h/2,w/2,-h/2+r);s.lineTo(w/2,h/2-r);s.quadraticCurveTo(w/2,h/2,w/2-r,h/2);s.lineTo(-w/2+r,h/2);s.quadraticCurveTo(-w/2,h/2,-w/2,h/2-r);s.lineTo(-w/2,-h/2+r);s.quadraticCurveTo(-w/2,-h/2,-w/2+r,-h/2);return s;}
 function panel(w,h,r,depth,mat,x,y,z){const m=new T.Mesh(new T.ExtrudeGeometry(roundRect(w,h,r),{depth,bevelEnabled:true,bevelSegments:3,bevelSize:.025,bevelThickness:.025,steps:1,curveSegments:12}),mat);m.position.set(x,y,z);m.castShadow=true;walker.add(m);return m;}
 panel(2.51,1.77,.16,.09,dark,0,.15,.45);
 panel(2.27,1.51,.06,.015,material(0xa9ad8c),0,.16,.575);
 const texture=new T.CanvasTexture(lcd);texture.minFilter=T.NearestFilter;texture.magFilter=T.NearestFilter;texture.colorSpace=T.SRGBColorSpace;
 const screen=new T.Mesh(new T.PlaneGeometry(2.13,1.42),new T.MeshBasicMaterial({map:texture}));screen.position.set(0,.16,.615);walker.add(screen);
 function label(text,x,y,z,w,h,color='#f1e7d4',size=32){const c=document.createElement('canvas');c.width=512;c.height=128;const ctx=c.getContext('2d');ctx.fillStyle=color;ctx.font=`600 ${size}px system-ui`;ctx.textAlign='center';ctx.fillText(text,256,78);const tex=new T.CanvasTexture(c);tex.colorSpace=T.SRGBColorSpace;const mesh=new T.Mesh(new T.PlaneGeometry(w,h),new T.MeshBasicMaterial({map:tex,transparent:true,depthWrite:false}));mesh.position.set(x,y,z);walker.add(mesh);}
 label('Pokéwalker',0,1.20,.39,1.65,.42,'#ffede1',55);
 const buttons=[];for(const [x,mask,text] of [[-.67,4,'◀'],[0,2,'●'],[.67,8,'▶']]){const button=panel(.53,.32,.1,.075,silver,x,-.94,.46);button.userData.button=mask;buttons.push(button);label(text,x,-.94,.565,.36,.14,'#394139',60);}
 const ir=new T.Mesh(new T.SphereGeometry(.12,24,12),material(0x512326));ir.scale.set(1,.38,.5);ir.position.set(0,1.62,.0);walker.add(ir);
 const smallDot=new T.Mesh(new T.CircleGeometry(.075,24),dark);smallDot.position.set(0,-1.36,.34);walker.add(smallDot);
 const floor=new T.Mesh(new T.PlaneGeometry(100,100),new T.ShadowMaterial({opacity:.28}));floor.rotation.x=-Math.PI/2;floor.position.y=-1.73;floor.receiveShadow=true;scene.add(floor);
 const grid=new T.GridHelper(20,50,0x31483a,0x26362d);grid.position.y=-1.75;grid.material.transparent=true;grid.material.opacity=.25;scene.add(grid);
 // The visible fan drives the same force setting as the physics worker.
 const fan=new T.Group();fan.position.set(-2.0,-.90,-.1);fan.scale.setScalar(.42);fan.rotation.y=.35;scene.add(fan);
 const ring=new T.Mesh(new T.TorusGeometry(.7,.06,12,64),silver);fan.add(ring);
 const hub=new T.Mesh(new T.CylinderGeometry(.16,.16,.23,24),dark);hub.rotation.x=Math.PI/2;fan.add(hub);
 const blades=new T.Group();fan.add(blades);
 for(let i=0;i<3;i++){const blade=new T.Mesh(new T.SphereGeometry(.45,24,12),material(0x718e7b,.4,.35));blade.scale.set(.7,1,.08);blade.position.y=.32;const arm=new T.Group();arm.rotation.z=i*Math.PI*2/3;arm.add(blade);blades.add(arm);}
 const stand=new T.Mesh(new T.CylinderGeometry(.06,.09,.7,12),silver);stand.position.y=-1.04;fan.add(stand);
 const base=new T.Mesh(new T.BoxGeometry(.8,.12,.5),dark);base.position.y=-1.4;fan.add(base);
 const airGeo=new T.BufferGeometry();const airPositions=new Float32Array(90*3);airGeo.setAttribute('position',new T.BufferAttribute(airPositions,3));const air=new T.Points(airGeo,new T.PointsMaterial({color:0xc7dca7,size:.022,transparent:true,opacity:.55}));scene.add(air);
 new GLTFLoader().load('./assets/pokewalker.glb',gltf=>{
 const asset=gltf.scene;asset.rotation.y=Math.PI/2;asset.updateMatrixWorld(true);
 const meshes=[];asset.traverse(o=>{if(o.isMesh)meshes.push(o)});const baseMesh=meshes.find(o=>o.material.name==='Base');
 const box=new T.Box3().setFromObject(baseMesh),size=box.getSize(new T.Vector3()),center=box.getCenter(new T.Vector3());
 const scale=3.3/size.y;asset.scale.setScalar(scale);asset.position.copy(center).multiplyScalar(-scale);asset.updateMatrixWorld(true);
 const glass=meshes.find(o=>o.material.name==='screen_overcover__0');if(glass)glass.visible=false;
 const displayMesh=meshes.find(o=>o.material.name==='Screen');const screenBox=new T.Box3().setFromObject(displayMesh),screenSize=screenBox.getSize(new T.Vector3()),screenCenter=screenBox.getCenter(new T.Vector3());
 displayMesh.visible=false;
 const nativeScreen=new T.Mesh(new T.PlaneGeometry(screenSize.x*.99,screenSize.y*.99),new T.MeshBasicMaterial({map:texture}));nativeScreen.position.set(screenCenter.x,screenCenter.y,screenBox.max.z+.004);
 asset.traverse(o=>{if(o.isMesh){o.castShadow=true;if(o.material){o.material.metalness=0;o.material.roughness=.6;}}});
 walker.clear();walker.add(asset,nativeScreen);buttons.length=0;
 const keys=meshes.find(o=>o.material.name==='Buttons');if(keys){keys.userData.button='cluster';buttons.push(keys);}
 renderer.domElement.dataset.model='provided-glb';
 },undefined,error=>console.warn('Provided model could not load; using procedural model.',error));
 let state={theta:0,phi:0,bob:0,fan:0},down=false,start,dragged=false,pressed=0;
 const ray=new T.Raycaster();const point=new T.Vector2();
 const element=renderer.domElement;element.setAttribute('aria-label','Interactive 3D Pokéwalker. Drag to swing it; use the buttons below to play.');element.style.touchAction='none';
 element.addEventListener('pointerdown',ev=>{element.setPointerCapture(ev.pointerId);const r=element.getBoundingClientRect();point.set((ev.clientX-r.left)/r.width*2-1,-(ev.clientY-r.top)/r.height*2+1);ray.setFromCamera(point,camera);const hits=ray.intersectObjects(buttons);if(hits.length){pressed=hits[0].object.userData.button;if(pressed==='cluster'){const point=walker.worldToLocal(hits[0].point.clone());pressed=point.x<-.28?4:point.x>.28?8:2;}onButton(pressed);return;}down=true;dragged=false;start=[ev.clientX,ev.clientY];});
 element.addEventListener('pointermove',ev=>{if(!down)return;const dx=(ev.clientX-start[0])/130,dy=(ev.clientY-start[1])/130;if(Math.abs(dx)+Math.abs(dy)>.02)dragged=true;onDrag([Math.max(-1.2,Math.min(1.2,dx)),Math.max(-.8,Math.min(.8,dy))]);});
 function release(){if(down)onDrag(null);if(pressed)onButton(0);down=false;pressed=0;}
 element.addEventListener('pointerup',release);element.addEventListener('pointercancel',release);
 const resize=()=>{const w=host.clientWidth,h=host.clientHeight;renderer.setSize(w,h);camera.aspect=w/h;camera.updateProjectionMatrix();};new ResizeObserver(resize).observe(host);resize();
 function draw(t){walker.rotation.z=T.MathUtils.lerp(walker.rotation.z,-state.theta,.12);walker.rotation.y=T.MathUtils.lerp(walker.rotation.y,state.phi,.12);walker.position.x=T.MathUtils.lerp(walker.position.x,Math.sin(state.theta)*.43,.12);walker.position.y=T.MathUtils.lerp(walker.position.y,.12+state.bob+(1-Math.cos(state.theta))*.3,.12);blades.rotation.z=t*.018*state.fan;
 for(let i=0;i<90;i++){const progress=(t*.0003*(.3+state.fan)+i/90)%1;airPositions[i*3]=-2+progress*4.5;airPositions[i*3+1]=-.85+Math.sin(i*32.15)*.46+progress*.3;airPositions[i*3+2]=Math.cos(i*8.21)*.7;}air.visible=state.fan>0;airGeo.attributes.position.needsUpdate=true;renderer.render(scene,camera);requestAnimationFrame(draw);}
 requestAnimationFrame(draw);
 return {updatePose:p=>state=p,updateScreen:()=>texture.needsUpdate=true,renderer};
}
