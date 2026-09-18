import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))
from ahc006_delta_benchmark import score_output


class AHC006BenchmarkTest(unittest.TestCase):
    def setUp(self):
        # 同じ座標を別注文が共有してもよい。最初のpickup < 最後のdeliveryを検査。
        self.text = "0 0 100 0\n" * 1000
        self.orders = list(range(1, 51))
        self.route = [(400, 400), (0, 0), (100, 0), (400, 400)]

    def output(self, orders=None, route=None):
        orders = self.orders if orders is None else orders
        route = self.route if route is None else route
        return " ".join(map(str, [len(orders), *orders, len(route), *(x for p in route for x in p)]))

    def test_valid(self):
        self.assertEqual(score_output(self.text, self.output()), (38462, 1600))
        repeat = [(400, 400), (100, 0), (0, 0), (100, 0), (400, 400)]
        self.assertEqual(score_output(self.text, self.output(route=repeat))[1], 1600)

    def test_invalid(self):
        bad = [self.output(orders=list(range(1, 50))),
               self.output(orders=[1] * 50),
               self.output(orders=list(range(952, 1002))),
               self.output(route=[(400, 400), (100, 0), (0, 0), (400, 400)]),
               self.output(route=[(400, 400), (0, 0), (100, 0)]),
               self.output(route=self.route + [(801, 0), (400, 400)]),
               self.output() + " 0", ""]
        for output in bad:
            with self.assertRaises(ValueError): score_output(self.text, output)


if __name__ == "__main__": unittest.main()
