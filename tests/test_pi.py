from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
EXAMPLE = ROOT / "examples" / "pi.toy"
PI = (
    "31415926535897932384626433832795028841971693993751"
    "058209749445923078164062862089986280348253421170679"
)


class PiDigitsTest(unittest.TestCase):
    def run_program(self, path):
        result = subprocess.run(
            [str(ROOT / "toyforth"), str(path)],
            check=True, capture_output=True, text=True, timeout=10,
        )
        self.assertNotIn("Run time error", result.stdout)
        self.assertEqual(result.stdout.splitlines()[-1], "Stack content at end: []")
        return result.stdout.splitlines()[1:-1]

    def definitions(self):
        return EXAMPLE.read_text().rsplit("\n10 pi_digits\n", 1)[0]

    def test_first_ten_digits(self):
        self.assertEqual(self.run_program(EXAMPLE), ["3141592653"])

    def test_other_lengths(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "pi.toy"
            for count in [0, 1, 2, 5, 20, 50, 100]:
                with self.subTest(count=count):
                    path.write_text(self.definitions() + f"\n{count} pi_digits\n10 emit\n")
                    self.assertEqual(self.run_program(path), [PI[:count]])

    def test_repeated_calls(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "pi.toy"
            path.write_text(
                self.definitions()
                + "\n10 pi_digits 10 emit\n20 pi_digits 10 emit\n"
            )
            self.assertEqual(self.run_program(path), [PI[:10], PI[:20]])


if __name__ == "__main__":
    unittest.main()
