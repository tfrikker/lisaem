/**************************************************************************************\
*                                                                                      *
*              The Lisa Emulator Project  V1.2.7      DEV 2021.03.26                   *
*                             http://lisaem.sunder.net                                 *
*                                                                                      *
*                  Copyright (C) 1998, 2021 Ray A. Arachelian                          *
*                                All Rights Reserved                                   *
*                                                                                      *
*           This program is free software; you can redistribute it and/or              *
*           modify it under the terms of the GNU General Public License                *
*           as published by the Free Software Foundation; either version 2             *
*           of the License, or (at your option) any later version.                     *
*                                                                                      *
*           This program is distributed in the hope that it will be useful,            *
*           but WITHOUT ANY WARRANTY; without even the implied warranty of             *
*           MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
*           GNU General Public License for more details.                               *
*                                                                                      *
*           You should have received a copy of the GNU General Public License          *
*           along with this program;  if not, write to the Free Software               *
*           Foundation, Inc., 59 Temple Place #330, Boston, MA 02111-1307, USA.        *
*                                                                                      *
*                   or visit: http://www.gnu.org/licenses/gpl.html                     *
*                                                                                      *
*                                                                                      *
*           High Level Emulation and hacks for Profile Hard Disk Routines              *
*     Interfacing code can be found reg68k.c, rom.c, romless.c, and profile.c          *
*                                                                                      *
\**************************************************************************************/

#include <generator.h>
#include <registers.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <reg68k.h>
#include <cpu68k.h>
#include <ui.h>
#include <vars.h>


void uniplus_set_partition_table_size(uint32 disk, uint32 sswap, uint32 rroot, int kernel)
{
   uint32 astart, bstart, cstart, dstart, estart, fstart, gstart, hstart,
          a_size, b_size, c_size, d_size, e_size, f_size, g_size, h_size;

   uint32 swap   = 2400;  // default swap for 5MB/10MB ProFile. Corvus=3959, Priam=4000 (these are physical disk geometry related)
   uint32 root   = 16955; // default root for 10MB+ disks
// uint32 disk   = 19456;
   uint32 save   = 0;
   uint32 max=disk;
   uint32 addr=0;

   if (disk==9728 || disk==19456) return; // default 5mb or 10mb profile, don't change table

   // are we patching kernel v1.1 ()/sunix), or 1.4 ()/unix ?
   if (kernel==11) addr=0x00024ddc;
   if (kernel==14) addr=0x00026b54;
   if (!addr) return;

   // only apply patches if the kernel hasn't already been patched.
   // if the kernel has already been patched, assume it came to LisaEm
   // via BLU and that the image came from a drive larger than 10MB
   
   int i=0, flag=0;

   if (fetchlong(addr+0+i)!=0x000009c5) {flag=1;}   if (fetchlong(addr+4+i)!=0x0000423b) {flag=1;}    i+=8; 
   if (fetchlong(addr+0+i)!=0x00000065) {flag=1;}   if (fetchlong(addr+4+i)!=0x00000960) {flag=1;}    i+=8;
   if (fetchlong(addr+0+i)!=0x000009c5) {flag=1;}   if (fetchlong(addr+4+i)!=0x00001c3b) {flag=1;}    i+=8;
   if (fetchlong(addr+0+i)!=0x00002600) {flag=1;}   if (fetchlong(addr+4+i)!=0x00002600) {flag=1;}    i+=8; 
   if (fetchlong(addr+0+i)!=0x00000000) {flag=1;}   if (fetchlong(addr+4+i)!=0x00000000) {flag=1;}    i+=8; 
   if (fetchlong(addr+0+i)!=0x00000000) {flag=1;}   if (fetchlong(addr+4+i)!=0x00001c00) {flag=1;}    i+=8;
   if (fetchlong(addr+0+i)!=0x00001c00) {flag=1;}   if (fetchlong(addr+4+i)!=0x000009c0) {flag=1;}    i+=8; 
   if (fetchlong(addr+0+i)!=0x00000065) {flag=1;}   if (fetchlong(addr+4+i)!=0x00004b9b) {flag=1;}    i+=8; 

   if (flag) return; // kernel was patched already.


   // use defaults or passed params?
   if (rroot==0xffffffff || rroot==0) root=disk-swap; else root=rroot;
   if (sswap) swap=sswap;

   astart =  101+swap; a_size =   root;  // root on 10MB disk
   bstart =  101;      b_size =   swap;  // swap (2400 blks normal
   cstart =    0;      c_size =       0; // root on 5MB disk

   dstart = astart+a_size; 
   d_size = max-dstart;              //  2nd fs on 10MB disk
   if (!d_size || (d_size & 0x80000000)) {dstart=0; d_size=0;}

   estart =    0;      e_size =       0; //  unused
   fstart =    0;      f_size =       0; //  old root - a
   gstart =    0;      g_size =       0; //  old swap -b
   hstart =  101;      h_size = max-101; //  whole disk (blocks 0-100 reserved for boot loader)

   storelong(addr + 0 ,astart);    storelong(addr + 4 ,a_size);  addr+=8;
   storelong(addr + 0 ,bstart);    storelong(addr + 4 ,b_size);  addr+=8;
   storelong(addr + 0 ,cstart);    storelong(addr + 4 ,c_size);  addr+=8;
   storelong(addr + 0 ,dstart);    storelong(addr + 4 ,d_size);  addr+=8;
   storelong(addr + 0 ,estart);    storelong(addr + 4 ,e_size);  addr+=8;
   storelong(addr + 0 ,fstart);    storelong(addr + 4 ,f_size);  addr+=8;
   storelong(addr + 0 ,gstart);    storelong(addr + 4 ,g_size);  addr+=8;
   storelong(addr + 0 ,hstart);    storelong(addr + 4 ,h_size);  addr+=8;
}


void  hle_intercept(void) {
      ALERT_LOG(0,".");
      check_running_lisa_os();

      ProFileType *P=NULL;
      uint32 a4=reg68k_regs[8+4]; // get a4 to use as the target for copying data to
      uint32 a5=reg68k_regs[8+5];
      long size=0;

      if  (reg68k_pc==0x00fe0090 || reg68k_pc==rom_profile_read_entry)
          {ALERT_LOG(0,"profile.c:state:hle - boot rom read pc:%08x",reg68k_pc); romless_entry(); return;} // Boot ROM ProFile Read HLE

      ALERT_LOG(0,"running os: %d, want:%d pc:%08x a4:%08x, a5:%08x",running_lisa_os,LISA_UNIPLUS_RUNNING,reg68k_pc,a4,a5);
      if (running_lisa_os==LISA_UNIPLUS_RUNNING) {
          int vianum=(  (((a5>>0xe)&3)<<1) | (((5>>0xb)&1)^1)  )+ 3;
          if ((reg68k_regs[8+5] & 0x00ffff00)==0x00fcd900) vianum=2;
          if (vianum>1 && vianum<9) P=via[vianum].ProFile;

          ALERT_LOG(0,"running os: %d pc:%08x a4:%08x, a5:%08x via:%d profile :%p",running_lisa_os,reg68k_pc,a4,a5,vianum,P);

          if (P)
          {
             switch(reg68k_pc) {
               case 0x00020c64: size=  4; reg68k_pc=0x00020c74; ALERT_LOG(0,"profile.c:hle:read status");
                                          P->indexread=0;                                   
                                          P->indexwrite=4;
                                          P->DataBlock[0]=0;
                                          P->DataBlock[1]=0;
                                          P->DataBlock[2]=0;
                                          P->DataBlock[3]=0;
                                          P->BSYLine=0;
                                          P->StateMachineStep=12;
                                          break; // read 4 status bytes into A4, increase a4+=4, PC=00020c74
  
               case 0x00020d1e: size= 20; reg68k_pc=0x00020d26; ALERT_LOG(0,"profile.c:state:hle:read tags");   break; // read 20 bytes of tags into *a4, increase a4+=20, D5= 0x0000ffff PC=00020d26
               case 0x00020d3e: size=512; reg68k_pc=0x00020d72; ALERT_LOG(0,"profile.c:state:hle:read sector"); break; // read 512 bytes of data into *a4, a4+=512 D5=0x0000ffff PC=0x00020d72
    
               // write tag/sector specific
               case 0x00020ebc:           reg68k_pc=0x00020ef0; ALERT_LOG(0,"profile.c:state:hle:write sector+tags");  // write tags and data in one shot, then return to 0x00020ef0
                                size=reg68k_regs[5]; // d5
                                a4=reg68k_regs[8+4];
                                while(size--) {
                                                 if (P->indexwrite>542) {ALERT_LOG(0,"ProFile buffer overrun!"); P->indexwrite=4;}
                                                 P->DataBlock[P->indexwrite++]=fetchbyte(a4++);
                                              }
    
                                return; // we're done, so return.
    
               default: ALERT_LOG(0,"Unknown F-Line error: PC:%08x",reg68k_pc); 
                        return;
             }
    
             // fall through common code for read status, tag, data
    
             reg68k_regs[8+4]+=size;     // final A4 value to return to UniPlus.
             regs.pc=pc24=reg68k_pc;
             reg68k_regs[5]=0x0000ffff;  // D5 is done in dbra loop for all 3 cases, mark it with -1 as done.
    
             while(size--) {
               uint8 r=P->DataBlock[P->indexread++];
               DEBUG_LOG(0,"profile hle:%02x to %08x from index:%d state:%d",r,a4,P->indexread-1,P->StateMachineStep);
               lisa_wb_ram(a4++,r);}

             ALERT_LOG(0,"returning to: %08x, a4:%08x,%08x",reg68k_pc, reg68k_regs[8+4],a4);
             return;
          }
      }
    
}    


// UniPlus's kernel does a test call to a 68010 MOVEC opcode to cause an Illegal instruction 
// exception on 680000 on purpose. This is used to test if it's running on a 68010. Using this
// as a UniPlus detector in order to patch it.
// Tried to patch the OS when it accesses the profile, however that happens too late and UniPlus
// ASSERT still gets triggered.  2021.03.06.
//
// I suspect there is 68010 support in the UniPlus kernel, but this is not evidence that the
// Lisa ever actually used an 010, but rather this is for cross system compatibility with other
// m68k systems that UniPlus ported to. I believe this is the case because I also randomly see
// some drivers use very exacting CPU timing delays which either use DBRA loops when interfacing
// with hardware or use NOPs. An 010 in a physical Lisa would throw off these time delay loops,
// and certainly when James Denton tried to use an 010 in a Lisa, one of the first things to fail
// was the very tight timing loop in the ROM that fetches the serial number of the Lisa. A similar
// one is used in uniplus boot loader, which also fails with "Can't get serial number." or such.

void apply_uniplus_hacks(void)
{
  ALERT_LOG(0,".");
  check_running_lisa_os();
    if (running_lisa_os==0) // this is zero when the 68010 opcode is executed, becuse the kernel isn't yet done initializing.
     {
       uint8 r=lisa_rb_ram(0x00020f9c); // check to see that we're on uniplus v1.4 or 1.1 or some other thing.

       ALERT_LOG(0,"**** UniPlus bootstrap - MOVEC detected, r=%02x running_lisa_os =%d ****",r,running_lisa_os);

           #ifdef DEBUG
           debug_on("uniplus");
           //find_uniplus_partition_table();
           #endif

       DEBUG_LOG(0,"**** UniPlus bootstrap - MOVEC detected, r=%02x running_lisa_os =%d ****",r,running_lisa_os);

      //v1.1 /sunix patch
      if (r==0x60)
      {
          ALERT_LOG(0,"Patching for UniPlus v1.1 sunix");
          ALERT_LOG(0,"Patching for UniPlus v1.1 sunix");
          ALERT_LOG(0,"Patching for UniPlus v1.1 sunix");
          ALERT_LOG(0,"Patching for UniPlus v1.1 sunix");

          lisa_wb_ram(0x0001fe24,0x60); // skip assert BSY 1
          lisa_wb_ram(0x0001ff38,0x01); // increase timeout to ludicrous length

          uniplus_hacks=0; // turn off short-circuit logic-AND flag
      }

      // v1.4 /unix patch
       if (r==0x67) { // v1.4 this checks to ensure that the pro.c driver is loaded before we modify it. Likely not necessary.
           ALERT_LOG(0,"Patching for UNIPLUS v1.4");
           ALERT_LOG(0,"Patching for UNIPLUS v1.4");
           ALERT_LOG(0,"Patching for UNIPLUS v1.4");
           ALERT_LOG(0,"Patching for UNIPLUS v1.4");

           // these two are needed to pass handshaking in Uni+ with our shitty profile emulation
           lisa_wb_ram(0x00020f9c,0x60);   // skip assert BSY
           lisa_wb_ram(0x000210b0,0x01);   // increase timeout to ludicrous length

           // these are optional for HLE acceleration of ProFile reads/writes
           if  (hle) {
               ALERT_LOG(0,"Patching for UNIPLUS v1.4 HLE");
               ALERT_LOG(0,"Patching for UNIPLUS v1.4 HLE");
               ALERT_LOG(0,"Patching for UNIPLUS v1.4 HLE");
               ALERT_LOG(0,"Patching for UNIPLUS v1.4 HLE");

               lisa_ww_ram(0x00020c64,0xf33d); // read 4 status bytes into A4, increase a4+=4, PC=00020c74
               lisa_ww_ram(0x00020d1e,0xf33d); // read 20 bytes of tags into *a4, increase a4+=20, D5= 0x0000ffff PC=00020d26
               lisa_ww_ram(0x00020d3e,0xf33d); // read 512 bytes of data into *a4, a4+=512 D5=0x0000ffff PC=0x00020d72
               lisa_ww_ram(0x00020ebc,0xf33d); // wrtite 512+20 bytes of data+tags in one shot. into *a4, a4+=512 D5=0x0000ffff PC=0x00020ef0
           }
           uniplus_hacks=0; // turn off short-circuit logic-AND flag
       }
     }

     else

     {
       if (running_lisa_os>1)  uniplus_hacks=0; // turn off short-circuit logic-AND flag if OS is not uniplus
     }

}

// return 1 will instruct the interrupt to code to skip processing, return 0 will wind up throwin an F-line exception
// back to the guest OS as usual.
int line15_hle_intercept(void) 
{ 
      uint16 opcode,fn;


      if (romless && (reg68k_pc & 0x00ff0000)==0x00fe0000)
      {
        if (romless_entry()) return 1;
      }

      abort_opcode=2; opcode=fetchword(reg68k_pc);          //if (abort_opcode==1) fn=0xffff;
      abort_opcode=2; fn=fetchword(reg68k_pc+2);            //if (abort_opcode==1) fn=0xffff;
      abort_opcode=0;

      ALERT_LOG(0,"Possible HLE call at %08x opcode:%04x",reg68k_pc,opcode);

      if (opcode == 0xf33d) {hle_intercept(); return 1;}

      if (opcode == 0xfeed)
      {
        ALERT_LOG(0,"F-Line-Wormhole %04lx",(long)fn);
        switch(fn)
        {
          #ifdef DEBUG

            case 0:   if (debug_log_enabled) { ALERT_LOG(0,"->debug_off");                     }
                      else                   { ALERT_LOG(0,"->debug_off, was already off!!!"); }
                      debug_off(); reg68k_pc+=4; return 1;

            case 1:   if (debug_log_enabled) { ALERT_LOG(0,"->tracelog, was already on! turning off!"); debug_off(); }
                      else                   { ALERT_LOG(0,"->tracelog on");            debug_on("F-line-wormhole"); }
                      reg68k_pc+=4; return 1; 
          #else

            case 0:
            case 1:   reg68k_pc+=4; 
                      ALERT_LOG(0,"tracelog -> but emulator wasn't compiled in with debugging enabled!"); 
                      return 1;
          #endif
          default: ALERT_LOG(0,"Unknown F-Line-wormhole:%04x or actual F-Line trap, falling through a LINE15 exception will occur.",fn);
          // anything unimplemented/undefined should fall through and generate an exception.
        }
      }
      if (opcode==0xfeef) {
                              ALERT_LOG(0,"Executing blank IPC @ %08lx",(long)reg68k_pc);
                              EXITR(99,0,"EEEK! Trying to execute blank IPC at %08lx - something is horribly wrong!",(long)reg68k_pc);
                          }
      ALERT_LOG(0,"Unhandled F-Line %04lx at %08lx",(long)opcode,(long)reg68k_pc);
      return 0;
}
