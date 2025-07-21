#include <iostream>
#include <vector>
#include <pthread.h>
#include <chrono>
#include <unistd.h>
#include <random>
#include <fstream>

using namespace std;

vector <int> buffer(10,0);
vector <chrono::microseconds> prod_times;
vector <chrono::microseconds> cons_times;

int capacity;
int np, nc;
int cntp, cntc;
double myu_p, myu_c;

int fill1 = 0;
int use1 = 0;
int count = 0;

pthread_mutex_t mutex;

string getSystime()
    {
        auto now = chrono::system_clock::now();
        time_t now_time = chrono::system_clock::to_time_t(now);
        tm *now_tm = localtime(&now_time);

        char time_buffer[80];
        strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", now_tm);
        return string(time_buffer);
    }

double expovariate(double lambda)
    {
        random_device rd;
        mt19937 gen(rd());
        exponential_distribution<> dist(lambda);
        return dist(gen);
    }

void put(int value)
    {
        buffer[fill1] = value;
        fill1 = (fill1 + 1) % capacity;
        count++;
    }

int get()
    {
        int tmp = buffer[use1];
        use1 = (use1 + 1) % capacity;
        count--;
        return tmp;
    }

ofstream outFile("output-lock.txt");

void *producer(void *arg)
    {
        int id = *(int *)arg;
        chrono::microseconds total_time(0);

        for (int i = 0; i < cntp; i++)
            {
                auto start = chrono::high_resolution_clock::now();

                int item = id + i + 1;

                pthread_mutex_lock(&mutex);

                while (count == capacity)
                    {
                        // busy waiting

                        pthread_mutex_unlock(&mutex);            // releasing lock to prevent deadlock

                        pthread_mutex_lock(&mutex);              
                    }
                
                put(item);
                string prodTime = getSystime();
                outFile << i + 1 << "th item: " << item << " produced by thread " << id << " at " << prodTime << " into buffer location " << (fill1 + capacity - 1) % capacity << endl;
                
                pthread_mutex_unlock(&mutex);

                double t1 = expovariate(1000.0/myu_p);
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

                pthread_mutex_lock(&mutex);

                while (count == 0)
                    {
                        pthread_mutex_unlock(&mutex);

                        pthread_mutex_lock(&mutex);
                    }
                
                int item = get();
                string consTime = getSystime();
                outFile << i + 1 << "th item: " << item << " consumed by thread " << id << " at " << consTime << " from buffer location " << (use1 + capacity - 1) % capacity << endl;

                pthread_mutex_unlock(&mutex);

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
        
        double avg_prod_time = total_prod_time.count()/np;
        double avg_cons_time = total_cons_time.count()/nc;

        outFile << "The average time taken by a producer thread is " << avg_prod_time << " microseconds." << endl;
        outFile << "The average time taken by a consumer thread is " << avg_cons_time << " microseconds." << endl;
    }

int main()
    {
        ifstream inpFile("inp-params.txt");

        if (!inpFile)
            {
                outFile << "Error opening input file." << endl;
                return -1;
            }
        
        inpFile >> capacity >> np >> nc >> cntp >> cntc >> myu_p >> myu_c;
        inpFile.close();

        int prodNum = np * cntp;
        int consNum = nc * cntc;

        if (prodNum > (capacity + consNum))
            {
                outFile << "The set of inputs leads to a deadlock state where no progress happens." << endl << "Reason: Buffer is full with more producers waiting but no consumers left.";
                return -1;
            }
        
        if (consNum > prodNum)
            {
                outFile << "The set of inputs leads to a deadlock state where no progress happens." << endl << "Reason: Consumers are waiting but there are no items to consume.";
                return -1;
            }

        buffer.resize(capacity);
        prod_times.resize(np);
        cons_times.resize(nc);

        pthread_mutex_init(&mutex, NULL);

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
                pthread_join(consumers[i], NULL);
            }
        
        pthread_mutex_destroy(&mutex);
        
        calculateTimes();

        outFile.close();

        return 0;
    }