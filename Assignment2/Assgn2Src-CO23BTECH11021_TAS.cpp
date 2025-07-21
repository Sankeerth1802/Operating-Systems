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
        // struct that logs the messages

        string message;
        chrono::system_clock::time_point timestamp;
        
        LogMessage(const string& msg, chrono::system_clock::time_point time) : message(msg), timestamp(time) {}
        
        bool operator<(const LogMessage& other) const 
            {
                return timestamp < other.timestamp;
            }
    };

string get_time_with_us()
    {
        // function to obtain the time stamp upto microsecond

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
        // struct that is passed into the thread function as argument.

        vector <vector <int>> sudoku;
        int N;
        int taskInc;
        int t_id;
        vector<LogMessage> log_messages;
        vector<double> entry_times;
        vector<double> exit_times;

    } t_inp;

int C = 0;                                          // the shared counter
atomic<bool> valid = true;                          // for the overall validity of Sudoku
atomic <bool> cancel_request(false);                // to track if any thread initiates cancellation of all the other threads.
atomic_flag lock = ATOMIC_FLAG_INIT;                // to lock the cs

void lock_tas(t_inp *t)
    {
        // locking using TAS

        while (lock.test_and_set()) {}
    }

void unlock_tas(t_inp *t)
    {
        // unlocking

        lock.clear();
    }

// functions to check the individual rows, columns and subgrids.
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
                // checks if any thread requested cancellation

                pthread_exit(nullptr);
            }

        t_inp *t = (t_inp *)param;        // typecasting the input 

        while(true)
            {
                auto req_time = chrono::high_resolution_clock::now();
                auto now = chrono::system_clock::now();
                t->log_messages.push_back(LogMessage("Thread " + to_string(t->t_id) + " requests to enter CS at " + get_time_with_us(), now));

                lock_tas(t);         // locking cs

                auto enter_time = chrono::high_resolution_clock::now();
                t->entry_times.push_back(chrono::duration<double, micro>(enter_time - req_time).count());

                now = chrono::system_clock::now();
                t->log_messages.push_back(LogMessage("Thread " + to_string(t->t_id) + " entered CS at " + get_time_with_us(), now));

                int task_start = C;          // incrementing shared counter

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

                unlock_tas(t);        // unlocking cs

                if (task_start >= 3*t->N)
                    {
                        // if a thread enters after completing all the checks

                        break;
                    }
                
                for (int i = task_start; i < (task_start + t->taskInc) && i < 3*t->N; i++)
                    {
                        if (cancel_request.load()) 
                            {
                                // for early termination

                                pthread_exit(nullptr);
                            }

                        if (i < t->N)
                            {
                                // checking rows

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
                                // checking columns

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
                                // checking subgrids

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
                                // if any thread finds any check as invalid then request for cancellation.

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

        ifstream inp("inp.txt");        // reading from input file
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

        vector <pthread_t> thread_ids(K);             // for thread ids
        vector <t_inp> tds(K);                        // to send as arguments

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
                // collecting all the messages from all the threads

                all_messages.insert(all_messages.end(), tds[i].log_messages.begin(), tds[i].log_messages.end());
            }
        
        sort(all_messages.begin(), all_messages.end());      // sorting using timestamps

        ofstream out("outputTas.txt");

        for (const auto& log : all_messages) 
            {
                // printing the logged messages

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
                        // calculating the average and worst case entry time

                        total_entry_time += time;
                        max_entry_time = max(max_entry_time, time);
                        total_entry_count++;
                    }
                
                for (const auto& time : tds[i].exit_times) 
                    {
                        // calculating the average and worst case exit time

                        total_exit_time += time;
                        max_exit_time = max(max_exit_time, time);
                        total_exit_count++;
                    }
            }
        
        // printing all the times

        out << "Time taken to check the validity of the Sudoku: " << total_time << " microseconds" << endl;
        out << "Average time taken by a thread to enter the CS: " << (total_entry_count > 0 ? total_entry_time / total_entry_count : 0) << " microseconds" << endl;
        out << "Average time taken by a thread to exit the CS: " << (total_exit_count > 0 ? total_exit_time / total_exit_count : 0) << " microseconds" << endl;
        out << "Worst-case time taken by a thread to enter the CS: " << max_entry_time << " microseconds" << endl;
        out << "Worst-case time taken by a thread to exit the CS: " << max_exit_time << " microseconds" << endl;

        out.close();

        return 0;
    }