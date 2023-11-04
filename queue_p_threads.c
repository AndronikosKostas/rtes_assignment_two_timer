#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h> 
#include <time.h>

#define QUEUESIZE 16                        
#define PRODUCERS 1
#define CONSUMERS 1
#define SECONDS 10
#define ISOLATED 0
#define COMBINED 1

// struct instaces for the gettimeofday
struct timeval start_time_s, start_pro_queue_add_s, end_pro_queue_add_s, start_con_queue_del_s, end_con_queue_del_s;

unsigned int WORK_LOST = 0;
long int TASKS_TO_EXECUTE, DONE_TASKS = 0;
// arrays for storing the data //
unsigned long long int producer_period[400000];
unsigned long long int producer_timestamps_period[400000];

unsigned long long int elapsed_time_queue_add[400000];
unsigned long long int elapsed_time_queue_del[400000];

typedef struct
{
  void *(*work)(void *arg);
  void *arg;
} workFunctionData;


typedef struct
{
  // buf: array with size: QUEUESIZE
  workFunctionData buf[QUEUESIZE];
  // head and tail are indexes for the array
  long head, tail;
  // full and empty are used as flags // 1 or 0 // TRUE or FALSE
  int full, empty;
  // mutex variable for controlling thread access to data
  pthread_mutex_t *mut;
  // condition variables to allow threads to synchronize based upon the actual value of data.
  pthread_cond_t *notFull, *notEmpty;
} queue;

typedef struct
{
  int period;         // time between the calls of TimerFcn (ms)
  long long int tasksToExecute; // how many times the TimerFcn will be executed
  int startDelay;     // how many seconds will be passed before the first start of TimerFcn occur
  int *tDrift;
  // all the four below variables are pointers that point to functions (StartFunc, StopFunc, TimerFunc, ErrorFunc)
  void (*startFunc)(void *arg); // function pointer //
  //
  void (*stopFunc)(void *arg); // function pointer //
  //
  void *(*timerFunc)(void *arg); // function pointer //
  //
  void (*errorFunc)(void *arg); // function pointer //
  //
  queue *fifo;
  //
  workFunctionData timer_func;
  // necessary for the pthread_create() function //
  pthread_t thread_id;
  //
  void *(*producer)(void *arg);

} Timer;



void *producer(void *args);
void *consumer(void *args);
queue *queueInit(void);
void queueDelete(queue *q);
void queueAdd(queue *q, workFunctionData in);
void queueDel(queue *q, workFunctionData *out);


/// @brief StartFunc: is being executed in case that TimerFunc needs initiliazation. In our case we will not use that
/// @param arg
void StartFunc(void *arg)
{
  printf("nothing to init");
}

/// @brief StopFunc: called after the last call of TimerFunc // may be it can be used for the end of the consumer thread
/// @param arg
void StopFunc(void *arg)
{
  printf("end of the last call of TimerFunc");
}

/// @brief TimerFunc : the function that is being entered or deleted from the FIFO, from the producer thread and the consumer thread correspondingly
/// @param arg
/// @return NULL pointer
void *TimerFunc(void *arg)
{
  printf("I am working now !!! \n");
  return NULL;
}

/// @brief ErrorFunc: called if the queue is full
/// @param arg
void ErrorFunc(void *arg)
{
  printf("queue is full and and the producer cannot put another task in the queue");
  WORK_LOST++;
}

/// @brief initiliazation of the timer struct
/// @param fifo pointer that points to the queue struct
/// @param period time between consecutive calls of the TimerFunc
/// @param startDelay delay before the first execution of the TimerFunc
/// @return Timer
Timer *init(queue *fifo, int period, int startDelay)
{
  Timer *t = (Timer *)malloc(sizeof(Timer));
  t->period = period;
  // tasks to execute calculation based on how many seconds we want to run the timer //
  float temp = (SECONDS / (period * 0.001));
  t->tasksToExecute = (long long int)temp;
  printf("tasksToExecute : %d \n", t->tasksToExecute);
  TASKS_TO_EXECUTE = t->tasksToExecute;
  printf("time to run : %d sec \n", SECONDS);
  t->startDelay = startDelay;
  t->startFunc = StartFunc;
  t->stopFunc = StopFunc;
  t->errorFunc = ErrorFunc;
  t->timerFunc = TimerFunc;
  t->fifo = fifo;
  t->timer_func.work = TimerFunc;
  t->timer_func.arg = NULL;
  t->producer = producer;

  return t;
}

/// @brief
/// @param t : t pointer of type Timer
void start(Timer *t)
{
  printf("producer thread creation \n");
  usleep(t->startDelay);
  pthread_create(&t->thread_id, NULL, t->producer, t); // the delay is inside the producer function //
}

/// @brief function that calculates the delay before the first execution of TimerFcn happens given the d/m/y h:min:sec
/// @param t pointer that points in Timer struct
/// @param year year to start
/// @param month month to start (1-12)
/// @param day day to start (1-31)
/// @param hour hour to start (0-23)
/// @param min min to start (0-59)
/// @param sec sec to start (0-59)
void startAt(Timer *t, int year, int month, int day, int hour, int min, int sec)
{
  struct tm time_info;
  time_t start_time, current_time;
  time_info.tm_year = year - 1900; // years since 1900
  time_info.tm_mon = month - 1;    // Months from 0 to 11
  time_info.tm_mday = day;         // Day of the month (1-31)
  time_info.tm_hour = hour;        // Hours (0-23)
  time_info.tm_min = min;          // Minutes (0-59)
  time_info.tm_sec = sec;          // Seconds (0-59)
  // SECONDS BEFORE THE PRODUCER STARTS //
  // convert the time_info structure into a time_t value
  start_time = mktime(&time_info);
  printf("start_time : %s \n", ctime(&start_time));
  time(&current_time);
  // calculate the seconds that are remaining //
  double seconds_remaning = difftime(start_time, current_time);
  // pass this value to the Timer startDelay variable
  t->startDelay = (int)seconds_remaning;
  // thread creation //
  usleep(t->startDelay);
  pthread_create(&t->thread_id, NULL, t->producer, t);
}

/// @brief main program
int main()
{
  // Create a pointer of type queue //
  queue *fifo;
  // Init the fifo
  fifo = queueInit();
  // if the queueInit don't run properly, print an error message //
  if (fifo == NULL)
  {
    fprintf(stderr, "main: Queue Init failed.\n");
    exit(1);
  }
  int mode = ISOLATED; //
  Timer *t;
  // Init the Timer t instance //
  t = init(fifo, 1000, 2);
  printf("SPECIFICATIONS \n");
  printf("PERIOD = %d ms \n", t->period);
  // create the consumer threads after the timer creation //
  pthread_t *c;
  c = (pthread_t *)malloc(CONSUMERS * sizeof(pthread_t));
  for (int i = 0; i < CONSUMERS; i++)
    pthread_create(&c[i], NULL, consumer, fifo);

  // start at 09/19/23 at 15:56:10z 
  // startAt(t, 2023, 9, 20, 0, 8, 40);

  // Start the timer and the producer thread //
  printf("START DELAY = %d sec \n", t->startDelay);
  start(t);
  // producer thread join //
  pthread_join(t->thread_id, NULL);
  // consumer thread join  //
  for (int i = 0; i < CONSUMERS; i++)
    pthread_join(c[i], NULL);
  // FIFO Delete
  queueDelete(fifo);
  printf("FIFO DELETED\n");

  // take the measurements //
  if (mode == ISOLATED)
  {
    FILE *f_iso = fopen("C:\\Users\\andro\\Desktop\\rtes_with_version_control\\data_isolated.txt", "w");
    if (f_iso == NULL)
    {
      perror("Unable to open the file");
      return 1;
    }
    for (int i = 0; i < t->tasksToExecute - 1; i++)
    {
      producer_period[i] = producer_timestamps_period[i + 1] - producer_timestamps_period[i];

      char str[32];
      sprintf(str, "%llu", producer_period[i]); // Convert the time into string to save it on the file
      fputs(str, f_iso);
      fputc(',', f_iso);
      sprintf(str, "%llu", elapsed_time_queue_add[i]);
      fputs(str, f_iso);
      fputc(',', f_iso);
      sprintf(str, "%llu", elapsed_time_queue_del[i]);
      fputs(str, f_iso);
      fputc('\n', f_iso);
    }
    // close the file //
    fclose(f_iso);
  }
  if (mode == COMBINED)
  {
    FILE *f_comb = fopen("C:\\Users\\andro\\Desktop\\rtes_with_version_control\\data_combined.txt", "w");
    if (f_comb == NULL)
    {
      perror("Unable to open the file");
      return 1;
    }
    printf("COMBINED \n");
  }

  // free the memory of the pointer t //
  free(t);
  // return //
  return 0;
}

/// @brief the producer function puts function pointers to the FIFO if it is not full
/// @param q
/// @return
void *producer(void *q)
{
  // in : instance of workfunction struct //
  workFunctionData in;
  // necessary typecast that connects with the pthread_create() in which the *arg
  // It is the pointer to the argument that will be passed to the producer function
  // Also we need the *t to connect the two fifos with each other 
  Timer *t = (Timer *)q;
  // make the two pointers to look at the same address // basically make the two fifos to be the same //
  queue *fifo;
  fifo = t->fifo;
  // here the producer starts to put tasks at the FIFO //

  struct timeval tProdExecStart, tProdExecEnd, tProdExecTemp;
  // int tDriftTotal = 0;
  // int driftCounter = -1;

  for (int i = 0; i < t->tasksToExecute; i++)
  {
    // Time drifting timestamps setup.
    gettimeofday(&tProdExecStart, NULL);

    // store the current time in the start_time struct //
    gettimeofday(&start_time_s, NULL);
    // timestamps for each call of the producer //
    producer_timestamps_period[i] = start_time_s.tv_sec * 1000000 + start_time_s.tv_usec;
    //  lock the mut of the fifo to provide access to onlu one thread at a time in fifo //
    pthread_mutex_lock(fifo->mut);
    printf("task to execute = %d \n", i);
    while (fifo->full)
    {
      printf("producer: queue FULL.\n");
      pthread_cond_wait(fifo->notFull, fifo->mut); // block the producer thread until the 'notFull' condition variable
                                                   // is signaled by the consumer thread to inform the producer that
                                                   // can continue put tasks in the queue. Ehile waiting the thread
                                                   // automatically releases the mutex, allowing other threads to
                                                   // access the critical section if needed
    }
    // associate the struct workFunction with the TimerFunc
    in.arg = NULL;
    in.work = TimerFunc;
    // Timing Calculation 
    long int start_pro_queue_add, end_pro_queue_add;
    gettimeofday(&start_pro_queue_add_s, NULL);
    start_pro_queue_add = start_pro_queue_add_s.tv_sec * (int)1e6 + start_pro_queue_add_s.tv_usec;
    queueAdd(t->fifo, in);
    gettimeofday(&end_pro_queue_add_s, NULL);
    end_pro_queue_add = end_pro_queue_add_s.tv_sec * (int)1e6 + end_pro_queue_add_s.tv_usec;
    elapsed_time_queue_add[i] = end_pro_queue_add - start_pro_queue_add;
    // Timing Calculation //
    // release the mutex to grant the consumer thread access to the FIFO //
    pthread_mutex_unlock(fifo->mut);
    // signal to the consumer thread that the FIFO is not empty, so it can perform dequeue //
    pthread_cond_signal(fifo->notEmpty);

    // Skip time drifting logic for first iteration.
    if (i == 0)
    {
      usleep(t->period * 1e3);
      continue;
    }

    gettimeofday(&tProdExecEnd, NULL);
    // tDrift
    long int tDrift = tProdExecEnd.tv_sec * (int)1e6 + tProdExecEnd.tv_usec - (tProdExecStart.tv_sec * (int)1e6 + tProdExecStart.tv_usec);
    usleep(1000 * (t->period) - tDrift);

    // usleep to support periodic calls of the TimerFunc //
  }
  return (NULL);
}

/// @brief dequeue items from the queue 
/// @param q
/// @return NULL pointer
void *consumer(void *q)
{
  queue *fifo;
  fifo = (queue *)q;
  // out : instance of workfunction struct //
  workFunctionData out;
  while (DONE_TASKS < TASKS_TO_EXECUTE)
  {
    // lock the mut of the fifo to provide access to onlu one thread at a time in fifo //
    pthread_mutex_lock(fifo->mut);
    while (fifo->empty)
    {
      printf("consumer: queue EMPTY.\n");
      // block the consumer thread until 'notEmpty' condition variable is signaled by the producer thread //
      pthread_cond_wait(fifo->notEmpty, fifo->mut);
    }
    // TIMING CALCULATIONS FOR THE CONSUMER //
    long int start_con_queue_del, end_con_queue_del;
    gettimeofday(&start_con_queue_del_s, NULL);
    start_con_queue_del = start_con_queue_del_s.tv_sec * 1000000 + start_con_queue_del_s.tv_usec;
    // delete from fifo //
    queueDel(fifo, &out);
    gettimeofday(&end_con_queue_del_s, NULL);
    end_con_queue_del = end_con_queue_del_s.tv_sec * 1000000 + end_con_queue_del_s.tv_usec;
    elapsed_time_queue_del[DONE_TASKS] = end_con_queue_del - start_con_queue_del;
    // execute the function //                           
    out.work(out.arg);                                  
    free(out.arg);
    // release the mutex to grant the producer thread access to the FIFO //
    pthread_mutex_unlock (fifo->mut);
    // sends the signal 'notFull, so the producer can add another task to the queue 
    pthread_cond_signal (fifo->notFull);                
    printf("consumer: recieved");
    printf("\n");
    DONE_TASKS++;
  }
  return (NULL);
}

/// @brief allocate space, init the queue variables and the synchronization mechanisms/variables of the queue
/// @param  void
/// @return the pointer which points to the first pos of the array/queue
queue *queueInit(void)
{
  // pointer of type queue to points to the data of struct queue
  queue *q;
  // allocate space for the queue
  q = (queue *)malloc(sizeof(queue));
  if (q == NULL) return (NULL);
  // q is empty = true //
  q->empty = 1;
  // q is full = false //
  q->full = 0;
  // head and tail of the queue are pointing in the first position of the array //
  q->head = 0;
  q->tail = 0;
  // allocate space for the mutex var //
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  // init mutex
  pthread_mutex_init (q->mut, NULL);
  // allocate space for the notFull condition var //
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  // init the notFull condition var
  pthread_cond_init (q->notFull, NULL);
  // allocate space for the notEmpty condition var //
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  // init the notEmpty condition var //
  pthread_cond_init (q->notEmpty, NULL);
  // returns a pointer that points in the first pos of the array/queue //
  return (q);
}

/// @brief free and destroy the variables the queue that were necessary for the queue and finnaly free the q array space
/// @param q 
void queueDelete (queue *q)
{
  // Destroy the mutex var //
  pthread_mutex_destroy (q->mut);
  // free the mutex's space //
  free (q->mut);	
  // Destroy the notFull condition var //
  pthread_cond_destroy (q->notFull);
  // free the notFull's space //
  free (q->notFull);
  // Destroy the notEmpty condition var // 
  pthread_cond_destroy (q->notEmpty);
  // free the notEmpty's space // 
  free (q->notEmpty);
  // free the q array space //
  free (q);
}

/// @brief add elements of type workFunctionData in the FIFO, basically add function pointers
/// @param q 
/// @param in 
void queueAdd (queue *q, workFunctionData in)
{
  // place the in value in the tail index of the array
  q->buf[q->tail] = in;
  
  // increase the tail index
  q->tail++;
  // If the tail went at the final element of the queue, place the tail at the start //
  if (q->tail == QUEUESIZE)
    q->tail = 0;
  
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;
  return;
}

/// @brief delete elements of type workFunctionData in the FIFO, basically add function pointers
/// @param q 
/// @param out 
void queueDel (queue *q, workFunctionData *out)
{
  *out = q->buf[q->head];
  q->head++;
  if (q->head == QUEUESIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;
  return;
}