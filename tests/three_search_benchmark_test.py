import sys
from pathlib import Path
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"benchmarks"))
from three_search_benchmark import score_015, candy_input, interact

class ThreeSearchBenchmarkTest(unittest.TestCase):
    def setUp(self):
        self.text=" ".join(["1"]*200)
    def test_replay(self):
        for moves in ("F\n"*100,"B\n"*99,"F\nB\nL\nR\n"*25):
            self.assertEqual(score_015(self.text,moves),1000000)
        for moves in ("F\n"*98,"F\n"*101,"Q\n"*100):
            with self.assertRaises(ValueError): score_015(self.text,moves)
        with self.assertRaises(ValueError): candy_input("1 1")
        with self.assertRaises(ValueError): candy_input(" ".join(["4"]+["1"]*199))
        with self.assertRaises(ValueError): candy_input(" ".join(["1"]*199+["2"]))
    def test_actual_interaction(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)
            good="import sys\nassert len(input().split())==100\nfor t in range(100):\n assert int(input())==1\n print('F',flush=True)\n"
            elapsed=interact([sys.executable,"-u","-c",good],self.text,p/"good.out",p/"good.err",timeout=3)
            self.assertLess(elapsed,3)
            self.assertEqual(len((p/"good.out").read_text().split()),100)
            # 将来rankを読むまで出力しないsolverは、全入力一括渡しなら動くが本物の対話では止まる。
            leak="input()\ninput()\ninput()\nprint('F',flush=True)\n"
            with self.assertRaises(TimeoutError):
                interact([sys.executable,"-u","-c",leak],self.text,p/"leak.out",p/"leak.err",timeout=0.2)

if __name__=="__main__": unittest.main()
