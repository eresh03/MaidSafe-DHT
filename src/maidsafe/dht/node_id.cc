/* Copyright (c) 2010 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "maidsafe/dht/node_id.h"
#include <bitset>
#include "maidsafe/dht/log.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace dht {

size_t BitToByteCount(const size_t &bit_count) {
  return static_cast<size_t>(0.999999 + static_cast<double>(bit_count) / 8);
}

NodeId::NodeId() : raw_id_(kZeroId) {}

NodeId::NodeId(const NodeId &other) : raw_id_(other.raw_id_) {}

NodeId::NodeId(const KadIdType &type) : raw_id_(kKeySizeBytes, -1) {
  switch (type) {
    case kMaxId :
      break;  // already set
    case kRandomId :
      std::generate(raw_id_.begin(), raw_id_.end(),
                    std::bind(&RandomUint32));
      break;
    default :
      break;
  }
}

NodeId::NodeId(const std::string &id) : raw_id_(id) {
  if (!IsValid())
    raw_id_.clear();
}

NodeId::NodeId(const std::string &id, const EncodingType &encoding_type)
    : raw_id_() {
  try {
    switch (encoding_type) {
      case kBinary : DecodeFromBinary(id);
        break;
      case kHex : raw_id_ = DecodeFromHex(id);
        break;
      case kBase32 : raw_id_ = DecodeFromBase32(id);
        break;
      case kBase64 : raw_id_ = DecodeFromBase64(id);
        break;
      default : raw_id_ = id;
    }
  }
  catch(const std::exception &e) {
    DLOG(ERROR) << "NodeId Ctor: " << e.what();
    raw_id_.clear();
    return;
  }
  if (!IsValid())
    raw_id_.clear();
}

NodeId::NodeId(const uint16_t &power) : raw_id_(kZeroId) {
  if (power >= kKeySizeBits) {
    raw_id_.clear();
    return;
  }
  uint16_t shift = power % 8;
  if (shift != 0) {
    raw_id_[kKeySizeBytes - BitToByteCount(power)] += 1 << shift;
  } else {
    raw_id_[kKeySizeBytes - BitToByteCount(power) - 1] = 1;
  }
}

NodeId::NodeId(const NodeId &id1, const NodeId &id2) : raw_id_(kZeroId) {
  if (!id1.IsValid() || !id2.IsValid()) {
    raw_id_.clear();
    return;
  }
  if (id1 == id2) {
    raw_id_ = id1.raw_id_;
    return;
  }
  std::string min_id(id1.raw_id_), max_id(id2.raw_id_);
  if (id1 > id2) {
    max_id = id1.raw_id_;
    min_id = id2.raw_id_;
  }
  bool less_than_upper_limit(false);
  bool greater_than_lower_limit(false);
  unsigned char max_id_char(0), min_id_char(0), this_char(0);
  for (size_t pos = 0; pos < kKeySizeBytes; ++pos) {
    if (!less_than_upper_limit) {
      max_id_char = max_id[pos];
      min_id_char = greater_than_lower_limit ? 0 : min_id[pos];
      if (max_id_char == 0) {
        raw_id_[pos] = 0;
      } else {
        raw_id_[pos] = (RandomUint32() % (max_id_char - min_id_char + 1))
                       + min_id_char;
        this_char = raw_id_[pos];
        less_than_upper_limit = (this_char < max_id_char);
        greater_than_lower_limit = (this_char > min_id_char);
      }
    } else if (!greater_than_lower_limit) {
      min_id_char = min_id[pos];
      raw_id_[pos] = static_cast<char>(RandomUint32() % (256 - min_id_char))
                     + min_id_char;
      this_char = raw_id_[pos];
      greater_than_lower_limit = (this_char > min_id_char);
    } else {
      raw_id_[pos] = static_cast<char>(RandomUint32());
    }
  }
}

std::string NodeId::EncodeToBinary() const {
  std::string binary;
  binary.reserve(kKeySizeBytes);
  for (size_t i = 0; i < kKeySizeBytes; ++i) {
    std::bitset<8> temp(static_cast<int>(raw_id_[i]));
    binary += temp.to_string();
  }
  return binary;
}

void NodeId::DecodeFromBinary(const std::string &binary_id) {
  std::bitset<kKeySizeBits> binary_bitset(binary_id);
  if (!IsValid()) {
    raw_id_.assign(kKeySizeBytes, 0);
  }
  for (size_t i = 0; i < kKeySizeBytes; ++i) {
    std::bitset<8> temp(binary_id.substr(i * 8, 8));
    raw_id_[i] = static_cast<char>(temp.to_ulong());
  }
}

bool NodeId::CloserToTarget(const NodeId &id1,
                            const NodeId &id2,
                            const NodeId &target_id) {
  if (!id1.IsValid() || !id2.IsValid() || !target_id.IsValid())
    return false;
  std::string raw_id1(id1.raw_id_);
  std::string raw_id2(id2.raw_id_);
  std::string raw_id_target(target_id.raw_id_);
  for (uint16_t i = 0; i < kKeySizeBytes; ++i) {
    unsigned char result1 = raw_id1[i] ^ raw_id_target[i];
    unsigned char result2 = raw_id2[i] ^ raw_id_target[i];
    if (result1 != result2)
      return result1 < result2;
  }
  return false;
}

const std::string NodeId::String() const {
  return raw_id_;
}

const std::string NodeId::ToStringEncoded(
    const EncodingType &encoding_type) const {
  if (!IsValid())
    return "";
  switch (encoding_type) {
    case kBinary:
      return EncodeToBinary();
    case kHex:
      return EncodeToHex(raw_id_);
    case kBase32:
      return EncodeToBase32(raw_id_);
    case kBase64:
      return EncodeToBase64(raw_id_);
    default:
      return raw_id_;
  }
}

bool NodeId::IsValid() const {
  return raw_id_.size() == kKeySizeBytes;
}

bool NodeId::operator == (const NodeId &rhs) const {
  return raw_id_ == rhs.raw_id_;
}

bool NodeId::operator != (const NodeId &rhs) const {
  return raw_id_ != rhs.raw_id_;
}

bool NodeId::operator < (const NodeId &rhs) const {
  return raw_id_ < rhs.raw_id_;
}

bool NodeId::operator > (const NodeId &rhs) const {
  return raw_id_ > rhs.raw_id_;
}

bool NodeId::operator <= (const NodeId &rhs) const {
  return raw_id_ <= rhs.raw_id_;
}

bool NodeId::operator >= (const NodeId &rhs) const {
  return raw_id_ >= rhs.raw_id_;
}

NodeId& NodeId::operator = (const NodeId &rhs) {
  this->raw_id_ = rhs.raw_id_;
  return *this;
}

const NodeId NodeId::operator ^ (const NodeId &rhs) const {
  NodeId result;
  std::string::const_iterator this_it = raw_id_.begin();
  std::string::const_iterator rhs_it = rhs.raw_id_.begin();
  std::string::iterator result_it = result.raw_id_.begin();
  for (; this_it != raw_id_.end(); ++this_it, ++rhs_it, ++result_it)
    *result_it = *this_it ^ *rhs_it;
  return result;
}

std::string DebugId(const NodeId &node_id) {
  std::string hex(node_id.ToStringEncoded(NodeId::kHex));
  return hex.substr(0, 7) + ".." +hex.substr(hex.size() - 7);
}

}  // namespace dht

}  // namespace maidsafe
