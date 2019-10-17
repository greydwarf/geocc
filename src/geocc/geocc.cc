#include "world_generated.h"
#include "geos_c.h"
#include "zstd.h"
#include <cstdio>
#include <stdarg.h>
#include <string>
#include <cassert>
#include <unordered_map>

struct Country {
   int id;
   std::string fips, iso3, name;
};

char* NOTICE=const_cast<char*>("NOTICE");
char* ERROR=const_cast<char*>("ERROR");
GEOSContextHandle_t ctx;

void geosMessageHandler(const char*message, void* prefix) {
   const char* p = static_cast<char*>(prefix);
   printf("GEOS %s - %s\n", p, message);
}

std::vector<std::unique_ptr<Country>> country_vec;

struct RegionEntry {
   GEOSGeometry *geom;
   const GEOSPreparedGeometry *prepared;
   const Country* country;
};

GEOSSTRtree* readWorldMap(FILE *f) {
   GEOSContext_setNoticeMessageHandler_r(ctx, geosMessageHandler, static_cast<void*>(NOTICE));
   GEOSContext_setErrorMessageHandler_r(ctx, geosMessageHandler, static_cast<void*>(ERROR));
   auto ret = GEOSSTRtree_create_r(ctx, 10);
   auto geosReader = GEOSWKTReader_create_r(ctx);
   while (true) {
      uint32_t uncompressed_sz;
      uint32_t compressed_sz;
      auto fcount = 0U;

      fcount = fread(&uncompressed_sz, sizeof(uncompressed_sz), 1, f);
      if (feof(f)) {
         break;
      }
      if (fcount == 0) {
         printf("ftell: %lu\n", ftell(f));
         throw std::runtime_error(strerror(errno));
      }

      fcount = fread(&compressed_sz, sizeof(compressed_sz), 1, f);
      if (fcount == 0) {
         throw std::runtime_error(strerror(errno));
      }

      auto c_buf = std::make_unique<char[]>(compressed_sz);
      fcount = fread(static_cast<void*>(c_buf.get()), compressed_sz, 1, f);
      if (fcount == 0) {
         throw std::runtime_error(strerror(errno));
      }

      auto u_buf = std::make_unique<char[]>(uncompressed_sz);
      auto c_result = ZSTD_decompress(static_cast<void*>(u_buf.get()), uncompressed_sz, 
                                      static_cast<void*>(c_buf.get()), compressed_sz);
      if (ZSTD_isError(c_result)) {
         fprintf(stderr, "Error during compression: %s\n", ZSTD_getErrorName(c_result));
         throw std::runtime_error("Compression Error");
      }
      auto fbc = fb::GetCountry(static_cast<void*>(u_buf.get()));
      auto country = std::make_unique<Country>();
      country->id = fbc->id();
      country->fips = std::string(fbc->fips()->c_str());
      country->iso3 = std::string(fbc->iso3()->c_str());
      country->name = std::string(fbc->name()->c_str());

      auto regions = fbc->regions();
      for (size_t ii = 0; ii < regions->Length(); ii++) {
         auto poly = regions->Get(ii);
         GEOSGeometry* geom = GEOSWKTReader_read_r(ctx, geosReader, poly->wkt()->c_str());
         auto prepared = GEOSPrepare_r(ctx, geom);
         auto entry = new RegionEntry{geom, prepared, country.get()};
         GEOSSTRtree_insert(ret, geom, static_cast<void*>(entry));
      }
      country_vec.push_back(std::move(country));
   }
   GEOSWKTReader_destroy_r(ctx, geosReader);
   return ret;
}

struct QCBParams {
   std::set<const Country*>* ret;
   GEOSGeometry* p;
};

void q_callback(void *item, void* vparams) {
   auto c = static_cast<const RegionEntry*>(item);
   auto params = static_cast<QCBParams*>(vparams);
   if (GEOSPreparedIntersects_r(ctx, c->prepared, params->p)) {
      params->ret->insert(c->country);
   }
}

std::set<const Country*> countryContaining(GEOSSTRtree *tree, double lat, double lon) {
   std::set<const Country*> ret;
   auto lonlat = GEOSCoordSeq_create_r(ctx,1, 2);
   if (lonlat == nullptr) {
      printf("error: 1\n");
      return ret;
   }
   if (GEOSCoordSeq_setX_r(ctx, lonlat, 0, lon) == 0 ||
      GEOSCoordSeq_setY_r(ctx, lonlat, 0, lat) == 0) {
      printf("error: 2\n");
      return ret;
   }

   auto point = GEOSGeom_createPoint_r(ctx, lonlat);
   if (point == nullptr) {
      printf("error: 4\n");
      return ret;
   }

   QCBParams params{&ret, point};
   GEOSSTRtree_query(tree, point, q_callback, &params);
   GEOSGeom_destroy_r(ctx, point);
   return ret;
}

void cleanRegions(void* item, void*) {
   auto c = static_cast<const RegionEntry*>(item);
   GEOSGeom_destroy_r(ctx, c->geom);
   GEOSPreparedGeom_destroy_r(ctx, c->prepared);
   delete c;
}

void clean(GEOSSTRtree* idx) {
   GEOSSTRtree_iterate(idx, cleanRegions, nullptr);
   GEOSSTRtree_destroy_r(ctx, idx);
   GEOS_finish_r(ctx);
}

int main() {
   ctx = GEOS_init_r();
   auto f = fopen("data/world_map.dat", "rb");
   if (f == nullptr)  {
      perror("Error: ");
      return 1;
   }

   auto idx = readWorldMap(f);
   auto all_ccs = countryContaining(idx, 39.7392, -104.9903);

   for (auto c: all_ccs) {
      printf("%s\n", c->name.c_str());
   }
   fclose(f);
   clean(idx);
}
