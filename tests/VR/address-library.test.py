"""Read-only mapping/patch equivalence checks; no game or native execution.

Usage: CODEX_PYTHON tests/VR/address-library.test.py <release CSV> <base commit>
The external release CSV is an input, never modified or bundled into the mod.
"""
import ast
import csv
import hashlib
import json
import pathlib
import re
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
CSV = pathlib.Path(sys.argv[1])
BASE = sys.argv[2]
sys.argv = sys.argv[:1]
CSV_BYTES = CSV.read_bytes()
BASELINE = json.loads((ROOT / "tests/VR/address-library-baseline.json").read_text())
ROWS = list(csv.DictReader(CSV_BYTES.decode("utf-8-sig").splitlines()))
LIBRARY = {int(r["id"]): int(r["offset"], 16) for r in ROWS
           if re.fullmatch(r"[0-9a-fA-F]+", r["offset"])}
HEADER = (ROOT / "skee64/VR/AddressLibrary.h").read_text()
IDS = {name: int(value) for name, value in re.findall(
    r"REL::RelocationID (\w+)\{ 0, 0, (\d+) \}", HEADER)}
BASE_ADDRESS = 0x140000000

def before(path):
    return subprocess.check_output(["git", "-C", str(ROOT), "show", BASE + ":" + path]).decode()

def evaluate(expression, absolute):
    expression = re.sub(r"SKEE::VR::Address::(\w+)\.address\(\)",
                        lambda m: str(BASE_ADDRESS + LIBRARY[IDS[m[1]]]), expression)
    expression = re.sub(r"REL::Offset\((0x[0-9A-Fa-f]+)\)\.address\(\)",
                        lambda m: str(BASE_ADDRESS + int(m[1], 16)), expression)
    load = LIBRARY[IDS["LoadSliders"]] + (BASE_ADDRESS if absolute else 0)
    expression = expression.replace("loadSliders", str(load)).strip()
    tree = ast.parse(expression, mode="eval")
    def number(node):
        if isinstance(node, ast.Constant) and type(node.value) is int: return node.value
        if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
            return number(node.left) + number(node.right)
        raise ValueError("unsupported address expression: " + expression)
    return number(tree.body)

class Mapping(unittest.TestCase):
    def test_compiled_baseline_matches_verified_release(self):
        # CSV files carry no package-version field. Bind the version label to
        # the retained complete release-file identity, not its mutable path.
        self.assertEqual(len(CSV_BYTES), BASELINE["bytes"])
        self.assertEqual(hashlib.sha256(CSV_BYTES).hexdigest(), BASELINE["sha256"])
        definitions = re.findall(r'SKEE_VR_ADDRESS_LIBRARY_VERSION="([^"]+)"',
                                 (ROOT / "CMakeLists.txt").read_text())
        self.assertEqual(definitions, [BASELINE["version"]])

    def test_complete_catalogue_matches_external_library(self):
        fixture = json.loads((ROOT / "tests/VR/address-library-map.json").read_text())
        self.assertEqual(set(IDS), {entry["name"] for entry in fixture})
        for entry in fixture:
            with self.subTest(entry=entry["name"]):
                self.assertEqual(IDS[entry["name"]], entry["id"])
                self.assertEqual(LIBRARY[entry["id"]], int(entry["qualifiedRva"], 16))

    def test_each_id_matches_qualified_source_entry(self):
        # The pre-migration validation windows identify the qualified addresses;
        # the independent CSV must resolve each replacement to that same entry.
        original = before("skee64/VR/SKEEHooks.cpp")
        current = (ROOT / "skee64/VR/SKEEHooks.cpp").read_text()
        old = dict(re.findall(r'validate\("([^"]+)", (0x[0-9A-Fa-f]+),', original))
        new = dict(re.findall(r'validate\("([^"]+)", ([^,\n]+),', current))
        self.assertEqual(old.keys(), new.keys())
        for name, rva in old.items():
            with self.subTest(helper=name):
                self.assertEqual(evaluate(new[name], True), BASE_ADDRESS + int(rva, 16))
        self.assertEqual(len(IDS), 32)
        for name, ident in IDS.items():
            self.assertIn(ident, LIBRARY, name)

    def test_call_patch_addresses_and_targets_are_unchanged(self):
        def tables(text):
            result = {}
            for match in re.finditer(r'VRCallPatch\{ "([^"]+)", (.*?), reinterpret_cast<std::uintptr_t>\([^\n]+?\), (.*?) \}', text):
                result[match[1]] = (match[2], match[3])
            extra = re.search(r'const VRCallPatch newModel\{\s*"([^"]+)",\s*([^,]+),\s*reinterpret_cast<std::uintptr_t>\([^\n]+?\),\s*([^\n]+)\s*\};', text)
            self.assertIsNotNone(extra)
            result[extra[1]] = (extra[2], extra[3].strip())
            return result
        old = tables(before("skee64/VR/SKEEHooks.cpp"))
        current = (ROOT / "skee64/VR/SKEEHooks.cpp").read_text()
        new = tables(current)
        self.assertEqual(len(old), 21)
        self.assertEqual(old.keys(), new.keys())
        for name, pair in old.items():
            for index in (0, 1):
                with self.subTest(patch=name, member=index):
                    self.assertEqual(evaluate(new[name][index], True), BASE_ADDRESS + evaluate(pair[index], False))
        self.assertNotRegex(current, r'REL::Offset\((?:a_patch|patch|newModel)\.')
        self.assertIn("const auto address = a_patch.address;", current)
        self.assertIn("const auto expectedTarget = a_patch.expectedTarget;", current)

    def test_runtime_helper_pointer_types_and_addresses_are_unchanged(self):
        old = before("skee64/VR/RuntimeHelpers.h")
        new = (ROOT / "skee64/VR/RuntimeHelpers.h").read_text()
        original = re.findall(r'VRFunction<([^\n]+)>\((0x[0-9A-Fa-f]+)\)', old)
        changed = re.findall(r'VR(?:Function|QualifiedFunctionRVA)<([^\n]+)>\(([^)\n]+)\)', new)
        self.assertEqual(len(original), len(changed))
        for (oldtype, rva), (newtype, address) in zip(original, changed):
            self.assertEqual(oldtype, newtype)
            if address.startswith("SKEE::VR::Address::"):
                name = address.rsplit("::", 1)[1]
                self.assertEqual(LIBRARY[IDS[name]], int(rva, 16))
            else:
                self.assertEqual(int(address, 16), int(rva, 16))

    def test_instruction_byte_windows_remain_identical(self):
        def windows(text):
            # Distinguish instruction-byte arrays from the VRCallPatch tables
            # whose addresses are deliberately changing representation.
            arrays = re.findall(r'(std::array(?:<[^;]+?>)?(?:\s+\w+)?\s*)\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}', text)
            return [body for declaration, body in arrays
                    if "VRCallPatch" not in body and
                    ("uint8_t" in declaration or "std::uint8_t{" in body or "std::uint8_t{ " in body)]
        self.assertEqual(windows(before("skee64/VR/SKEEHooks.cpp")),
                         windows((ROOT / "skee64/VR/SKEEHooks.cpp").read_text()))

if __name__ == "__main__":
    print(json.dumps({"addressLibraryRelease": {
        "version": BASELINE["version"], "bytes": len(CSV_BYTES),
        "sha256": hashlib.sha256(CSV_BYTES).hexdigest()}}, sort_keys=True), flush=True)
    unittest.main()
