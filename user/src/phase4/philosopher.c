#include "philosopher.h"

// TODO: define some sem if you need
int forks[5];
int philosophers;

void init() {
  // init some sem if you need
  // TODO();
  for (int i = 0; i < 5; i++) {
    forks[i] = sem_open(1);
  }
  philosophers = sem_open(4);
}

void philosopher(int id) {
  // implement philosopher, remember to call `eat` and `think`
  while (1) {
    // TODO();
    P(philosophers);
    P(forks[id]);
    P(forks[(id + 1) % 5]);
    eat(id);
    V(forks[id]);
    V(forks[(id + 1) % 5]);
    V(philosophers);
  }
}
