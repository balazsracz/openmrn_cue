#include <inttypes.h>

#include "registry.hxx"

#include <vector>

namespace automata {

std::vector<RegisteredVariable>* registered_variables() {
  static std::vector<RegisteredVariable> v;
  return &v;
}

void RegisterEventVariable(const string& name, uint64_t event_on,
                           uint64_t event_off) {
  RegisteredVariable var;
  var.event_on = event_on;
  var.event_off = event_off;
  var.name = name;
  registered_variables()->push_back(var);
}

uint8_t eventbyte(uint64_t event, int b) { return (event >> (b * 8)) & 0xff; }

string EventToJmriFormat(uint64_t event) {
  char buf[100];
  sprintf(buf, "%02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X", eventbyte(event, 7),
          eventbyte(event, 6), eventbyte(event, 5), eventbyte(event, 4),
          eventbyte(event, 3), eventbyte(event, 2), eventbyte(event, 1),
          eventbyte(event, 0));
  return buf;
}

void PrintAllEventVariables(FILE* f) {
  fprintf(stderr, "%zd total variables \n", registered_variables()->size());
  for (const auto& v : *registered_variables()) {
    string name = v.name;
    if (name.substr(0, 7) == "logic2.") {
      name.erase(5, 1);
    }
    fprintf(f,
            "<sensor systemName=\"MS%s;%s\" inverted=\"false\" "
            "userName=\"%s\"> <systemName>MS%s;%s</systemName> "
            "<userName>%s</userName> </sensor>\n",
            EventToJmriFormat(v.event_on).c_str(),
            EventToJmriFormat(v.event_off).c_str(), name.c_str(),
            EventToJmriFormat(v.event_on).c_str(),
            EventToJmriFormat(v.event_off).c_str(), name.c_str());
  }
}

void PrintAllEventVariablesInBashFormat(FILE* f) {
  for (const auto& v : *registered_variables()) {
    fprintf(f, "%016" PRIX64 " %s=on\n", v.event_on, v.name.c_str());
    fprintf(f, "%016" PRIX64 " %s=off\n", v.event_off, v.name.c_str());
  }
}

}  // namespace automata
