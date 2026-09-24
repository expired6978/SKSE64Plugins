// SPDX-License-Identifier: GPL-3.0-or-later
const test=require('node:test'), assert=require('node:assert/strict'), fs=require('node:fs'), path=require('node:path');
const text=fs.readFileSync(path.join(__dirname,'..','packaging','skee64_custom.ini'),'utf8');
function parse(source){
  let section='', result={};
  for(const raw of source.split(/\r?\n/)){
    const line=raw.trim();
    if(!line||line.startsWith(';')) continue;
    const heading=line.match(/^\[([^\]]+)\]$/); if(heading){section=heading[1];continue;}
    const at=line.indexOf('='); assert.notEqual(at,-1,`invalid INI line: ${line}`);
    const key=`${section}/${line.slice(0,at)}`; assert.equal(result[key],undefined,`duplicate key: ${key}`);
    result[key]=line.slice(at+1);
  }
  return result;
}
test('published custom INI mirrors the qualified default template',()=>{
  const got=parse(text);
  const populated={
    'VR/bUseQuillSteamVR':'1','VR/bUseQuillOCU':'0','VR/bOverrideExistingPlayerName':'0',
    'Menu Profile Flat/bConsolidatePanel':'0','Menu Profile VR Normal/bConsolidatePanel':'1',
    'Menu Profile VR Normal/bForceVertical':'1','Menu Profile VR Normal/fAzimuth':'30',
    'Menu Profile VR Normal/fElevation':'0','Menu Profile VR Normal/fHeightOffset':'0',
    'Menu Profile VR Normal/fDistance':'100','Menu Profile VR Normal/fSurfaceScale':'100',
    'Menu Profile VR Normal/fSculptAzimuth':'48','Menu Profile VR Normal/fColorPickerAzimuth':'38',
    'Menu Profile VR Normal/fColorPickerElevation':'0','Menu Profile VR Normal/fColorPickerHeightOffset':'0',
    'Menu Profile VR Normal/fColorPickerDistance':'100','Menu Profile VR Normal/fColorPickerSurfaceScale':'100',
    'Menu Profile VR Face/bConsolidatePanel':'1','Menu Profile VR Face/fFaceDistance':'45',
    'Menu Profile VR Face/fFaceEyeHeight':'5'
  };
  for(const [key,value] of Object.entries(got)) if(value!=='') assert.equal(value,populated[key],`non-default value: ${key}`);
  assert.deepEqual(Object.fromEntries(Object.entries(got).filter(([,v])=>v!=='')),populated);
  assert.equal(got['VR/sPlayerName'],'');
  assert.equal(got['VR Lighting/bEnableFrontRig'],undefined); // omission deliberately selects native defaults
});
