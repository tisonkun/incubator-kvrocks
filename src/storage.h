#pragma once

#include <event2/bufferevent.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/backupable_db.h>
#include <utility>

#include "status.h"
#include "lock_manager.h"
#include "config.h"

namespace Engine {

class Storage {
 public:
  explicit Storage(Config *config)
      :backup_env_(rocksdb::Env::Default()),
       config_(config),
       lock_mgr_(16) {}
  ~Storage();

  void InitOptions(rocksdb::Options *options);
  Status Open();
  Status CreateColumnFamiles(rocksdb::Options &options);
  Status CreateBackup();
  Status DestroyBackup();
  Status RestoreFromBackup(rocksdb::SequenceNumber *seq);
  Status GetWALIter(rocksdb::SequenceNumber seq,
                    std::unique_ptr<rocksdb::TransactionLogIterator> *iter);
  Status WriteBatch(std::string &&raw_batch);
  rocksdb::SequenceNumber LatestSeq();
  rocksdb::Status Write(const rocksdb::WriteOptions& options, rocksdb::WriteBatch* updates);

  rocksdb::Status Compact(const rocksdb::Slice *begin, const rocksdb::Slice *end);
  rocksdb::DB *GetDB();
  const std::string GetName() {
    return config_->db_name;
  }
  rocksdb::ColumnFamilyHandle *GetCFHandle(std::string name);
  std::vector<rocksdb::ColumnFamilyHandle*>* GetCFHandles();
  LockManager *GetLockManager() { return &lock_mgr_; }

  Storage(const Storage &) = delete;
  Storage &operator=(const Storage &) = delete;

  class BackupManager {
   public:
    // Master side
    static Status OpenLatestMeta(Storage *storage,
                                 int *fd,
                                 rocksdb::BackupID *meta_id,
                                 uint64_t *file_size);
    static int OpenDataFile(Storage *storage, std::string rel_path,
                            uint64_t *file_size);

    // Slave side
    struct MetaInfo {
      int64_t timestamp;
      rocksdb::SequenceNumber seq;
      std::string meta_data;
      // [[filename, checksum]...]
      std::vector<std::pair<std::string, uint32_t>> files;
    };
    static MetaInfo ParseMetaAndSave(Storage *storage,
                                     rocksdb::BackupID meta_id,
                                     evbuffer *evbuf);
    static std::unique_ptr<rocksdb::WritableFile> NewTmpFile(
        Storage *storage, std::string rel_path);
    static Status SwapTmpFile(Storage *storage, std::string rel_path);
    static bool FileExists(Storage *storage, std::string rel_path);
    static Status PurgeBackup(Storage *storage);
  };

 private:
  rocksdb::DB *db_ = nullptr;
  rocksdb::BackupEngine *backup_ = nullptr;
  rocksdb::Env *backup_env_;
  Config *config_ = nullptr;
  std::vector<rocksdb::ColumnFamilyHandle *> cf_handles_;
  LockManager lock_mgr_;
};

}  // namespace Engine
