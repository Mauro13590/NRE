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

#include <kobj/GlobalThread.h>
#include <util/Profiler.h>
#include <CPU.h>

#include "ThreadsTest.h"

using namespace nre;
using namespace nre::test;

static void test_threads();

const TestCase threads = {
    "Threads", test_threads,
};
static const size_t TEST_COUNT = 10;
int value = TEST_COUNT;
int cpu = 0;

static void dummy(void*) {
    WVPRINT("GlobalThread_"<<value);
    value--;
    while(1);
}

static void test_threads() {
    static Reference<GlobalThread> threads[TEST_COUNT];
    AvgProfiler prof(TEST_COUNT);
    for(size_t i = 0; i < TEST_COUNT; ++i) {
        prof.start();
        threads[i] = GlobalThread::create(dummy, cpu, "GlobalThreadTest");
        threads[i]->start();
        cpu++;
        if(cpu == CPU::count()) cpu = 0;
        prof.stop();
    }

    WVPERF(prof.avg(), "cycles for thread creation");
    WVPRINT("min: " << prof.min());
    WVPRINT("max: " << prof.max());

    for(size_t i = 0; i < TEST_COUNT; ++i)
        threads[i] = Reference<GlobalThread>();

}
