# switch.S - context switch between kernel threads
#
# void switch_to(struct context *from, struct context *to);
#   a0 = from, a1 = to
#
# Saves callee-saved registers (ra, sp, s0-s11) into *from, then
# restores them from *to. Layout must match struct context in proc.h,
# with REGBYTES = 8 on riscv64.

	.set REGBYTES, 8			# sizeof(uintptr_t) on riscv64

	.text
	.globl switch_to
switch_to:
	# save current context into *from
	sd	ra, 0 * REGBYTES(a0)		# from->ra
	sd	sp, 1 * REGBYTES(a0)		# from->sp
	sd	s0, 2 * REGBYTES(a0)		# from->s0
	sd	s1, 3 * REGBYTES(a0)		# from->s1
	sd	s2, 4 * REGBYTES(a0)		# from->s2
	sd	s3, 5 * REGBYTES(a0)		# from->s3
	sd	s4, 6 * REGBYTES(a0)		# from->s4
	sd	s5, 7 * REGBYTES(a0)		# from->s5
	sd	s6, 8 * REGBYTES(a0)		# from->s6
	sd	s7, 9 * REGBYTES(a0)		# from->s7
	sd	s8, 10 * REGBYTES(a0)		# from->s8
	sd	s9, 11 * REGBYTES(a0)		# from->s9
	sd	s10, 12 * REGBYTES(a0)		# from->s10
	sd	s11, 13 * REGBYTES(a0)		# from->s11

	# restore next context from *to
	ld	ra, 0 * REGBYTES(a1)		# ra = to->ra (return into next thread)
	ld	s0, 2 * REGBYTES(a1)		# to->s0
	ld	s1, 3 * REGBYTES(a1)		# to->s1
	ld	s2, 4 * REGBYTES(a1)		# to->s2
	ld	s3, 5 * REGBYTES(a1)		# to->s3
	ld	s4, 6 * REGBYTES(a1)		# to->s4
	ld	s5, 7 * REGBYTES(a1)		# to->s5
	ld	s6, 8 * REGBYTES(a1)		# to->s6
	ld	s7, 9 * REGBYTES(a1)		# to->s7
	ld	s8, 10 * REGBYTES(a1)		# to->s8
	ld	s9, 11 * REGBYTES(a1)		# to->s9
	ld	s10, 12 * REGBYTES(a1)		# to->s10
	ld	s11, 13 * REGBYTES(a1)		# to->s11
	ld	sp, 1 * REGBYTES(a1)		# restore sp last, after all memory accesses

	ret					# return address is to->ra
