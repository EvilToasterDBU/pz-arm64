#!/usr/bin/env python3
"""Launcher tests (no game, no Java, no arm64 needed): runs tool/pz-arm64 with PZ_DRY_RUN=1 against a fake game folder
and a fake Steam Workshop and checks the command line it would start.

    python3 tests/test_launcher.py
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZB_ID = "3619862853"


class LauncherTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="pz-launcher-test-")
        t = self.tool = os.path.join(self.tmp, "tool")
        os.makedirs(os.path.join(t, "lib", "bridge"))
        os.makedirs(os.path.join(t, "jre25", "bin"))
        os.makedirs(os.path.join(t, "jre25", "lib"))
        for f in ("pz-arm64", "pzgen.py"):
            shutil.copy(os.path.join(ROOT, "tool", f), t)
        for f in (os.path.join("lib", "bridge", "libpzshim.so"), os.path.join("jre25", "bin", "java"),
                  os.path.join("jre25", "lib", "libjsig.so"), os.path.join("jre25", "lib", "libinstrument.so")):
            open(os.path.join(t, f), "w").close()
        self.home = os.path.join(self.tmp, "home")
        os.makedirs(self.home)
        # <library>/steamapps/common/ProjectZomboid/projectzomboid  (a Build 42 style json: no lwjgl jars on the classpath)
        self.lib = os.path.join(self.tmp, "library")
        self.inst = os.path.join(self.lib, "steamapps", "common", "ProjectZomboid", "projectzomboid")
        os.makedirs(self.inst)
        with open(os.path.join(self.inst, "ProjectZomboid64.json"), "w") as f:
            json.dump({"mainClass": "zombie/gameStates/MainScreenState", "classpath": [".", "projectzomboid.jar"],
                       "vmArgs": ["-Djava.awt.headless=true", "-Xmx3072m", "-XX:+UseZGC"]}, f)
        with zipfile.ZipFile(os.path.join(self.inst, "projectzomboid.jar"), "w") as z:   # valid, empty
            z.writestr("README", "x")
        self.zb_jar = os.path.join(self.lib, "steamapps", "workshop", "content", "108600", ZB_ID,
                                   "mods", "ZombieBuddy", "libs", "ZombieBuddy.jar")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def add_zb_jar(self):
        os.makedirs(os.path.dirname(self.zb_jar))
        open(self.zb_jar, "w").close()

    def run_launcher(self, *user_args, env_extra=None):
        env = {"PATH": os.environ.get("PATH", ""), "HOME": self.home, "PZ_DRY_RUN": "1",
               "STEAM_COMPAT_INSTALL_PATH": os.path.dirname(self.inst)}
        env.update(env_extra or {})
        cmd = [sys.executable, "-I", os.path.join(self.tool, "pz-arm64"), "run", "--",
               os.path.join(self.inst, "projectzomboid.sh"), *user_args]
        r = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        return json.loads(r.stdout.strip().splitlines()[-1])

    @staticmethod
    def split(cmd):
        """-> (jvm part before -cp, game part after the main class)"""
        i, j = cmd.index("-cp"), cmd.index("zombie.gameStates.MainScreenState")
        return cmd[1:i], cmd[j + 1:]

    def test_plain_launch_has_no_agent(self):
        jvm, game = self.split(self.run_launcher())
        self.assertFalse([a for a in jvm if a.startswith("-javaagent")])
        self.assertIn("-novoip", game)

    def test_agent_launch_option_is_a_jvm_arg_and_resolved_from_workshop(self):
        self.add_zb_jar()
        jvm, game = self.split(self.run_launcher("-javaagent:ZombieBuddy.jar", "--"))
        agents = [a for a in jvm if a.startswith("-javaagent:")]
        self.assertEqual(len(agents), 1)
        self.assertTrue(agents[0].endswith("ZombieBuddy.jar"))
        self.assertTrue(os.path.isabs(agents[0][len("-javaagent:"):]))
        self.assertNotIn("--", game)
        self.assertFalse([a for a in game if a.startswith("-javaagent")])

    def test_agent_options_and_game_args_after_double_dash(self):
        self.add_zb_jar()
        jvm, game = self.split(self.run_launcher("-javaagent:ZombieBuddy.jar=policy=deny-new,verbosity=1", "--", "-debug"))
        agent = [a for a in jvm if a.startswith("-javaagent:")][0]
        self.assertTrue(agent.endswith("ZombieBuddy.jar=policy=deny-new,verbosity=1"))
        self.assertIn("-debug", game)
        self.assertNotIn("--", game)

    def test_without_double_dash_everything_is_a_game_arg(self):
        jvm, game = self.split(self.run_launcher("-debug", "-nosteam"))
        self.assertIn("-debug", game)
        self.assertIn("-nosteam", game)

    def test_zombiebuddy_option_adds_the_agent_once(self):
        self.add_zb_jar()
        jvm, _ = self.split(self.run_launcher(env_extra={"PZ_ZOMBIEBUDDY": "1"}))
        self.assertEqual(len([a for a in jvm if a.startswith("-javaagent:")]), 1)
        # also given as launch option: still only one agent
        jvm, _ = self.split(self.run_launcher("-javaagent:ZombieBuddy.jar", "--", env_extra={"PZ_ZOMBIEBUDDY": "1"}))
        self.assertEqual(len([a for a in jvm if a.startswith("-javaagent:")]), 1)

    def test_zombiebuddy_option_with_args(self):
        self.add_zb_jar()
        with open(os.path.join(self.tool, "pz-arm64.json"), "w") as f:
            json.dump({"zombiebuddy": True, "zombiebuddy_args": "verbosity=2"}, f)
        jvm, _ = self.split(self.run_launcher())
        agent = [a for a in jvm if a.startswith("-javaagent:")][0]
        self.assertTrue(agent.endswith("ZombieBuddy.jar=verbosity=2"))

    def test_missing_jar_does_not_break_the_launch(self):
        jvm, game = self.split(self.run_launcher("-javaagent:ZombieBuddy.jar", "--", "-debug"))
        self.assertIn("-javaagent:ZombieBuddy.jar", jvm)          # left as is; the JVM will report it
        self.assertIn("-debug", game)
        jvm, _ = self.split(self.run_launcher(env_extra={"PZ_ZOMBIEBUDDY": "1"}))
        self.assertFalse([a for a in jvm if a.startswith("-javaagent")])

    def test_game_directory_is_not_modified(self):
        self.add_zb_jar()
        before = sorted(os.listdir(self.inst))
        self.run_launcher("-javaagent:ZombieBuddy.jar", "--")
        self.assertEqual(sorted(os.listdir(self.inst)), before)


if __name__ == "__main__":
    unittest.main(verbosity=2)
