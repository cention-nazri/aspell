
#include "config.hpp"
#include "file_util.hpp"
#include "file_data_util.hpp"

namespace aspell {
  
  void fill_data_dir(const Config * config, String & dir1, String & dir2) {
    if (config->have("local-data-dir")) {
      dir1 = config->retrieve("local-data-dir");
      if (dir1[dir1.size()-1] != '/') dir1 += '/';
    } else {
      dir1 = config->retrieve("master-path");
      dir1.resize(dir1.rfind('/') + 1);
    }
    dir2 = config->retrieve("data-dir");
    if (dir2[dir2.size()-1] != '/') dir2 += '/';
  }
  
  void find_file(String & file,
		 const String & dir1, const String & dir2, 
		 const String & name, const char * extension)
  {
    file = dir1 + name + extension;
    if (file_exists(file)) return;
    file = dir2 + name + extension;
  }
  
}
