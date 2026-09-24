// SPDX-License-Identifier: GPL-3.0-or-later
'use strict';
const {test} = require('node:test');
const assert = require('node:assert/strict');
const zlib = require('node:zlib');
const {unpack,make,apply,auditPatch} = require('../tools/swf-byte-patch.cjs');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
function swf(body, compressed=false) {
  const h = Buffer.alloc(8); h.write(compressed ? 'CWS' : 'FWS'); h[3]=15; h.writeUInt32LE(body.length+8,4);
  return Buffer.concat([h,compressed ? zlib.deflateSync(body) : body]);
}
const original = swf(Buffer.from('synthetic original graphics/action test bytes '.repeat(20)),true);
const wanted = swf(Buffer.from('synthetic original graphics/action test bytes '.repeat(9)+'new native extension!'+
  'synthetic original graphics/action test bytes '.repeat(11)));
const valid = make(original,wanted).patch;
function changed(at,value) { const b=Buffer.from(valid); b.writeUInt32LE(value,at); return b; }

test('release payload, review manifest and independent native pin agree',()=> {
  const manifest=require('../packaging/runtime-patches/racesex-menu.manifest.json');
  const patch=fs.readFileSync(path.join(__dirname,'../packaging/runtime-patches/racesex-menu.rmp'));
  assert.equal(crypto.createHash('sha256').update(patch).digest('hex'),manifest.patchSha256);
  assert.equal(patch.length,manifest.patchBytes);
  assert.equal(patch.toString('ascii',0,8),manifest.format);
  for(const [at,key] of [[8,'originalFileBytes'],[12,'canonicalOriginalBytes'],[16,'canonicalOutputBytes'],[20,'operations']])
    assert.equal(patch.readUInt32LE(at),manifest[key]);
  for(const [at,key] of [[24,'originalFileSha256'],[56,'canonicalOriginalSha256'],[88,'canonicalOutputSha256']])
    assert.equal(patch.subarray(at,at+32).toString('hex'),manifest[key]);
  const native=fs.readFileSync(path.join(__dirname,'../skee64/SwfBytePatch.cpp'),'utf8');
  const pin=native.match(/constexpr Hash patchHash\{([\s\S]*?)\};/)[1];
  assert.equal(Buffer.from(pin.match(/0x[0-9a-f]{2}/g).map(n=>Number(n))).toString('hex'),manifest.patchSha256);
  let p=120,copied=0,inserted=0,longest=0;
  for(let i=0;i<manifest.operations;i++) {
    const kind=patch[p++],n=patch.readUInt32LE(p);p+=4;
    if(kind===0) {copied+=patch.readUInt32LE(p);p+=4;}
    else {assert.equal(kind,1);inserted+=n;longest=Math.max(longest,n);p+=n;}
  }
  assert.equal(p,patch.length);
  assert.equal(copied,manifest.copiedOutputBytes);
  assert.equal(inserted,manifest.literalBytes);
  assert.equal(longest,manifest.longestLiteralRun);
});
test('deterministic, exact round trip and canonical CWS/FWS equivalence',()=> {
  assert.deepEqual(apply(original,valid),wanted);
  assert.deepEqual(make(original,wanted).patch,valid);
  assert.deepEqual(unpack(swf(Buffer.from('abc'),true)),swf(Buffer.from('abc')));
});
test('audit does not carry original four-byte windows in any literal run',()=> {
  const audit=auditPatch(original,valid);
  assert.equal(audit.originalFourByteWindowsInLiteralRuns,0);
  assert.ok(audit.literalBytes < wanted.length);
});
test('wrong file, edited original, hashes and release metadata rejected',()=> {
  assert.throws(()=>apply(swf(Buffer.from('unrelated')),valid));
  const b=Buffer.from(original); b[b.length-1]^=1; assert.throws(()=>apply(b,valid),/hash/);
  for (const at of [24,56,88]) { const p=Buffer.from(valid); p[at]^=1; assert.throws(()=>apply(original,p),/hash/); }
  for (const [at,n] of [[8,1],[12,0],[12,0xffffffff],[16,7],[16,0xffffffff],[20,0],[20,200001]])
    assert.throws(()=>apply(original,changed(at,n)));
});
test('every truncation and trailing patch bytes rejected',()=> {
  for (let n=0;n<valid.length;n++) assert.throws(()=>apply(original,valid.subarray(0,n)),String(n));
  assert.throws(()=>apply(original,Buffer.concat([valid,Buffer.from([0])])));
});
test('invalid opcodes, copy range, zero lengths, output overflow rejected',()=> {
  assert.equal(valid[120],0);
  const bad=Buffer.from(valid); bad[120]=255; assert.throws(()=>apply(original,bad));
  for (const [at,n] of [[121,0xffffffff],[125,0],[125,0xffffffff]]) assert.throws(()=>apply(original,changed(at,n)));
});
test('invalid SWF signatures, lengths, compressed trailing data and expansion rejected',()=> {
  for (const input of [Buffer.alloc(0),Buffer.from('ZWS12345'),Buffer.alloc(8),swf(Buffer.from('abc')).subarray(0,9)])
    assert.throws(()=>unpack(input));
  assert.throws(()=>unpack(Buffer.concat([original,Buffer.from([0])])));
  const bomb=swf(Buffer.alloc(8*1024*1024),true); bomb.writeUInt32LE(100,4);
  assert.throws(()=>unpack(bomb));
  const tooBig=Buffer.from(original); tooBig.writeUInt32LE(0xffffffff,4); assert.throws(()=>unpack(tooBig));
});
// Optional private-asset qualification. No upstream fixtures in the repository.
if (process.env.VR2_ORIGINAL_SWF && process.env.VR2_TARGET_SWF) test('private accepted movie exact reconstruction',()=> {
  const fs=require('node:fs');
  const source=fs.readFileSync(process.env.VR2_ORIGINAL_SWF), target=fs.readFileSync(process.env.VR2_TARGET_SWF);
  const {patch,audit}=make(source,target);
  assert.deepEqual(apply(source,patch),unpack(target));
  assert.equal(audit.literalBytes,8397);
  assert.equal(audit.canonicalOutputSha256,'56f8bce4204bab5c466c9c9cb6b9f1a1e5a49b7cadf4b40a504e46a8aeda638b');
  assert.deepEqual(patch,fs.readFileSync(require('node:path').join(__dirname,'../packaging/runtime-patches/racesex-menu.rmp')));
});
