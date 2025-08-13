#ifndef _BRACZ_AUTOMATA_REGISTRY_HXX_
#define _BRACZ_AUTOMATA_REGISTRY_HXX_

#include <stdio.h>
#include <inttypes.h>
#include <string>
#include <vector>
using std::string;


namespace automata {

struct RegisteredVariable {
  uint64_t event_on;
  uint64_t event_off;
  string name;
};

std::vector<RegisteredVariable>* registered_variables();

void RegisterEventVariable(const string &name, uint64_t event_on,
                           uint64_t event_off);

void PrintAllEventVariables(FILE* f);
void PrintAllEventVariablesInBashFormat(FILE* f);

} // namespace

#endif // _BRACZ_AUTOMATA_REGISTRY_HXX_
