// SPDX-License-Identifier: GPL-3.0-or-later
'use strict';
const fs = require('node:fs');
const crypto = require('node:crypto');
const zlib = require('node:zlib');
const LIMIT = 8 * 1024 * 1024;
const HEADER = 120;
const digest = b => crypto.createHash('sha256').update(b).digest();
function unpack(b) {
  if (b.length < 8 || b.length > LIMIT || !['CWS', 'FWS'].includes(b.toString('ascii', 0, 3))) throw Error('Unsupported SWF');
  const length = b.readUInt32LE(4);
  if (length < 8 || length > LIMIT) throw Error('Invalid SWF length');
  const body = b[0] === 67 ? zlib.inflateSync(b.subarray(8), {maxOutputLength: LIMIT - 8, info: true}) : null;
  if (body && body.engine.bytesWritten !== b.length - 8) throw Error('Trailing compressed data');
  const out = Buffer.concat([Buffer.from(b.subarray(0, 8)), body ? body.buffer : b.subarray(8)]);
  out[0] = 70;
  if (out.length !== length) throw Error('SWF length mismatch');
  return out;
}
function indexSource(source) {
  const index = new Map();
  for (let i = 0; i + 4 <= source.length; i++) {
    const key = source.readUInt32LE(i);
    if (!index.has(key)) index.set(key, []);
    // Matching any four bytes is sufficient to exclude them from literals.
    // A bounded candidate set makes generation predictable for repetitive data.
    const offsets = index.get(key);
    if (offsets.length < 64) offsets.push(i);
  }
  return index;
}
function make(sourceFile, targetFile) {
  const source = unpack(sourceFile), target = unpack(targetFile);
  const index = indexSource(source);
  const ops = [];
  let literals = [];
  function flush() { if (literals.length) { ops.push({kind: 1, bytes: Buffer.from(literals)}); literals = []; } }
  for (let i = 0; i < target.length;) {
    let bestOffset = 0, bestLength = 0;
    const candidates = i + 4 <= target.length ? index.get(target.readUInt32LE(i)) : undefined;
    for (const offset of candidates || []) {
      let length = 4;
      while (offset + length < source.length && i + length < target.length && source[offset + length] === target[i + length]) length++;
      if (length > bestLength) { bestOffset = offset; bestLength = length; }
    }
    if (bestLength >= 4) {
      flush();
      const previous = ops.at(-1);
      if (previous?.kind === 0 && previous.offset + previous.length === bestOffset) previous.length += bestLength;
      else ops.push({kind: 0, offset: bestOffset, length: bestLength});
      i += bestLength;
    } else literals.push(target[i++]);
  }
  flush();
  const header = Buffer.alloc(HEADER);
  header.write('RMSWFP01', 0, 'ascii');
  header.writeUInt32LE(sourceFile.length, 8); header.writeUInt32LE(source.length, 12);
  header.writeUInt32LE(target.length, 16); header.writeUInt32LE(ops.length, 20);
  digest(sourceFile).copy(header, 24); digest(source).copy(header, 56); digest(target).copy(header, 88);
  const chunks = [header];
  for (const op of ops) {
    const record = Buffer.alloc(op.kind === 0 ? 9 : 5);
    record[0] = op.kind;
    record.writeUInt32LE(op.kind === 0 ? op.offset : op.bytes.length, 1);
    if (op.kind === 0) record.writeUInt32LE(op.length, 5);
    chunks.push(record);
    if (op.kind === 1) chunks.push(op.bytes);
  }
  const patch = Buffer.concat(chunks);
  const audit = auditPatch(sourceFile, patch);
  if (!apply(sourceFile, patch).equals(target)) throw Error('Generator round-trip mismatch');
  return {patch, audit};
}
function decode(patch) {
  if (patch.length < 120 || patch.length > LIMIT || patch.toString('ascii', 0, 8) !== 'RMSWFP01') throw Error('Invalid patch header');
  const count = patch.readUInt32LE(20), outputLength = patch.readUInt32LE(16), sourceLength = patch.readUInt32LE(12);
  if (!count || count > 200000 || outputLength < 8 || outputLength > LIMIT || sourceLength < 8 || sourceLength > LIMIT) throw Error('Invalid patch limits');
  let p = 120, total = 0;
  const ops = [];
  for (let i = 0; i < count; i++) {
    if (p + 5 > patch.length) throw Error('Truncated operation');
    const kind = patch[p++], value = patch.readUInt32LE(p); p += 4;
    if (kind === 0) {
      if (p + 4 > patch.length) throw Error('Truncated copy');
      const length = patch.readUInt32LE(p); p += 4;
      if (!length || value > patch.readUInt32LE(12) || length > patch.readUInt32LE(12) - value) throw Error('Invalid copy');
      ops.push({kind, offset: value, length}); total += length;
    } else if (kind === 1) {
      if (!value || value > patch.length - p) throw Error('Invalid insert');
      ops.push({kind, bytes: patch.subarray(p, p + value)}); p += value; total += value;
    } else throw Error('Unknown operation');
    if (total > outputLength) throw Error('Output overflow');
  }
  if (p !== patch.length || total !== outputLength) throw Error('Patch length mismatch');
  return ops;
}
function apply(sourceFile, patch) {
  const ops = decode(patch);
  if (sourceFile.length !== patch.readUInt32LE(8) || !digest(sourceFile).equals(patch.subarray(24, 56))) throw Error('Original file hash mismatch');
  const source = unpack(sourceFile);
  if (source.length !== patch.readUInt32LE(12) || !digest(source).equals(patch.subarray(56, 88))) throw Error('Original canonical hash mismatch');
  const out = Buffer.concat(ops.map(op => op.kind === 0 ? source.subarray(op.offset, op.offset + op.length) : op.bytes));
  if (!digest(out).equals(patch.subarray(88, 120))) throw Error('Output hash mismatch');
  if (out[0] !== 70) throw Error('Output must be canonical FWS');
  unpack(out);
  return out;
}
function auditPatch(sourceFile, patch) {
  const source = unpack(sourceFile), target = apply(sourceFile, patch), ops = decode(patch);
  const originalWords = indexSource(source);
  let copied = 0, inserted = 0, carriedWindows = 0, maxLiteral = 0;
  for (const op of ops) {
    if (op.kind === 0) copied += op.length;
    else {
      inserted += op.bytes.length; maxLiteral = Math.max(maxLiteral, op.bytes.length);
      for (let i = 0; i + 4 <= op.bytes.length; i++) if (originalWords.has(op.bytes.readUInt32LE(i))) carriedWindows++;
    }
  }
  if (carriedWindows) throw Error('Literal audit failed: original four-byte sequence carried');
  return {format: 'RMSWFP01', sourceFileSha256: digest(sourceFile).toString('hex'),
    canonicalSourceSha256: digest(source).toString('hex'), canonicalOutputSha256: digest(target).toString('hex'),
    sourceBytes: sourceFile.length, canonicalSourceBytes: source.length, outputBytes: target.length,
    patchBytes: patch.length, operations: ops.length, copiedBytes: copied, literalBytes: inserted,
    originalFourByteWindowsInLiteralRuns: carriedWindows, longestLiteralRun: maxLiteral,
    limit: 'Byte reuse evidence only; not an authorship or licence classifier.'};
}
if (require.main === module) {
  const [command, originalPath, otherPath, outputPath] = process.argv.slice(2);
  if (!['make','audit','apply'].includes(command) || !originalPath || !otherPath ||
      (command !== 'audit' && !outputPath))
    throw Error('Usage: make|audit|apply original.swf target.swf|patch.rmp [new-output]');
  const original = fs.readFileSync(originalPath);
  if (command === 'make') {
    const {patch, audit} = make(original, fs.readFileSync(otherPath));
    fs.writeFileSync(outputPath, patch, {flag: 'wx'});
    process.stdout.write(JSON.stringify(audit, null, 2) + '\n');
  } else if (command === 'audit') process.stdout.write(JSON.stringify(auditPatch(original, fs.readFileSync(otherPath)), null, 2) + '\n');
  else if (command === 'apply') fs.writeFileSync(outputPath, apply(original, fs.readFileSync(otherPath)), {flag: 'wx'});
  else throw Error('Usage: make|audit|apply original.swf target.swf|patch.rmp [new-output]');
}
module.exports = {unpack, make, apply, auditPatch};
