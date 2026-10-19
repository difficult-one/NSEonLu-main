#ifndef TUNING_STORAGE_H
#define TUNING_STORAGE_H
#include <stdbool.h>
#include <stdint.h>
#define TUNING_RECORD_WORDS 128U
#define TUNING_STORED_GROUPS 3U
#define TUNING_STORED_VALUES 24U
typedef struct {
    uint32_t mask, modes[TUNING_STORED_GROUPS];
    float values[TUNING_STORED_GROUPS][TUNING_STORED_VALUES];
} TuningStoredImage;
typedef struct {
    const volatile uint32_t *banks[2];
    unsigned bank_words;
    void *context;
    bool (*program)(void *, unsigned bank, unsigned word, uint32_t value);
    bool (*erase)(void *, unsigned bank);
} TuningStorageIO;
typedef struct {
    TuningStorageIO io;
    TuningStoredImage image;
    uint32_t sequence;
    unsigned active, latest_word, next[2];
    bool valid;
} TuningStorage;
void TuningStorageInit(TuningStorage *store, const TuningStorageIO *io);
bool TuningStorageSave(TuningStorage *store, const TuningStoredImage *image);
#endif
