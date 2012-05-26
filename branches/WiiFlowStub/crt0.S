/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

# This code comes from the Twilight Hack
# Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>

# Slightly modified to imitate the homebrew channel stub

*/

	.globl fake_loader
fake_loader:
	b 0x10
	.string "STUBHAXX\0\0\0"

	.globl _start
_start:
	# Disable interrupts
	mfmsr 3 ; rlwinm 3,3,0,17,15 ; mtmsr 3 ; isync

	# Setup stack.
	lis 1,_stack_top@ha ; addi 1,1,_stack_top@l ; li 0,0 ; stwu 0,-64(1)

	# Go!
	bl main
