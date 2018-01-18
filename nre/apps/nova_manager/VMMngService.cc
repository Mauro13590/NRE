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

#include <services/Network.h>

#include "VMMngService.h"

using namespace nre;

VMMngService *VMMngService::_inst = nullptr;

void VMMngService::portal(VMMngServiceSession *sess) {
    nre::UtcbFrameRef uf;
    try {
        VMManager::Command cmd;
        uf >> cmd;

        switch(cmd) {
            case VMManager::INIT: {
                capsel_t dssel = uf.get_delegated(0).offset();
                capsel_t smsel = uf.get_delegated(0).offset();
                capsel_t pdsel = uf.get_translated(0).offset();
                uf.finish_input();

                sess->init(new nre::DataSpace(dssel), new Sm(smsel, false), pdsel);
                uf.accept_delegates();
                uf << E_SUCCESS;
                break;
            }

            case VMManager::GEN_MAC: {
                uf.finish_input();
                Network::EthernetAddr addr(BASE_MAC | (sess->id() << 16) | sess->request_mac());
                uf << E_SUCCESS << addr;
                break;
            }
        }
    }
    catch(const nre::Exception &e) {
        nre::Syscalls::revoke(uf.delegation_window(), true);
        uf.clear();
        uf << e;
    }
}
