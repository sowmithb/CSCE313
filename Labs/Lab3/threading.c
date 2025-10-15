#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <stdio.h>
#include "threading.h"

void t_init()
{
  // initialize
  for (int i = 0; i < NUM_CTX; i++) {
    contexts[i].state = INVALID;
  }
  // capture main
  getcontext(&contexts[0].context);
  contexts[0].state = VALID;
  current_context_idx = 0;
}

int32_t t_create(fptr foo, int32_t arg1, int32_t arg2)
{
  if (foo == NULL) {
    return 1;
  }

  // find the first free context slot (skip 0 which holds main)
  volatile int availableIndex = -1;
  for (volatile int slotIndex = 1; slotIndex < NUM_CTX; slotIndex++) {
    if (contexts[slotIndex].state == INVALID) {
      availableIndex = slotIndex;
      break;
    }
  }
  if (availableIndex == -1) {
    return 1; // no available slots
  }

  // initialize the context and allocate stack memory
  if (getcontext(&contexts[availableIndex].context) == -1) {
    return 1;
  }

  void *workerStackMemory = malloc((size_t)STK_SZ);
  if (workerStackMemory == NULL) {
    return 1;
  }

  contexts[availableIndex].context.uc_stack.ss_sp = workerStackMemory;
  contexts[availableIndex].context.uc_stack.ss_size = (size_t)STK_SZ;
  contexts[availableIndex].context.uc_stack.ss_flags = 0;

  // When the worker function returns, resume main (stored in index 0 by t_init)
  contexts[availableIndex].context.uc_link = &contexts[0].context;

  makecontext(&contexts[availableIndex].context, (void (*)())foo, 2, (int)arg1, (int)arg2);

  contexts[availableIndex].state = VALID;
  return 0;
}

int32_t t_yield()
{
  int currentIndex = (int)current_context_idx;
  int chosenNextIndex = -1;

  // Count other runnable contexts and pick the first one
  int runnableOthersCount = 0;
  for (int candidate = 0; candidate < NUM_CTX; candidate++) {
    if (candidate != currentIndex && contexts[candidate].state == VALID) {
      if (chosenNextIndex == -1) {
        chosenNextIndex = candidate;
      }
      runnableOthersCount++;
    }
  }

  if (runnableOthersCount == 0) {
    return -1;
  }

  if (chosenNextIndex == -1) {
    for (int offset = 1; offset < NUM_CTX; offset++) {
      int probeIndex = (currentIndex + offset) % NUM_CTX;
      if (contexts[probeIndex].state == VALID) {
        chosenNextIndex = probeIndex;
        break;
      }
    }
  }

  //context switch
  current_context_idx = (uint8_t)chosenNextIndex;
  swapcontext(&contexts[currentIndex].context, &contexts[chosenNextIndex].context);

  return runnableOthersCount;
}

void t_finish()
{
  int runningIndex = (int)current_context_idx;

  // mark the running context as done and release its stack if applicable
  contexts[runningIndex].state = DONE;

  if (runningIndex != 0 && contexts[runningIndex].context.uc_stack.ss_sp != NULL) {
    free(contexts[runningIndex].context.uc_stack.ss_sp);
    contexts[runningIndex].context.uc_stack.ss_sp = NULL;
    contexts[runningIndex].context.uc_stack.ss_size = 0;
    contexts[runningIndex].context.uc_stack.ss_flags = 0;
  }

  contexts[runningIndex].state = INVALID;
  (void)t_yield();
}
