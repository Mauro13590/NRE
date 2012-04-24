/*
 * TODO comment me
 *
 * Copyright (C) 2012, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL.
 *
 * NUL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#define REGPARMS(X)			__attribute__((regparm(X)))
#define ALIGNED(X)			__attribute__((aligned(X)))
#define PACKED				__attribute__((packed))
#define NORETURN		 	__attribute__((__noreturn__))
#define INIT_PRIORITY(X)    __attribute__((init_priority((X))))
#define WEAK				__attribute__((weak))
