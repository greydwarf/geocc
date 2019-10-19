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
   std::set<const geocc::Country*> countryContaining(double lat, double lon) const;
private:
   constexpr static int NUM_POINTS = 36;
   constexpr static double KM_PER_LON = 111.6;
   constexpr static double KM_PER_LAT_EQ = 111.321;
   GEOSContextHandle_t ctx;
   GEOSSTRtree* tree;
   std::vector<std::unique_ptr<geocc::Country>> country_vec;
   std::array<double, NUM_POINTS> sines;
   std::array<double, NUM_POINTS> cosines;

   GEOSGeometry* make_ellipse(double lat, double lon, double smaj, double smin, double orient) const;
};

void clean(GEOSSTRtree* idx);
}

#endif /* end of include guard: MAPINDEX_H_PXQDU5IT */
