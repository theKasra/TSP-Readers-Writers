#include <iostream>
#include <Windows.h>
#include <time.h>
#include <stdlib.h>
#include <vector>
#include <fstream>
#include <string>
#include <thread>
#include <algorithm>

struct PathInfo {
    std::vector<int> path;
    int cost;
};

// global variables
const int cores = std::thread::hardware_concurrency();
int input_number, runtime, cities;
std::vector<std::vector<int>> map;
PathInfo best_answer;

HANDLE r, w, q, m, t, c, p;   // semaphores => read, write, queue, main, thread, counter, writers' vector pushback
int read_count = 0;
std::vector<int> writers;
int thread_end_counter = 0;


int GetTotalNumberOfCities(unsigned int input_number)
{
    std::ifstream f;
    std::string temp;

    if (input_number == 1)
    {
        f.open("input_1.txt");
        if (f.is_open())
        {
            std::getline(f, temp);
            f.close();
            return std::stoi(temp);
        }
        else return -1;
    }

    else if (input_number == 2)
    {
        f.open("input_2.txt");
        if (f.is_open())
        {
            std::getline(f, temp);
            f.close();
            return std::stoi(temp);
        }
        else return -1;
    }

    else return -1;
}
std::vector<std::vector<int>> ReadInput(std::string file_path, char split_char, int cities)
{
    std::vector<std::vector<int>> map;

    std::ifstream f;
    f.open(file_path);

    if (f.is_open())
    {
        std::string first_line;
        getline(f, first_line);

        std::string str;
        while (!f.eof())
        {
            std::vector<int> row;
            for (int i = 0; i < cities; i++)
            {
                getline(f, str, split_char);
                row.push_back(std::stoi(str));
            }
            map.push_back(row);
        }
    }

    else
        std::cout << "An error occured during reading the file!" << std::endl;

    return map;
}
int FindCost(std::vector<std::vector<int>> map, int curr_city, int travel_to)
{
    for (int i = 0; map.size(); i++)
    {
        if (i == curr_city)
        {
            std::vector<int> temp = map[i];
            for (int j = 0; temp.size(); j++)
            {
                if (j == travel_to)
                {
                    return temp.at(j);
                }
            }
        }
    }
}
void PrintAnswer(PathInfo answer)
{
    std::cout << "\n";
    for (int i = 0; i < answer.path.size(); i++)
    {
        std::cout << answer.path[i];
        if (i < answer.path.size() - 1) std::cout << " -> ";
    }
    std::cout << "\nCost: " << answer.cost << std::endl;
}
void TSP_RandomPaths(std::vector<std::vector<int>> map, int cities)
{
    int thread_id = GetCurrentThreadId();

    int curr_city = 0;
    int curr_cost = 0;
    int travel_to;

    int rand_min = 1;
    int rand_max = cities - 1;

    std::vector<int> visited;
    visited.push_back(curr_city);

    srand(time(NULL) + thread_id);
    int Time = time(NULL);

    while (time(NULL) - Time < runtime)
    {
        // right before the end, make it a cycle
        if (visited.size() < cities)
            travel_to = ((rand() % rand_max - rand_min + 1) + rand_min);
        else
        {
            travel_to = 0;
            curr_cost += FindCost(map, curr_city, travel_to);
            visited.push_back(travel_to);
            curr_city = travel_to;
        }

        // avoid revisiting cities
        if (std::find(visited.begin(), visited.end(), travel_to) != visited.end() && visited.size() < cities)
            continue;

        // found a hamiltonian cycle
        if (curr_cost != 0 && visited.size() == cities + 1)
        {
            // reader
            WaitForSingleObject(q, INFINITE);
            WaitForSingleObject(r, INFINITE);
            if (read_count == 0)
                WaitForSingleObject(w, INFINITE);
            read_count++;
            ReleaseSemaphore(q, 1, NULL);
            ReleaseSemaphore(r, 1, NULL);

            if (curr_cost < best_answer.cost)
            {
                WaitForSingleObject(p, INFINITE);
                writers.push_back(thread_id);
                ReleaseSemaphore(p, 1, NULL);
            }

            WaitForSingleObject(r, INFINITE);
            read_count--;
            if (read_count == 0)
                ReleaseSemaphore(w, 1, NULL);
            ReleaseSemaphore(r, 1, NULL);

            if (std::find(writers.begin(), writers.end(), thread_id) != writers.end())
            {
                // writer
                WaitForSingleObject(q, INFINITE);
                WaitForSingleObject(w, INFINITE);
                ReleaseSemaphore(q, 1, NULL);

                if (curr_cost < best_answer.cost)
                {
                    best_answer.path = visited;
                    best_answer.cost = curr_cost;
                    writers.erase(std::remove(writers.begin(), writers.end(), thread_id), writers.end());

                    ReleaseSemaphore(m, 1, NULL);
                    WaitForSingleObject(t, INFINITE);
                }

                ReleaseSemaphore(w, 1, NULL);
            }

            // prepare for finding another path
            visited.clear();
            visited.push_back(curr_city);
            curr_cost = 0;
            continue;
        }

        curr_cost += FindCost(map, curr_city, travel_to);
        visited.push_back(travel_to);
        curr_city = travel_to;
    }

    WaitForSingleObject(c, INFINITE);
    thread_end_counter++;
    if (thread_end_counter == cores)
        ReleaseSemaphore(m, 1, NULL);
    ReleaseSemaphore(c, 1, NULL);
}


DWORD WINAPI f(LPVOID Params)
{
    std::vector<std::vector<int>>& map = *(std::vector<std::vector<int>>*) Params;
    TSP_RandomPaths(map, cities);
    return 0;
}


int main()
{
    r = CreateSemaphore(NULL, 1, 1, NULL);
    w = CreateSemaphore(NULL, 1, 1, NULL);
    q = CreateSemaphore(NULL, 1, 1, NULL);
    m = CreateSemaphore(NULL, 0, 1, NULL);
    t = CreateSemaphore(NULL, 0, 1, NULL);
    c = CreateSemaphore(NULL, 1, 1, NULL);
    best_answer.cost = INT_MAX;

    std::cout << "Which input would you like to choose?\n" << "1. input_1.txt - 5 cities\n" << "2. input_2.txt - 100 cities\n\n" << "Enter the number: ";
    std::cin >> input_number;
    std::cout << "\nPlease set the runtime value in seconds: ";
    std::cin >> runtime;
    std::cout << std::endl;

    cities = GetTotalNumberOfCities(input_number);

    if (input_number == 1)
    {
        map = ReadInput("input_1.txt", '\t', cities);
    }

    else if (input_number == 2)
    {
        map = ReadInput("input_2.txt", ' ', cities);
    }

    HANDLE* h = new HANDLE[cores];
    for (int i = 0; i < cores; i++)
        h[i] = CreateThread(NULL, 0, f, &map, 0, NULL);

    while (true)
    {
        WaitForSingleObject(m, INFINITE);

        if (thread_end_counter >= cores)
            break;

        std::cout << "> Current best cost: " << best_answer.cost << std::endl;

        ReleaseSemaphore(t, 1, NULL);
    }

    WaitForMultipleObjects(cores, h, true, INFINITE);

    PrintAnswer(best_answer);

    for (int i = 0; i < cores; i++)
        CloseHandle(h[i]);

    delete[] h;
    return 0;
}
