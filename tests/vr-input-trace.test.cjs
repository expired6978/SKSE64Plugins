// Executes the JS-compatible AS2 method bodies against a small CLIK/Mouse
// surrogate. This verifies wrapper semantics, not GFx engine hit testing.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../tools/vr-racesex-patches/InputTrace.as.inc'), 'utf8');
// Structural guardrail only; this is not a native/runtime re-entrancy assay.
const nativeSource = fs.readFileSync(path.join(__dirname, '../skee64/RaceSexMenuVRInput.cpp'), 'utf8');
const nativeRead = nativeSource.slice(nativeSource.indexOf('} else if (action == kRead) {'), nativeSource.indexOf('if (p.retVal) p.retVal->SetBoolean(true);'));
assert.ok(nativeRead.indexOf('guard.unlock();') < nativeRead.indexOf('movie->Invoke('));
const copying = nativeRead.slice(nativeRead.indexOf('guard.lock();'), nativeRead.lastIndexOf('guard.unlock();'));
assert.doesNotMatch(copying, /movie->|p\.movie->|p\.thisPtr->/);
assert.match(nativeRead, /target_inspection_error/);
assert.match(nativeRead, /targetSnapshotError/);
// Explicit method boundaries avoid changing nested wrapper functions.
const objectSource = source.replace(/^   function (\w+)\(/gm, '$1(').replace(/^   }[ \t]*$/gm, '},');
let now = 0, begins = 0, ends = 0, rows = [], recordOk = true;
global.getTimer = () => now;
global.setInterval = () => 123;
global.clearInterval = () => {};
global._root = {_target:'/', _xmouse:50, _ymouse:60, localToGlobal() {}, getDepth:()=>0};
global._global = {skse:{IsVR:()=>true, plugins:{CharGen:{
  BeginVRInputTrace() { begins++; return true; },
  RecordVRMovieInput(...args) { rows.push(args); return recordOk; },
  ReadVRInputTrace() { return true; },
  EndVRInputTrace() { ends++; return true; }
}}}};
const listeners = new Set();
global.Mouse = {addListener:l=>listeners.add(l), removeListener:l=>listeners.delete(l)};
// Object method syntax accepts the AS2 bodies after only method declaration
// and separating-comma conversion; no wrapper logic is reimplemented here.
const prototype = Function('return ({' + objectSource + '\n})')();
function receiver(target) {
  return {_target:target, _xmouse:10, _ymouse:20, state:'up', getDepth:()=>5, hitTest:()=>true};
}
function fixture() {
  const owner = Object.assign(Object.create(prototype), {
    colorField:{_visible:false}, textEntry:{_visible:false}, makeupPanel:{_visible:false},
    vrFilterButton:receiver('/filter'), vrNameButton:receiver('/name'),
    searchWidget:receiver('/search'), racePanel:receiver('/panel'),
    bottomBar:{playerInfo:receiver('/playerInfo')}, navPanel:{buttons:[]}
  });
  owner.searchWidget.isDisabled = false;
  owner.searchWidget.textField = receiver('/search/text');
  owner.searchWidget.icon = receiver('/search/icon');
  owner.vrFilterButton._listeners = {click:[{listenerFunction:'onVRFilterButtonClicked'}]};
  owner.vrNameButton._listeners = {click:[{listenerFunction:'onVRNameButtonClicked'}]};
  return owner;
}
const owner = fixture();
// Self-inspection is bounded and does not dispatch, arm, focus or alter routing.
const inspected = fixture();
let inspectedPresses = 0;
inspected.vrFilterButton.onPress = () => ++inspectedPresses;
inspected.vrFilterButton.getBounds = target => {
  assert.equal(target, _root);
  return {xMin:100,xMax:200,yMin:10,yMax:30};
};
inspected.vrFilterButton.hitTest = (x,y,shape) => {
  assert.deepEqual([x,y,shape], [150,20,true]);
  return true;
};
inspected.navPanel.buttons = Array.from({length:10}, (_,i)=>receiver('/stock'+i));
const beginsBeforeInspection = begins;
assert.equal(inspected.ReadVRInputTargetSnapshot(), true);
assert.equal(inspected.vrInputTargetSnapshot.length, 14);
const inspectedFilter = inspected.vrInputTargetSnapshot.find(r=>r.role==='filter');
assert.equal(inspectedFilter.onPressKind, 'function');
assert.equal(inspectedFilter.onReleaseKind, 'undefined');
assert.equal(inspectedFilter.centerShapeHit, true);
assert.equal(inspectedPresses, 0);
assert.equal(begins, beginsBeforeInspection);
assert.equal(inspected.searchWidget.isDisabled, false);
assert.equal(inspected.vrFilterButton._listeners.click[0].listenerFunction, 'onVRFilterButtonClicked');
assert.equal(listeners.size, 0);
const filter = owner.vrFilterButton;
let calls = [];
const original = filter.onPress = function(...args) { calls.push({receiver:this,args}); return 'original-result'; };
// Disabled observation records nothing, even when called directly.
owner.RecordVRInputHandler(filter, 'onPress', [0,0,0]);
assert.equal(rows.length, 0);
assert.equal(owner.ArmVRInputTrace(), true);
assert.equal(owner.searchWidget.isDisabled, true);
assert.equal(owner.vrFilterButton._listeners.click[0].listenerFunction, 'onZoomClicked');
assert.equal(filter.onPress(2,7,0), 'original-result');
assert.equal(calls.length, 1);
assert.equal(calls[0].receiver, filter);
assert.deepEqual(calls[0].args, [2,7,0]);
assert.equal(rows[0][0], 'onPress');
assert.equal(rows[0][1], '/filter');
assert.equal(rows[0][4], 2);
const wrapped = filter.onPress;
owner.WrapVRInputHandler(filter, 'onPress');
assert.equal(filter.onPress, wrapped, 'duplicate receiver/method must not be wrapped twice');
assert.equal(owner.ArmVRInputTrace(), false);
assert.equal(begins, 1);
assert.equal(filter.onPress, wrapped);
// Observer rejection is visible but cannot suppress original dispatch.
recordOk = false;
assert.equal(filter.onPress(0,0,0), 'original-result');
assert.equal(owner.vrInputTraceFault, 'native_record_rejected');
recordOk = true;
assert.equal(owner.DisarmVRInputTrace(), true);
assert.equal(filter.onPress, original);
assert.equal(owner.searchWidget.isDisabled, false);
assert.equal(owner.vrFilterButton._listeners.click[0].listenerFunction, 'onVRFilterButtonClicked');
assert.equal(owner.vrNameButton._listeners.click[0].listenerFunction, 'onVRNameButtonClicked');
assert.equal(listeners.size, 0);
// A noisy original dispatch still runs; only its observation is suppressed.
const broad = fixture();
let dispatches = 0, zooms = 0;
broad._target = '/owner'; broad.getDepth = () => 1;
broad.onZoomClicked = () => ++zooms;
broad.handleInput = function(event) { return this.onZoomClicked(event); };
broad.vrFilterButton.dispatchEvent = function() { dispatches++; return 'dispatched'; };
assert.equal(broad.ArmVRInputTrace(), true);
const beforeNoise = rows.length;
assert.equal(broad.vrFilterButton.dispatchEvent({type:'stateChange'}), 'dispatched');
assert.equal(dispatches, 1);
assert.equal(rows.length, beforeNoise);
assert.equal(broad.vrInputTraceIgnoredStateChanges, 1);
assert.equal(broad.handleInput({controllerIdx:0,details:{code:42,value:'keyDown',navEquivalent:'enter'}}), 1);
assert.equal(zooms, 1);
assert.equal(rows[beforeNoise][0], 'handleInput');
assert.match(rows[beforeNoise][9], /code=42;value=keyDown;nav=enter/);
assert.equal(rows[beforeNoise + 1][0], 'onZoomClicked');
assert.equal(broad.DisarmVRInputTrace(), true);
const retained = rows.length;
filter.onPress(0,0,0);
assert.equal(rows.length, retained);
assert.equal(owner.DisarmVRInputTrace(), false);
const failure = new Error('original exception');
filter.onPress = function() { throw failure; };
assert.equal(owner.ArmVRInputTrace(), true);
assert.throws(()=>filter.onPress(0,0,0), e=>e===failure);
now = 90000;
owner.CheckVRInputTraceDeadline();
assert.equal(owner.vrInputTraceArmed, false);
assert.equal(listeners.size, 0);
assert.equal(ends, 3);
const aborted = fixture();
aborted.WrapVRInputHandler = () => { throw new Error('installation failed'); };
assert.equal(aborted.ArmVRInputTrace(), false);
assert.equal(aborted.vrInputTraceArmed, false);
assert.equal(aborted.searchWidget.isDisabled, false);
assert.equal(aborted.vrFilterButton._listeners.click[0].listenerFunction, 'onVRFilterButtonClicked');
assert.equal(aborted.vrNameButton._listeners.click[0].listenerFunction, 'onVRNameButtonClicked');
assert.equal(listeners.size, 0);
// The stock-button assay survives the normal menu's clear/reflow cycle and
// restores the actual listener array, key-code contents and update function.
const stock = fixture();
const spare = Object.assign(receiver('/stock3'), {
  label:'', _visible:false, _x:0, _y:0, _keyCodes:[256],
  _listeners:{click:[{listenerFunction:'onChooseColorClicked'}]},
  setButtonData(data) { this.label=data.text; this._keyCodes.splice(0); }
});
const originalClicks = spare._listeners.click;
const originalKeys = spare._keyCodes;
stock.navPanel.buttons = [receiver('/stock0'),receiver('/stock1'),receiver('/stock2'),spare];
stock.navPanel._buttonCount = 3;
let stockRefreshes = 0;
const stockUpdate = stock.navPanel.doUpdateButtons = function() {
  ++stockRefreshes; spare.label=''; spare._visible=false; spare._x=0; return 'updated';
};
assert.equal(stock.ArmVRStockButtonAssay(), true);
assert.equal(spare.label, 'Filter Test');
assert.equal(spare._visible, true);
assert.equal(spare._listeners.click[0].listenerObject, stock);
assert.equal(spare._listeners.click[0].listenerFunction, 'onZoomClicked');
assert.equal(stock.navPanel.doUpdateButtons(), 'updated');
assert.equal(stockRefreshes, 1);
assert.equal(spare.label, 'Filter Test');
assert.equal(spare._x, 220);
assert.equal(stock.DisarmVRInputTrace(), true);
assert.equal(stock.navPanel.doUpdateButtons, stockUpdate);
assert.equal(spare._listeners.click, originalClicks);
assert.equal(spare._keyCodes, originalKeys);
assert.deepEqual(spare._keyCodes, [256]);
assert.equal(spare.label, '');
assert.equal(spare._visible, false);
assert.equal(spare._x, 0);
assert.equal(stock.vrStockButtonAssayActive, false);
assert.equal(listeners.size, 0);
// A newly available real action must retain its fresh state and only its real
// listener; neither the test Zoom listener nor stale keys may survive takeover.
stock.navPanel.doUpdateButtons = function() {
  return 'claimed';
};
const takeoverUpdate = stock.navPanel.doUpdateButtons;
assert.equal(stock.ArmVRStockButtonAssay(), true);
// Match actual updateBottomBar order: addButton/addEventListener first, then
// updateButtons(true) invokes doUpdateButtons and its assay wrapper.
stock.navPanel._buttonCount = 4;
spare.label='Choose Color'; spare._visible=true; spare._x=190;
spare._keyCodes.splice(0); spare._keyCodes.push(999);
spare._listeners.click.push({listenerFunction:'onChooseColorClicked'});
assert.equal(stock.navPanel.doUpdateButtons(), 'claimed');
assert.equal(stock.vrInputTraceArmed, false);
assert.equal(stock.vrStockButtonAssayActive, false);
assert.equal(stock.navPanel.doUpdateButtons, takeoverUpdate);
assert.equal(spare.label, 'Choose Color');
assert.equal(spare._visible, true);
assert.equal(spare._x, 190);
assert.deepEqual(spare._keyCodes, [999]);
assert.equal(spare._listeners.click.some(l=>l.listenerFunction==='onZoomClicked'), false);
assert.equal(spare._listeners.click.at(-1).listenerFunction, 'onChooseColorClicked');
assert.equal(listeners.size, 0);
// An already occupied spare slot must be rejected without arming any observer.
spare.label='Choose Color'; spare._visible=true;
const preRejectedBegins = begins;
assert.equal(stock.ArmVRStockButtonAssay(), false);
assert.equal(begins, preRejectedBegins);
// A hidden but claimed slot is also occupied.
spare.label=''; spare._visible=false;
assert.equal(stock.ArmVRStockButtonAssay(), false);
assert.equal(begins, preRejectedBegins);
// Replacement keeps the new clip untouched and restores the old owned clip.
stock.navPanel._buttonCount=3;
stock.navPanel.doUpdateButtons=stockUpdate;
assert.equal(stock.ArmVRStockButtonAssay(), true);
const replacement = {label:'Replacement',_visible:true};
stock.navPanel.buttons[3]=replacement;
stock.PlaceVRStockButtonAssay();
assert.equal(stock.vrInputTraceArmed, false);
assert.equal(spare.label, '');
assert.equal(spare._visible, false);
assert.deepEqual(replacement, {label:'Replacement',_visible:true});
// Preserve original refresh exceptions while immediately cleaning the assay.
stock.navPanel.buttons[3]=spare;
const refreshError = new Error('refresh failed');
stock.navPanel.doUpdateButtons = () => {throw refreshError;};
assert.equal(stock.ArmVRStockButtonAssay(), true);
assert.throws(()=>stock.navPanel.doUpdateButtons(), e=>e===refreshError);
assert.equal(stock.vrInputTraceArmed, false);
assert.equal(spare.label, '');
assert.equal(listeners.size, 0);
console.log('PASS: AS2 surrogate wrappers, restoration, deadline and refresh-safe stock-button assay. GFx targeting unverified.');
