"""Source-bound regression for the attachment loaders' resolved-form guards.

Evaluates the actual C++ guard over a controlled form-type domain. This does not
execute an SKSE functor, deserialize a game save, or establish engine ABI safety.
Pass --source to check a retained earlier PapyrusNiOverride.cpp without editing it.
"""
from pathlib import Path
import argparse
import re
import unittest


parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1] / "skee64/PapyrusNiOverride.cpp")
args, remaining = parser.parse_known_args()
source = args.source.read_text(encoding="utf-8")


def rejects(class_name, form_type):
    body = source.split("class " + class_name + " :", 1)[1].split("\n\tprotected:", 1)[0]
    load = body.split("virtual bool Load(", 1)[1].split("virtual void Run(", 1)[0]
    guard = re.search(r"if \((!form \|\| .*?)\) \{\s*m_ref = nullptr;\s*return false;", load)
    if not guard:
        raise AssertionError("resolved-form rejection guard not found: " + class_name)
    expression = guard[1]
    expression = re.sub(
        r"form->IsNot\(RE::FormType::(\w+)\)",
        lambda match: str(form_type != match[1]), expression)
    expression = expression.replace("!form", str(form_type is None)).replace("||", " or ").replace("&&", " and ")
    # Refuse unexpected source syntax rather than silently approximating it.
    if re.sub(r"True|False|and|or|[()\s]", "", expression):
        raise AssertionError("unsupported guard syntax: " + expression)
    return eval(expression, {"__builtins__": {}}, {})


class ReferenceDomain(unittest.TestCase):
    def test_detach_restores_non_actor_reference(self):
        self.assertFalse(rejects("DetachMeshLatentFunctor", "Reference"))

    def test_actor_and_reference_domains_match(self):
        for form_type in ("ActorCharacter", "Reference"):
            for name in ("AttachMeshLatentFunctor", "DetachMeshLatentFunctor"):
                with self.subTest(functor=name, form_type=form_type):
                    self.assertFalse(rejects(name, form_type))

    def test_null_and_unrelated_forms_rejected(self):
        for form_type in (None, "Armor", "NPC", "Static", "None"):
            for name in ("AttachMeshLatentFunctor", "DetachMeshLatentFunctor"):
                with self.subTest(functor=name, form_type=form_type):
                    self.assertTrue(rejects(name, form_type))


if __name__ == "__main__":
    unittest.main(argv=[__file__, *remaining])
