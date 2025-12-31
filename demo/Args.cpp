#include "Args.h"
#include "iostream"
#include "cstring"

using namespace std;

Args::Args(int argc, char **argv) : m_argc(argc), m_argv(argv) {
}

bool Args::error() const {
    return m_error;
}

void Args::parse() {
    m_error = true;
    if (m_argc != 4) {
        cout << "bad number of parameters" << endl;
        return;
    }
    if (m_argv[1][0] != '-') {
        cout << "first parameter must begin with -"  << endl;
        return;
    }
    if (strlen(m_argv[1]) != 4) {
        cout << "first parameter must have len = 4";
        return;
    }
    if (m_argv[1][1]!='c' || !isdigit(m_argv[1][2]) || !isdigit(m_argv[1][3])) {
        cout << "first parameter must be form -cMN where M and N are digits" << endl;
        return;
    }
    inFormat = m_argv[1][2] - '0';
    outFormat = m_argv[1][3] - '0';
    for (int i=2; i<=3; i++)
        if (m_argv[i][0] == '-') {
            cout << "parameters (except first) must not begin with -"  << endl;
            return;
        }
    inFile = m_argv[2];
    outFile = m_argv[3];
    m_error = false;
}
