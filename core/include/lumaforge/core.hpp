#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lumaforge {

struct Color {
  uint8_t r{0}, g{0}, b{0}, w{0};
  static Color hex(const std::string& value);
  Color scaled(float factor) const;
  bool operator==(const Color& other) const;
};

struct Point { float x{0}, y{0}; };
struct LayoutLed { std::size_t logical{0}, physical{0}; Point position; };
struct Strip {
  std::string id, name, output{"main"};
  Point start, end;
  std::size_t ledCount{1}, startIndex{0};
  bool reversed{false};
  float spacing{1};
};

class Layout {
 public:
  void setStrips(std::vector<Strip> strips);
  const std::vector<Strip>& strips() const { return strips_; }
  const std::vector<LayoutLed>& leds() const { return leds_; }
  std::optional<std::size_t> physicalFor(std::size_t logical) const;
 private:
  std::vector<Strip> strips_;
  std::vector<LayoutLed> leds_;
};

struct Zone { std::string id, name; std::vector<std::size_t> leds; };
enum class Effect { Solid, Blink, Pulse, Wipe, Chase, Rainbow, Scanner };
enum class ColorMode { Solid, Rainbow };
enum class Direction {
  Forward,
  Reverse,
  PingPong,
  PingPongReverse,
  CenterOut,
  OutsideIn,
  CenterOutAndBack,
  OutsideInAndBack
};
struct Animation {
  std::string id;
  Effect effect{Effect::Solid};
  std::vector<std::size_t> target;
  Color color{0,174,239,0}, secondary{0,0,0,0};
  double start{0}, duration{5}, speed{1};
  float brightness{1};
  Direction direction{Direction::Forward};
  int priority{0};
  float width{.15f};
  ColorMode colorMode{ColorMode::Solid};
};
struct Scene { std::string id, name; std::vector<Animation> animations; };

struct Layer {
  int priority{0};
  std::vector<Color> pixels;
  std::vector<uint8_t> active;
};

class LedOutput {
 public:
  virtual ~LedOutput() = default;
  virtual void setPixel(std::size_t index, Color color) = 0;
  virtual void show() = 0;
  virtual void clear() = 0;
  virtual std::size_t pixelCount() const = 0;
};

class SimulatorLedOutput final : public LedOutput {
 public:
  explicit SimulatorLedOutput(std::size_t count);
  void resize(std::size_t count);
  void setPixel(std::size_t index, Color color) override;
  void show() override {}
  void clear() override;
  std::size_t pixelCount() const override { return pixels_.size(); }
  const std::vector<Color>& pixels() const { return pixels_; }
 private: std::vector<Color> pixels_;
};

class Renderer {
 public:
  explicit Renderer(std::size_t count = 0);
  void resize(std::size_t count);
  const std::vector<Color>& render(const Scene& scene, double seconds);
  const std::vector<Color>& compose(const std::vector<Layer>& layers);
 private:
  Layer evaluate(const Animation& animation, double seconds) const;
  std::size_t count_{0};
  std::vector<Color> frame_;
};

Effect parseEffect(const std::string& value);
std::string colorJson(const std::vector<Color>& colors);

} // namespace lumaforge
