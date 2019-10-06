#include "world_generated.h"
#include "s2/s2shape_index.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2contains_point_query.h"
#include "s2/s2polygon.h"
#include "s2/s2builderutil_s2polygon_layer.h"
#include <stdio.h>
#include <string>
#include <cassert>
#include <unordered_map>

struct Country {
   int id;
   std::string fips, iso3, name;
};

std::vector<std::unique_ptr<Country>> country_vec;
std::unordered_map<uint32_t, Country*> countries;
S2ShapeIndex* readWorldMap(FILE *f) {
   auto ret = new MutableS2ShapeIndex();
   uint32_t id = 0;
   while (true) {
      int32_t sz;
      auto fcount = fread(&sz, sizeof(sz), 1, f);
      if (feof(f)) {
         break;
      }
      if (fcount == 0) {
         throw std::runtime_error(strerror(errno));
      }

      auto buf = std::make_unique<char[]>(sz);
      fcount = fread(static_cast<void*>(buf.get()), sz, 1, f);
      if (fcount == 0) {
         throw std::runtime_error(strerror(errno));
      }

      auto fbc = fb::GetCountry(static_cast<void*>(buf.get()));
      auto country = std::make_unique<Country>();
      country->id = fbc->id();
      country->fips = std::string(fbc->fips()->c_str());
      country->iso3 = std::string(fbc->iso3()->c_str());
      country->name = std::string(fbc->name()->c_str());
      printf("%s - %s\n", country->name.c_str(), country->iso3.c_str());

      auto regions = fbc->regions();
      for (auto ii = 0UL; ii < regions->Length(); ii++) {
         auto region = regions->Get(ii);
         auto fb_loops = region->loops();

         std::vector<std::unique_ptr<S2Loop>> s2loops;
         for (auto jj = 0UL; jj < fb_loops->Length(); jj++) {
            auto fb_loop = fb_loops->Get(jj);
            auto vertices = fb_loop->vertices();

            std::vector<S2Point> points;
            for (auto kk = 0UL; kk < vertices->Length(); kk++) {
               const fb::Point* v = vertices->Get(kk);
               points.emplace_back(v->x(), v->y(), v->z());
            }
            auto s2loop = std::make_unique<S2Loop>(points);
            s2loops.emplace_back(std::move(s2loop));
         }
         auto s2poly = std::make_unique<S2Polygon>(std::move(s2loops));
         auto t = std::unique_ptr<const S2Polygon>(std::move(s2poly));
         auto shape = std::make_unique<S2Polygon::OwningShape>(std::move(t));
         countries[id++] = country.get();
         ret->Add(std::move(shape));
      }
      country_vec.push_back(std::move(country));
   }
   return ret;
}

std::set<const Country*> countryContaining(const S2ShapeIndex *idx, double lat, double lon) {
   auto query = MakeS2ContainsPointQuery(idx);
   auto loc = S2Point(S2LatLng::FromDegrees(lat, lon));
   auto shapes = query.GetContainingShapes(loc);
   std::set<const Country*> ret;
   for (const auto& shape: shapes) {
      auto id = shape->id();
      printf("id: %d\n", shape->id());
      ret.insert(countries[id]);
   }
   return ret;
}

int main() {
   auto f = fopen("data/world_map.dat", "rb");
   if (f == nullptr)  {
      perror("Error: ");
      return 1;
   }

   auto idx = readWorldMap(f);
   auto all_ccs = countryContaining(idx, 39.7392, -104.9903);

   for (auto c: all_ccs) {
      printf("%s", c->name.c_str());
   }
}
