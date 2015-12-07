#include <map>
#include <iostream>
#include <cstdio>
using namespace std;

map <string, int> trueMap;
map <string, int> allMap;

extern "C" {


void branchTrue (char* fnName) {
    string temp(fnName);
    if (trueMap.count(temp) == 0) {
        trueMap[temp] = 1;
    }
    else {
        trueMap[temp]++;
    }
}

void branchAll (char* fnName) {
    string temp (fnName);
    if (allMap.count(temp) == 0) {
        allMap[temp] = 1;
    }
    else {
        allMap[temp]++;
    }
}

void printResults () {
        // No branches in benchmark
        if (allMap.size() == 0) {
            fprintf (stderr, "%-35s %-5s %5s %-5s\n", "FUNCTION", "BIAS", "TAKEN", "TOTAL");
            fprintf (stderr, "%-35s %-5s %5d %5d\n", "main", "NA", 0, 0);
            return;
        }

        fprintf (stderr, "%-35s %-5s %5s %5s\n", "FUNCTION", "BIAS", "TAKEN", "TOTAL");
    for (map<string,int>::iterator it=trueMap.begin(); it!=trueMap.end(); ++it) {
        string funcName = it->first;
        int taken = it->second;
        int total = allMap[funcName];
        fprintf (stderr, "%-35s %-3.2f %5d %5d\n", funcName.c_str(), ((double)taken / total) * 100, taken, total);
    }
}


}


