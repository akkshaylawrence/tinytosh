// Prints the config JSON the firmware's serializer produces, for run.sh to compare.
//   harness default          the config a new device starts with
//   harness roundtrip FILE   FILE parsed over the defaults, then serialized again

#include <fstream>
#include <iostream>
#include <sstream>

#include "JsonSerializer.h"
#include "PopulationService.h"
#include "TimeService.h"

unsigned long millis() { return 0; }
EspClass ESP;

String TimeService::getCurrentTimeShort(String) { return ""; }
String TimeService::getFullDate() { return ""; }
String TimeService::formatMinsFromMidnight(int, String, bool) { return ""; }
String TimeService::formatDurationMins(int) { return ""; }
long long PopulationService::getLivePopulation(long long, double, int) { return 0; }

int main(int argc, char** argv) {
  AppState state;
  if (argc == 3 && std::string(argv[1]) == "roundtrip") {
    std::ifstream file(argv[2]);
    std::stringstream text;
    text << file.rdbuf();
    if (!JsonSerializer::parseConfig(text.str().c_str(), state)) {
      std::cerr << argv[2] << " did not parse\n";
      return 1;
    }
  }
  std::cout << JsonSerializer::buildConfigJson(state.config).c_str() << "\n";
  return 0;
}
