// SPDX-License-Identifier: GPL-3.0-or-later
const test=require('node:test'), assert=require('node:assert/strict'), fs=require('node:fs');
const read=p=>fs.readFileSync(require('node:path').join(__dirname,'..',p),'utf8');
test('Light routes VR to owned rig, preserves non-VR and opt-out workflow',()=>{
  const recipe=read('tools/patch-vr-racesex-swf.ps1');
  assert.match(recipe,/Light toggle anchor not unique/);
  assert.match(recipe,/_global\.skse\.IsVR\(\) && _global\.skse\.plugins\.CharGen\.avatarLightingSupported/);
  assert.match(recipe,/SetAvatarLighting\(!this\.bShowLight\)/);
  const as=read('tools/vr-racesex-patches/Appearance.as.inc');
  assert.match(as,/SendModEvent\(_global\.eventPrefix \+ "ToggleLight","",0\)/);
  assert.match(as,/api\.UpdateAvatarLighting\(\)/);
});
test('Three non-shadow sources and close/revert cleanup, no controller/light hijack',()=>{
  const native=read('skee64/AvatarLighting.cpp');
  assert.doesNotMatch(native,/RemoveAllLights|firstPersonLight|thirdPersonLight|HmdNode|RoomNode/);
  assert.match(native,/params\.restrictedNode = node/);
  assert.match(native,/scene->RemoveLight\(light\.get\(\)\)/);
  assert.equal((read('skee64/CharacterCreationInterface.cpp').match(/AvatarLighting::Reset\(\)/g)||[]).length,2);
  for (const name of ['Key','Fill','Body'])
    for (const suffix of ['Right','Front','Height','Radius','Brightness'])
      assert.match(read('packaging/menu-profiles.ini.inc'),new RegExp('f'+name+suffix+'='));
});
test('Light clicks coalesce into a pending refresh instead of being rejected',()=>{
  const native=read('skee64/AvatarLighting.cpp');
  assert.match(native,/if \(!refresh\) requests\.SetDesired\(on\)/);
  assert.match(native,/if \(!requests\.TryQueue\(\)\) return true/);
  assert.match(native,/if \(!requests\.Desired\(\)\) Remove\(\)/);
  assert.doesNotMatch(native,/\[movie, on, refresh, epoch\]/);
});
test('VR lighting uses integer attenuation ABI and explicit spatial bounds',()=>{
  const native=read('skee64/AvatarLighting.cpp');
  assert.match(native,/using Function = void \(\*\)\(RE::NiPointLight\*, std::uint32_t\)/);
  assert.match(native,/GetLightRuntimeData\(\)\.radius = \{extent, extent, extent\}/);
  assert.match(native,/attenuation\(light, radius\)/);
  assert.doesNotMatch(native,/light->SetLightAttenuation\(/);
  assert.match(native,/SetRadius\(light, AvatarLightingPolicy::WorldRadius/);
});
