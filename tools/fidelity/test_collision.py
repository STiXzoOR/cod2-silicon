"""Independent geometry cases for the temporary-model trace probe."""
import unittest
import spotcheck_collision as probe


class CollisionTests(unittest.TestCase):
    def test_reference_cases_are_far_from_capsule_boundary(self):
        self.assertEqual(probe.reference_blocked(9, 9), False)
        self.assertEqual(probe.reference_blocked(-9, 9), False)
        self.assertEqual(probe.reference_blocked(0, 0), True)
        self.assertEqual(probe.reference_blocked(9, 0), True)
        self.assertEqual(probe.reference_blocked(16, 0), False)


if __name__ == '__main__':
    unittest.main()
