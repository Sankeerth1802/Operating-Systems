#include <iostream>
#include <vector>
#include <math.h>
#include <fstream>
#include <chrono>
using namespace std;

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

int main()
    {
        auto start = chrono::high_resolution_clock::now();

        int K,N,taskInc;
        vector <vector <int>> Sudoku(N,vector<int>(N,0));
        bool valid = true;

        ifstream inp("inp.txt");
        inp >> K >> N >> taskInc;

        for (int i = 0; i < N; i++)
            {
                for (int j = 0; j < N; j++)
                    {
                        inp >> Sudoku[i][j];
                    }
            }
        
        inp.close();

        ofstream out("outputSeq.txt");

        for (int i = 0; i < N; i++)
            {
                if (!check_row(Sudoku,N,i))
                    {
                        valid = false;
                        break;
                    }
                
                else if (!check_col(Sudoku,N,i))
                    {
                        valid = false;
                        break;
                    }
            }
        
        if (valid)
            {
                for (int i = 0; i < N; i += N)
                    {
                        for (int j = 0; j < N; j += N)
                            {
                                if(!check_subgrid(Sudoku,N,i,j))
                                    {
                                        valid = false;
                                        break;
                                    }
                            }
                    }
            }
        
        out << "Sudoku is " << (valid?"valid.":"invalid.") << endl;

        auto end = chrono::high_resolution_clock::now();
        chrono::duration <double> time_taken = end - start;
        out << "Total time taken: " << time_taken.count() << " seconds." << endl;

        out.close();

        return 0;
    }