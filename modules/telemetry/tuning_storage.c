#include "tuning_storage.h"
#include <string.h>
#define MAGIC 0x54554e45U
#define SCHEMA 1U
#define COMMIT 0x434f4d4dU
#define CRC_WORD (TUNING_RECORD_WORDS - 2U)
#define COMMIT_WORD (TUNING_RECORD_WORDS - 1U)
_Static_assert(sizeof(float)==sizeof(uint32_t), "Storage requires float32");
static uint32_t CRC(const uint32_t *record)
{
    uint32_t crc=UINT32_MAX;
    for (unsigned i=0;i<CRC_WORD;++i)
        for (unsigned byte=0;byte<4;++byte) {
            crc ^= (record[i] >> (byte*8)) & 255U;
            for (unsigned bit=0;bit<8;++bit)
                crc=(crc>>1)^((0U-(crc&1U))&0xedb88320U);
        }
    return ~crc;
}
static void Read(const TuningStorage *s,unsigned bank,unsigned word,uint32_t *out)
{
    for (unsigned i=0;i<TUNING_RECORD_WORDS;++i) out[i]=s->io.banks[bank][word+i];
}
static bool Valid(const uint32_t *r)
{
    return r[0]==MAGIC && r[1]==SCHEMA && r[3] && !(r[3]&~7U) &&
        r[COMMIT_WORD]==COMMIT && r[CRC_WORD]==CRC(r);
}
static void Decode(const uint32_t *r,TuningStoredImage *image)
{
    image->mask=r[3];
    memcpy(image->modes,&r[4],sizeof image->modes);
    memcpy(image->values,&r[7],sizeof image->values);
}
void TuningStorageInit(TuningStorage *s,const TuningStorageIO *io)
{
    memset(s,0,sizeof *s); s->io=*io;
    if (!io->banks[0] || !io->banks[1] || io->bank_words<TUNING_RECORD_WORDS ||
        io->bank_words%TUNING_RECORD_WORDS) return;
    uint32_t record[TUNING_RECORD_WORDS];
    for (unsigned bank=0;bank<2;++bank)
        for (unsigned word=0;word<io->bank_words;word+=TUNING_RECORD_WORDS) {
            Read(s,bank,word,record);
            bool blank=true;
            for (unsigned i=0;i<TUNING_RECORD_WORDS;++i) blank &= record[i]==UINT32_MAX;
            if (!blank) s->next[bank]=word+TUNING_RECORD_WORDS;
            if (Valid(record) && (!s->valid ||
                (record[2]!=s->sequence && (uint32_t)(record[2]-s->sequence)<0x80000000U))) {
                Decode(record,&s->image); s->sequence=record[2];
                s->active=bank; s->latest_word=word; s->valid=true;
            }
        }
}
bool TuningStorageSave(TuningStorage *s,const TuningStoredImage *image)
{
    if (!image || !image->mask || (image->mask&~7U) || !s->io.program || !s->io.erase ||
        !s->io.banks[0] || !s->io.banks[1] || s->io.bank_words<TUNING_RECORD_WORDS ||
        s->io.bank_words%TUNING_RECORD_WORDS) return false;
    if (s->valid && !memcmp(image,&s->image,sizeof *image)) return true;
    unsigned bank=s->active;
    if (s->next[bank]+TUNING_RECORD_WORDS>s->io.bank_words) {
        bank ^= 1U;
        /* The sector holding the last committed configuration is never erased. */
        if (!s->io.erase(s->io.context,bank)) return false;
        s->next[bank]=0;
    }
    unsigned word=s->next[bank];
    uint32_t record[TUNING_RECORD_WORDS];
    memset(record,0xff,sizeof record);
    record[0]=MAGIC; record[1]=SCHEMA; record[2]=s->valid ? s->sequence+1U : 1U;
    record[3]=image->mask;
    memcpy(&record[4],image->modes,sizeof image->modes);
    memcpy(&record[7],image->values,sizeof image->values);
    record[CRC_WORD]=CRC(record);
    s->next[bank]+=TUNING_RECORD_WORDS; /* Never reuse a possibly torn slot. */
    for (unsigned i=0;i<COMMIT_WORD;++i) {
        if (record[i]!=UINT32_MAX &&
            !s->io.program(s->io.context,bank,word+i,record[i])) return false;
        if (s->io.banks[bank][word+i]!=record[i]) return false;
    }
    /* Commit is programmed only after every payload word and its CRC verify. */
    if (!s->io.program(s->io.context,bank,word+COMMIT_WORD,COMMIT)) return false;
    Read(s,bank,word,record);
    if (!Valid(record)) return false;
    s->image=*image; s->sequence=record[2]; s->active=bank;
    s->latest_word=word; s->valid=true;
    return true;
}
