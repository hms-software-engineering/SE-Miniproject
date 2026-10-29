import unittest

from check_service_coverage import evaluate, normalize


class CoverageThresholdTests(unittest.TestCase):
    def test_module_at_threshold_passes(self):
        source = normalize("service.cpp")
        summaries, failures = evaluate(
            {"auth": [source]}, {source: {1: 1, 2: 1, 3: 0, 4: 1}}, 75.0
        )
        self.assertEqual(failures, [])
        self.assertIn("3/4 executable lines", summaries[0])

    def test_below_threshold_names_module_and_percentage(self):
        source = normalize("service.cpp")
        summaries, failures = evaluate(
            {"auth": [source]}, {source: {1: 1, 2: 0, 3: 0}}, 70.0
        )
        self.assertIn("33.33%", summaries[0])
        self.assertTrue(any("auth" in failure and "33.33%" in failure for failure in failures))

    def test_missing_source_data_fails(self):
        source = normalize("missing.cpp")
        _, failures = evaluate({"patients": [source]}, {}, 70.0)
        self.assertTrue(any("patients" in failure and "missing coverage data" in failure for failure in failures))
        self.assertTrue(any("patients" in failure and "no executable-line" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
