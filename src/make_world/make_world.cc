#include "world_generated.h"
#include <zstd.h>
#include <stdio.h>
#include <string>
#include <cassert>
#include <pqxx/pqxx>

struct Country {
   int id;
   std::string fips;
   std::string iso3;
   std::string name;
   bool is_water;
};

void write_countries_and_regions(FILE* f,
                                 const std::map<int, Country>& countries,
                                 const std::map<int, std::vector<std::string>>& region_map) {

   flatbuffers::FlatBufferBuilder builder(1024*1024);
   for (auto& cpair: countries) {
      const auto& country = cpair.second;
      const auto& regions = region_map.find(cpair.first);
      std::vector<flatbuffers::Offset<fb::Polygon>> fb_regions;
      for (const auto& region: (*regions).second) {
         auto x = builder.CreateString(region.c_str());
         auto p = fb::CreatePolygon(builder, x);
         fb_regions.push_back(p);
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
      
      uint32_t sz = builder.GetSize();
      uint32_t c_size = ZSTD_compressBound(sz);

      auto compressed_buf = std::make_unique<char[]>(c_size);
      auto c_result = ZSTD_compress(compressed_buf.get(), c_size, builder.GetBufferPointer(), sz, 8);
      if (ZSTD_isError(c_result)) {
         fprintf(stderr, "Error during compression: %s\n", ZSTD_getErrorName(c_result));
         throw std::runtime_error("Compression Error");
      }
      c_size = c_result;
      fprintf(stdout, "%s: regions=%zd, compressed=%u\n", 
              country.name.c_str(), regions->second.size(), c_size);
      fwrite(&sz, sizeof(sz), 1, f);
      fwrite(&c_size, sizeof(c_size), 1, f);
      fwrite(compressed_buf.get(), c_size, 1, f);
      builder.Clear();
   }
}

std::map<int, Country> generate_countries_from_db(pqxx::work& txn) {
   std::map<int, Country> ret;
   auto db_countries = 
      txn.exec("SELECT id, fips, iso3, name, is_water FROM map");
   for (const auto& db_country: db_countries) {
      Country c;
      c.id = db_country[0].as<int>(0);
      c.fips = db_country[1].as<std::string>("");
      c.iso3 = db_country[2].as<std::string>("");
      c.name = db_country[3].as<std::string>("");
      c.is_water = db_country[4].as<bool>(false);
      ret.emplace(c.id, c);
   }
   return ret;
}

std::map<int, std::vector<std::string>> generate_regions_from_db(pqxx::work& txn) {
   std::map<int, std::vector<std::string>> ret;
   txn.exec("SET bytea_output='escape'");
   auto db_regions = 
      txn.exec("SELECT id, ST_AsText((ST_Dump(ST_Subdivide(border, 64))).geom) "
               "FROM map");
   for (const auto& row: db_regions) {
      auto id = row[0].as<int>(0);
      ret[id].emplace_back(row[1].as<std::string>(""));
   }
   return ret;
}

int main() {
   auto f = fopen("data/world_map.dat", "wb");
   if (f == nullptr)  {
      perror("Error: ");
      exit(1);
   }

   pqxx::connection conn("host='localhost' user='bsellers' dbname='cclookup'");
   pqxx::work txn(conn);
   auto countries = generate_countries_from_db(txn);
   auto regions = generate_regions_from_db(txn);
   write_countries_and_regions(f, countries, regions);
   fclose(f);
}
