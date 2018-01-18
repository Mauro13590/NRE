/*
 * Copyright (C) 2012, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NRE (NOVA runtime environment).
 *
 * NRE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NRE is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <mem/DataSpace.h>
#include <util/Profiler.h>
#include <util/Util.h>

#include "DataSpaceTest.h"

using namespace nre;
using namespace nre::test;

static const size_t DS_SIZE     = ExecEnv::PAGE_SIZE;
static const size_t MAP_COUNT   = 10000;

static void test_ds();

const TestCase dstest = {
    "DataSpace performance", test_ds
};
static uint64_t alloc_times[MAP_COUNT];
static uint64_t delete_times[MAP_COUNT];

static void test_ds() {
    DataSpaceDesc *desc1, *desc2, *desc3;
    uint64_t tic, tac, heap_alloc, heap_delete;

    // measure malloc time
    {
        Profiler prof;
        prof.start();
        desc1 = new DataSpaceDesc();
        desc2 = new DataSpaceDesc();
        desc3 = new DataSpaceDesc();
        heap_alloc = prof.stop() / 3 - prof.rdtsc();
    }

    // measure free time
    {
        Profiler prof;
        prof.start();
        delete desc3;
        delete desc2;
        delete desc1;
        heap_delete = prof.stop() / 3 - prof.rdtsc();
    }

    Profiler prof;
    for(size_t i = 0; i < MAP_COUNT; ++i) {
        tic = Util::tsc();
        DataSpace *ds = new DataSpace(DS_SIZE, DataSpaceDesc::ANONYMOUS, DataSpaceDesc::RW);
        tac = Util::tsc();
        alloc_times[i] = tac - tic - prof.rdtsc() - heap_alloc;

        tic = Util::tsc();
        delete ds;
        tac = Util::tsc();
        delete_times[i] = tac - tic - prof.rdtsc() - heap_delete;
    }

    uint64_t alloc_avg = 0;
    uint64_t delete_avg = 0;
    for(size_t i = 0; i < MAP_COUNT; i++) {
        alloc_avg += alloc_times[i];
        delete_avg += delete_times[i];
    }

    alloc_avg = alloc_avg / MAP_COUNT;
    delete_avg = delete_avg / MAP_COUNT;
    WVPERF(heap_alloc, "cycles");
    WVPERF(heap_delete, "cycles");
    WVPERF(alloc_avg, "cycles");
    WVPERF(delete_avg, "cycles");
}
