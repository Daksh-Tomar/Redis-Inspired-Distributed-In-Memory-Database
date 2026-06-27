#include "storage/rdb_persistence.h"

#include <chrono>
#include <fstream>
#include <iostream>

namespace redisdb {

static uint64_t crc64_tab[256];
static bool crc64_initialized = false;

static void init_crc64() {
  if (crc64_initialized) return;
  const uint64_t poly = 0xad93d23594bc8429ULL;
  for (int i = 0; i < 256; i++) {
    uint64_t crc = i;
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ poly;
      } else {
        crc >>= 1;
      }
    }
    crc64_tab[i] = crc;
  }
  crc64_initialized = true;
}

static uint64_t crc64_update(uint64_t crc, const unsigned char *s,
                             uint64_t l) {
  init_crc64();
  while (l--) {
    crc = crc64_tab[(crc ^ *s++) & 0xff] ^ (crc >> 8);
  }
  return crc;
}

static void writeByte(std::ofstream &out, uint8_t val, uint64_t &checksum) {
  out.write(reinterpret_cast<const char *>(&val), 1);
  checksum = crc64_update(checksum, &val, 1);
}

static uint8_t readByte(std::ifstream &in, uint64_t &checksum) {
  uint8_t val;
  in.read(reinterpret_cast<char *>(&val), 1);
  checksum = crc64_update(checksum, &val, 1);
  return val;
}

static void writeLength(std::ofstream &out, uint64_t len,
                        uint64_t &checksum) {
  out.write(reinterpret_cast<const char *>(&len), sizeof(len));
  checksum = crc64_update(
      checksum, reinterpret_cast<const unsigned char *>(&len), sizeof(len));
}

static uint64_t readLength(std::ifstream &in, uint64_t &checksum) {
  uint64_t len;
  in.read(reinterpret_cast<char *>(&len), sizeof(len));
  checksum = crc64_update(
      checksum, reinterpret_cast<const unsigned char *>(&len), sizeof(len));
  return len;
}

static void writeString(std::ofstream &out, const std::string &str,
                        uint64_t &checksum) {
  writeLength(out, str.size(), checksum);
  out.write(str.data(), str.size());
  checksum = crc64_update(
      checksum, reinterpret_cast<const unsigned char *>(str.data()),
      str.size());
}

static std::string readString(std::ifstream &in, uint64_t &checksum) {
  uint64_t len = readLength(in, checksum);
  std::string str(len, '\0');
  in.read(str.data(), len);
  checksum = crc64_update(
      checksum, reinterpret_cast<const unsigned char *>(str.data()), len);
  return str;
}

static void writeDouble(std::ofstream &out, double val, uint64_t &checksum) {
  out.write(reinterpret_cast<const char *>(&val), sizeof(val));
  checksum = crc64_update(
      checksum, reinterpret_cast<const unsigned char *>(&val), sizeof(val));
}

static double readDouble(std::ifstream &in, uint64_t &checksum) {
  double val;
  in.read(reinterpret_cast<char *>(&val), sizeof(val));
  checksum = crc64_update(
      checksum, reinterpret_cast<const unsigned char *>(&val), sizeof(val));
  return val;
}

bool RdbPersistence::saveToFile(const std::vector<Database> &databases,
                                const std::string &filename) {
  std::ofstream out(filename, std::ios::binary);
  if (!out) {
    std::cerr << "[RDB] Failed to open file for writing: " << filename
              << std::endl;
    return false;
  }

  uint64_t checksum = 0;

  out.write("REDIS0009", 9);
  checksum = crc64_update(
      checksum, reinterpret_cast<const unsigned char *>("REDIS0009"), 9);

  for (size_t i = 0; i < databases.size(); ++i) {
    const auto &db = databases[i];
    if (db.size() == 0) continue;

    writeByte(out, RDB_OPCODE_SELECTDB, checksum);
    writeLength(out, i, checksum);

    writeByte(out, RDB_OPCODE_RESIZEDB, checksum);
    writeLength(out, db.size(), checksum);
    writeLength(out, db.expiresSize(), checksum);

    for (const auto &key : db.keys("*")) {
      const RedisObject *obj = db.getObject(key);
      if (!obj) continue;

      int64_t pttl = db.pttl(key);
      if (pttl >= 0) {
        auto now = std::chrono::system_clock::now();
        auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();
        uint64_t expireMs = epoch + pttl;

        writeByte(out, RDB_OPCODE_EXPIRETIME_MS, checksum);
        out.write(reinterpret_cast<const char *>(&expireMs), 8);
        checksum = crc64_update(
            checksum, reinterpret_cast<const unsigned char *>(&expireMs), 8);
      }

      switch (obj->type()) {
      case ObjectType::String: {
        writeByte(out, RDB_TYPE_STRING, checksum);
        writeString(out, key, checksum);
        writeString(out, obj->asString(), checksum);
        break;
      }
      case ObjectType::List: {
        writeByte(out, RDB_TYPE_LIST, checksum);
        writeString(out, key, checksum);
        const auto &list = obj->asList();
        writeLength(out, list.size(), checksum);
        for (int j = 0; j < static_cast<int>(list.size()); ++j) {
          auto val = list.get(j);
          if (val) writeString(out, *val, checksum);
        }
        break;
      }
      case ObjectType::Hash: {
        writeByte(out, RDB_TYPE_HASH, checksum);
        writeString(out, key, checksum);
        auto &hash =
            const_cast<HashTable<std::string, std::string> &>(obj->asHash());
        writeLength(out, hash.size(), checksum);
        for (auto kv : hash) {
          writeString(out, kv.first, checksum);
          writeString(out, kv.second, checksum);
        }
        break;
      }
      case ObjectType::Set: {
        writeByte(out, RDB_TYPE_SET, checksum);
        writeString(out, key, checksum);
        auto &set = const_cast<HashSet &>(obj->asSet());
        writeLength(out, set.size(), checksum);
        for (const auto &member : set.members()) {
          writeString(out, member, checksum);
        }
        break;
      }
      case ObjectType::SortedSet: {
        writeByte(out, RDB_TYPE_ZSET, checksum);
        writeString(out, key, checksum);
        const auto &zset = obj->asSortedSet();
        writeLength(out, zset.size(), checksum);
        auto elements = zset.rangeByRank(0, -1);
        for (const auto &pair : elements) {
          writeString(out, pair.first, checksum);
          writeDouble(out, pair.second, checksum);
        }
        break;
      }
      default:
        break;
      }
    }
  }

  writeByte(out, RDB_OPCODE_EOF, checksum);

  out.write(reinterpret_cast<const char *>(&checksum), 8);

  return true;
}

bool RdbPersistence::loadFromFile(std::vector<Database> &databases,
                                  const std::string &filename) {
  std::ifstream in(filename, std::ios::binary);
  if (!in) {
    return false;
  }

  uint64_t checksum = 0;

  char magic[9];
  in.read(magic, 9);
  checksum = crc64_update(
      checksum, reinterpret_cast<const unsigned char *>(magic), 9);
  if (std::string(magic, 9) != "REDIS0009") {
    std::cerr << "[RDB] Invalid format or unsupported version" << std::endl;
    return false;
  }

  for (auto &db : databases) {
    db.flushDb();
  }

  int currentDb = 0;
  uint64_t expireMs = 0;
  bool hasExpire = false;

  while (in.peek() != EOF) {
    uint8_t opcode = readByte(in, checksum);

    if (opcode == RDB_OPCODE_EOF) {
      break;
    }

    if (opcode == RDB_OPCODE_SELECTDB) {
      currentDb = readLength(in, checksum);
      continue;
    }

    if (opcode == RDB_OPCODE_RESIZEDB) {
      readLength(in, checksum);
      readLength(in, checksum);
      continue;
    }

    if (opcode == RDB_OPCODE_EXPIRETIME) {
      uint32_t expireSec;
      in.read(reinterpret_cast<char *>(&expireSec), 4);
      checksum = crc64_update(
          checksum, reinterpret_cast<const unsigned char *>(&expireSec), 4);
      expireMs = static_cast<uint64_t>(expireSec) * 1000;
      hasExpire = true;
      continue;
    }

    if (opcode == RDB_OPCODE_EXPIRETIME_MS) {
      in.read(reinterpret_cast<char *>(&expireMs), 8);
      checksum = crc64_update(
          checksum, reinterpret_cast<const unsigned char *>(&expireMs), 8);
      hasExpire = true;
      continue;
    }

    std::string key = readString(in, checksum);

    if (currentDb >= static_cast<int>(databases.size())) {
      continue;
    }

    auto &db = databases[currentDb];

    switch (opcode) {
    case RDB_TYPE_STRING: {
      std::string val = readString(in, checksum);
      db.setObject(key, RedisObject(std::move(val)));
      break;
    }
    case RDB_TYPE_LIST: {
      auto list = std::make_unique<LinkedList>();
      uint64_t len = readLength(in, checksum);
      for (uint64_t i = 0; i < len; ++i) {
        list->pushBack(readString(in, checksum));
      }
      db.setObject(key, RedisObject(std::move(list)));
      break;
    }
    case RDB_TYPE_HASH: {
      auto hash =
          std::make_unique<HashTable<std::string, std::string>>();
      uint64_t len = readLength(in, checksum);
      for (uint64_t i = 0; i < len; ++i) {
        std::string f = readString(in, checksum);
        std::string v = readString(in, checksum);
        hash->set(f, v);
      }
      db.setObject(key, RedisObject(std::move(hash)));
      break;
    }
    case RDB_TYPE_SET: {
      auto set = std::make_unique<HashSet>();
      uint64_t len = readLength(in, checksum);
      for (uint64_t i = 0; i < len; ++i) {
        set->add(readString(in, checksum));
      }
      db.setObject(key, RedisObject(std::move(set)));
      break;
    }
    case RDB_TYPE_ZSET: {
      auto zset = std::make_unique<SortedSet>();
      uint64_t len = readLength(in, checksum);
      for (uint64_t i = 0; i < len; ++i) {
        std::string member = readString(in, checksum);
        double score = readDouble(in, checksum);
        zset->add(member, score);
      }
      db.setObject(key, RedisObject(std::move(zset)));
      break;
    }
    default:
      std::cerr << "[RDB] Unknown value type: " << static_cast<int>(opcode)
                << std::endl;
      break;
    }

    if (hasExpire) {
      db.setExpiryAt(key, expireMs);
      hasExpire = false;
    }
  }

  uint64_t expectedChecksum;
  in.read(reinterpret_cast<char *>(&expectedChecksum), 8);
  if (expectedChecksum != 0 && expectedChecksum != checksum) {
    std::cerr << "[RDB] CRC64 Checksum mismatch! File is corrupted."
              << std::endl;
    return false;
  }

  std::cout << "[RDB] Successfully loaded database from disk." << std::endl;
  return true;
}

} // namespace redisdb
