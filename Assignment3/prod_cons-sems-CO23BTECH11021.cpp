#include <iostream>
#include <vector>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <chrono>
#include <random>
#include <fstream>

using namespace std;

vector <int> buffer(10,0);                                          // buffer capacity is taken as 10 which further will be updating.
vector <chrono::microseconds> prod_times;                           // time taken by producer threads
vector <chrono::microseconds> cons_times;                           // time taken by consumer threads
 
int capacity;
int cntp,cntc;
int np,nc;
double myu_p,myu_c;

int fill1 = 0;                                                      // index where new item is added in buffer
int use1 = 0;                                                       // index where an item is consumed from buffer
 
sem_t empty1;                                                       // semaphore to ensure buffer is empty
sem_t full;                                                         // semaphore to ensure buffer is full
sem_t lock;                                                         // semaphore to acquire the critical section

string getSystime()
    {
        // function that returns the present system time

        auto now = chrono::system_clock::now();
        time_t now_time = chrono::system_clock::to_time_t(now);
        tm *now_tm = localtime(&now_time);

        char time_buffer[80];
        strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", now_tm);
        return string(time_buffer);
    }

double expovariate(double lambda)
    {
        // function that returns an exponential distributed value about given mean

        random_device rd;
        mt19937 gen(rd());
        exponential_distribution<> dist(lambda);
        return dist(gen);
    }

void put(int value)
    {
        // function to add items to the buffer

        buffer[fill1] = value;
        fill1 = (fill1 + 1)%capacity;
    }

int get()
    {
        // function to consume items from the buffer

        int tmp = buffer[use1];
        use1 = (use1 + 1)%capacity;
        return tmp;
    }

ofstream outFile("output-sem.txt");


void *producer(void *arg)
    {
        int id = *(int *)arg;                                                // thread id
        chrono::microseconds total_time(0);

        for (int i = 0; i < cntp; i++)
            {
                auto start = chrono::high_resolution_clock::now();
                
                int item = id + i + 1;                                       // new item to be produced

                sem_wait(&empty1);                                           // Ensuring the buffer is not full
                sem_wait(&lock);
 
                put(item);
                string prodTime = getSystime();
                outFile << i + 1 << "th item: " << item << " produced by thread " << id << " at " << prodTime << " into buffer location " << (fill1 + capacity - 1) % capacity << endl;

                sem_post(&lock);
                sem_post(&full);

                double t1 = expovariate(1000.0/(myu_p));
                usleep(t1 * 1e6);

                auto end = chrono::high_resolution_clock::now();
                total_time += chrono::duration_cast<chrono::microseconds>(end - start);
            }

        prod_times[id - 1] = total_time/cntp;

        return nullptr;
    }

void *consumer(void *arg)
    {
        int id = *(int *)arg;
        chrono::microseconds total_time(0);

        for (int i = 0; i < cntc; i++)
            {
                auto start = chrono::high_resolution_clock::now();

                sem_wait(&full);                                           // ensuring the buffer is not empty
                sem_wait(&lock);

                int item = get();
                string consTime = getSystime();
                outFile << i + 1 << "th item: " << item << " consumed by thread " << id << " at " << consTime << " from buffer location " << (use1 + capacity - 1) % capacity << endl;

                sem_post(&lock);
                sem_post(&empty1);

                double t2 = expovariate(1.0/(myu_c/1000));
                usleep(t2 * 1e6);

                auto end = chrono::high_resolution_clock::now();
                total_time += chrono::duration_cast<chrono::microseconds>(end - start);
            }
        
        cons_times[id - 1] = total_time/cntc;

        return nullptr;
    }

void calculateTimes()
    {
        // function that calculates the average time taken by threads.

        chrono::microseconds total_prod_time(0);
        chrono::microseconds total_cons_time(0);

        for (int i = 0; i < np; i++)
            {
                total_prod_time += prod_times[i];
            }
        
        for (int i = 0; i < nc; i++)
            {
                total_cons_time += cons_times[i];
            }
        
        double avg_prod_time = total_prod_time.count()/(np);
        double avg_cons_time = total_cons_time.count()/(nc);

        outFile << "The average time taken by a producer thread is " << avg_prod_time << " microseconds." << endl;
        outFile << "The average time taken by a consumer thread is " << avg_cons_time << " microseconds." << endl;

    }

int main()
    {
        ifstream infile("inp-params.txt");

        if(!infile)
            {
                outFile << "Error opening input file" << endl;
                return -1;
            }
        
        infile >> capacity >> np >> nc >> cntp >> cntc >> myu_p >> myu_c;
        infile.close();

        int prodNum = np * cntp;                         // total number of items to be produced
        int consNum = nc * cntc;                         // total number of items to be consumed

        if (prodNum > (capacity + consNum))
            {
                // error handling case 1

                outFile << "The set of inputs leads to a deadlock state where no progress happens." << endl << "Reason: Buffer is full with more producers waiting but no consumers left.";
                return -1;
            }
        
        if (consNum > prodNum)
            {
                // error handling case 2

                outFile << "The set of inputs leads to a deadlock state where no progress happens." << endl << "Reason: Consumers are waiting but there are no items to consume.";
                return -1;
            }

        buffer.resize(capacity);                             // resizing the buffer and producer,consumer time vectors
        prod_times.resize(np);
        cons_times.resize(nc);

        sem_init(&empty1, 0, capacity);                      // initialising the semaphores
        sem_init(&full, 0, 0);
        sem_init(&lock, 0, 1);

        pthread_t producers[np];
        pthread_t consumers[nc];
        int producer_ids[np];
        int consumer_ids[nc];

        for (int i = 0; i < np; i++)
            {
                producer_ids[i] = i + 1;
                pthread_create(&producers[i], NULL, producer, &producer_ids[i]);
            }
        
        for (int i = 0; i < nc; i++)
            {
                consumer_ids[i] = i + 1;
                pthread_create(&consumers[i], NULL, consumer, &consumer_ids[i]);
            }

        for (int i = 0; i < np; i++)
            {
                pthread_join(producers[i], NULL);
            }
        
        for (int i = 0; i < nc; i++)
            {
                pthread_join(consumers[i],NULL);
            }
        
        sem_destroy(&empty1);
        sem_destroy(&full);
        sem_destroy(&lock);

        calculateTimes();

        outFile.close();

        return 0;
    }

