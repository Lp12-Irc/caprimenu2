#pragma once
#include "entities.hpp"
#include <fstream>
#include <filesystem>
#include <cstring>
#include <sstream>
#include <compressapi.h>
// Prebuilt BVH of static map physics. Unknown maps fail closed.
class MapCollision {
public:
 struct Triangle {uint32_t a,b,c;};
 struct Node {Vec3 low,high;uint32_t left=0,right=0,start=0,count=0;};
 static std::shared_ptr<const MapCollision> load(const std::filesystem::path& path){
  std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)return {};
  const auto packed_size=input.tellg();if(packed_size<20 || packed_size>160*1024*1024)return {};input.seekg(0);
  std::string bytes(size_t(packed_size),'\0');input.read(bytes.data(),bytes.size());if(!input)return {};
  if(bytes.compare(0,8,"CAPRIZ01")==0){uint32_t expanded=0;std::memcpy(&expanded,bytes.data()+8,4);
   if(expanded<20 || expanded>160*1024*1024)return {};
   DECOMPRESSOR_HANDLE decoder=nullptr;if(!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&decoder))return {};
   std::string raw(expanded,'\0');SIZE_T actual=0;
   const bool ok=Decompress(decoder,bytes.data()+12,bytes.size()-12,raw.data(),raw.size(),&actual)!=FALSE;
   CloseDecompressor(decoder);if(!ok || actual!=expanded)return {};bytes=std::move(raw);
  }
  const auto size=std::streamoff(bytes.size());std::istringstream file(std::move(bytes),std::ios::binary);
  char magic[8]{};uint32_t h[4]{};file.read(magic,8);file.read(reinterpret_cast<char*>(h),sizeof(h));
  if(std::memcmp(magic,"CAPRIBVH",8) || h[0]!=2 || !h[1] || !h[2] || !h[3] || h[1]>5000000 || h[2]>2000000 || h[3]>5000000 ||
     size!=std::streamoff(24+uint64_t(h[3])*sizeof(Vec3)+uint64_t(h[1])*sizeof(Triangle)+uint64_t(h[2])*sizeof(Node)))return {};
  auto result=std::make_shared<MapCollision>();result->vertices_.resize(h[3]);result->triangles_.resize(h[1]);result->nodes_.resize(h[2]);
  file.read(reinterpret_cast<char*>(result->vertices_.data()),result->vertices_.size()*sizeof(Vec3));
  file.read(reinterpret_cast<char*>(result->triangles_.data()),result->triangles_.size()*sizeof(Triangle));
  file.read(reinterpret_cast<char*>(result->nodes_.data()),result->nodes_.size()*sizeof(Node));if(!file)return {};
  for(const auto& v:result->vertices_)if(!v.finite())return {};
  for(const auto& t:result->triangles_)if(t.a>=h[3] || t.b>=h[3] || t.c>=h[3])return {};
  for(size_t i=0;i<result->nodes_.size();++i){const auto& n=result->nodes_[i];
   if(!n.low.finite() || !n.high.finite() || n.low.x>n.high.x || n.low.y>n.high.y || n.low.z>n.high.z)return {};
   if(n.count){if(uint64_t(n.start)+n.count>result->triangles_.size())return {};}
   else if(n.left<=i || n.right<=i || n.left>=h[2] || n.right>=h[2])return {};
  }return result;
 }
 bool clear(const Vec3& from,const Vec3& to) const {
  if(nodes_.empty() || !from.finite() || !to.finite())return false;
  const Vec3 direction=to-from;if(direction.len()<.01f)return false;
  std::array<uint32_t,64> stack{};size_t count=1;
  while(count){const auto& node=nodes_[stack[--count]];if(!box(from,direction,node))continue;
   if(node.count){for(uint32_t i=0;i<node.count;++i)if(hit(from,direction,triangles_[node.start+i]))return false;}
   else {if(count+2>stack.size())return false;stack[count++]=node.left;stack[count++]=node.right;}
  }return true;
 }
private:
 static bool box(Vec3 origin,Vec3 d,const Node& n){
  const float o[]={origin.x,origin.y,origin.z},v[]={d.x,d.y,d.z},lo[]={n.low.x,n.low.y,n.low.z},hi[]={n.high.x,n.high.y,n.high.z};
  float enter=.0001f,leave=.9999f;
  for(int i=0;i<3;++i){if(std::fabs(v[i])<1e-8f){if(o[i]<lo[i] || o[i]>hi[i])return false;continue;}
   float a=(lo[i]-o[i])/v[i],b=(hi[i]-o[i])/v[i];if(a>b)std::swap(a,b);
   enter=(std::max)(enter,a);leave=(std::min)(leave,b);if(enter>leave)return false;
  }return true;
 }
 static Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
 static float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
 bool hit(Vec3 from,Vec3 d,const Triangle& t) const {
  const auto a=vertices_[t.a];const Vec3 e1=vertices_[t.b]-a,e2=vertices_[t.c]-a,p=cross(d,e2);const float determinant=dot(e1,p);
  if(std::fabs(determinant)<1e-7f)return false;
  const float inverse=1.f/determinant;const Vec3 s=from-a;const float u=dot(s,p)*inverse;if(u<0 || u>1)return false;
  const Vec3 q=cross(s,e1);const float v=dot(d,q)*inverse;if(v<0 || u+v>1)return false;
  const float distance=dot(e2,q)*inverse;return distance>.0001f && distance<.9999f;
 }
 std::vector<Vec3> vertices_;std::vector<Triangle> triangles_;std::vector<Node> nodes_;
};
static_assert(sizeof(MapCollision::Triangle)==12 && sizeof(MapCollision::Node)==40);
