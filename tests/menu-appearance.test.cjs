// Executes the actual AS2 adapter functions with a Scaleform surrogate. Does
// not qualify Skyrim's texture loader or physical headset projection.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const source = fs.readFileSync(require('node:path').join(__dirname,'../tools/vr-racesex-patches/Appearance.as.inc'),'utf8');
const objectSource = source.replace(/^   function (\w+)\(/gm,'$1(').replace(/^   }[ \t]*$/gm,'},');
const prototype = Function('return ({'+objectSource+'\n})')();
let colours=[], timers=[], cleared=[], unloads=0, loader;
global.Color = class { constructor(clip){this.clip=clip;} setRGB(rgb){colours.push([this.clip,rgb]);} };
global.TextField = class { constructor(parent,color){this._parent=parent;this.textColor=color;} };
global.MovieClip = class {};
global.MovieClipLoader = class {
  constructor(){loader=this;}
  addListener(listener){this.listener=listener;}
  removeListener(listener){assert.equal(listener,this.listener);this.listenerRemoved=true;}
  loadClip(url,target){this.url=url;this.target=target;return true;}
  unloadClip(target){assert.equal(target,this.target);this.unloaded=true;}
};
global.setInterval = (...args)=>{timers.push(args);return 7;};
global.clearInterval = id=>cleared.push(id);
let now=0;
global.getTimer = ()=>now;
const api={menuBackgroundColor:-1,menuTextColor:-1,menuBackgroundImage:'',
  menuBackgroundImageWidth:1200,menuBackgroundImageHeight:800,MountMenuBackgroundImage:()=>true,
  GetMenuBackgroundImageState:()=>2,GetMenuBackgroundProjectionScale:()=>1};
global._global={skse:{plugins:{CharGen:api}}};
function fixture(sx=.7598724365,sy=1.357345581,ancestorX=1,ancestorY=1){
  const o=Object.assign(new MovieClip(),prototype);
  const panel=o.racePanel=new MovieClip();panel._parent=o;
  panel.tintCount=new TextField(panel,0xFFFFFF);
  const bg=panel.ListBackground=new MovieClip();bg._parent=panel;
  Object.assign(bg,{_x:0,_y:0,_rotation:0,_xscale:sx*100,_yscale:sy*100,
    getDepth:()=>-16383,getBounds:()=>({xMin:0,xMax:562.45,yMin:0,yMax:754.4}),
    localToGlobal(p){p.x=80+p.x*sx*ancestorX;p.y=20+p.y*sy*ancestorY;}});
  panel.getInstanceAtDepth=()=>undefined;
  panel.createEmptyMovieClip=(name,depth)=>{
    assert.equal(depth,-16382);
    const image=new MovieClip();image._parent=panel;
    image.getBounds=()=>({xMin:0,xMax:1200,yMin:0,yMax:800});
    image.removeMovieClip=()=>{image.removed=true;};
    panel[name]=image;return image;
  };
  o.onUnload=()=>unloads++;
  panel.neutral=new TextField(panel,0xFFFFFF);
  panel.grey=new TextField(panel,0x999999);
  panel.active=new TextField(panel,0x00FF00);
  panel.owner=o; // Must not recurse through model/owner references.
  return o;
}
const stock=fixture();stock.SetupVRMenuAppearance();
assert.equal(stock.vrBackgroundImageState,'absent');assert.equal(colours.length,0);assert.equal(timers.length,1);
assert.equal(stock.racePanel.neutral.textColor,0xFFFFFF);
// Shared/profile overrides leave defaults untouched until explicitly supplied.
const profileFixture=fixture();
profileFixture.itemList={selectedClip:{selectIndicator:new MovieClip()}};
api.menuProfiles={'VR Normal':{backgroundColor:0x123456,backgroundOpacity:70,textColor:0xABCDEF,textOpacity:80,accentColor:0xFF8800,accentOpacity:90,selectionColor:0x445566,selectionOpacity:30,borderColor:-1,panelScale:-9999,panelX:-9999,panelY:-9999,colorPickerScale:-9999,consolidate:0,imageOpacity:50}};
profileFixture.SetupVRMenuAppearance();profileFixture.ApplyVRTextColor();
assert.equal(profileFixture.racePanel.ListBackground._alpha,70);
assert.equal(profileFixture.racePanel.neutral.textColor,0xABCDEF);
assert.equal(profileFixture.racePanel.neutral._alpha,80);
assert.equal(profileFixture.racePanel.active.textColor,0xFF8800);
assert.equal(profileFixture.racePanel.active._alpha,90);
assert.ok(colours.some(([clip,color])=>clip===profileFixture.itemList.selectedClip.selectIndicator&&color===0x445566));
assert.equal(profileFixture.itemList.selectedClip.selectIndicator._alpha,30);
api.menuProfiles['VR Normal'].textColor=0xFEDCBA;
profileFixture.ApplyVRMenuProfile('VR Normal');profileFixture.ApplyVRTextColor();
assert.equal(profileFixture.racePanel.neutral.textColor,0xFEDCBA,'neutral profile colours can change without overwriting semantics');
delete api.menuProfiles;
api.menuBackgroundColor=0;api.menuTextColor=0xE8D8B8;api.menuBackgroundImage='img://RaceMenuNGBackground';
for(const [sx,sy,ax,ay] of [[.7598724365,1.357345581,1,1],[.5,2,3,1],[1,1,1,2]]){
  const o=fixture(sx,sy,ax,ay);o.SetupVRMenuAppearance();
  assert.equal(o.vrBackgroundImageState,'loading');
  assert.equal(colours.at(-1)[0],o.racePanel.ListBackground); // PNG is a sibling, never tinted.
  assert.equal(colours.at(-1)[1],0);
  assert.equal(loader.url,api.menuBackgroundImage);
  loader.listener.onLoadInit(loader.target);
  const image=o.vrBackgroundImage;
  assert.equal(o.vrBackgroundImageState,'loaded');
  assert.ok(Math.abs(image._width*ax/(image._height*ay)-1.5)<1e-9,'final PNG aspect preserved');
  assert.ok(image._width<=562.45*sx+1e-8 && image._height<=754.4*sy+1e-8,'contained in panel');
  assert.ok(Math.abs(image._x+image._width/2-562.45*sx/2)<1e-8,'centred X');
  assert.ok(Math.abs(image._y+image._height/2-754.4*sy/2)<1e-8,'centred Y');
  assert.equal(o.racePanel.neutral.textColor,api.menuTextColor);
  assert.equal(o.racePanel.grey.textColor,api.menuTextColor);
  assert.equal(o.racePanel.active.textColor,0x00FF00);
  o.racePanel.dynamic=new TextField(o.racePanel,0xFFFFFF);o.ApplyVRTextColor();
  assert.equal(o.racePanel.dynamic.textColor,api.menuTextColor);
  o.onUnload();assert.ok(loader.listenerRemoved && loader.unloaded);assert.equal(cleared.at(-1),7);
}
const bad=fixture();bad.SetupVRMenuAppearance();loader.listener.onLoadError(loader.target,'failed');
assert.equal(bad.vrBackgroundImageState,'error: failed');assert.ok(bad.vrBackgroundImage.removed);
const placeholder=fixture();placeholder.SetupVRMenuAppearance();
loader.target.getBounds=()=>({xMin:0,xMax:57,yMin:0,yMax:36});
loader.listener.onLoadInit(loader.target);
assert.equal(placeholder.vrBackgroundImageState,'error: unexpected image dimensions');
assert.ok(placeholder.vrBackgroundImage.removed);
api.MountMenuBackgroundImage=()=>false;
api.GetMenuBackgroundImageState=()=>3;
const nativeFailure=fixture();nativeFailure.SetupVRMenuAppearance();
assert.equal(nativeFailure.vrBackgroundImageState,'error: native texture registration');
assert.equal(nativeFailure.vrBackgroundImage,undefined);
api.MountMenuBackgroundImage=()=>true;
api.GetMenuBackgroundImageState=()=>2;
api.menuBackgroundImageWidth=57;api.menuBackgroundImageHeight=36;
const realSmall=fixture();realSmall.SetupVRMenuAppearance();
loader.target.getBounds=()=>({xMin:0,xMax:57,yMin:0,yMax:36});
loader.listener.onLoadInit(loader.target);assert.equal(realSmall.vrBackgroundImageState,'loaded');
api.menuBackgroundClipWidth=64;api.menuBackgroundClipHeight=64;
api.menuBackgroundImageWidth=855;api.menuBackgroundImageHeight=1634;
const registeredVR=fixture();registeredVR.SetupVRMenuAppearance();
loader.target.getBounds=()=>({xMin:0,xMax:64,yMin:0,yMax:64});
loader.listener.onLoadInit(loader.target);
assert.equal(registeredVR.vrBackgroundImageState,'loaded','VR registered-image wrapper accepted');
assert.equal(registeredVR.vrBackgroundImageWidth,64,'clip bounds recorded separately');
assert.ok(Math.abs(registeredVR.vrBackgroundImage._width/registeredVR.vrBackgroundImage._height-855/1634)<1e-9,'PNG aspect retained despite square wrapper');
const wrongWrapper=fixture();wrongWrapper.SetupVRMenuAppearance();
loader.target.getBounds=()=>({xMin:0,xMax:57,yMin:0,yMax:36});
loader.listener.onLoadInit(loader.target);
assert.equal(wrongWrapper.vrBackgroundImageState,'error: unexpected image dimensions');
delete api.menuBackgroundClipWidth;delete api.menuBackgroundClipHeight;
api.menuBackgroundImageWidth=1200;api.menuBackgroundImageHeight=800;
assert.equal(unloads,3);
// World-surface widening is compensated in the PNG alone, not the controls.
for (const projection of [0.5, 16/9, 2]) for (const [sx,sy,ax,ay] of [[.7598724365,1.357345581,1,1],[.5,2,3,1]]) {
  api.GetMenuBackgroundProjectionScale=()=>projection;
  const o=fixture(sx,sy,ax,ay);o.SetupVRMenuAppearance();loader.listener.onLoadInit(loader.target);
  const image=o.vrBackgroundImage;
  assert.ok(Math.abs(image._width*ax*projection/(image._height*ay)-1.5)<1e-9,'physical PNG aspect preserved');
  assert.ok(image._width<=562.45*sx+1e-8 && image._height<=754.4*sy+1e-8,'projected image contained');
  assert.ok(Math.abs(image._x+image._width/2-562.45*sx/2)<1e-8,'projected image centred X');
  assert.ok(Math.abs(image._y+image._height/2-754.4*sy/2)<1e-8,'projected image centred Y');
  o.onUnload();
}
api.GetMenuBackgroundProjectionScale=()=>0;
const pendingProjection=fixture();pendingProjection.SetupVRMenuAppearance();
assert.equal(pendingProjection.vrBackgroundImage,undefined,'image waits for measured projection');
api.GetMenuBackgroundProjectionScale=()=>2;pendingProjection.CheckVRBackgroundMount();
assert.equal(pendingProjection.vrBackgroundProjectionScale,2);pendingProjection.onUnload();
api.GetMenuBackgroundProjectionScale=()=>-1;
const unsupportedProjection=fixture();unsupportedProjection.SetupVRMenuAppearance();
assert.equal(unsupportedProjection.vrBackgroundImageState,'error: unavailable VR surface projection');
assert.equal(unsupportedProjection.vrBackgroundImage,undefined,'no guessed physical image fit');unsupportedProjection.onUnload();
api.GetMenuBackgroundProjectionScale=()=>1;
api.MountMenuBackgroundImage=()=>false;
api.GetMenuBackgroundImageState=()=>1;
const pending=fixture();pending.SetupVRMenuAppearance();
assert.equal(pending.vrBackgroundImageState,'waiting: native texture registration');
assert.equal(pending.vrBackgroundImage,undefined,'no loader before native registration completes');
api.GetMenuBackgroundImageState=()=>2;pending.CheckVRBackgroundMount();
assert.equal(pending.vrBackgroundImageState,'loading');
pending.onUnload();
api.GetMenuBackgroundImageState=()=>1;
const timeout=fixture();timeout.SetupVRMenuAppearance();now=5000;timeout.CheckVRBackgroundMount();
assert.equal(timeout.vrBackgroundImageState,'error: native texture registration timeout');
assert.equal(timeout.vrBackgroundImage,undefined);timeout.onUnload();
now=0;
const closedPending=fixture();closedPending.SetupVRMenuAppearance();closedPending.onUnload();
api.GetMenuBackgroundImageState=()=>2;closedPending.CheckVRBackgroundMount();
assert.equal(closedPending.vrBackgroundImage,undefined,'unloaded menu never starts a late image load');
const occupied=fixture();occupied.racePanel.getInstanceAtDepth=()=>({});occupied.SetupVRMenuAppearance();
assert.equal(occupied.vrBackgroundImageState,'error: occupied background depth');occupied.onUnload();assert.equal(cleared.at(-1),7);
const missing=fixture();delete missing.racePanel.ListBackground;missing.SetupVRMenuAppearance();assert.equal(missing.vrAppearanceSetup,false);
console.log('Menu appearance adapter: defaults, RGB black, sibling colour isolation, final aspect/containment, dynamic text, fallback and cleanup passed');
