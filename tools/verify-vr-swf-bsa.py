"""Read-only, bounded verification of one SWF member in a Skyrim SE v105 BSA.

No extraction or archive modification. Requires lz4 only for a compressed member.
Format reference: fo76utils/fo76utils libfo76utils/src/ba2file.cpp loadBSAFile.
This is maintainer/provider verification, not a player installation dependency.
"""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import hashlib
import json
import struct
from pathlib import Path

LIMIT = 8 * 1024 * 1024
ARCHIVE_LIMIT = 64 * 1024 * 1024
MOVIE = 'interface/vr/racesex_menu.swf'
EXPECTED = '3a012da4fed80637ce3257b9b2b89243befab29a4bec5316a935cea87c889963'


def member(data):
    if len(data) < 36 or len(data) > ARCHIVE_LIMIT:
        raise ValueError('Archive exceeds bounded verification scope')
    magic, version, records, flags, folders, files, folder_names, file_names, _ = struct.unpack_from('<4s8I', data)
    if magic != b'BSA\0' or version != 105 or records != 36 or flags & 3 != 3 or flags & ~0x1bf:
        raise ValueError('Unsupported BSA header/flags')
    if not 0 < folders <= 10000 or not 0 < files <= 100000 or folders * 24 + 36 > len(data):
        raise ValueError('Invalid BSA directory limits')
    counts = [struct.unpack_from('<I', data, 36+i*24+8)[0] for i in range(folders)]
    if sum(counts) != files:
        raise ValueError('Folder/file count mismatch')
    p = 36+folders*24
    entries = []
    for count in counts:
        if p >= len(data):
            raise ValueError('Truncated folder block')
        n = data[p]
        p += 1
        if not n or p+n+count*16 > len(data) or data[p+n-1] != 0:
            raise ValueError('Invalid folder block')
        folder = data[p:p+n-1].decode('ascii').replace('\\', '/').lower()
        p += n
        for _ in range(count):
            _, size, offset = struct.unpack_from('<QII', data, p)
            entries.append((folder, size, offset))
            p += 16
    if file_names > LIMIT or p+file_names > len(data):
        raise ValueError('Invalid file-name table')
    names = data[p:p+file_names].split(b'\0')
    if len(names) != files+1 or names[-1] != b'':
        raise ValueError('Invalid file-name count/terminator')
    metadata_end = p+file_names
    matches = []
    for (folder, size, offset), raw_name in zip(entries, names):
        name = raw_name.decode('ascii').replace('\\', '/').lower()
        if '/'.join(filter(None, (folder, name))) != MOVIE:
            continue
        length = size & 0x3fffffff
        if not length or length > LIMIT or offset < metadata_end or offset+length > len(data) or size & 0x80000000:
            raise ValueError('Invalid target member range')
        payload = data[offset:offset+length]
        compressed = bool(flags & 4) != bool(size & 0x40000000)
        if flags & 0x100:
            if not payload or payload[0]+1 > len(payload):
                raise ValueError('Invalid embedded member name')
            payload = payload[1+payload[0]:]
        if compressed:
            if len(payload) < 4:
                raise ValueError('Truncated compressed member')
            declared, = struct.unpack_from('<I', payload)
            if not 0 < declared <= LIMIT:
                raise ValueError('Invalid expanded member limit')
            import lz4.frame
            decoder = lz4.frame.LZ4FrameDecompressor()
            payload = decoder.decompress(payload[4:], max_length=LIMIT)
            if not decoder.eof or decoder.unused_data or len(payload) != declared:
                raise ValueError('Invalid compressed member length/trailing data')
        matches.append((payload, offset, length, compressed))
    if len(matches) != 1:
        raise ValueError('Missing or ambiguous VR SWF member')
    return matches[0], version, files


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    args = parser.parse_args()
    stat = args.archive.stat()
    if stat.st_size > ARCHIVE_LIMIT:
        raise ValueError('Archive too large for this verifier')
    with args.archive.open('rb') as source:
        data = source.read(ARCHIVE_LIMIT+1)
    (movie, offset, stored, compressed), version, files = member(data)
    digest = hashlib.sha256(movie).hexdigest()
    if digest != EXPECTED:
        raise ValueError('VR SWF member does not match qualified original')
    with args.archive.open('rb') as source:
        after = hashlib.file_digest(source, 'sha256').hexdigest()
    before = hashlib.sha256(data).hexdigest()
    if before != after or args.archive.stat().st_mtime_ns != stat.st_mtime_ns:
        raise ValueError('Archive changed during verification')
    print(json.dumps({'archive': str(args.archive.resolve()), 'archiveBytes': len(data),
        'archiveSha256': before, 'bsaVersion': version, 'fileCount': files,
        'member': MOVIE, 'physicalOffset': offset, 'storedBytes': stored,
        'compressed': compressed, 'memberBytes': len(movie), 'memberSha256': digest,
        'access': 'read-only', 'originalUnchanged': True, 'qualifiedOriginalMatch': True}, indent=2))
