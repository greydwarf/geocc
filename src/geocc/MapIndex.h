#ifndef MAPINDEX_H_PXQDU5IT
#define MAPINDEX_H_PXQDU5IT

#include <string>
#include <set>
#include <vector>
#include <memory>
#include <geos_c.h>

namespace geocc {
struct Country {
   int id;
   std::string fips, iso3, name;
};

class MapIndex {
public:
   MapIndex();
   ~MapIndex();
   void readWorldMap(FILE *f);
   std::set<const geocc::Country*> countryContaining(double lat, double lon);
private:
   GEOSContextHandle_t ctx;
   GEOSSTRtree* tree;
   std::vector<std::unique_ptr<geocc::Country>> country_vec;
};

void clean(GEOSSTRtree* idx);
}

#endif /* end of include guard: MAPINDEX_H_PXQDU5IT */
