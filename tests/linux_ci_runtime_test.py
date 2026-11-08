import os
from pathlib import Path
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/configure_linux_ci_runtime.sh"


class LinuxCiRuntimeTest(unittest.TestCase):
    def configure(self, existing_path):
        with tempfile.TemporaryDirectory() as directory:
            env_file = Path(directory) / "github_env"
            env = os.environ.copy()
            env["CONDA_PREFIX"] = "/opt/test conda"
            env["GITHUB_ENV"] = str(env_file)
            if existing_path is None:
                env.pop("LD_LIBRARY_PATH", None)
            else:
                env["LD_LIBRARY_PATH"] = existing_path
            subprocess.run(["bash", str(SCRIPT)], env=env, check=True)
            name, value = env_file.read_text().rstrip("\n").split("=", 1)
            self.assertEqual(name, "LD_LIBRARY_PATH")
            return value

    def test_sets_path_when_unset(self):
        self.assertEqual(self.configure(None), "/opt/test conda/lib")

    def test_preserves_existing_path(self):
        self.assertEqual(self.configure("/existing/lib:/another/lib"),
                         "/opt/test conda/lib:/existing/lib:/another/lib")

    def test_requires_conda_prefix(self):
        env = os.environ.copy()
        env.pop("CONDA_PREFIX", None)
        with tempfile.TemporaryDirectory() as directory:
            env["GITHUB_ENV"] = str(Path(directory) / "github_env")
            result = subprocess.run(["bash", str(SCRIPT)], env=env, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"CONDA_PREFIX", result.stderr)


if __name__ == "__main__":
    unittest.main()
