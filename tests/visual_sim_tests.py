"""Black-box tests of the actual persistent C++ engine; no third-party modules."""
import json
from pathlib import Path
import subprocess
import sys
import unittest

BINARY = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None
sys.argv = sys.argv[:1]


class SimulatorChecks(unittest.TestCase):
    def setUp(self):
        self.proc = subprocess.Popen([str(BINARY)], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, text=True, bufsize=1)
        self.initial = self.read()

    def tearDown(self):
        self.proc.stdin.close()
        self.proc.wait(timeout=5)

    def read(self):
        return json.loads(self.proc.stdout.readline())

    def action(self, cmd):
        self.proc.stdin.write(cmd + '\n')
        self.proc.stdin.flush()
        return self.read()

    def test_initial_four_agents_and_map(self):
        s = self.initial
        self.assertTrue(s['ok'])
        self.assertEqual((s['world']['width'], s['world']['height']), (20, 12))
        self.assertEqual(len(s['belief']), 240)
        self.assertAlmostEqual(sum(s['belief']), 1, places=4)
        self.assertGreater(len(s['trooperPath']), 1)
        self.assertEqual(s['securityAI']['steps'], ['PatrolSector'])
        self.assertEqual(len(s['director']['scores']), 4)

    def test_closed_door_forces_longer_valid_path(self):
        first = self.initial
        closed = self.action('DOOR')
        self.assertFalse(closed['world']['doorOpen'])
        self.assertGreater(len(closed['trooperPath']), len(first['trooperPath']))
        self.assertNotIn(closed['world']['door'], closed['trooperPath'])
        reopened = self.action('DOOR')
        self.assertEqual(len(reopened['trooperPath']), len(first['trooperPath']))

    def test_sound_updates_belief_and_goap(self):
        s = self.action('GUNSHOT')
        self.assertEqual(s['hollowTarget'], s['player'])
        self.assertEqual(s['securityAI']['steps'], ['InvestigateSound'])
        s = self.action('SPOT')
        self.assertEqual(s['securityAI']['steps'][0], 'TakeCover')
        s = self.action('STEP')
        self.assertEqual(s['tick'], 1)
        self.assertGreater(len(s['trooperPath']), 0)

    def test_director_fairness_constraints(self):
        s = self.action('SET health 0.2')
        wave = next(v for v in s['director']['scores'] if v['name'] == 'HollowWave')
        self.assertFalse(wave['allowed'])
        s = self.action('SAFE 1')
        self.assertFalse(next(v for v in s['director']['scores'] if v['name'] == 'VisualDistortion')['allowed'])
        self.assertTrue(s['state']['safeZone'])

    def test_sound_does_not_magically_reveal_player(self):
        s = self.action('GUNSHOT')
        self.assertEqual(s['securityAI']['steps'], ['InvestigateSound'])
        for _ in range(12):
            s = self.action('STEP')
        self.assertFalse(s['securityAI']['knowsTarget'])
        self.assertNotEqual(s['securityAI']['lastAction'], 'SuppressTarget')

    def test_dynamic_hazard_changes_route_without_blocking_floor(self):
        before = self.initial
        hazard = before['world']['hazard']
        self.assertIn(hazard, before['trooperPath'])
        during = self.action('HAZARD')
        self.assertTrue(during['world']['hazardOn'])
        self.assertNotIn(hazard, during['trooperPath'])
        self.assertEqual(during['world']['rows'][hazard[1]][hazard[0]], '.')
        after = self.action('HAZARD')
        self.assertIn(hazard, after['trooperPath'])

    def test_hollow_belief_survives_door_changes(self):
        self.action('GUNSHOT')
        before = self.action('STEP')
        self.assertGreater(before['belief'][7*20+16], 0.01)
        blocked = self.action('DOOR')
        self.assertAlmostEqual(sum(blocked['belief']), 1, places=3)
        self.assertGreater(blocked['belief'][7*20+16], 0.01)
        self.assertEqual(blocked['belief'][5*20+9],0)

    def test_guard_retreats_at_low_health_and_loses_target(self):
        s = self.action('SPOT')
        self.assertTrue(s['securityAI']['knowsTarget'])
        s = self.action('SET securityHealth 0.2')
        self.assertEqual(s['securityAI']['steps'][0], 'Retreat')
        s = self.action('LOST')
        self.assertFalse(s['securityAI']['knowsTarget'])
        self.assertEqual(s['securityAI']['steps'][0], 'Retreat')

    def test_wave_lifecycle_expires_and_can_spawn_again(self):
        s=self.action('STEP')
        self.assertGreater(s['state']['activeHollows'],0)
        for _ in range(7):
            s=self.action('STEP')
        self.assertLessEqual(s['state']['activeHollows'],1)
        self.assertTrue(any('dissipated' in entry for entry in s['history']))

    def test_invalid_does_not_mutate_state(self):
        bad = self.action('PLAYER 0 0')
        self.assertFalse(bad['ok'])
        after = self.action('SET ammo 18')
        self.assertEqual(self.initial['player'], after['player'])
        bad = self.action('SET health 200')
        self.assertFalse(bad['ok'])
        self.assertTrue(self.action('RESET')['ok'])


if __name__ == '__main__':
    if not BINARY or not BINARY.is_file():
        raise SystemExit('Usage: python tests/visual_sim_tests.py <path-to-himera_sim>')
    unittest.main()
