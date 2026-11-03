#include "map_collision.hpp"
#include <cstdio>
#include <stdexcept>
#include <chrono>
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
 auto path=std::filesystem::temp_directory_path()/("capri-collision-"+std::to_string(GetCurrentProcessId())+".bvh");
 try{
  Vec3 vertices[]={{5,-10,0},{5,10,0},{5,10,10},{5,-10,10}};
  MapCollision::Triangle triangles[]={{0,1,2},{0,2,3}};
  MapCollision::Node node{{5,-10,0},{5,10,10},0,0,0,2};
  {std::ofstream file(path,std::ios::binary);uint32_t header[]={2,2,1,4};
   file.write("CAPRIBVH",8);file.write(reinterpret_cast<char*>(header),sizeof(header));file.write(reinterpret_cast<char*>(vertices),sizeof(vertices));
   file.write(reinterpret_cast<char*>(triangles),sizeof(triangles));file.write(reinterpret_cast<char*>(&node),sizeof(node));}
  const auto map=MapCollision::load(path);check(bool(map),"fixture load");
  check(!map->clear({0,0,5},{10,0,5}) && !map->clear({10,0,5},{0,0,5}),"wall blocks both directions");
  check(map->clear({0,0,5},{4,0,5}),"wall beyond target does not block");
  check(map->clear({0,0,15},{10,0,15}) && map->clear({0,15,5},{10,15,5}),"rays outside wall stay clear");
  check(!map->clear({0,0,5},{NAN,0,5}),"invalid ray fails closed");
  {std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<"broken";}
  check(!MapCollision::load(path),"corrupt map fails closed");std::filesystem::remove(path);
  int count=0;const auto start=std::chrono::steady_clock::now();
  if(argc>1)for(const auto& entry:std::filesystem::directory_iterator(argv[1]))if(entry.path().extension()==".bvh"){
   auto asset=MapCollision::load(entry.path());check(bool(asset),"compressed collision asset rejected");
   for(int i=0;i<1000;++i)asset->clear({-2000,float(i),64},{2000,float(i),64});
   ++count;std::printf("PASS asset: %s\n",entry.path().filename().string().c_str());
  }
  std::printf("PASS: wall, segment limits, missing geometry, compressed assets=%d; load+queries_ms=%.1f\n",count,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());return 0;
 }catch(const std::exception& e){std::filesystem::remove(path);std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
