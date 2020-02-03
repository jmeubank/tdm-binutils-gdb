/* Frame unwinder for frames with DWARF Call Frame Information.

   Copyright (C) 2003-2019 Free Software Foundation, Inc.

   Contributed by Mark Kettenis.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef DWARF2_FRAME_H
#define DWARF2_FRAME_H 1

struct gdbarch;
struct objfile;
struct frame_info;
struct dwarf2_per_cu_data;
struct agent_expr;
struct axs_value;

/* Register rule.  */

enum dwarf2_frame_reg_rule
{
  /* Make certain that 0 maps onto the correct enum value; the
     corresponding structure is being initialized using memset zero.
     This indicates that CFI didn't provide any information at all
     about a register, leaving how to obtain its value totally
     unspecified.  */
  DWARF2_FRAME_REG_UNSPECIFIED = 0,

  /* The term "undefined" comes from the DWARF2 CFI spec which this
     code is moddeling; it indicates that the register's value is
     "undefined".  GCC uses the less formal term "unsaved".  Its
     definition is a combination of REG_UNDEFINED and REG_UNSPECIFIED.
     The failure to differentiate the two helps explain a few problems
     with the CFI generated by GCC.  */
  DWARF2_FRAME_REG_UNDEFINED,
  DWARF2_FRAME_REG_SAVED_OFFSET,
  DWARF2_FRAME_REG_SAVED_REG,
  DWARF2_FRAME_REG_SAVED_EXP,
  DWARF2_FRAME_REG_SAME_VALUE,

  /* These are defined in Dwarf3.  */
  DWARF2_FRAME_REG_SAVED_VAL_OFFSET,
  DWARF2_FRAME_REG_SAVED_VAL_EXP,

  /* These aren't defined by the DWARF2 CFI specification, but are
     used internally by GDB.  */
  DWARF2_FRAME_REG_FN,		/* Call a registered function.  */
  DWARF2_FRAME_REG_RA,		/* Return Address.  */
  DWARF2_FRAME_REG_RA_OFFSET,	/* Return Address with offset.  */
  DWARF2_FRAME_REG_CFA,		/* Call Frame Address.  */
  DWARF2_FRAME_REG_CFA_OFFSET	/* Call Frame Address with offset.  */
};

/* Register state.  */

struct dwarf2_frame_state_reg
{
  /* Each register save state can be described in terms of a CFA slot,
     another register, or a location expression.  */
  union {
    LONGEST offset;
    ULONGEST reg;
    struct
    {
      const gdb_byte *start;
      ULONGEST len;
    } exp;
    struct value *(*fn) (struct frame_info *this_frame, void **this_cache,
			 int regnum);
  } loc;
  enum dwarf2_frame_reg_rule how;
};

enum cfa_how_kind
{
  CFA_UNSET,
  CFA_REG_OFFSET,
  CFA_EXP
};

struct dwarf2_frame_state_reg_info
{
  dwarf2_frame_state_reg_info () = default;
  ~dwarf2_frame_state_reg_info ()
  {
    delete prev;
  }

  /* Copy constructor.  */
  dwarf2_frame_state_reg_info (const dwarf2_frame_state_reg_info &src)
    : reg (src.reg), cfa_offset (src.cfa_offset),
      cfa_reg (src.cfa_reg), cfa_how (src.cfa_how), cfa_exp (src.cfa_exp),
      prev (src.prev)
  {
  }

  /* Assignment operator for both move-assignment and copy-assignment.  */
  dwarf2_frame_state_reg_info&
  operator= (dwarf2_frame_state_reg_info rhs)
  {
    swap (*this, rhs);
    return *this;
  }

  /* Move constructor.  */
  dwarf2_frame_state_reg_info (dwarf2_frame_state_reg_info &&rhs) noexcept
    : reg (std::move (rhs.reg)), cfa_offset (rhs.cfa_offset),
      cfa_reg (rhs.cfa_reg), cfa_how (rhs.cfa_how), cfa_exp (rhs.cfa_exp),
      prev (rhs.prev)
  {
    rhs.prev = nullptr;
  }

  /* If necessary, enlarge the register set to hold NUM_REGS_REQUESTED
     registers.  */
  void alloc_regs (int num_regs_requested)
  {
    gdb_assert (num_regs_requested > 0);

    if (num_regs_requested <= reg.size ())
      return;

    reg.resize (num_regs_requested);
  }

  std::vector<struct dwarf2_frame_state_reg> reg;

  LONGEST cfa_offset = 0;
  ULONGEST cfa_reg = 0;
  enum cfa_how_kind cfa_how = CFA_UNSET;
  const gdb_byte *cfa_exp = NULL;

  /* Used to implement DW_CFA_remember_state.  */
  struct dwarf2_frame_state_reg_info *prev = NULL;

private:
  friend void swap (dwarf2_frame_state_reg_info& lhs,
		    dwarf2_frame_state_reg_info& rhs)
  {
    using std::swap;

    swap (lhs.reg, rhs.reg);
    swap (lhs.cfa_offset, rhs.cfa_offset);
    swap (lhs.cfa_reg, rhs.cfa_reg);
    swap (lhs.cfa_how, rhs.cfa_how);
    swap (lhs.cfa_exp, rhs.cfa_exp);
    swap (lhs.prev, rhs.prev);
  }
};

struct dwarf2_cie;

/* Structure describing a frame state.  */

struct dwarf2_frame_state
{
  dwarf2_frame_state (CORE_ADDR pc, struct dwarf2_cie *cie);

  /* Each register save state can be described in terms of a CFA slot,
     another register, or a location expression.  */
  struct dwarf2_frame_state_reg_info regs {};

  /* The PC described by the current frame state.  */
  CORE_ADDR pc;

  /* Initial register set from the CIE.
     Used to implement DW_CFA_restore.  */
  struct dwarf2_frame_state_reg_info initial {};

  /* The information we care about from the CIE.  */
  const LONGEST data_align;
  const ULONGEST code_align;
  const ULONGEST retaddr_column;

  /* Flags for known producer quirks.  */

  /* The ARM compilers, in DWARF2 mode, assume that DW_CFA_def_cfa
     and DW_CFA_def_cfa_offset takes a factored offset.  */
  bool armcc_cfa_offsets_sf = false;

  /* The ARM compilers, in DWARF2 or DWARF3 mode, may assume that
     the CFA is defined as REG - OFFSET rather than REG + OFFSET.  */
  bool armcc_cfa_offsets_reversed = false;
};

/* When this is true the DWARF frame unwinders can be used if they are
   registered with the gdbarch.  Not all architectures can or do use the
   DWARF unwinders.  Setting this to true on a target that does not
   otherwise support the DWARF unwinders has no effect.  */
extern int dwarf2_frame_unwinders_enabled_p;

/* Set the architecture-specific register state initialization
   function for GDBARCH to INIT_REG.  */

extern void dwarf2_frame_set_init_reg (struct gdbarch *gdbarch,
				       void (*init_reg) (struct gdbarch *, int,
					     struct dwarf2_frame_state_reg *,
					     struct frame_info *));

/* Set the architecture-specific signal trampoline recognition
   function for GDBARCH to SIGNAL_FRAME_P.  */

extern void
  dwarf2_frame_set_signal_frame_p (struct gdbarch *gdbarch,
				   int (*signal_frame_p) (struct gdbarch *,
							  struct frame_info *));

/* Set the architecture-specific adjustment of .eh_frame and .debug_frame
   register numbers.  */

extern void
  dwarf2_frame_set_adjust_regnum (struct gdbarch *gdbarch,
				  int (*adjust_regnum) (struct gdbarch *,
							int, int));

/* Append the DWARF-2 frame unwinders to GDBARCH's list.  */

void dwarf2_append_unwinders (struct gdbarch *gdbarch);

/* Return the frame base methods for the function that contains PC, or
   NULL if it can't be handled by the DWARF CFI frame unwinder.  */

extern const struct frame_base *
  dwarf2_frame_base_sniffer (struct frame_info *this_frame);

/* Compute the DWARF CFA for a frame.  */

CORE_ADDR dwarf2_frame_cfa (struct frame_info *this_frame);

/* Find the CFA information for PC.

   Return 1 if a register is used for the CFA, or 0 if another
   expression is used.  Throw an exception on error.

   GDBARCH is the architecture to use.
   DATA is the per-CU data.

   REGNUM_OUT is an out parameter that is set to the register number.
   OFFSET_OUT is the offset to use from this register.
   These are only filled in when 1 is returned.

   TEXT_OFFSET_OUT, CFA_START_OUT, and CFA_END_OUT describe the CFA
   in other cases.  These are only used when 0 is returned.  */

extern int dwarf2_fetch_cfa_info (struct gdbarch *gdbarch, CORE_ADDR pc,
				  struct dwarf2_per_cu_data *data,
				  int *regnum_out, LONGEST *offset_out,
				  CORE_ADDR *text_offset_out,
				  const gdb_byte **cfa_start_out,
				  const gdb_byte **cfa_end_out);

#endif /* dwarf2-frame.h */
