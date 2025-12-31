#ifndef CPG_CPMANAGER_H
#define CPG_CPMANAGER_H

#include <vector>
#include <string>
#include <cpg/Codepage.h>

class CpManager {
    std::vector<Codepage*> codepages;
public:
    CpManager();
    virtual ~CpManager();

    Codepage* getByName(const std::string& name);
    std::vector<std::string> listAll();
    size_t size() const { return codepages.size(); }
};

#endif //CPG_CPMANAGER_H
