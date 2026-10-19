#include "tuning_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t flash[2][TUNING_RECORD_WORDS * 2];
static int fail_after = -1;
static unsigned writes, erases;
static bool program(void *ctx, unsigned bank, unsigned word, uint32_t value)
{
    (void)ctx;
    if (fail_after == 0) return false;
    if (fail_after > 0) --fail_after;
    assert((flash[bank][word] & value) == value);
    flash[bank][word] &= value; ++writes; return true;
}
static bool erase(void *ctx, unsigned bank)
{
    (void)ctx;
    if (fail_after == 0) {
        memset(flash[bank], 0xff, sizeof flash[bank] / 2);
        return false;
    }
    if (fail_after > 0) --fail_after;
    memset(flash[bank], 0xff, sizeof flash[bank]); ++erases; return true;
}
static TuningStorage open_store(void)
{
    TuningStorage s;
    TuningStorageIO io = {.banks={flash[0],flash[1]},
        .bank_words=sizeof flash[0]/sizeof(uint32_t), .program=program, .erase=erase};
    TuningStorageInit(&s, &io); return s;
}
static uint32_t record_crc(const uint32_t *r)
{
    uint32_t crc=UINT32_MAX;
    for (unsigned i=0;i<TUNING_RECORD_WORDS-2;++i)
        for (unsigned byte=0;byte<4;++byte) {
            crc^=(r[i]>>(8*byte))&255U;
            for (unsigned bit=0;bit<8;++bit) crc=(crc>>1)^((0U-(crc&1U))&0xedb88320U);
        }
    return ~crc;
}
int main(void)
{
    memset(flash, 0xff, sizeof flash);
    TuningStorage s = open_store();
    assert(!s.valid);
    TuningStoredImage a = {.mask=7, .modes={0,3,2}, .values={{.75f},{1},{800}}};
    assert(TuningStorageSave(&s, &a));
    unsigned before=writes;
    assert(TuningStorageSave(&s, &a) && writes==before);
    uint32_t baseline[2][TUNING_RECORD_WORDS * 2];
    memcpy(baseline,flash,sizeof flash);
    TuningStoredImage b=a; b.values[0][0]=2;
    /* Reboot at every word boundary: incomplete records must never replace a. */
    for (int cut=0;cut<=(int)TUNING_RECORD_WORDS;++cut) {
        memcpy(flash,baseline,sizeof flash); fail_after=cut; s=open_store();
        bool ok=TuningStorageSave(&s,&b);
        TuningStorage reboot=open_store();
        assert(reboot.valid);
        assert(reboot.image.values[0][0]==(ok ? 2 : .75f));
    }
    fail_after=-1; memcpy(flash,baseline,sizeof flash); s=open_store();
    assert(TuningStorageSave(&s,&b)); /* Active sector is now full. */
    memcpy(baseline,flash,sizeof flash);
    TuningStoredImage c=b; c.values[0][0]=3;
    for (int cut=0;cut<=(int)TUNING_RECORD_WORDS+1;++cut) {
        memcpy(flash,baseline,sizeof flash); fail_after=cut; s=open_store();
        bool ok=TuningStorageSave(&s,&c);
        TuningStorage reboot=open_store();
        assert(reboot.valid && reboot.image.values[0][0]==(ok ? 3 : 2));
    }
    fail_after=-1; memcpy(flash,baseline,sizeof flash); s=open_store();
    assert(TuningStorageSave(&s,&c) && erases>0);
    flash[s.active][s.latest_word+12] ^= 1; /* CRC damage: previous sector survives. */
    s=open_store(); assert(s.valid && s.image.values[0][0]==2);
    memcpy(flash,baseline,sizeof flash); s=open_store();
    assert(TuningStorageSave(&s,&c));
    flash[s.active][s.latest_word+TUNING_RECORD_WORDS-1] ^= 1;
    s=open_store(); assert(s.valid && s.image.values[0][0]==2);
    /* Manufacture a valid near-wrap sequence, then append across uint32 wrap. */
    memset(flash,0xff,sizeof flash); s=open_store();
    assert(TuningStorageSave(&s,&a));
    flash[0][2]=UINT32_MAX;
    flash[0][TUNING_RECORD_WORDS-2]=record_crc(flash[0]);
    s=open_store(); assert(s.sequence==UINT32_MAX);
    assert(TuningStorageSave(&s,&b) && s.sequence==0);
    s=open_store(); assert(s.sequence==0 && s.image.values[0][0]==2);
    puts("Power-loss journal tests passed");
}
