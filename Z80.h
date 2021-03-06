#pragma once
/*	Copyright  (c)	Günter Woigk 1996 - 2019
					mailto:kio@little-bat.de

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	Permission to use, copy, modify, distribute, and sell this software and
	its documentation for any purpose is hereby granted without fee, provided
	that the above copyright notice appear in all copies and that both that
	copyright notice and this permission notice appear in supporting
	documentation, and that the name of the copyright holder not be used
	in advertising or publicity pertaining to distribution of the software
	without specific, written prior permission.  The copyright holder makes no
	representations about the suitability of this software for any purpose.
	It is provided "as is" without express or implied warranty.

	THE COPYRIGHT HOLDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
	INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
	EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
	CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
	DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
	TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
	PERFORMANCE OF THIS SOFTWARE.

	Z80 cpu emulation version 2.3.0
*/

#include "Item.h"			// note: copy template into your project
#include "Z80options.h"		// note: copy template into your project

#ifndef BYTE_ORDER
# ifdef HAVE_MACHINE_ENDIAN_H
#  include <machine/endian.h>
# else
#  ifdef HAVE_ENDIAN_H
#   include <endian.h>
#  else
#   include <unistd.h>
#  endif
# endif
#endif


#define	CPU_PAGESIZE	(1<<CPU_PAGEBITS)
#define	CPU_PAGEMASK	(CPU_PAGESIZE-1)
#define	CPU_PAGES		(0x10000>>CPU_PAGEBITS)



// ----	helper ----

#if BYTE_ORDER == BIG_ENDIAN  // m68k, ppc
static inline uint8& lowbyte(CoreByte& n) noexcept { return *(((uint8*)&n)+(sizeof(CoreByte)-1)); }
#endif
#if BYTE_ORDER == LITTLE_ENDIAN // i386
static inline uint8& lowbyte(CoreByte& n) noexcept { return (uint8&)n; }
#endif



// ----	memory pages ----

struct PageInfo
{
	CoreByte*	data_r;			// flags in the high bytes, data in the low byte
	CoreByte*	data_w;			// flags in the high bytes, data in the low byte
};


// ----	z80 registers ----

union Z80Registers
{
	uint16 nn[16];
	struct { uint16 af,bc,de,hl, af2,bc2,de2,hl2, ix,iy,pc,sp, iff, ir; };
	#if BYTE_ORDER == BIG_ENDIAN // m68k, ppc
	struct { uint8 a,f,b,c,d,e,h,l, a2,f2,b2,c2,d2,e2,h2,l2, xh,xl,yh,yl,pch,pcl,sph,spl, iff1,iff2, i,r, im,xxx; };
	#endif
	#if BYTE_ORDER == LITTLE_ENDIAN // i386
	struct { uint8 f,a,c,b,e,d,l,h, f2,a2,c2,b2,e2,d2,l2,h2, xl,xh,yl,yh,pcl,pch,spl,sph, iff1,iff2, r,i, im,xxx; };
	#endif
};


class Z80 : public Item
{
public:			// public access to internal data
				// in general FOR READING ONLY!

	CoreByte	noreadpage[CPU_PAGESIZE];	// used for reading from unmapped addresses, default fill value is 0xFF
	CoreByte	nowritepage[CPU_PAGESIZE];	// used for writing to unmapped addresses

	PageInfo	page[CPU_PAGES];	// mapped memory
	Z80Registers registers;			// z80 registers
	int32		cc;					// cpu clock cycle	(inside run() a local variable cc is used!)
	uint		nmi;				// idle=0, flipflop set=1, execution pending=2
	bool		halt;				// HALT instruction executed
	// these are expected to be defined in class Item and reused by the Z80 here to 'collect the state':
	// uint8	int_ack_byte;		// byte read in int ack cycle
	// uint16	int0_call_address;	// mostly unused
	// int32	cc_next_update;		// next time to call update()
	// bool		irpt;				// interrupt asserted

private:
				Z80				(const Z80&);		// prohibit
	Z80&		operator=		(const Z80&);		// prohibit

	void		reset_registers	();
	void		handle_output	(int32 cc, uint a, uint b);
	uint		handle_input	(int32 cc, uint a);
	int32		handle_update	(int32 cc, int32 cc_next);
	void		_init();


public:
	template<class T>			Z80(T t)		:Item(t){ _init(); }
	template<class T,class U>	Z80(T t, U u)	:Item(t,u){ _init(); }
	virtual						~Z80() override;

// Item interface:
	virtual void	init		() override;
	virtual void	reset		(int32 cc) override;
	//virtual bool	input		(int32 cc, uint16 addr, uint8& byte) override;
	//virtual bool	output		(int32 cc, uint16 addr, uint8 byte) override;
	//virtual void	update		(int32) override;
	virtual void	shift_cc	(int32, int32) override;


// Run the Cpu:
	int			run				(int32 cc_exit, uint options=0);

	void		setNmi			() noexcept		{ if(nmi==0) { nmi=3; cc_next_update=cc; } nmi|=1; }
	void		clearNmi		() noexcept		{ nmi &= ~1u; }		// clear FF.set only

	// note: there is no setInterrupt() and clearInterrupt(), because the cpu
	// collects the interrupt state of all attached items in handle_update().


// Map/Unmap memory:
	void		mapNoRom		(uint16 addr, uint16 size, CoreByte* data) noexcept; // map a special noreadpage[]
	void		mapNoWom		(uint16 addr, uint16 size, CoreByte* data) noexcept; // map a special nowritepage[]
	void		mapRom			(uint16 addr, uint16 size, CoreByte* data) noexcept;
	void		mapWom			(uint16 addr, uint16 size, CoreByte* data) noexcept;
	void		mapRam			(uint16 addr, uint16 size, CoreByte* data) noexcept;
	void		mapRam			(uint16 addr, uint16 size, CoreByte* r, CoreByte* w) noexcept;

	void		unmapRom		(uint16 addr, uint16 size) noexcept;
	void		unmapWom		(uint16 addr, uint16 size) noexcept;
	void		unmapRam		(uint16 addr, uint16 size) noexcept;

	void		unmapAllMemory	() noexcept				{ unmapRam(0,0); }
	void		unmapMemory		(CoreByte* a, uint32 size) noexcept;

// Access memory:
	PageInfo&		getPage		(uint16 addr) noexcept			{ return page[addr>>CPU_PAGEBITS]; }
	const PageInfo& getPage		(uint16 addr) const noexcept	{ return page[addr>>CPU_PAGEBITS]; }

	CoreByte*	rdPtr			(uint16 addr) noexcept			{ return getPage(addr).data_r + addr; }
	CoreByte*	wrPtr			(uint16 addr) noexcept			{ return getPage(addr).data_w + addr; }

	uint8		peek			(uint16 addr) const noexcept		{ return lowbyte(getPage(addr).data_r[addr]); }
	void		poke			(uint16 addr, uint8 c) noexcept	{		 lowbyte(getPage(addr).data_w[addr]) = c; }
	uint16		peek2			(uint16 addr) const noexcept;
	void		poke2			(uint16 addr, uint16 n) noexcept;
	uint16		pop2			() noexcept;
	void		push2			(uint16 n) noexcept;

	void		copyBufferToRam	(uint8 const* q, uint16 z_addr, uint16 cnt) noexcept;
	void		copyRamToBuffer	(uint16 q_addr, uint8* z, uint16 cnt) noexcept;
	void		copyBufferToRam	(const CoreByte* q, uint16 z_addr, uint16 cnt) noexcept;
	void		copyRamToBuffer	(uint16 q_addr, CoreByte *z, uint16 cnt) noexcept;
	void		readRamFromFile	(class FD& fd, uint16 z_addr, uint16 cnt);
	void		writeRamToFile	(class FD& fd, uint16 q_addr, uint16 cnt);


static inline void c2b(CoreByte const* q, uint8*    z, uint n) noexcept { for(uint i=0; i<n; i++) z[i] = q[i]; }
static inline void b2c(uint8    const* q, CoreByte* z, uint n) noexcept { for(uint i=0; i<n; i++) lowbyte(z[i]) = q[i]; }
static inline void c2c(CoreByte const* q, CoreByte* z, uint n) noexcept { for(uint i=0; i<n; i++) lowbyte(z[i]) = q[i]; }
};



















