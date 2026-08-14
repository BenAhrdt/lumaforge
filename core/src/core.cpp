#include "lumaforge/core.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lumaforge {
namespace {
uint8_t component(const std::string& s, std::size_t p) {
  return static_cast<uint8_t>(std::stoul(s.substr(p, 2), nullptr, 16));
}
Color hsv(double h, double s, double v) {
  h -= std::floor(h); const double i = std::floor(h * 6), f = h * 6 - i;
  const double p=v*(1-s), q=v*(1-f*s), t=v*(1-(1-f)*s);
  double r=0,g=0,b=0;
  switch (static_cast<int>(i)%6) { case 0:r=v;g=t;b=p;break; case 1:r=q;g=v;b=p;break;
    case 2:r=p;g=v;b=t;break; case 3:r=p;g=q;b=v;break; case 4:r=t;g=p;b=v;break;
    default:r=v;g=p;b=q; }
  return {static_cast<uint8_t>(r*255),static_cast<uint8_t>(g*255),static_cast<uint8_t>(b*255),0};
}
}
Color Color::hex(const std::string& value) {
  if (value.size()!=7 || value[0]!='#') throw std::invalid_argument("invalid color");
  try { return {component(value,1),component(value,3),component(value,5),0}; }
  catch (...) { throw std::invalid_argument("invalid color"); }
}
Color Color::scaled(float f) const { f=std::clamp(f,0.0f,1.0f); return {
  static_cast<uint8_t>(r*f),static_cast<uint8_t>(g*f),static_cast<uint8_t>(b*f),static_cast<uint8_t>(w*f)}; }
bool Color::operator==(const Color& o) const { return r==o.r&&g==o.g&&b==o.b&&w==o.w; }

void Layout::setStrips(std::vector<Strip> strips) {
  strips_=std::move(strips); leds_.clear(); std::size_t logical=0;
  for (const auto& strip: strips_) {
    if (!strip.ledCount) throw std::invalid_argument("strip needs LEDs");
    for (std::size_t i=0;i<strip.ledCount;++i) {
      const float t=strip.ledCount==1 ? .5f : static_cast<float>(i)/(strip.ledCount-1);
      const auto physical=strip.startIndex+(strip.reversed ? strip.ledCount-1-i : i);
      leds_.push_back({logical++,physical,{strip.start.x+(strip.end.x-strip.start.x)*t,
                                          strip.start.y+(strip.end.y-strip.start.y)*t}});
    }
  }
}
std::optional<std::size_t> Layout::physicalFor(std::size_t logical) const {
  if (logical>=leds_.size()) return std::nullopt;
  return leds_[logical].physical;
}
SimulatorLedOutput::SimulatorLedOutput(std::size_t count):pixels_(count){}
void SimulatorLedOutput::resize(std::size_t count){pixels_.assign(count,{});}
void SimulatorLedOutput::setPixel(std::size_t i,Color c){if(i<pixels_.size())pixels_[i]=c;}
void SimulatorLedOutput::clear(){std::fill(pixels_.begin(),pixels_.end(),Color{});}
Renderer::Renderer(std::size_t count){resize(count);} void Renderer::resize(std::size_t count){count_=count;frame_.assign(count,{});}
Layer Renderer::evaluate(const Animation& a,double now) const {
  Layer l; l.priority=a.priority;l.pixels.assign(count_,{});l.active.assign(count_,0);
  const double local=now-a.start;if(local<0 || (a.duration>0&&local>a.duration))return l;
  const bool pingPong=a.direction==Direction::PingPong||a.direction==Direction::PingPongReverse||
    a.direction==Direction::CenterOutAndBack||a.direction==Direction::OutsideInAndBack;
  const double directionScale=pingPong?.5:1.0;
  const double cycle=std::fmod(std::max(0.0,local)*std::max(.01,a.speed)*directionScale,1.0);
  const double triangle=1.0-std::abs(2.0*cycle-1.0);
  const double phase=a.direction==Direction::PingPong?triangle:a.direction==Direction::PingPongReverse?1.0-triangle:cycle;
  for(std::size_t pos=0;pos<a.target.size();++pos){const auto idx=a.target[pos];if(idx>=count_)continue;
    const double p=a.target.size()<2?0.0:static_cast<double>(pos)/(a.target.size()-1);
    const double ordered=a.direction==Direction::Reverse?1-p:p;
    Color c=a.colorMode==ColorMode::Rainbow?hsv(p,1,1):a.color; bool on=true;
    switch(a.effect){case Effect::Solid:break;case Effect::Blink:on=phase<.5;break;
      case Effect::Pulse:c=c.scaled(static_cast<float>(.15+.85*(.5+.5*std::sin(local*a.speed*6.283185))));break;
      case Effect::Wipe:on=ordered<=phase;break;case Effect::Chase:on=std::fmod(ordered*10-phase*4+10,1.0)<.35;break;
      case Effect::Scanner:{double distance=std::abs(ordered-phase);if(a.direction==Direction::CenterOut||a.direction==Direction::CenterOutAndBack){const double progress=a.direction==Direction::CenterOutAndBack?triangle:cycle;const double left=.5-.5*progress,right=.5+.5*progress;distance=std::min(std::abs(p-left),std::abs(p-right));}else if(a.direction==Direction::OutsideIn||a.direction==Direction::OutsideInAndBack){const double progress=a.direction==Direction::OutsideInAndBack?triangle:cycle;const double left=.5*progress,right=1.0-.5*progress;distance=std::min(std::abs(p-left),std::abs(p-right));}const double width=std::clamp<double>(a.width,.01,1.0);on=distance<width;if(on)c=c.scaled(static_cast<float>(1.0-distance/width*.82));break;}
      case Effect::Rainbow:c=hsv(ordered+phase,1,1);break;}
    l.active[idx]=1;l.pixels[idx]=on?c.scaled(a.brightness):Color{};
  }return l;
}
const std::vector<Color>& Renderer::render(const Scene& scene,double now){std::vector<Layer> layers;layers.reserve(scene.animations.size());for(const auto&a:scene.animations)layers.push_back(evaluate(a,now));return compose(layers);}
const std::vector<Color>& Renderer::compose(const std::vector<Layer>& layers){std::fill(frame_.begin(),frame_.end(),Color{});std::vector<int>priority(count_,-2147483647);for(const auto&l:layers)for(std::size_t i=0;i<count_&&i<l.active.size()&&i<l.pixels.size();++i)if(l.active[i]&&l.priority>=priority[i]){priority[i]=l.priority;frame_[i]=l.pixels[i];}return frame_;}
Effect parseEffect(const std::string& s){if(s=="blink")return Effect::Blink;if(s=="pulse")return Effect::Pulse;if(s=="wipe")return Effect::Wipe;if(s=="chase")return Effect::Chase;if(s=="rainbow")return Effect::Rainbow;if(s=="scanner")return Effect::Scanner;return Effect::Solid;}
std::string colorJson(const std::vector<Color>& cs){std::string out="[";for(std::size_t i=0;i<cs.size();++i){if(i)out+=",";out+="["+std::to_string(cs[i].r)+","+std::to_string(cs[i].g)+","+std::to_string(cs[i].b)+","+std::to_string(cs[i].w)+"]";}return out+"]";}
} // namespace lumaforge
