#include "lumaforge/core.hpp"
#include <chrono>
#include <iostream>
#include <sstream>

using namespace lumaforge;
namespace {
std::vector<std::string> split(const std::string& value,char delimiter){std::vector<std::string> out;std::stringstream ss(value);std::string part;while(std::getline(ss,part,delimiter))out.push_back(part);return out;}
std::vector<std::size_t> indices(const std::string& value){std::vector<std::size_t> out;for(const auto&p:split(value,','))if(!p.empty())out.push_back(std::stoul(p));return out;}
}
int main(){
  std::ios::sync_with_stdio(false);Renderer renderer;Scene scene{"runtime","Runtime",{}};std::string line;
  while(std::getline(std::cin,line))try{
    auto fields=split(line,'|');if(fields.empty())continue;
    if(fields[0]=="COUNT"&&fields.size()==2){renderer.resize(std::stoul(fields[1]));std::cout<<"OK\n";}
    else if(fields[0]=="CLEAR"){scene.animations.clear();std::cout<<"OK\n";}
    else if(fields[0]=="ANIM"&&fields.size()>=11){Animation a;a.id=fields[1];a.effect=parseEffect(fields[2]);a.target=indices(fields[3]);a.color=Color::hex(fields[4]);a.brightness=std::stof(fields[5]);a.speed=std::stod(fields[6]);a.start=std::stod(fields[7]);a.duration=std::stod(fields[8]);a.priority=std::stoi(fields[9]);a.direction=fields[10]=="reverse"?Direction::Reverse:fields[10]=="pingpong"?Direction::PingPong:fields[10]=="pingpong-reverse"?Direction::PingPongReverse:fields[10]=="center-out"?Direction::CenterOut:fields[10]=="outside-in"?Direction::OutsideIn:fields[10]=="center-out-and-back"?Direction::CenterOutAndBack:fields[10]=="outside-in-and-back"?Direction::OutsideInAndBack:Direction::Forward;if(fields.size()>=12)a.width=std::stof(fields[11]);if(fields.size()>=13&&fields[12]=="rainbow")a.colorMode=ColorMode::Rainbow;scene.animations.push_back(a);std::cout<<"OK\n";}
    else if(fields[0]=="TICK"&&fields.size()==2)std::cout<<"FRAME|"<<colorJson(renderer.render(scene,std::stod(fields[1])))<<"\n";
    else std::cout<<"ERROR|unknown command\n";
  }catch(const std::exception&e){std::cout<<"ERROR|"<<e.what()<<"\n";}
}
