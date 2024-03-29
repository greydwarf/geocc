#include "MapIndex.h"
#include "world_generated.h"
#include "geos_c.h"
#include "zstd.h"
#include <cstdio>
#include <stdarg.h>
#include <cassert>
#include <cmath>
#include <unordered_map>

char* NOTICE=const_cast<char*>("NOTICE");
char* ERROR=const_cast<char*>("ERROR");

namespace {
struct RegionEntry {
   GEOSGeometry *geom;
   const GEOSPreparedGeometry *prepared;
   const geocc::Country* country;
};

struct QCBParams {
   const GEOSContextHandle_t* ctx;
   std::set<const geocc::Country*>* ret;
   GEOSGeometry* p;
};

void geosMessageHandler(const char*message, void* prefix) {
   const char* p = static_cast<char*>(prefix);
   printf("GEOS %s - %s\n", p, message);
}

void q_callback(void *item, void* vparams) {
   auto c = static_cast<const RegionEntry*>(item);
   auto params = static_cast<QCBParams*>(vparams);
   if (GEOSPreparedIntersects_r(*params->ctx, c->prepared, params->p)) {
      params->ret->insert(c->country);
   }
}

void cleanRegions(void* item, void* vctx) {
   GEOSContextHandle_t* ctx = static_cast<GEOSContextHandle_t*>(vctx);
   auto c = static_cast<const RegionEntry*>(item);
   GEOSGeom_destroy_r(*ctx, c->geom);
   GEOSPreparedGeom_destroy_r(*ctx, c->prepared);
   delete c;
}
}

geocc::MapIndex::MapIndex() {
   ctx = GEOS_init_r();
   GEOSContext_setNoticeMessageHandler_r(ctx, geosMessageHandler, static_cast<void*>(NOTICE));
   GEOSContext_setErrorMessageHandler_r(ctx, geosMessageHandler, static_cast<void*>(ERROR));
   tree = GEOSSTRtree_create_r(ctx, 10);
   for (int ii=0; ii < NUM_POINTS; ++ii) {
      double theta = 2*M_PI/NUM_POINTS * ii;
      sines[ii] = sin(theta);
      cosines[ii] = cos(theta);
   }
}

geocc::MapIndex::~MapIndex() {
   GEOSSTRtree_iterate(tree, cleanRegions, &ctx);
   GEOSSTRtree_destroy_r(ctx, tree);
   GEOS_finish_r(ctx);
}

GEOSGeometry* geocc::MapIndex::make_ellipse(double lat, double lon, double smaj, double smin, double orient) const {
   auto ellipse_points = GEOSCoordSeq_create_r(ctx, NUM_POINTS+1, 2);
   
   orient = orient*M_PI/180;    // convert degrees to radians
   orient = M_PI/2.0 - orient;  // Convert from compass to cartesian orientation

   // Convert Km to degrees latitude/longitude
   smaj = smaj/110.0;
   smin = smin/110.0;

   for (int ii=0; ii < NUM_POINTS; ++ii) {
      if (GEOSCoordSeq_setX_r(ctx, ellipse_points, ii, 
                          lon + smaj*cosines[ii]*cos(orient) - smin*sines[ii]*sin(orient)) == 0) {
         printf("error: 1\n");
      }
      if (GEOSCoordSeq_setY_r(ctx, ellipse_points, ii,
                          lat + smaj*cosines[ii]*sin(orient) + smin*sines[ii]*cos(orient)) == 0) {
         printf("error: 2\n");
      }
   }
   double x;
   double y;
   GEOSCoordSeq_getX_r(ctx, ellipse_points, 0, &x);
   GEOSCoordSeq_getY_r(ctx, ellipse_points, 0, &y);
   GEOSCoordSeq_setX_r(ctx, ellipse_points, NUM_POINTS, x);
   GEOSCoordSeq_setY_r(ctx, ellipse_points, NUM_POINTS, y);
   return GEOSGeom_createLinearRing_r(ctx, ellipse_points);
}

void geocc::MapIndex::readWorldMap(FILE *f) {
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
      auto country = std::make_unique<geocc::Country>();
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
         GEOSSTRtree_insert(tree, geom, static_cast<void*>(entry));
      }
      country_vec.push_back(std::move(country));
   }
   GEOSWKTReader_destroy_r(ctx, geosReader);
}

std::set<const geocc::Country*> geocc::MapIndex::countryContaining(double lat, double lon) const {
   std::set<const geocc::Country*> ret;

   auto point = make_ellipse(lat, lon, 0.5, 0.5, 0);
   if (point == nullptr) {
      printf("error: 4\n");
      return ret;
   }

   QCBParams params{&ctx, &ret, point};
   GEOSSTRtree_query(tree, point, q_callback, &params);
   GEOSGeom_destroy_r(ctx, point);
   return ret;
}

