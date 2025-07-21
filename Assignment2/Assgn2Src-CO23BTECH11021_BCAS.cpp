#include <iostream>
#include <fstream>
#include <pthread.h>
#include <math.h>
#include <vector>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
using namespace std;

struct LogMessage 
    {
        string message;
        chrono::system_clock::time_point timestamp;
        
        LogMessage(const string& msg, chrono::system_clock::time_point time): message(msg), timestamp(time) {}
        
        bool operator<(const LogMessage& other) const 
            {
                return timestamp < other.timestamp;
            }
    };

string get_time_with_us()
    {
        auto now = chrono::system_clock::now();
        auto us = chrono::duration_cast<chrono::microseconds>(now.time_since_epoch()) % 1000000;
        time_t time_now = chrono::system_clock::to_time_t(now);
        tm* local_time = localtime(&time_now);
        stringstream ss;
        ss << setfill('0') << setw(2) << local_time->tm_hour << ":"
           << setfill('0') << setw(2) << local_time->tm_min << ":"
           << setfill('0') << setw(2) << local_time->tm_sec << "."
           << setfill('0') << setw(6) << us.count();

        return ss.str();
    }

typedef struct t_inp
    {
        vector <vector <int>> sudoku;
        int N;
        int taskInc;
        int t_id;
        vector<LogMessage> log_messages;
        vector<double> entry_times, exit_times;

    } t_inp;

int C = 0;
atomic<bool> valid = true;
atomic<bool> cancel_request(false); 
atomic<int> lock_value(0); 
atomic<int> waiting_threads(0); // number of threads in the waiting array
const int MAX_THREADS = 100;              // defining a maximum limit on number of threads in waiting array
atomic<bool> waiting_flags[MAX_THREADS];  // waiting array
atomic<int> next_thread(0);               // next thread that gets the access 

void initialise_waiting(int K) 
    {
        // initialising the waiting array

        for (int i = 0; i < K; i++) 
            {
                waiting_flags[i].store(false);
            }

        next_thread.store(0);
    }

void lock_bcas(t_inp *t)
    {
        // implementing bounded CAS as given in the book using compare_exchange_string instead of compare and swap

        int thread_id = t->t_id - 1;
        
        waiting_flags[thread_id].store(true);
        waiting_threads.fetch_add(1);           // updating the no. of threads that are waiting
        
        bool key = true;
        while (waiting_flags[thread_id].load() && key) 
            {
                int expected = 0;
                key = !lock_value.compare_exchange_strong(expected, 1);
                
                if (key) 
                    {
                        expected = 0;  // resetting the expected value if lock is not equal to expected value
                    }
            }
        
        waiting_flags[thread_id].store(false);
        
        if (key) 
            {
                int expected = 0;
                while (!lock_value.compare_exchange_strong(expected, 1)) 
                    {
                        expected = 0; 
                    }
            }
        
        waiting_threads.fetch_sub(1);      // reducing the no. of waiting threads
    }

void unlock_bcas(t_inp *t)
    {
        int thread_id = t->t_id - 1; 
        int K = MAX_THREADS; 
        
        if (waiting_threads.load() > 0)
            {
                // finding the next waiting thread implementing bounded waiting
                int current = next_thread.load();
                int start = current;
                
                do 
                    {
                        if (waiting_flags[current].load()) 
                            {
                                waiting_flags[current].store(false);
                                next_thread.store((current + 1) % K);
                                break;
                            }

                        current = (current + 1) % K;

                    } while (current != start);
            }
        
        lock_value.store(0);
    }

bool check_row(const vector < vector <int> > &sudoku, int size, int row)
    {
        bool output = true;
        vector <bool> numbers(size,false);

        for (int j = 0; j < size; j++)
            {
                int read_num = sudoku[row][j];
                if (numbers[read_num - 1])
                    {
                        output = false;
                        break;
                    }
                numbers[read_num - 1] = true;
            }
        
        return output;
    }

bool check_col(const vector < vector <int> > &sudoku, int size, int col)
    {
        bool output = true;

        vector <bool> numbers(size,false);

        for (int i = 0; i < size; i++)
            {
                int read_num = sudoku[i][col];

                if (numbers[read_num - 1])
                    {
                        output = false;
                        break;
                    }
                
                numbers[read_num - 1] = true;
            }
        
        return output;
    }

bool check_subgrid(const vector < vector <int> > &sudoku, int size, int row, int col)
    {
        bool output = true;
        int n = sqrt(size);

        vector <bool> numbers(size,false);

        for (int i = row; i < row + n; i++)
            {
                for (int j = col; j < col + n; j++)
                    {
                        int read_num = sudoku[i][j];

                        if (numbers[read_num - 1])
                            {
                                output = false;
                                break;
                            }
                        
                        numbers[read_num - 1] = true;
                    }
            }
        
        return output;
    }

void* validate(void* param)
    {
        if (cancel_request.load())
            {
                pthread_exit(nullptr);
            }

        t_inp *t = (t_inp *)param;

        while(true)
            {
                auto req_time = chrono::high_resolution_clock::now();
                auto now = chrono::system_clock::now();
                t->log_messages.push_back(LogMessage("Thread " + to_string(t->t_id) + " requests to enter CS at " + get_time_with_us(), now));

                lock_bcas(t);

                auto enter_time = chrono::high_resolution_clock::now();
                t->entry_times.push_back(chrono::duration<double, micro>(enter_time - req_time).count());

                now = chrono::system_clock::now();
                t->log_messages.push_back(LogMessage("Thread " + to_string(t->t_id) + " entered CS at " + get_time_with_us(), now));

                int task_start = C;

                if (t->taskInc <= (3*t->N - C))
                    {
                        C += t->taskInc;
                    }

                else
                    {
                        C += (3*t->N - C);
                    }

                auto exit_time = chrono::high_resolution_clock::now();
                t->exit_times.push_back(chrono::duration<double, micro>(exit_time - enter_time).count());

                now = chrono::system_clock::now();
                t->log_messages.push_back(LogMessage("Thread " + to_string(t->t_id) + " leaves CS at " + get_time_with_us(), now));

                unlock_bcas(t);

                if (task_start >= 3*t->N)
                    {
                        break;
                    }
                
                for (int i = task_start; i < (task_start + t->taskInc) && i < 3*t->N; i++)
                    {
                        if (cancel_request.load())
                            {
                                pthread_exit(nullptr);
                            }

                        if (i < t->N)
                            {
                                auto now = chrono::system_clock::now();
                                t->log_messages.push_back(LogMessage("Thread " + to_string(t->t_id) + " grabs row " + to_string(i + 1) + " at " + get_time_with_us(), now));

                                bool row_valid = check_row(t->sudoku, t->N, i);

                                if (!row_valid)
                                    {
                                        valid.store(false);
                                    }

                                now = chrono::system_clock::now();
                                t->log_messages.push_back(LogMessage("Thread " + to_string(t->t_id) + " completes checking row " + to_string(i + 1) + " at " + get_time_with_us() + " and finds it as " + ((row_valid)?"valid" : "invalid"), now));
                                
                            }
                        
                        else if (i < 2*t->N)
                            {
                                auto now = chrono::system_clock::now();
                                t->log_messages.push_back(LogMessage("Thread " + to_string(t->t_id) + " grabs column " + to_string(i - t->N + 1) + " at " + get_time_with_us(), now));

                                bool col_valid = check_col(t->sudoku,t->N,i - t->N);

                                if (!col_valid)
                                    {
                                        valid.store(false);
                                    }
                                
                                now = chrono::system_clock::now();
                                t->log_messages.push_back(LogMessage("Thread " + to_string(t->t_id) + " completes checking column " + to_string(i - t->N + 1) + " at " + get_time_with_us() + " and finds it as " + ((col_valid)?"valid" : "invalid"), now));

                            }
                        
                        else
                            {
                                int grid = i - 2*t->N;
                                int n = sqrt(t->N);

                                int row = (grid/n) * n;
                                int col = (grid%n) * n;

                                int subgrid_no = (row/n) * n + (col/n) + 1;

                                auto now = chrono::system_clock::now();
                                t->log_messages.push_back(LogMessage("Thread " + to_string(t->t_id) + " grabs subgrid " + to_string(subgrid_no) + " at " + get_time_with_us(), now));

                                bool subgrid_valid = check_subgrid(t->sudoku,t->N,row,col);

                                if (!subgrid_valid)
                                    {
                                        valid.store(false);
                                    }
                                
                                now = chrono::system_clock::now();
                                t->log_messages.push_back(LogMessage("Thread " + to_string(t->t_id) + " completes checking subgrid " + to_string(subgrid_no) + " at " + get_time_with_us() + " and finds it as " + ((subgrid_valid)?"valid" : "invalid"), now));

                            }

                        if (!valid.load())
                            {
                                cancel_request.store(true);
                                pthread_exit(nullptr);
                            }
                    }
            }
        
        return nullptr;
    }

int main()
    {
        valid = true;
        auto start_time = chrono::high_resolution_clock::now();

        t_inp t;
        t.N = 0;
        t.t_id = 0;
        t.taskInc = 0;
        int K;
        ifstream inp("inp.txt");
        inp >> K >> t.N >> t.taskInc;

        t.sudoku.resize(t.N,vector <int> (t.N));
        for (int i = 0; i < t.N; i++)
            {
                for (int j = 0; j < t.N; j++)
                {
                    inp >> t.sudoku[i][j];
                }
            }

        inp.close();

        initialise_waiting(K);              // initialising the waiting array

        vector <pthread_t> thread_ids(K);
        vector <t_inp> tds(K);

        for (int i = 0; i < K; i++)
            {   
                tds[i].N = t.N;
                tds[i].sudoku = t.sudoku;
                tds[i].t_id = i + 1;
                tds[i].taskInc = t.taskInc;

                pthread_create(&thread_ids[i],nullptr,validate,&tds[i]);
            }
        
        for (int i = 0; i < K; i++)
            {
                pthread_join(thread_ids[i],nullptr);

                if (cancel_request.load())
                    {
                        for (int j = i + 1; j < K; j++)
                            {
                                pthread_cancel(thread_ids[j]);
                            }
                        break;
                    }
            }
        
        auto end_time = chrono::high_resolution_clock::now();
        double total_time = chrono::duration<double, micro>(end_time - start_time).count();

        vector<LogMessage> all_messages;
        for (int i = 0; i < K; i++) 
            {
                all_messages.insert(all_messages.end(), tds[i].log_messages.begin(), tds[i].log_messages.end());
            }
        
        sort(all_messages.begin(), all_messages.end());

        ofstream out("outputBoundedcas.txt");

        for (const auto& log : all_messages) 
            {
                out << log.message << endl;
            }
            
        out << (valid.load()?"Valid Sudoku":"Invalid Sudoku") << endl;

        double total_entry_time = 0.0;
        double total_exit_time = 0.0;
        double max_entry_time = 0.0;
        double max_exit_time = 0.0;
        int total_entry_count = 0;
        int total_exit_count = 0;
        
        for (int i = 0; i < K; i++) 
            {
                for (const auto& time : tds[i].entry_times) 
                    {
                        total_entry_time += time;
                        max_entry_time = max(max_entry_time, time);
                        total_entry_count++;
                    }
                
                for (const auto& time : tds[i].exit_times) 
                    {
                        total_exit_time += time;
                        max_exit_time = max(max_exit_time, time);
                        total_exit_count++;
                    }
            }
        //cout<<C<<endl;
        out << "Time taken to check the validity of the Sudoku: " << total_time << " microseconds" << endl;
        out << "Average time taken by a thread to enter the CS: " << (total_entry_count > 0 ? total_entry_time / total_entry_count : 0) << " microseconds" << endl;
        out << "Average time taken by a thread to exit the CS: " << (total_exit_count > 0 ? total_exit_time / total_exit_count : 0) << " microseconds" << endl;
        out << "Worst-case time taken by a thread to enter the CS: " << max_entry_time << " microseconds" << endl;
        out << "Worst-case time taken by a thread to exit the CS: " << max_exit_time << " microseconds" << endl;

        out.close();

        return 0;
    }