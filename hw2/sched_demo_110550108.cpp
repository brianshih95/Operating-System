#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

using namespace std;


struct ThreadArgs {
    int id;
    int policy;
    int priority;
    double time_wait;
    pthread_barrier_t *barrier;
};


void *thread_func(void *arg) {
    /* 1. Wait until all threads are ready */
    ThreadArgs *args = static_cast<ThreadArgs *>(arg);
    pthread_barrier_wait(args->barrier);

    /* 2. Do the task */
    for (int i = 0; i < 3; i++) {
        printf("Thread %d is starting\n", args->id);
	    /* Busy for <time_wait> seconds */
        time_t start = time(nullptr);
        while (true) {
            if ((time(nullptr) - start) > args->time_wait)
                break;
        }
    }
    /* 3. Exit the function */
    pthread_exit(nullptr);
}

int main(int argc, char *argv[]) {
    int opt;
    int num_threads = 0;
    double time_wait = 0.0;
    vector<int> policies;
    vector<int> priorities;

    /* 1. Parse program arguments */
    while ((opt = getopt(argc, argv, "n:t:s:p:")) != -1) {
        switch (opt) {
        case 'n':
            num_threads = atoi(optarg);
            break;
        case 't':
            time_wait = atof(optarg);
            break;
        case 's': {
            char *policy = strtok(optarg, ",");
            while (policy) {
                policies.push_back(strcmp(policy, "FIFO") == 0 ? SCHED_FIFO : SCHED_OTHER);
                policy = strtok(nullptr, ",");
            }
            break;
        }
        case 'p': {
            char *priority = strtok(optarg, ",");
            while (priority) {
                priorities.push_back(atoi(priority));
                priority = strtok(nullptr, ",");
            }
            break;
        }
        default:
            cerr << "Usage: " << argv[0] << " -n <num_threads> -t <time_wait> -s <policies> -p <priorities>\n";
            exit(1);
        }
    }
    /* 2. Create <num_threads> worker threads */
    pthread_t threads[num_threads];
    pthread_attr_t attrs[num_threads];
    ThreadArgs args[num_threads];
    pthread_barrier_t barrier;

    /* 3. Set CPU affinity */
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(0, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set);
    pthread_barrier_init(&barrier, nullptr, num_threads);

    for (int i = 0; i < num_threads; i++) {
        /* 4. Set the attributes to each thread */
        pthread_attr_init(&attrs[i]);
        pthread_attr_setinheritsched(&attrs[i], PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attrs[i], policies[i]);
        sched_param param{};
        param.sched_priority = priorities[i];
        pthread_attr_setschedparam(&attrs[i], &param);
        args[i] = {i, policies[i], priorities[i], time_wait, &barrier};
        pthread_create(&threads[i], &attrs[i], thread_func, &args[i]);
    }

    /* 5. Wait for all threads to finish */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], nullptr);
        pthread_attr_destroy(&attrs[i]);
    }
    pthread_barrier_destroy(&barrier);

    return 0;
}
