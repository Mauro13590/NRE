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

#include <region/RegionManager.h>
#include <util/ScopedPtr.h>

#include "RegMngTest.h"

using namespace nre;
using namespace nre::test;

static void test_regmng();

const TestCase regmng = {
    "RegionManager", test_regmng
};

void test_regmng() {
    uintptr_t addr1, addr2, addr3;
    {
        ScopedPtr<RegionManager<>> rm(new RegionManager<>());
        rm->free(0x100000, 0x3000);
        rm->free(0x200000, 0x1000);
        rm->free(0x280000, 0x2000);

        addr1 = rm->alloc(0x1000);
        addr2 = rm->alloc(0x2000);
        addr3 = rm->alloc(0x2000);

        rm->free(addr1, 0x1000);
        rm->free(addr2, 0x2000);
        rm->free(addr3, 0x2000);

        WVPASSEQ(rm->total_count(), static_cast<size_t>(0x6000));
        auto it = rm->begin();
        WVPASSEQ(it->addr, static_cast<uintptr_t>(0x200000));
        WVPASSEQ(it->size, static_cast<size_t>(0x1000));
        ++it;
        WVPASSEQ(it->addr, static_cast<uintptr_t>(0x100000));
        WVPASSEQ(it->size, static_cast<size_t>(0x3000));
        ++it;
        WVPASSEQ(it->addr, static_cast<uintptr_t>(0x280000));
        WVPASSEQ(it->size, static_cast<size_t>(0x2000));
    }

    {
        ScopedPtr<RegionManager<>> rm(new RegionManager<>());
        rm->free(0x100000, 0x3000);
        rm->free(0x200000, 0x1000);
        rm->free(0x280000, 0x2000);

        addr1 = rm->alloc(0x1000);
        addr2 = rm->alloc(0x1000);
        addr3 = rm->alloc(0x1000);

        rm->free(addr1, 0x1000);
        rm->free(addr3, 0x1000);
        rm->free(addr2, 0x1000);

        auto it = rm->begin();
        WVPASSEQ(it->addr, static_cast<uintptr_t>(0x200000));
        WVPASSEQ(it->size, static_cast<size_t>(0x1000));
        ++it;
        WVPASSEQ(it->addr, static_cast<uintptr_t>(0x280000));
        WVPASSEQ(it->size, static_cast<size_t>(0x2000));
        ++it;
        WVPASSEQ(it->addr, static_cast<uintptr_t>(0x100000));
        WVPASSEQ(it->size, static_cast<size_t>(0x3000));
    }
}
