#include "lumaforge/core.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace lumaforge;
void expect(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
  Layout layout;layout.setStrips({{"a","A","main",{0,0},{100,0},3,10,false,1},{"b","B","main",{100,0},{100,100},2,20,true,1}});
  expect(layout.leds().size()==5,"mapping count");expect(layout.physicalFor(0)==10,"mapping forward");expect(layout.physicalFor(3)==21,"mapping reverse");
  Zone zone{"door","Door",{1,3}};expect(zone.leds.size()==2,"zone selection");
  Renderer renderer(4);Layer low{1,std::vector<Color>(4,Color{255,0,0,0}),std::vector<uint8_t>(4,1)};Layer high{10,std::vector<Color>(4,Color{0,0,255,0}),{0,1,0,0}};
  auto frame=renderer.compose({low,high});expect(frame[0]==Color{255,0,0,0},"low layer");expect(frame[1]==Color{0,0,255,0},"priority composition");
  Animation wipe{"wipe",Effect::Wipe,{0,1,2,3},Color{0,255,0,0},{},0,10,1,1,Direction::Forward,0};Scene scene{"s","Scene",{wipe}};
  frame=renderer.render(scene,.25);expect(frame[0]==Color{0,255,0,0},"scene schedule start");expect(frame[3]==Color{},"wipe progression");
  frame=renderer.render(scene,11);expect(frame[0]==Color{},"scene schedule end");
  Animation scanner;scanner.id="scanner";scanner.effect=Effect::Scanner;scanner.target={0,1,2,3,4};scanner.color={255,0,0,0};scanner.duration=10;scanner.speed=1;scanner.direction=Direction::CenterOutAndBack;scanner.width=.1f;
  Scene scannerScene{"scanner-scene","Scanner",{scanner}};
  Renderer scannerRenderer(5);
  frame=scannerRenderer.render(scannerScene,0);expect(frame[2].r>0&&frame[0].r==0,"scanner starts in center");
  frame=scannerRenderer.render(scannerScene,1);expect(frame[0].r>0&&frame[4].r>0&&frame[2].r==0,"scanner reaches both outer edges");
  scanner.direction=Direction::Forward;scanner.colorMode=ColorMode::Rainbow;scanner.width=1;
  scannerScene.animations[0]=scanner;
  frame=scannerRenderer.render(scannerScene,0);expect(frame[0].r>0&&frame[2].g>0&&!(frame[0]==frame[2]),"scanner supports a spatial rainbow color mode");
  expect(Color::hex("#00AEEF")==Color{0,174,239,0},"serialization color");
  std::cout<<"10 core tests passed\n";return EXIT_SUCCESS;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return EXIT_FAILURE;}}
