#include "compat.h"
#include "../duke3d.h"
#include "reality.h"
#include "reality_sound.h"

//
// Uses info and snippets from this code:
// https://github.com/jombo23/N64-Tools/blob/master/N64%20Sound%20Tool/N64SoundListToolUpdated/N64SoundLibrary/N64AIFCAudio.cpp
//

rt_CTL_t *soundCtl;
rt_instrument_t *soundInfo;
rt_soundinstance_t rt_soundinstance[MAXRTSOUNDINSTANCES];

int16_t rt_soundrate[MAXSOUNDS];

static rt_env_t *RT_LoadEnv(uint32_t ctlOffset, uint32_t envOffset)
{
    rt_env_t *env = (rt_env_t*)Xcalloc(1, sizeof(rt_env_t));
    lseek(rt_group, ctlOffset+envOffset, SEEK_SET);
    read(rt_group, &env->attack_time, sizeof(env->attack_time));
    env->attack_time = B_BIG32(env->attack_time);
    read(rt_group, &env->decay_time, sizeof(env->decay_time));
    env->decay_time = B_BIG32(env->decay_time);
    read(rt_group, &env->release_time, sizeof(env->release_time));
    env->release_time = B_BIG32(env->release_time);
    read(rt_group, &env->attack_volume, sizeof(env->attack_volume));
    read(rt_group, &env->decay_volume, sizeof(env->decay_volume));
    return env;
}

static rt_key_t *RT_LoadKey(uint32_t ctlOffset, uint32_t keyOffset)
{
    rt_key_t*key = (rt_key_t*)Xcalloc(1, sizeof(rt_key_t));
    lseek(rt_group, ctlOffset+keyOffset, SEEK_SET);
    read(rt_group, &key->velocitymin, sizeof(key->velocitymin));
    read(rt_group, &key->velocitymax, sizeof(key->velocitymax));
    read(rt_group, &key->keymin, sizeof(key->keymin));
    read(rt_group, &key->keymax, sizeof(key->keymax));
    read(rt_group, &key->keybase, sizeof(key->keybase));
    read(rt_group, &key->detune, sizeof(key->detune));
    return key;
}

static rt_adpcm_loop_t *RT_LoadADPCMLoop(uint32_t ctlOffset, uint32_t loopOffset)
{
    rt_adpcm_loop_t *loop = (rt_adpcm_loop_t*)Xcalloc(1, sizeof(rt_adpcm_loop_t));
    lseek(rt_group, ctlOffset+loopOffset, SEEK_SET);
    read(rt_group, &loop->start, sizeof(loop->start));
    loop->start = B_BIG32(loop->start);
    read(rt_group, &loop->end, sizeof(loop->end));
    loop->end = B_BIG32(loop->end);
    read(rt_group, &loop->count, sizeof(loop->count));
    loop->count = B_BIG32(loop->count);
    read(rt_group, &loop->state, sizeof(loop->state));
    for (int i = 0; i < 16; i++)
    {
        loop->state[i] = B_BIG16(loop->state[i]);
    }
    return loop;
}

static rt_adpcm_book_t *RT_LoadADPCMBook(uint32_t ctlOffset, uint32_t bookOffset)
{
    rt_adpcm_book_t *book = (rt_adpcm_book_t*)Xcalloc(1, sizeof(rt_adpcm_book_t));
    lseek(rt_group, ctlOffset+bookOffset, SEEK_SET);
    read(rt_group, &book->order, sizeof(book->order));
    book->order = B_BIG32(book->order);
    read(rt_group, &book->npredictors, sizeof(book->npredictors));
    book->npredictors = B_BIG32(book->npredictors);
    book->predictors = (int16_t*)Xcalloc(book->order * book->npredictors * 8, sizeof(int16_t));
    read(rt_group, book->predictors, book->order * book->npredictors * 8 * sizeof(int16_t));
    for (int i = 0; i < book->order * book->npredictors * 8; i++)
    {
        book->predictors[i] = B_BIG16(book->predictors[i]);
    }
    return book;
}

static rt_raw_loop_t *RT_LoadRAWLoop(uint32_t ctlOffset, uint32_t loopOffset)
{
    rt_raw_loop_t *loop = (rt_raw_loop_t*)Xcalloc(1, sizeof(rt_raw_loop_t));
    lseek(rt_group, ctlOffset+loopOffset, SEEK_SET);
    read(rt_group, &loop->start, sizeof(loop->start));
    loop->start = B_BIG32(loop->start);
    read(rt_group, &loop->end, sizeof(loop->end));
    loop->end = B_BIG32(loop->end);
    read(rt_group, &loop->count, sizeof(loop->count));
    loop->count = B_BIG32(loop->count);
    return loop;
}

static rt_wave_t *RT_LoadWave(uint32_t ctlOffset, uint32_t waveOffset, uint32_t tblOffset)
{
    rt_wave_t *wave = (rt_wave_t*)Xcalloc(1, sizeof(rt_wave_t));
    lseek(rt_group, ctlOffset+waveOffset, SEEK_SET);
    read(rt_group, &wave->base, sizeof(wave->base));
    wave->base = B_BIG32(wave->base) + tblOffset;
    read(rt_group, &wave->len, sizeof(wave->len));
    wave->len = B_BIG32(wave->len);
    read(rt_group, &wave->type, sizeof(wave->type));
    read(rt_group, &wave->flags, sizeof(wave->flags));
    uint16_t pad;
    read(rt_group, &pad, sizeof(pad));
    switch (wave->type)
    {
    case 0: // ADPCM
    {
        uint32_t loopOffset, bookOffset;
        read(rt_group, &loopOffset, sizeof(loopOffset));
        loopOffset = B_BIG32(loopOffset);
        read(rt_group, &bookOffset, sizeof(bookOffset));
        bookOffset = B_BIG32(bookOffset);
        wave->adpcm = (rt_adpcm_wave_t*)Xcalloc(1, sizeof(rt_adpcm_wave_t));
        if (loopOffset)
            wave->adpcm->loop = RT_LoadADPCMLoop(ctlOffset, loopOffset);
        if (bookOffset)
            wave->adpcm->book = RT_LoadADPCMBook(ctlOffset, bookOffset);
        break;
    }
    case 1: // RAW
    {
        uint32_t loopOffset;
        read(rt_group, &loopOffset, sizeof(loopOffset));
        loopOffset = B_BIG32(loopOffset);
        wave->raw = (rt_raw_wave_t*)Xcalloc(1, sizeof(rt_raw_wave_t));
        if (loopOffset)
            wave->raw->loop = RT_LoadRAWLoop(ctlOffset, loopOffset);
        break;
    }
    }
    return wave;
}

static rt_sound_t *RT_LoadSound(uint32_t ctlOffset, uint32_t soundOffset, uint32_t tblOffset)
{
    rt_sound_t *snd = (rt_sound_t*)Xcalloc(1, sizeof(rt_sound_t));
    lseek(rt_group, ctlOffset+soundOffset, SEEK_SET);
    uint32_t envOffset, keyOffset, waveOffset;
    read(rt_group, &envOffset, sizeof(envOffset));
    envOffset = B_BIG32(envOffset);
    read(rt_group, &keyOffset, sizeof(keyOffset));
    keyOffset = B_BIG32(keyOffset);
    read(rt_group, &waveOffset, sizeof(waveOffset));
    waveOffset = B_BIG32(waveOffset);
    read(rt_group, &snd->sample_pan, sizeof(snd->sample_pan));
    read(rt_group, &snd->sample_volume, sizeof(snd->sample_volume));
    read(rt_group, &snd->flags, sizeof(snd->flags));
    snd->flags = B_BIG16(snd->flags);
    snd->env = RT_LoadEnv(ctlOffset, envOffset);
    snd->key = RT_LoadKey(ctlOffset, keyOffset);
    snd->wave = RT_LoadWave(ctlOffset, waveOffset, tblOffset);
    return snd;
}

static rt_instrument_t *RT_LoadInstrument(uint32_t ctlOffset, uint32_t instOffset, uint32_t tblOffset)
{
    rt_instrument_t *inst = (rt_instrument_t*)Xcalloc(1, sizeof(rt_instrument_t));
    lseek(rt_group, ctlOffset+instOffset, SEEK_SET);
    read(rt_group, &inst->volume, sizeof(inst->volume));
    read(rt_group, &inst->pan, sizeof(inst->pan));
    read(rt_group, &inst->priority, sizeof(inst->priority));
    read(rt_group, &inst->flags, sizeof(inst->flags));
    read(rt_group, &inst->trem_type, sizeof(inst->trem_type));
    read(rt_group, &inst->trem_rate, sizeof(inst->trem_rate));
    read(rt_group, &inst->trem_depth, sizeof(inst->trem_depth));
    read(rt_group, &inst->trem_delay, sizeof(inst->trem_delay));
    read(rt_group, &inst->vib_type, sizeof(inst->vib_type));
    read(rt_group, &inst->vib_rate, sizeof(inst->vib_rate));
    read(rt_group, &inst->vib_depth, sizeof(inst->vib_depth));
    read(rt_group, &inst->vib_delay, sizeof(inst->vib_delay));
    read(rt_group, &inst->bend_range, sizeof(inst->bend_range));
    inst->bend_range = B_BIG16(inst->bend_range);
    read(rt_group, &inst->sound_count, sizeof(inst->sound_count));
    inst->sound_count = B_BIG16(inst->sound_count);
    uint32_t *soundOffset = (uint32_t*)Xcalloc(inst->sound_count, sizeof(uint32_t));
    read(rt_group, soundOffset, sizeof(uint32_t) * inst->sound_count);
    inst->sounds = (rt_sound_t**)Xcalloc(inst->sound_count, sizeof(rt_sound_t*));
    for (int i = 0; i < inst->sound_count; i++)
    {
        soundOffset[i] = B_BIG32(soundOffset[i]);
    }

    for (int gap = inst->sound_count / 2; gap > 0; gap /= 2)
        for (int i = gap; i < inst->sound_count; i++)
            for (int j = i - gap; j >= 0 && soundOffset[j] > soundOffset[j + gap]; j -= gap)
                swap(&soundOffset[j], &soundOffset[j + gap]);

    for (int i = 0; i < inst->sound_count; i++)
    {
        inst->sounds[i] = RT_LoadSound(ctlOffset, soundOffset[i], tblOffset);
    }
    Xfree(soundOffset);
    return inst;
}

static rt_bank_t *RT_LoadBank(uint32_t ctlOffset, uint32_t bankOffset, uint32_t tblOffset)
{
    rt_bank_t *bank = (rt_bank_t*)Xcalloc(1, sizeof(rt_bank_t));
    lseek(rt_group, ctlOffset+bankOffset, SEEK_SET);
    read(rt_group, &bank->inst_count, sizeof(bank->inst_count));
    bank->inst_count = B_BIG16(bank->inst_count);
    read(rt_group, &bank->flags, sizeof(bank->flags));
    bank->flags = B_BIG16(bank->flags);
    read(rt_group, &bank->unused, sizeof(bank->unused));
    bank->unused = B_BIG16(bank->unused);
    read(rt_group, &bank->rate, sizeof(bank->rate));
    bank->rate = B_BIG16(bank->rate);
    uint32_t percOffset, *instOffset;
    read(rt_group, &percOffset, sizeof(percOffset));
    percOffset = B_BIG32(percOffset);
    instOffset = (uint32_t*)Xcalloc(bank->inst_count, sizeof(uint32_t));
    read(rt_group, instOffset, sizeof(uint32_t) * bank->inst_count);

    bank->inst = (rt_instrument_t**)Xcalloc(bank->inst_count, sizeof(rt_instrument_t*));

    if (percOffset)
        bank->perc = RT_LoadInstrument(ctlOffset, percOffset, tblOffset);
    for (int i = 0; i < bank->inst_count; i++)
    {
        bank->inst[i] = RT_LoadInstrument(ctlOffset, B_BIG32(instOffset[i]), tblOffset);
    }

    Xfree(instOffset);

    return bank;
}

rt_CTL_t *RT_LoadCTL(uint32_t ctlOffset, uint32_t tblOffset)
{
    rt_CTL_t *ctl = (rt_CTL_t*)Xcalloc(1, sizeof(rt_CTL_t));
    lseek(rt_group, ctlOffset, SEEK_SET);
    read(rt_group, &ctl->signature, sizeof(ctl->signature));
    ctl->signature = B_BIG16(ctl->signature);
    read(rt_group, &ctl->bank_count, sizeof(ctl->bank_count));
    ctl->bank_count = B_BIG16(ctl->bank_count);
    if (ctl->signature != CTLSIGNATURE || ctl->bank_count <= 0)
    {
        initprintf("Error loading sound bank %x\n", ctlOffset);
        Xfree(ctl);
        return nullptr;
    }
    uint32_t *bank_offset = (uint32_t*)Xcalloc(ctl->bank_count, sizeof(uint32_t));
    read(rt_group, bank_offset, sizeof(uint32_t) * ctl->bank_count);
    // Load banks
    ctl->bank = (rt_bank_t**)Xcalloc(ctl->bank_count, sizeof(rt_bank_t*));
    for (int i = 0; i < ctl->bank_count; i++)
    {
        ctl->bank[i] = RT_LoadBank(ctlOffset, B_BIG32(bank_offset[i]), tblOffset);
    }
    Xfree(bank_offset);
    return ctl;
}

void RT_InitSound(void)
{
    static const uint32_t soundCtlOffset = 0x5fe770;
    static const uint32_t soundTblOffset = 0x60b330;
    soundCtl = RT_LoadCTL(soundCtlOffset, soundTblOffset);
    if (soundCtl && soundCtl->bank[0] && soundCtl->bank[0]->inst_count > 0)
        soundInfo = soundCtl->bank[0]->inst[0];

    RT_MusicInit();

    if (!soundInfo)
        return;
    
    g_highestSoundIdx = soundInfo->sound_count;

    memset(rt_soundinstance, 0, sizeof(rt_soundinstance));

    static const uint32_t soundvoOffset = 0xb4990;
    static const uint32_t soundpeOffset = 0xb4be8;
    static const uint32_t soundpsOffset = 0xb4e40;
    static const uint32_t soundmOffset = 0xb5098;
    static const uint32_t soundrateOffset = 0x8a4a8;
    int16_t *tempshort = (int16_t*)tempbuf;
    lseek(rt_group, soundvoOffset, SEEK_SET);
    read(rt_group, tempshort, soundInfo->sound_count * sizeof(int16_t));
    for (int i = 0; i < soundInfo->sound_count; i++)
    {
        if (g_sounds[i].filename == nullptr)
        {
            g_sounds[i].vo = B_BIG16(tempshort[i]);
            g_sounds[i].pr = 200; // FIXME
            g_sounds[i].volume = fix16_one;
        }
    }
    lseek(rt_group, soundpeOffset, SEEK_SET);
    read(rt_group, tempshort, soundInfo->sound_count * sizeof(int16_t));
    for (int i = 0; i < soundInfo->sound_count; i++)
    {
        if (g_sounds[i].filename == nullptr)
            g_sounds[i].pe = B_BIG16(tempshort[i]);
    }
    lseek(rt_group, soundpsOffset, SEEK_SET);
    read(rt_group, tempshort, soundInfo->sound_count * sizeof(int16_t));
    for (int i = 0; i < soundInfo->sound_count; i++)
    {
        if (g_sounds[i].filename == nullptr)
            g_sounds[i].ps = B_BIG16(tempshort[i]);
    }
    lseek(rt_group, soundmOffset, SEEK_SET);
    read(rt_group, tempbuf, soundInfo->sound_count * sizeof(uint8_t));
    for (int i = 0; i < soundInfo->sound_count; i++)
    {
        if (g_sounds[i].filename == nullptr)
        {
            g_sounds[i].m = tempbuf[i];
            g_sounds[i].m &= ~SF_ONEINST_INTERNAL;
            if (g_sounds[i].m & SF_LOOP)
                g_sounds[i].m |= SF_ONEINST_INTERNAL;
            g_sounds[i].m |= SF_REALITY_INTERNAL;
        }
    }
    lseek(rt_group, soundrateOffset, SEEK_SET);
    read(rt_group, rt_soundrate, soundInfo->sound_count * sizeof(int16_t));
    for (int i = 0; i < soundInfo->sound_count; i++)
    {
        rt_soundrate[i] = B_BIG16(rt_soundrate[i]);
    }
}

int RT_LoadSound(int num)
{
    if (!soundCtl)
        return 0;
    if (num < 0 || num >= soundInfo->sound_count)
        return 0;
    
    auto &snd = g_sounds[num];
    rt_sound_t *sound = soundInfo->sounds[num];
    lseek(rt_group, sound->wave->base, SEEK_SET);
    int l = sound->wave->len;
    g_soundlocks[num] = 200;
    snd.siz = sound->wave->len;
    g_cache.allocateBlock((intptr_t *)&snd.ptr, l, (char *)&g_soundlocks[num]);
    l = read(rt_group, snd.ptr, l);

    return l;
}

void RT_Decode8(char *in, int16_t *out, int index, int16_t *pred1, int16_t lastsmp[8])
{
    static int16_t itable[16] = {
        0, 1, 2, 3, 4, 5, 6, 7,
        -8, -7, -6, -5, -4, -3, -2, -1,
    };
    int16_t tmp[8];
    int total = 0;
    int16_t *pred2 = (pred1 + 8);
    for (int i = 0; i < 8; i++)
        tmp[i] = itable[(i&1) ? (*in++ & 0xf) : ((*in >> 4) & 0xf)] << index;
    
    for (int i = 0; i < 8; i++)
    {
        total = (pred1[i] * lastsmp[6]);
        total += (pred2[i] * lastsmp[7]);
        if (i > 0)
        {
            for (int x = i - 1; x > -1; x--)
            {
                total += tmp[((i - 1) - x)] * pred2[x];
            }
        }

        int result = ((tmp[i] << 0xb) + total) >> 0xb;
        out[i] = clamp(result, INT16_MIN, INT16_MAX);
    }
    // update the last sample set for subsequent iterations
    memcpy(lastsmp, out, sizeof(int16_t)*8);
}

void RT_SoundDecode(const char **ptr, uint32_t *length, void *userdata)
{
    auto snd = (rt_soundinstance_t*)userdata;
    if (snd->endofdata)
    {
        *ptr = nullptr;
        *length = 0;
        RT_FreeSoundSlot(snd);
        return;
    }
    if (snd->rtsound->wave->adpcm)
    {
        *ptr = (char*)snd->buf;
        auto predictors = snd->rtsound->wave->adpcm->book->predictors;
        auto wavelen = snd->rtsound->wave->len;
        int i;
        for (i = 0; i < RTSNDBLOCKSIZE; i++)
        {
            while (snd->outleft == 0)
            {
                if (snd->loop && snd->samples + 16 > snd->loop_end)
                {
                    if (snd->samples < snd->loop_end)
                    {
                        int todo = snd->loop_end - snd->samples;
                        int index = (*snd->ptr >> 4) & 0xf;
                        int pred = (*snd->ptr) & 0xf;

                        int16_t *pred1 = &predictors[pred * 16];

                        RT_Decode8(++snd->ptr, &snd->out[0], index, pred1, snd->lastsmp);
                        snd->ptr += 4;

                        RT_Decode8(snd->ptr, &snd->out[8], index, pred1, snd->lastsmp);
                        snd->ptr += 4;
                        snd->outleft = todo;
                        snd->samples += todo;
                        snd->outptr = 0;
                    }
                    else
                    {
                        if (snd->loop > 0)
                            snd->loop--;
                        Bmemcpy(snd->out, snd->loopstate, 16 * sizeof(int16_t));
                        Bmemcpy(snd->lastsmp, snd->loopstate+8, 8 * sizeof(int16_t));
                        int b = (snd->loop_start / 16) + 1;
                        int s = b * 16 - snd->loop_start;
                        snd->ptr = snd->snd + b * 9;
                        snd->outleft = s;
                        snd->outptr = 16 - s;
                        snd->samples = b * 16;
                    }
                }
                else
                {
                    if (snd->ptr >= snd->snd + snd->rtsound->wave->len)
                    {
                        snd->endofdata = 1;
                        break;
                    }
                    int index = (*snd->ptr >> 4) & 0xf;
                    int pred = (*snd->ptr) & 0xf;

                    int16_t *pred1 = &predictors[pred * 16];

                    RT_Decode8(++snd->ptr, &snd->out[0], index, pred1, snd->lastsmp);
                    snd->ptr += 4;

                    RT_Decode8(snd->ptr, &snd->out[8], index, pred1, snd->lastsmp);
                    snd->ptr += 4;

                    snd->outleft = 16;
                    snd->samples += 16;
                    snd->outptr = 0;
                }
            }
            if (snd->endofdata)
                break;
            snd->buf[i] = snd->out[snd->outptr++];
            snd->outleft--;
        }
        *length = i;
    }
    else
    {
        *length = 0;
    }
}

rt_soundinstance_t *RT_FindSoundSlot(int snum, int id)
{
    if (!soundCtl)
        return nullptr;
    if (snum < 0 || snum >= soundInfo->sound_count)
        return nullptr;
    rt_sound_t *rtsound = soundInfo->sounds[snum];
    for (int i = 0; i < MAXRTSOUNDINSTANCES; i++)
    {
        auto snd = &rt_soundinstance[i];
        if (!snd->status)
        {
            memset(snd, 0, sizeof(rt_soundinstance_t));
            snd->status = 1;
            snd->ptr = snd->snd = g_sounds[snum].ptr;
            snd->rtsound = rtsound;
            snd->id = id;
            if (rtsound->wave->adpcm && rtsound->wave->adpcm->loop)
            {
                snd->loop = rtsound->wave->adpcm->loop->count;
                snd->loop_start = rtsound->wave->adpcm->loop->start;
                snd->loop_end = rtsound->wave->adpcm->loop->end;
            }
            // if (rtsound->wave->raw && rtsound->wave->raw->loop)
            //     snd->loop = rtsound->wave->raw->loop->count;
            return snd;
        }
    }
    return nullptr;
}

void RT_FreeSoundSlot(rt_soundinstance_t *snd)
{
    if (!snd)
        return;

    snd->status = 0;
}

void RT_FreeSoundSlotId(int id)
{
    for (int i = 0; i < MAXSOUNDINSTANCES; i++)
    {
        auto snd = &rt_soundinstance[i];
        if (snd->status && snd->id == id)
            RT_FreeSoundSlot(snd);
    }
}
