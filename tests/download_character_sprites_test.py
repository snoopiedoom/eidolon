import importlib.util
from pathlib import Path
import sys
import unittest


SCRIPT = Path(__file__).parents[1] / "tools" / "download_character_sprites.py"
SPEC = importlib.util.spec_from_file_location("download_character_sprites", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def page(title: str) -> dict[str, object]:
    return {
        "title": f"File:{title}",
        "imageinfo": [
            {
                "url": "https://static.example/sprite.png",
                "descriptionurl": "https://example/File:sprite.png",
                "size": 42,
                "width": 927,
                "height": 1280,
                "sha1": "abc",
                "timestamp": "2026-07-20T00:00:00Z",
            }
        ],
    }


class SpriteGroupingTest(unittest.TestCase):
    def test_base_character(self) -> None:
        record = MODULE.parse_sprite(page("Yuzu 07.png"))
        self.assertEqual(record.character, "Yuzu")
        self.assertIsNone(record.variant)
        self.assertEqual(record.model, "yuzu")
        self.assertEqual(record.relative_path, "yuzu/portraits/Yuzu_07.png")

    def test_variant(self) -> None:
        record = MODULE.parse_sprite(page("Airi (Band) 00.png"))
        self.assertEqual(record.variant, "Band")
        self.assertEqual(record.model, "airi-band")
        self.assertEqual(record.relative_path, "airi-band/portraits/Airi_(Band)_00.png")

    def test_existing_bunny_asuna_layout(self) -> None:
        record = MODULE.parse_sprite(page("Asuna (Bunny Girl) 99.png"))
        self.assertEqual(record.model, "asuna-bunny")
        self.assertEqual(
            record.relative_path,
            "asuna-bunny/portraits/Asuna_(Bunny_Girl)_99.png",
        )

    def test_unknown_filename_fails_loudly(self) -> None:
        with self.assertRaisesRegex(ValueError, "unrecognized sprite title"):
            MODULE.parse_sprite(page("Asuna portrait.png"))


if __name__ == "__main__":
    unittest.main()
