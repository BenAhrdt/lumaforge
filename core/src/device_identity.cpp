#include "lumaforge/device_identity.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace lumaforge {
namespace {

constexpr std::array<uint32_t, 64> kRoundConstants = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

uint32_t rotateRight(uint32_t value, uint32_t count) {
  return (value >> count) | (value << (32 - count));
}

std::array<uint8_t, 32> sha256(const std::string& input) {
  std::vector<uint8_t> message(input.begin(), input.end());
  const uint64_t bitLength = static_cast<uint64_t>(message.size()) * 8;
  message.push_back(0x80);
  while (message.size() % 64 != 56) message.push_back(0);
  for (int shift = 56; shift >= 0; shift -= 8) message.push_back(static_cast<uint8_t>(bitLength >> shift));

  std::array<uint32_t, 8> hash = {
    0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
  };
  for (size_t offset = 0; offset < message.size(); offset += 64) {
    std::array<uint32_t, 64> words{};
    for (size_t index = 0; index < 16; ++index) {
      const size_t position = offset + index * 4;
      words[index] = (static_cast<uint32_t>(message[position]) << 24) |
        (static_cast<uint32_t>(message[position + 1]) << 16) |
        (static_cast<uint32_t>(message[position + 2]) << 8) | message[position + 3];
    }
    for (size_t index = 16; index < words.size(); ++index) {
      const uint32_t s0 = rotateRight(words[index - 15], 7) ^ rotateRight(words[index - 15], 18) ^ (words[index - 15] >> 3);
      const uint32_t s1 = rotateRight(words[index - 2], 17) ^ rotateRight(words[index - 2], 19) ^ (words[index - 2] >> 10);
      words[index] = words[index - 16] + s0 + words[index - 7] + s1;
    }
    uint32_t a=hash[0],b=hash[1],c=hash[2],d=hash[3],e=hash[4],f=hash[5],g=hash[6],h=hash[7];
    for (size_t index = 0; index < words.size(); ++index) {
      const uint32_t sum1=rotateRight(e,6)^rotateRight(e,11)^rotateRight(e,25);
      const uint32_t choice=(e&f)^((~e)&g);
      const uint32_t temporary1=h+sum1+choice+kRoundConstants[index]+words[index];
      const uint32_t sum0=rotateRight(a,2)^rotateRight(a,13)^rotateRight(a,22);
      const uint32_t majority=(a&b)^(a&c)^(b&c);
      const uint32_t temporary2=sum0+majority;
      h=g;g=f;f=e;e=d+temporary1;d=c;c=b;b=a;a=temporary1+temporary2;
    }
    hash[0]+=a;hash[1]+=b;hash[2]+=c;hash[3]+=d;hash[4]+=e;hash[5]+=f;hash[6]+=g;hash[7]+=h;
  }
  std::array<uint8_t, 32> digest{};
  for (size_t index = 0; index < hash.size(); ++index) {
    digest[index * 4] = static_cast<uint8_t>(hash[index] >> 24);
    digest[index * 4 + 1] = static_cast<uint8_t>(hash[index] >> 16);
    digest[index * 4 + 2] = static_cast<uint8_t>(hash[index] >> 8);
    digest[index * 4 + 3] = static_cast<uint8_t>(hash[index]);
  }
  return digest;
}

}  // namespace

std::string deviceIdFromHardwareMac(uint64_t hardwareMac) {
  char mac[13];
  std::snprintf(mac, sizeof(mac), "%012llx", static_cast<unsigned long long>(hardwareMac & 0xffffffffffffULL));
  const auto digest = sha256(std::string("LumaForge:") + mac);
  char identifier[16] = "lf-";
  for (size_t index = 0; index < 6; ++index) std::snprintf(identifier + 3 + index * 2, 3, "%02x", digest[index]);
  return identifier;
}

}  // namespace lumaforge
