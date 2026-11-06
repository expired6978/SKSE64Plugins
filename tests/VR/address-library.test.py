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
        # Bind the baseline to the complete release identity and version row,
        # rather than trusting a mutable input path or its filename.
        self.assertEqual(len(CSV_BYTES), BASELINE["bytes"])
        self.assertEqual(hashlib.sha256(CSV_BYTES).hexdigest(), BASELINE["sha256"])
        definitions = re.findall(r'SKEE_VR_ADDRESS_LIBRARY_VERSION="([^"]+)"',
                                 (ROOT / "CMakeLists.txt").read_text())
        self.assertEqual(definitions, [BASELINE["version"]])
        self.assertEqual(ROWS[0]["offset"], BASELINE["version"])

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
        self.assertEqual(len(IDS), 52)
        for name, ident in IDS.items():
            self.assertIn(ident, LIBRARY, name)

    def test_call_patch_addresses_and_targets_are_unchanged(self):
        def tables(text):
            result = {}
            for match in re.finditer(r'VRCallPatch\{ "([^"]+)", (.*?), reinterpret_cast<std::uintptr_t>\(([^\n]+?)\), (.*?) \}', text):
                result[match[1]] = (match[2], match[4], match[3])
            extra = re.search(r'const VRCallPatch newModel\{\s*"([^"]+)",\s*([^,]+),\s*reinterpret_cast<std::uintptr_t>\(([^\n]+?)\),\s*([^\n]+)\s*\};', text)
            self.assertIsNotNone(extra)
            result[extra[1]] = (extra[2], extra[4].strip(), extra[3])
            return result
        old = tables(before("skee64/VR/SKEEHooks.cpp"))
        current = (ROOT / "skee64/VR/SKEEHooks.cpp").read_text()
        new = tables(current)
        self.assertEqual(len(old), 21)
        self.assertEqual(old.keys(), new.keys())
        for name, pair in old.items():
            self.assertEqual(new[name][2], pair[2], name)
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

    def test_other_migrated_sites_keep_the_qualified_addresses(self):
        current = (ROOT / "skee64/VR/SKEEHooks.cpp").read_text()
        original = before("skee64/VR/SKEEHooks.cpp")
        # These sites are outside the VRCallPatch tables. Compare actual
        # expressions with the independent pre-migration source as well.
        old_preprocessing = int(re.search(r'preprocessing = (0x[0-9A-Fa-f]+);', original)[1], 16)
        new_preprocessing = re.search(r'preprocessing = ([^;]+);', current)[1]
        for site in ("site1", "site2", "site3"):
            old_expression = re.search(r'const auto '+site+r' = REL::Offset\(preprocessing \+ (0x[0-9A-Fa-f]+)\)', original)[1]
            new_expression = re.search(r'const auto '+site+r' = ([^;]+);', current)[1]
            new_expression = new_expression.replace("preprocessing", new_preprocessing)
            self.assertEqual(evaluate(new_expression, True), BASE_ADDRESS + old_preprocessing + int(old_expression, 16))
        old_armor = int(re.search(r'kVRCreateArmorNodeRVA = (0x[0-9A-Fa-f]+);', original)[1], 16)
        self.assertIn("const auto expectedTarget = SKEE::VR::Address::CreateArmorNode.address();", current)
        self.assertEqual(LIBRARY[IDS["CreateArmorNode"]], old_armor)
        old_set_model = re.search(r'const auto setModel = (REL::Offset\(0x[0-9A-Fa-f]+\)\.address\(\));', original)[1]
        new_set_model = re.search(r'const auto setModel = ([^;]+);', current)[1]
        self.assertEqual(evaluate(new_set_model, True), evaluate(old_set_model, True))
        old_cache = original.split("SKEEHookGroupResult InstallVRFaceGenCacheBypass()", 1)[1]
        new_cache = current.split("SKEEHookGroupResult InstallVRFaceGenCacheBypass()", 1)[1]
        self.assertEqual(evaluate(re.search(r'const auto address = ([^;]+);', new_cache)[1], True),
                         evaluate(re.search(r'const auto address = ([^;]+);', old_cache)[1], True))

    def test_cursor_pointer_type_and_data_address_are_unchanged(self):
        path = "skee64/VR/RaceSexMenuVRInput.cpp"
        original, current = before(path), (ROOT / path).read_text()
        old = re.search(r'REL::Relocation<([^>]+)> g_mouseCoords\{ REL::Offset\((0x[0-9A-Fa-f]+)\) \}', original)
        saved = re.search(r'(MouseCoords\*\*) g_mouseCoords\{ nullptr \}', current)
        resolved = re.search(r'REL::Relocation<([^>]+)> mouseCoords\{ REL::Offset\(cursor->offset\) \}', current)
        self.assertEqual(old[1], saved[1])
        self.assertEqual(old[1], resolved[1])
        self.assertEqual(LIBRARY[IDS["MenuCursor"]], int(old[2], 16))

    def test_cursor_lookup_is_confined_to_runtime_qualification(self):
        current = (ROOT / "skee64/VR/RaceSexMenuVRInput.cpp").read_text()
        qualification = current.split("bool ValidateRaceSexMenuVRRuntimeContract()", 1)[1].split(
            "bool ApplyRaceSexMenuWorldTransform()", 1)[0]
        self.assertEqual(current.count("Address::MenuCursor"), 1)
        self.assertRegex(qualification,
                         r"(?s)if \(!REL::Module::IsVR\(\) \|\| module.version\(\) != .*?return false;\s*}"
                         r".*?Address::MenuCursor.id\(\);\s*const REL::Offset2ID mappings;")
        self.assertRegex(qualification, r"(?s)std::ranges::find\(mappings, cursorId, &REL::IDDB::mapping_t::id\);"
                         r"\s*if \(cursor == mappings.end\(\) \|\| cursor->offset == 0 .*?"
                         r"return false;\s*}.*?!IsInsideSegment\(mouseCoords.address\(\), sizeof\(\*g_mouseCoords\), data\)"
                         r".*?return false;\s*}\s*g_mouseCoords = mouseCoords.get\(\);")
        registration = current.split("bool RegisterRaceSexMenuPointerInput()", 1)[1]
        self.assertRegex(registration, r"(?s)if \(!ValidateRaceSexMenuVRRuntimeContract\(\)\) \{\s*"
                         r"return false;\s*}.*?HookTransaction::Commit\(")
        self.assertIn("g_mouseCoords ? *g_mouseCoords : nullptr", current)

    def test_slider_callback_guards_arguments_before_read_and_conversion(self):
        current = (ROOT / "skee64/ScaleformCharGenFunctions.cpp").read_text()
        callback = current.split("void SKSEScaleform_GetSliderData::Call(", 1)[1]
        prefix = callback.split("GetRaceMenuSlider(sliderId)", 1)[0]
        self.assertRegex(prefix, r"(?s)using namespace ScaleformUtils;\s*"
                         r"if \(a_params.argCount < 2 \|\| !a_params.args \|\|\s*"
                         r"a_params.args\[0\].GetType\(\) != RE::GFxValue::ValueType::kNumber \|\|\s*"
                         r"a_params.args\[1\].GetType\(\) != RE::GFxValue::ValueType::kNumber\) \{\s*return;\s*}"
                         r"\s*const auto id = a_params.args\[0\].GetNumber\(\);\s*"
                         r"if \(!std::isfinite\(id\) \|\| id < 0.0 \|\| std::trunc\(id\) != id \|\|\s*"
                         r"id > static_cast<double>\(std::numeric_limits<std::uint32_t>::max\(\)\)\) \{\s*return;\s*}"
                         r"\s*const auto sliderId = static_cast<std::uint32_t>\(id\);\s*"
                         r"const auto value = a_params.args\[1\].GetNumber\(\);")
        # Interpret the verified source guard in order. Accessing a missing
        # argument here raises, so the short-array cases exercise short circuit.
        guard = re.search(r"if \((a_params.argCount < 2.*?)\) \{", prefix, re.S)[1]
        for count, types, rejected in [
            (0, [], True), (1, ["number"], True), (2, None, True),
            (2, ["string", "number"], True), (2, ["number", "string"], True),
            (2, ["number", "number"], False),
        ]:
            with self.subTest(count=count, types=types):
                terms = {
                    "a_params.argCount < 2": lambda: count < 2,
                    "!a_params.args": lambda: types is None,
                    "a_params.args[0].GetType() != RE::GFxValue::ValueType::kNumber": lambda: types[0] != "number",
                    "a_params.args[1].GetType() != RE::GFxValue::ValueType::kNumber": lambda: types[1] != "number",
                }
                self.assertEqual(any(terms[term.strip()]() for term in guard.split("||")), rejected)
        self.assertIn("skee::GetRaceMenuHeadPartList(raceMenu, slider->index)", callback)
        self.assertIn("SKEE::HeadPartSlider::PartIndexForValue(value, headPartList->size())", callback)

    def test_no_remaining_mapped_entry_rva_exceptions(self):
        current = (ROOT / "skee64/VR/SKEEHooks.cpp").read_text()
        helpers = (ROOT / "skee64/VR/RuntimeHelpers.h").read_text()
        self.assertNotIn("VRQualifiedFunctionRVA", helpers)
        self.assertNotIn("REL::Offset", helpers)
        # The separate data flag still lacks an exact library entry.
        self.assertEqual(re.findall(r'REL::Offset\(([^)]+)\)', current), ["0x01EA71B0"])
        self.assertNotIn("kVRCreateArmorNodeRVA", current)
        self.assertNotIn("0x00CB8800", current)
        self.assertNotIn("0x00CB9000", current)

if __name__ == "__main__":
    print(json.dumps({"addressLibraryRelease": {
        "version": BASELINE["version"], "bytes": len(CSV_BYTES),
        "sha256": hashlib.sha256(CSV_BYTES).hexdigest()}}, sort_keys=True), flush=True)
    unittest.main()
