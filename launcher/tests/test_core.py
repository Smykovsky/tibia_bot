import unittest
from launcher.core.models import (
    Position, PlayerState, CreatureInfo, ModuleInfo,
    HealRule, Waypoint, MonsterTarget, LootItem, AoeRule
)
from launcher.core.engine_client import EngineClient
from launcher.core.engine_process import EngineProcessManager

class TestLauncherCore(unittest.TestCase):
    def test_models(self):
        pos = Position(32300, 32200, 7)
        self.assertEqual(pos.x, 32300)
        self.assertEqual(pos.y, 32200)

        player = PlayerState(name="TestHero", hp=800, max_hp=1000, hp_percent=80.0)
        self.assertEqual(player.name, "TestHero")
        self.assertEqual(player.hp_percent, 80.0)

        heal = HealRule(name="Vita", threshold_percent=50.0, spell_words="exura vita")
        self.assertEqual(heal.spell_words, "exura vita")

        wp = Waypoint(type="stand", x=32300, y=32200, z=7, label="Bank")
        self.assertEqual(wp.type, "stand")

        target = MonsterTarget(name="Dragon", priority=1, stance="keep_distance", desired_distance=4)
        self.assertEqual(target.desired_distance, 4)

    def test_engine_client_init(self):
        client = EngineClient(host="127.0.0.1", port=8765)
        self.assertFalse(client.is_connected)
        self.assertEqual(client.port, 8765)

    def test_engine_process_init(self):
        pm = EngineProcessManager(port=8765)
        self.assertFalse(pm.is_running())

if __name__ == "__main__":
    unittest.main()
