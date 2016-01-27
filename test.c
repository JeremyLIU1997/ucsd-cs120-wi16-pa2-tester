/*
 * pa2 tester
 * bmilton
 * 25 jan 2016
 * latest update: 26 jan 2016
 */
#include "mykernel2.h"
#include "aux.h"
#include "sys.h"
#include <time.h>
#include <stdlib.h>

#ifndef SLACK
#define SLACK 1
#endif

int totalFailCounter = 0;

int inSlackRange(int expected, int actual) {
  if(actual >= expected) return 1;
  if(!SLACK) return 0;
  double slack = expected * 0.10;
  return abs(actual - expected) <= slack;
}

int test_fifo_normal(int numprocs) {
  int failCounter = 0;
  SetSchedPolicy(FIFO);
  InitSched();
  int proc;
  for (proc = 1; proc <= numprocs; proc++) {
    StartingProc(proc);
  }

  for (proc = 1; proc <= numprocs; proc++) {
    int next = SchedProc();
    if (next != proc) {
      Printf("FIFO ERR: Received process %d but expected %d\n", next, proc);
      failCounter++;
    }

    EndingProc(proc);
  }

  /* check if all process are exited */
  if (SchedProc()) {
    Printf("FIFO ERR: Not all processes have exited\n");
    failCounter++;
  }

  totalFailCounter += failCounter;
  return failCounter;
}

int test_lifo_normal(int numprocs) {
  int failCounter = 0;
  SetSchedPolicy(LIFO);
  InitSched();
  int proc;

  for (proc = 1; proc <= numprocs; proc++) {
    StartingProc(proc);
  }

  for (proc = numprocs; proc > 0; proc--) {
    int next = SchedProc();
    if (next != proc) {
      Printf("LIFO ERR: Received process %d but expected %d\n", next, proc);
      failCounter++;
    }

    EndingProc(proc);
  }

  /* check if all process are exited */
  if (SchedProc()) {
    Printf("LIFO ERR: Not all processes have exited\n");
    failCounter++;
  }

  totalFailCounter += failCounter;
  return failCounter;
}

int test_rr_normal(int numprocs) {
  int failCounter = 0;
  SetSchedPolicy(ROUNDROBIN);
  InitSched();
  int prevproc, nextproc, iter, i;
  int trueNext1, trueNext2;

  for (prevproc = 1; prevproc <= numprocs; prevproc++) {
    StartingProc(prevproc);
  }

  prevproc = SchedProc();
  for (iter = 0; iter < 5; iter++) {
    for (i = 1; i <= numprocs; i++) {
      nextproc = SchedProc();
      trueNext1 = (prevproc + 1) > numprocs ? 1 : prevproc + 1;
      trueNext2 = (prevproc - 1) < 1 ? numprocs : prevproc - 1;
      if (trueNext1 != nextproc && trueNext2 != nextproc) {
        Printf("ROUND ROBIN ERR: Encountered process %d\n when expecting %d or %d\n", 
            nextproc, trueNext1, trueNext2);
        failCounter++;
      }
      prevproc = nextproc;
    }
  }

  for (prevproc = 1; prevproc <= numprocs; prevproc++)
    EndingProc(prevproc);

  /* check if all process are exited */
  if (SchedProc()) {
    Printf("ROUND ROBIN ERR: Not all processes have exited\n");
    failCounter++;
  }

  totalFailCounter += failCounter;
  return failCounter;
}

int test_proportional_normal(int numprocs) {
  srand(time(NULL));
  int failCounter = 0;
  SetSchedPolicy(PROPORTIONAL);
  InitSched();
  int i, j;


  int proportions[numprocs+1];
  int counts[numprocs+1];
  for (i = 1; i <= numprocs; i++) {
    StartingProc(i);
    proportions[i] = (rand() % (100 / numprocs)) + 1;
    counts[i] = 0;
  }

  for (i = 1; i <= numprocs; i++) {
    if (MyRequestCPUrate(i, proportions[i]) == -1) {
      failCounter++;
      Printf("PROPORTIONAL ERR: Process requested %d%% CPU and MyRequestCPUrate wrongly returned -1\n",
          proportions[i]);
    }
  }

  for (i = 0; i < 100; i++) {
    counts[SchedProc()]++;
  }

  for (i = 1; i <= numprocs; i++) {
    if (!inSlackRange(proportions[i], counts[i]) && 
        counts[i] < proportions[i]) {
      Printf("PROPORTIONAL ERR: %d requested %d%% but received %d\n", i, 
          proportions[i], counts[i]);
      failCounter++;
    }
  }

  for (i = 1; i <= numprocs; i++)
    EndingProc(i);

  /* check if all process are exited */
  if (SchedProc()) {
    Printf("PROPORTIONAL ERR: Not all processes have exited\n");
    failCounter++;
  }

  totalFailCounter += failCounter;
  return failCounter;
}

int test_proportional_hog(int numprocs) {
  int failCounter = 0;
  SetSchedPolicy(PROPORTIONAL);
  InitSched();
  int i, iter;
  int counts[numprocs+1];

  for (i = 1; i <= numprocs; i++) {
    StartingProc(i);
    counts[i] = 0;
  }

  if (MyRequestCPUrate(1, 100) == -1) {
    Printf("PROPORTIONAL2 ERR: A single process requested 100%% CPU and MyRequestCPURate returned -1\n");
    failCounter++;
  }

  for (i = 2; i <= numprocs; i++) {
    if (MyRequestCPUrate(i, 20) != -1) {
      Printf("PROPORTIONAL2 ERR: Process %d requested unavailable space and MyRequestCPUrate returned 0\n",
          i);
      failCounter++;
    }
  }

  for (iter = 0; iter < 100; iter++) {
    counts[SchedProc()]++;
  }

  if (counts[1] < (100 - (numprocs - 1))) {
    Printf("PROPORTIONAL2 ERR: Process 1 should have received %d%% CPU time but got %d%%\n",
        (100 - (numprocs - 1)), counts[1]);
    failCounter++;
  }

  for (i = 2; i <= numprocs; i++) {
    if (counts[i] > 1) {
      Printf("PROPORTIONAL2 ERR: Process %d shouldn't have received >1%% CPU time (Received %d%%) " 
          "since process 1 requested 100%%. \n",
          i, counts[i]);
      failCounter++;
    }
    counts[i] = 0;
  }

  EndingProc(1);
  for (iter = 0; iter < 100; iter++) {
    counts[SchedProc()]++;
  }

  for (i = 2; i <= numprocs; i++) {
    if (!inSlackRange(100 / (numprocs-1), counts[i])) {
      Printf("PROPORTIONAL2 ERR: Process %d was expected to receive %d%% CPU (RR after no requests"
        " for CPU were made), but received %d%%\n", i, (100/(numprocs-1)), counts[i]);
      failCounter++;
    }
    EndingProc(i);
  }

  if (SchedProc()) {
    Printf("PROPORTIONAL2 ERR: Not all processes have exited\n");
    failCounter++;
  }

  totalFailCounter += failCounter;
  return failCounter;
}

int test_proportional_huge(int numprocs) {
  int failCounter = 0;
  SetSchedPolicy(PROPORTIONAL);
  InitSched();
  int i, iter;
  int counts[numprocs+1];

  for (i = 1; i <= numprocs; i++) {
    StartingProc(i);
    counts[i] = 0;
  }

  if (MyRequestCPUrate(1, 100) == -1) {
    Printf("PROPORTIONAL3 ERR: MyRequestCPUrate returned -1 when CPU was available\n");
    failCounter++;
  }

  for (iter = 0; iter < 500; iter++) {
    counts[SchedProc()]++;
  }

  if (counts[1] < (500 - (numprocs - 1))) {
    Printf("PROPORTIONAL3 ERR: Process 1 should have received at least %d CPU ticks but got %d\n",
        (500 - (numprocs - 1)), counts[1]);
    failCounter++;
  }

  for (i = 2; i <= numprocs; i++) {
    if (counts[i] > 1) {
      Printf("PROPORTIONAL3 ERR: Process %d shouldnt have received >1 CPU tick (Recieved %d ticks) "
          "since process 1 requested 100%%\n", i, counts[i]);
      failCounter++;
    }
  }

  for (i = 1; i <= numprocs; i++)
    EndingProc(i);

  if (SchedProc()) {
    Printf("PROPORTIONAL3 ERR: Not all processes have exited\n");
    failCounter++;
  }

  totalFailCounter += failCounter;
  return failCounter;
}

int test_havoc(int numprocs){
  int errors = 0;
  SetSchedPolicy(PROPORTIONAL);
  InitSched();
  int decision[100];
  int pid_top = 0;
  int last_event = 0;
  int* allocated = calloc(numprocs + 1, 4);
  int remaining_allocation = 100;
  
  srand(0);
  
  for(int t = 0; t < 1000000; t++) {
    // Start a new process?
    if(pid_top < numprocs && !(rand() % 900)) {

      if(!StartingProc(++pid_top)) {
	Printf("StartingProc failed\n");
	errors++;
      }
      // Printf("Started new process %d\n", pid_top);
      last_event = t;
    }
    
    // End the last process?
    // TOOD: with more bookkeeping we could end a random process   
    if(pid_top && !(rand() % 1000)) {

      for(int i = 0; i < 100; i++){
	if(decision[i] == pid_top) decision[i] = 0;
      }

      remaining_allocation += allocated[pid_top];
      allocated[pid_top] = 0;
      
      // Printf("Ending process %d\n", pid_top);	    
      
      if(!EndingProc(pid_top--)) {
	Printf("EndingProc failed\n");
	errors++;
      }
      
      last_event = t;
    }

    // Change the priority of a random process?
    if(pid_top && !(rand() % 500)) {
      int pid = rand() % pid_top + 1;
      int max_allocation = remaining_allocation + allocated[pid];

      if(MyRequestCPUrate(pid, max_allocation + 1) != -1) {	
	Printf("Didn't reject overallocation request of %d\n", max_allocation+1);
	errors++;
      }

      if(max_allocation) {
	int new_allocation = 1 + rand() % max_allocation;

	// Printf("Changing allocation of %d: %d -> %d\n", pid, allocated[pid], new_allocation);
      
	if(MyRequestCPUrate(pid, new_allocation) != 0){
	  Printf("Rejected valid request (remaining %d)\n", remaining_allocation);
	  errors++;
	}
      
	remaining_allocation += allocated[pid] - new_allocation;
	allocated[pid] = new_allocation;
	last_event = t;
      }
    }

    // if we've had more than 100 ticks since the last event,
    // validate the last 100 decisions
    if(t - last_event >= 100) {
      int* totals = calloc(4, numprocs+1);

      // total the ticks that each process received
      for(int i = 0; i < 100; i++)
	totals[decision[i]]++;
      
      for(int i = 1; i <= numprocs; i++){
	if(!inSlackRange(allocated[i], totals[i])) {
	  Printf("Process %d received %d ticks; expected at least %d\n", i, totals[i], allocated[i]);
	  errors++;
	}
      }

      free(totals);
    }

    decision[t % 100] = SchedProc();
  }

  free(allocated);
    
  totalFailCounter += errors;
  return errors;
}

int test(int (*testerFunction) (int)) {
  int i, failures;
  failures = 0;
  for (i = 1; i <= MAXPROCS; i++) {
    failures += testerFunction(i);
  }
  return failures;
}

void Main() {
  Printf("%d fifo failures\n", test(&test_fifo_normal));
  Printf("%d lifo failures\n", test(&test_lifo_normal));
  Printf("%d roundrobin failures\n", test(&test_rr_normal));
  Printf("%d proportional failures\n", test(&test_proportional_normal));
  Printf("%d proportional2 failures\n", test(&test_proportional_hog));
  Printf("%d proportional3 failures\n", test(&test_proportional_huge));
  Printf("%d havoc failures\n", test_havoc(MAXPROCS));

  Printf("%d Failures\n", totalFailCounter);
  if (totalFailCounter == 0)
    Printf("ALL TESTS PASSED!");
}
