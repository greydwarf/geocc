#include "world_generated.h"
#include "s2/s2polygon.h"
#include "s2/s2builderutil_s2polygon_layer.h"
#include <zstd.h>
#include <stdio.h>
#include <string>
#include <cassert>
#include <pqxx/pqxx>

typedef std::pair<double, double> latlon;
typedef std::vector<latlon> raw_loop;
typedef std::vector<raw_loop> raw_poly;
typedef std::vector<std::unique_ptr<S2Polygon>> Regions;

struct Country {
   int id;
   std::string fips;
   std::string iso3;
   std::string name;
   bool is_water;
   std::unique_ptr<Regions> regions;
};

static bool starts_with(const std::string& a, const std::string& prefix) {
   return std::equal(prefix.begin(), prefix.end(), a.begin());
}

static S2Point to_point(double lat, double lon) {
   auto safe_lat = std::min(std::max(-89.999999, lat), 89.999999);
   auto safe_lon = std::min(std::max(-179.999999, lon), 179.999999);
   return S2Point(S2LatLng::FromDegrees(safe_lat, safe_lon));
}

char eat_char(const std::string& str, size_t& pos, const char* expected) {
   for (const char *x = expected; x; ++x) {
      if (str[pos] == *x) {
         pos++;
         return *x;
      }
   }
   std::cerr << "wkt: " << str << "\npos: " << pos << 
      ".  Expected one of '" << expected << "', Instead found: " << str[pos] << std::endl;
   throw std::runtime_error("Parse error");
}

std::unique_ptr<S2Loop> parse_loop(const std::string& wkt, size_t& start) {
   std::vector<S2Point> points;
   eat_char(wkt, start, "(");
   while (true) {
      size_t end;
      double lon = std::stod(wkt.substr(start), &end);
      start += end;
      eat_char(wkt, start, " ");

      double lat = std::stod(wkt.substr(start), &end);
      points.emplace_back(to_point(lat, lon));
      start+= end;
      char next = eat_char(wkt, start, "),");
      if (next == ')') {
         points.pop_back();
         auto ret = std::make_unique<S2Loop>(points);
         return ret;
      } 
   }
}

std::unique_ptr<S2Polygon> parse_polygon(const std::string& wkt, size_t& start) {
   auto loops = std::vector<std::unique_ptr<S2Loop>>();
   while (true) {
      eat_char(wkt, start, "(");
      loops.emplace_back(parse_loop(wkt, start));
      char next = eat_char(wkt, start, "),");
      if (next == ')') {
         auto ret = std::make_unique<S2Polygon>();
         ret->InitNested(std::move(loops));
         return ret;
      }
   }
}

std::unique_ptr<Regions> parse_wkt(const std::string& wkt) {
   size_t start = 0;
   auto ret = std::make_unique<Regions>();
   if (!starts_with(wkt, "MULTIPOLYGON(")) return ret;
   start = strlen("MULTIPOLYGON(");
   while (true) {
      ret->emplace_back(parse_polygon(wkt, start));

      char next = eat_char(wkt, start, "),");
      if (next == ')') {
         start++;
         return ret;
      }
   }
}

void write_country(FILE* out, const Country& country) {
   flatbuffers::FlatBufferBuilder builder(1024*1024);
   std::vector<flatbuffers::Offset<fb::Polygon>> fb_regions;
   for (auto& s2poly : *country.regions) {
      std::vector<flatbuffers::Offset<fb::Loop>> fb_loops;
      for (int ii=0; ii < s2poly->num_loops(); ++ii) {
         const auto s2loop = s2poly->loop(ii);
         std::vector<flatbuffers::Offset<fb::Point>> fb_points;
         for (int jj=0; jj < s2loop->num_vertices(); ++jj) {
            const auto& vertex = s2loop->vertex(jj);
            const auto& data = vertex.Data();
            auto point = fb::CreatePoint(builder, data[0], data[1], data[2]);
            fb_points.push_back(point);
         }
         auto vertices = builder.CreateVector(fb_points);
         auto fb_loop = CreateLoop(builder, vertices);
         fb_loops.push_back(fb_loop);
      }
      auto fb_loop_vector = builder.CreateVector(fb_loops);
      auto fb_poly = CreatePolygon(builder, fb_loop_vector);
      fb_regions.push_back(fb_poly);
   }
   auto fb_poly_vector = builder.CreateVector(fb_regions);
   auto fb_fips = builder.CreateString(country.fips);
   auto fb_iso3 = builder.CreateString(country.iso3);
   auto fb_name = builder.CreateString(country.name);

   fb::CountryBuilder cb(builder);
   cb.add_regions(fb_poly_vector);
   cb.add_name(fb_name);
   cb.add_iso3(fb_iso3);
   cb.add_fips(fb_fips);
   cb.add_is_water(country.is_water);
   auto fb_country = cb.Finish();
   builder.Finish(fb_country);

   builder.Finish(fb_country);
   uint32_t sz = builder.GetSize();
   fwrite(&sz, sizeof(sz), 1, out);
   fwrite(builder.GetBufferPointer(), sz, 1, out);
}

void generate_countries_from_db(FILE* out) {
   pqxx::connection conn("host='localhost' user='bsellers' dbname='cclookup'");
   pqxx::work txn(conn);

   auto db_countries = 
      txn.exec("SELECT id, fips, iso3, name, is_water, ST_AsText(border) "
               "FROM map where fips in ('US', 'FP', 'IR') order by id");
   for (const auto& db_country: db_countries) {
      Country c;
      c.id = db_country[0].as<int>(0);
      c.fips = db_country[1].as<std::string>("");
      c.iso3 = db_country[2].as<std::string>("");
      c.name = db_country[3].as<std::string>("");
      c.is_water = db_country[4].as<bool>(false);
      printf("%s: s2 ", c.name.c_str());
      fflush(stdout);
      c.regions = std::move(parse_wkt(db_country[5].as<std::string>("")));
      printf("done. write "); 
      fflush(stdout);
      write_country(out, c);
      printf("done.\n"); 
   }
}

int main() {
   auto f = fopen("data/world_map.dat", "wb");
   if (f == nullptr)  {
      perror("Error: ");
      exit(1);
   }

   generate_countries_from_db(f);
}
