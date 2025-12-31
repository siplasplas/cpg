#ifndef CONVUTF_ARGS_H
#define CONVUTF_ARGS_H

#include <string>

class Args {
    int m_argc;
    char **m_argv;
    bool m_error = false;
public:
    Args(int argc, char **argv);
    bool error() const;
    void parse();
    int inFormat;
    int outFormat;
    std::string inFile;
    std::string outFile;
};

#endif //CONVUTF_ARGS_H
