int main() {
  Timer timer;
  Random random(123);
  SimulatedAnnealing annealing(10.0, 0.1, 456);
  const int value = random.next_int(0, 10);
  const bool accepted = annealing.accept(0.0, timer.progress(1000.0));
  return (0 <= value && value < 10 && accepted) ? 0 : 1;
}
