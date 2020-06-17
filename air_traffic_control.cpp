#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <deque>
#include <cstring>

struct Plane {
    int plane_id;
    time_t request_time;
    time_t runway_time;
    char status;
};

int pthread_sleep(int seconds);
void *tower(void *); // Controlling the runway traffic.
void *arriving_plane(void *);
void *departing_plane(void *);
void print_console(long time_diff);
int generate_plane(char stat);

using namespace std;
// using deques in order to travers over them.
deque <Plane> arrive_queue; // queue for planes that are landing.
deque <Plane> emergency_arrival_queue; // queue for planes that are departing.
deque <Plane> depart_queue; // queue for planes that are departing.
// locks
pthread_mutex_t runway_lock;
pthread_mutex_t t_lock;
// conds
pthread_cond_t on_runway;
pthread_cond_t t_cond;
// attribute
pthread_attr_t attr;
// initial threads
pthread_t tower_thread;
pthread_t arrivals_thread;
pthread_t departures_thread;
// atomic integers
int num_planes = 0; // total number of planes.
int plane_on_runway = 0; // plane with the granted permission.
// initial variables
int planeID;
int n_sec = 0;
int simulation_time = 60;
double prob = 0.5;
double probCheck;
bool endSim = false;
time_t start_time;
time_t current_time;

// main program
int main(int argc, char *argv[]) {

    for(int i = 1; i < argc; i++) {
        // obtaining command line arguments.
        if(strcmp(argv[i], "-s") == 0) {
            simulation_time = atoi(argv[i+1]);
        } else if (strcmp(argv[i], "-p") == 0) {
            prob = atof(argv[i+1]);
        } else if (strcmp(argv[i], "-n") == 0) {
            n_sec = atoi(argv[i+1]);
        }

    }
    // different random seed with each run
    srand(time(NULL));
    // initializing mutex locks.
    pthread_mutex_init(&runway_lock, NULL);
    pthread_mutex_init(&t_lock, NULL);
    // initializing condition variables.
    pthread_cond_init(&on_runway, NULL);
    pthread_cond_init(&t_cond, NULL);
    // initiating pthread attribute.
    pthread_attr_init(&attr);
    // creating initial threads (an arriving plane, a departing plane, and the tower).
    pthread_create(&tower_thread, &attr, tower, NULL);
    pthread_create(&arrivals_thread, &attr, arriving_plane, NULL);
    pthread_create(&departures_thread, &attr, departing_plane, NULL);
    // waiting for planes.
    while(num_planes < 1) { /* making sure the simulation doesn't start without any planes */ }
    // in order to control the simulation flow.
    start_time = time(NULL);
    current_time = time(NULL);
    // simulating for simulation_time
    while(current_time < start_time + simulation_time) {
        // calculating time difference to check for emergencies.
        long time_diff = (current_time - start_time);
        // generating random probability value.
        probCheck = (double) rand() / (double) RAND_MAX;
        // to create a new thread in each iteration.
        pthread_t new_thread;
        if(probCheck <=  prob || (time_diff % 40 == 0 && time_diff != 0)) {
            // a plane arrives.
            pthread_create(&new_thread, &attr, arriving_plane, (void *) time_diff);
        } 
        if(probCheck <= (1-prob)) {
            // a plane departs.
            pthread_create(&new_thread, &attr, departing_plane, NULL);
        }
        // planes either arriving at or departing from the runway every second.
        pthread_sleep(1);
        // printing to the console for time_diff >= n_sec.
        print_console(time_diff);
        // updating current_time.
        current_time = time(NULL);
    }
    endSim = true;
    // destroying mutexes, condition variables, and attribute.

    pthread_attr_destroy(&attr);

    pthread_mutex_destroy(&runway_lock);
    pthread_mutex_destroy(&t_lock);

    pthread_cond_destroy(&on_runway);
    pthread_cond_destroy(&t_cond);
    // exiting.
    exit(0);
}

// controlling the runway usage.
void *tower(void *) {
    // waiting for the first plane to arrive.
    pthread_mutex_lock(&runway_lock);
    while(num_planes < 1) { pthread_cond_wait(&on_runway, &runway_lock); }
    pthread_mutex_unlock(&runway_lock);
    cout << "Starting simulation."<< endl;
    // initializing the log file.
    FILE *plane_log;
    plane_log = fopen("./planes.log", "w+");
    fprintf(plane_log, "Plane ID   \tStatus   \tRequest Time   \tRunway Time   \tTurnaround Time\n");
    fprintf(plane_log, "_______________________________________________________________________________\n");
    // counter to avoid starvation.
    int cnt;
    // entering simulation.
    while(!endSim) {
        // Each arrival and departure takes 2t.
        pthread_sleep(2);
        Plane plane2log;
        // securing the runway
        pthread_mutex_lock(&runway_lock);
        if(!emergency_arrival_queue.empty()) { // checking whether there is a plane that requires emergency landing.
            // going to be editing the plane that uses the runway.
            pthread_mutex_lock(&t_lock);
            // obtaining the plane in the front of the emergency plane queue.
            plane2log = emergency_arrival_queue.front();
            // updating the runway permission.
            plane_on_runway = plane2log.plane_id;
            // removing the plane in the front of the emergency plane queue.
            emergency_arrival_queue.pop_front();
            // decreasing the number of planes.
            num_planes--;
            // signaling and unlocking the locks.
            pthread_cond_broadcast(&t_cond);
            pthread_mutex_unlock(&t_lock);
            pthread_mutex_unlock(&runway_lock);
        } else {
            // still favoring the landing planes, but causing no starvation.
            if(!depart_queue.empty() // checking if the depart queue is not empty.
                // careful for the empty arrival queue.
                && ((cnt % 10) < 4 || arrive_queue.empty()) // cnt check in order to avoid starvation.
                && (depart_queue.size() > 5 // maximum of 5 planes waiting to depart, or
                    || arrive_queue.empty())) { // no arriving planes.
                // going to be editing the plane that uses the runway.
                pthread_mutex_lock(&t_lock);
                // obtaining the plane in the front of the arriving plane queue.
                plane2log = depart_queue.front();
                // updating the runway permission.
                plane_on_runway = plane2log.plane_id;
                // removing the plane in the front of the arriving plane queue.
                depart_queue.pop_front();
                // decreasing the number of planes.
                num_planes--;
                // signaling and unlocking the locks.
                pthread_cond_broadcast(&t_cond);
                pthread_mutex_unlock(&t_lock);
            } else {
                // going to be editing the plane that uses the runway.
                pthread_mutex_lock(&t_lock);
                // obtaining the plane in the front of the arriving plane queue.
                plane2log = arrive_queue.front();
                // updating the runway permission.
                plane_on_runway = plane2log.plane_id;
                // removing the plane in the front of the arriving plane queue.
                arrive_queue.pop_front();
                // decreasing the number of planes.
                num_planes--;
                // signaling and unlocking the locks.
                pthread_cond_broadcast(&t_cond);
                pthread_mutex_unlock(&t_lock);
            }
            // updating the counter.
            cnt++;
            pthread_mutex_unlock(&runway_lock);
        }
        // logging in the plane that has used the runway.
        plane2log.runway_time = time(NULL);
        fprintf(plane_log, "%d \t\t %c \t\t %ld \t\t %ld \t\t %ld\n",
                plane2log.plane_id,
                plane2log.status,
                plane2log.request_time - start_time,
                plane2log.runway_time - start_time,
                plane2log.runway_time - plane2log.request_time);
    }
    cout << "Simulation ended." << endl;
    // ending the logs and the simulation.
    fprintf(plane_log, "_______________________________________________________________________________\n");
    fclose(plane_log);
    pthread_cond_broadcast(&t_cond); // in order to be able to destroy the mutex.
    pthread_exit(0);
}
// arriving plane thread, exits when (signal indicating that its turn has come is) received from the tower.
void *arriving_plane(void *time_diff) {
    // need to check for the elapes time for emergency landings to occur in every 40 seconds.
    long elapsed_time = (long) time_diff;
    // ID of the newly generated plane.
    int buff_id;
    if(elapsed_time % 40 == 0 && elapsed_time > 0) {
        buff_id = generate_plane('E'); // emergency landing.
    } else {
        buff_id = generate_plane('L');
    }
    // waiting for the signal from the tower.
    pthread_mutex_lock(&t_lock);
    while(plane_on_runway != buff_id && !endSim) { pthread_cond_wait(&t_cond, &t_lock); } //  or if the mutext is locked at the end of simulation
    pthread_mutex_unlock(&t_lock);
    // cout << buff_id << " has arrived." << endl;
    // plane has arrived, thread exits.
    pthread_exit(0);
}
// departing plane thread, exits when signal (indicating that its turn has come) is received from the tower.
void *departing_plane(void *) {
    // ID of the newly generated plane.
    int buff_id;
    buff_id = generate_plane('D');
    // waiting for the signal from the tower.
    pthread_mutex_lock(&t_lock);
    while(plane_on_runway != buff_id && !endSim) { pthread_cond_wait(&t_cond, &t_lock); } // or if the mutext is locked at the end of simulation
    pthread_mutex_unlock(&t_lock);
    // cout << buff_id << " has departed." << endl;
    // plane has departed, thread exits.
    pthread_exit(0);
}
// generating the requested plane according to the given status, and placing it in the corresponding queue.
int generate_plane(char stat) {
    // securing the queues and num_planes.
    pthread_mutex_lock(&runway_lock);
    // ID of the plane to be inserted.
    int ins_planeID;
    // plane generation time.
    time_t gen_time = time(NULL);
    // plane to be inserted.
    Plane plane2Insert;
    // incrementing the number of planes.
    num_planes++;
    // fixing for the initial delay.
    // if(num_planes == 1 || num_planes == 2) gen_time++;
    // generating and inserting planes according to their status.
    if(stat == 'L') {
        ins_planeID = 2 * (planeID++);
        plane2Insert.plane_id = ins_planeID;
        plane2Insert.request_time = gen_time;
        plane2Insert.runway_time = NULL;
        plane2Insert.status = stat;
        arrive_queue.push_back(plane2Insert);
    } else if(stat == 'E') {
        ins_planeID = 2 * (planeID++);
        plane2Insert.plane_id = ins_planeID;
        plane2Insert.request_time = gen_time;
        plane2Insert.runway_time = NULL;
        plane2Insert.status = stat;
        emergency_arrival_queue.push_back(plane2Insert);
    } else if (stat == 'D') {
        ins_planeID = 2 * (planeID++) + 1;
        plane2Insert.plane_id = ins_planeID;
        plane2Insert.request_time = gen_time;
        plane2Insert.runway_time = NULL;
        plane2Insert.status = stat;
        depart_queue.push_back(plane2Insert);
    }
    // signaling if it is the first plane, to start the simulation.
    if(num_planes == 1) {
        pthread_cond_signal(&on_runway);
    }
    // unlocking.
    pthread_mutex_unlock(&runway_lock);
    // returning the ID of the newly-generated plane.
    return ins_planeID;
}

// prints to the console after the requested amount of time.
void print_console(long time_diff) {
    while(num_planes == 0) {  }
    // going to be printing the content of each queue for time_diff greater than n_sec.
    if(time_diff >= n_sec) {

        cout << "Planes in the air at " << time_diff << "s: " << endl;
        for (int i = 0; i < emergency_arrival_queue.size(); i++)
            cout << "| " << emergency_arrival_queue[i].plane_id << " |";
        for (int i = 0; i < arrive_queue.size(); i++)
            cout << "| " << arrive_queue[i].plane_id << " |";
        cout << endl;

        cout << "Planes on the ground at " << time_diff << "s." << endl;
        for(int i=0; i<depart_queue.size(); i++)
            cout << "| " << depart_queue[i].plane_id << " |";
        cout << endl;
    }
}

/**************************************************************************
 pthread_sleep takes an integer number of seconds to pause the current thread
 original by Yingwu Zhu
 updated by Muhammed Nufail Farooqi
 *****************************************************************************/
int pthread_sleep (int seconds)
{
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    struct timespec timetoexpire;
    if(pthread_mutex_init(&mutex,NULL))
    {
        return -1;
    }
    if(pthread_cond_init(&conditionvar,NULL))
    {
        return -1;
    }
    struct timeval tp;
    //When to expire is an absolute time, so get the current time and add //it to our delay time
    gettimeofday(&tp, NULL);
    timetoexpire.tv_sec = tp.tv_sec + seconds; timetoexpire.tv_nsec = tp.tv_usec * 1000;

    pthread_mutex_lock (&mutex);
    int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock (&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);

    //Upon successful completion, a value of zero shall be returned
    return res;

}