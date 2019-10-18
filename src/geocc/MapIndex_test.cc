#include <MapIndex.h>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <sys/time.h>

template <class Container>
void split(const std::string& str, Container& cont,
                          char delim = ' ')
{
   std::size_t current, previous = 0;
   current = str.find(delim);
   while (current != std::string::npos) {
      cont.push_back(str.substr(previous, current - previous));
      previous = current + 1;
      current = str.find(delim, previous);
   }
   cont.push_back(str.substr(previous, current - previous));
}

bool iequals(const std::string& a, const std::string& b) {
   return std::equal(a.begin(), a.end(),
              b.begin(), b.end(),
              [](char a, char b) {
              return tolower(a) == tolower(b);
              });
}

typedef unsigned long long timestamp_t;
static timestamp_t get_timestamp () {
   struct timeval now;
   gettimeofday (&now, NULL);
   return  now.tv_usec + (timestamp_t)now.tv_sec * 1000000;
}

struct City {
   std::string name;
   std::string expected;
   double lat;
   double lon;
};
std::vector<City> read_cities(std::istream& cities) {
   std::vector<City> ret;
   std::unordered_map<std::string, std::string> fips_fixer = {
      {"az","aj"}, {"ad","an"}, {"zw","zi"}, {"ru","rs"}, {"zr","cg"},
      {"zm","za"}, {"ye","ym"}, {"au","as"}, {"at","au"}, {"cn","ch"}, 
      {"ba","bk"}, {"bd","bg"}, {"do","br"}, {"vn","vm"}, {"bf","uv"}, 
      {"ua","up"}, {"tr","tu"}, {"gb","uk"}, {"es","sp"}, {"ee","en"}, 
      {"bg","bu"}, {"bj","bn"}, {"bo","bl"}, {"by","bo"}, {"cd","cg"},
      {"bi","by"}, {"bw","bc"}, {"cf","ct"}, {"cg","cf"}, {"ch","sz"},
      {"ci","iv"}, {"cr","cs"}, {"cz","ez"}, {"de","gm"}, {"dk","da"},
      {"cl","ci"}, {"dz","ag"}, {"ga","gb"}, {"ge","gg"},
      {"gm","ga"}, {"gn","gv"}, {"gq","ek"}, {"gw","pu"}, {"hn","ho"},
      {"tn","ts"}, {"tj","ti"}, {"tg","to"}, {"td","cd"}, {"sv","es"},
      {"sn","sg"}, {"sk","lo"}, {"se","sw"}, {"sd","su"}, {"rs","rb"},
      {"za","sf"}, {"sz","wz"}, {"sb","bp"}, {"py","pa"}, {"pt","po"},
      {"ph","rp"}, {"pg","pp"}, {"pa","pm"}, {"om","mu"}, {"ni","nu"},
      {"ng","ni"}, {"ne","ng"}, {"mw","mi"}, {"mn","mg"}, {"mm","bm"},
      {"mg","ma"}, {"me","mj"}, {"ma","mo"}, {"lv","lg"}, {"lt","lh"},
      {"lr","li"}, {"lk","ce"}, {"lb","le"}, {"kr","ks"}, {"kp","kn"},
      {"bh","ba"}, {"gf","fg"}, {"ht","ha"},
      {"ie","ei"}, {"il","is"}, {"iq","iz"}, {"jp","ja"}, {"kh","cb"}
   };
   std::string line;
   while (getline(cities, line)) {
      std::vector<std::string> parts;
      parts.reserve(4);
      split(line,parts, ',');
      auto lat = std::strtod(parts[2].c_str(), nullptr);
      auto lon = std::strtod(parts[3].c_str(), nullptr);
      auto fips = parts[0];
      auto city = parts[1];

      if (fips_fixer.find(fips) != fips_fixer.end()) {
         fips = fips_fixer[fips];
      }
      ret.push_back({city, fips, lat, lon});
   }
   return ret;
}

void test_cities(const geocc::MapIndex& idx, const std::vector<City>& cities) {
   int passed = 0;
   int failed = 0;
   for (auto& city: cities) {
      auto ccs = idx.countryContaining(city.lat, city.lon);
      bool found = false;
      for ( const auto cc : ccs) {
         if (iequals(cc->fips, city.expected)) {
            found = true;
            break;
         }
      }
      if (found) {
         passed++;
      } else {
         failed++;
      }

   }
   printf("passed: %d, failed: %d\n", passed, failed);
}

int main() {
   geocc::MapIndex idx;
   FILE* map_file = fopen("data/world_map.dat", "r");
   idx.readWorldMap(map_file);
   fclose(map_file);

   std::ifstream city_file("data/thinned_cities.csv");
   auto cities = read_cities(city_file);

   timestamp_t start = get_timestamp();
   test_cities(idx, cities);
   printf("Took %.3fs\n", ((double)get_timestamp() - start)/1000000);
   return 0;
}
