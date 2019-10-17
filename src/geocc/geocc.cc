#include "MapIndex.h"
#include "world_generated.h"
#include "geos_c.h"
#include <cstdio>
#include <stdarg.h>
#include <string>
#include <cassert>
#include <unordered_map>

int main() {
   auto f = fopen("data/world_map.dat", "rb");
   if (f == nullptr)  {
      perror("Error: ");
      return 1;
   }

   geocc::MapIndex map;
   map.readWorldMap(f);
   auto all_ccs = map.countryContaining(39.7392, -104.9903);

   for (auto c: all_ccs) {
      printf("%s\n", c->name.c_str());
   }
   fclose(f);
}
