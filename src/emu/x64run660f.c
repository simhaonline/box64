#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "debug.h"
#include "box64stack.h"
#include "x64emu.h"
#include "x64run.h"
#include "x64emu_private.h"
#include "x64run_private.h"
#include "x64primop.h"
#include "x64trace.h"
#include "x87emu_private.h"
#include "box64context.h"
#include "bridge.h"
//#include "signals.h"
#ifdef DYNAREC
#include "../dynarec/arm_lock_helper.h"
#endif

#include "modrm.h"

int Run660F(x64emu_t *emu, rex_t rex)
{
    uint8_t opcode;
    uint8_t nextop;
    uint8_t tmp8u;
    uint16_t tmp16u;
    uint64_t tmp64u;
    reg64_t *oped, *opgd;
    sse_regs_t *opex, *opgx;
    mmx87_regs_t *opem;

    opcode = F8;

    switch(opcode) {

    case 0x10:                      /* MOVUPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        memcpy(GX, EX, 16); // unaligned...
        break;
    case 0x11:                      /* MOVUPD Ex, Gx */
        nextop = F8;
        GETEX(0);
        GETGX;
        memcpy(EX, GX, 16); // unaligned...
        break;
    case 0x12:                      /* MOVLPD Gx, Eq */
        nextop = F8;
        GETED(0);
        GETGX;
        GX->q[0] = ED->q[0];
        break;
    case 0x13:                      /* MOVLPD Eq, Gx */
        nextop = F8;
        GETED(0);
        GETGX;
        ED->q[0] = GX->q[0];
        break;
    case 0x14:                      /* UNPCKLPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->q[1] = EX->q[0];
        break;
    case 0x15:                      /* UNPCKHPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->q[0] = GX->q[1];
        GX->q[1] = EX->q[1];
        break;

    case 0x1F:                      /* NOP (multi-byte) */
        nextop = F8;
        GETED(0);
        break;

    case 0x28:                      /* MOVAPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->q[0] = EX->q[0];
        GX->q[1] = EX->q[1];
        break;
    case 0x29:                      /* MOVAPD Ex, Gx */
        nextop = F8;
        GETEX(0);
        GETGX;
        EX->q[0] = GX->q[0];
        EX->q[1] = GX->q[1];
        break;
    case 0x2A:                      /* CVTPI2PD Gx, Em */
        nextop = F8;
        GETEM(0);
        GETGX;
        GX->d[0] = EM->sd[0];
        GX->d[1] = EM->sd[1];
        break;

    case 0x2E:                      /* UCOMISD Gx, Ex */
        // no special check...
    case 0x2F:                      /* COMISD Gx, Ex */
        RESET_FLAGS(emu);
        nextop = F8;
        GETEX(0);
        GETGX;
        if(isnan(GX->d[0]) || isnan(EX->d[0])) {
            SET_FLAG(F_ZF); SET_FLAG(F_PF); SET_FLAG(F_CF);
        } else if(isgreater(GX->d[0], EX->d[0])) {
            CLEAR_FLAG(F_ZF); CLEAR_FLAG(F_PF); CLEAR_FLAG(F_CF);
        } else if(isless(GX->d[0], EX->d[0])) {
            CLEAR_FLAG(F_ZF); CLEAR_FLAG(F_PF); SET_FLAG(F_CF);
        } else {
            SET_FLAG(F_ZF); CLEAR_FLAG(F_PF); CLEAR_FLAG(F_CF);
        }
        CLEAR_FLAG(F_OF); CLEAR_FLAG(F_AF); CLEAR_FLAG(F_SF);
        break;

    GOCOND(0x40
        , nextop = F8;
        CHECK_FLAGS(emu);
        GETEW(0);
        GETGW;
        , if(rex.w) GW->q[0] = EW->q[0]; else GW->word[0] = EW->word[0];
        ,
    )                               /* 0x40 -> 0x4F CMOVxx Gw,Ew */ // conditional move, no sign

    case 0x54:                      /* ANDPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->q[0] &= EX->q[0];
        GX->q[1] &= EX->q[1];
        break;
    case 0x55:                      /* ANDNPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->q[0] = (~GX->q[0]) & EX->q[0];
        GX->q[1] = (~GX->q[1]) & EX->q[1];
        break;
    case 0x56:                      /* ORPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->q[0] |= EX->q[0];
        GX->q[1] |= EX->q[1];
        break;
    case 0x57:                      /* XORPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->q[0] ^= EX->q[0];
        GX->q[1] ^= EX->q[1];
        break;
    case 0x58:                      /* ADDPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->d[0] += EX->d[0];
        GX->d[1] += EX->d[1];
        break;
    case 0x59:                      /* MULPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->d[0] *= EX->d[0];
        GX->d[1] *= EX->d[1];
        break;
    case 0x5A:                      /* CVTPD2PS Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->f[0] = EX->d[0];
        GX->f[1] = EX->d[1];
        GX->q[1] = 0;
        break;
    case 0x5B:                      /* CVTPS2DQ Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->sd[0] = EX->f[0];
        GX->sd[1] = EX->f[1];
        GX->sd[2] = EX->f[2];
        GX->sd[3] = EX->f[3];
        break;
    case 0x5C:                      /* SUBPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->d[0] -= EX->d[0];
        GX->d[1] -= EX->d[1];
        break;
    case 0x5D:                      /* MINPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        if (isnan(GX->d[0]) || isnan(EX->d[0]) || isless(EX->d[0], GX->d[0]))
            GX->d[0] = EX->d[0];
        if (isnan(GX->d[1]) || isnan(EX->d[1]) || isless(EX->d[1], GX->d[1]))
            GX->d[1] = EX->d[1];
        break;
    case 0x5E:                      /* DIVPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->d[0] /= EX->d[0];
        GX->d[1] /= EX->d[1];
        break;
    case 0x5F:                      /* MAXPD Gx, Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        if (isnan(GX->d[0]) || isnan(EX->d[0]) || isgreater(EX->d[0], GX->d[0]))
            GX->d[0] = EX->d[0];
        if (isnan(GX->d[1]) || isnan(EX->d[1]) || isgreater(EX->d[1], GX->d[1]))
            GX->d[1] = EX->d[1];
        break;
        
    case 0x62:  /* PUNPCKLDQ Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->ud[3] = EX->ud[1];
        GX->ud[2] = GX->ud[1];
        GX->ud[1] = EX->ud[0];
        break;
    case 0x63:  /* PACKSSWB Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        if(GX==EX) {
            for(int i=0; i<8; ++i)
                GX->sb[i] = (EX->sw[i]<-128)?-128:((EX->sw[i]>127)?127:EX->sw[i]);
            GX->q[1] = GX->q[0];
        } else {
            for(int i=0; i<8; ++i)
                GX->sb[i] = (GX->sw[i]<-128)?-128:((GX->sw[i]>127)?127:GX->sw[i]);
            for(int i=0; i<8; ++i)
                GX->sb[8+i] = (EX->sw[i]<-128)?-128:((EX->sw[i]>127)?127:EX->sw[i]);
        }
        break;
    case 0x64:  /* PCMPGTB Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        for(int i=0; i<16; ++i)
            GX->ub[i] = (GX->sb[i]>EX->sb[i])?0xFF:0x00;
        break;
    case 0x65:  /* PCMPGTW Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        for(int i=0; i<8; ++i)
            GX->uw[i] = (GX->sw[i]>EX->sw[i])?0xFFFF:0x0000;
        break;
    case 0x66:  /* PCMPGTD Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        for(int i=0; i<4; ++i)
            GX->ud[i] = (GX->sd[i]>EX->sd[i])?0xFFFFFFFF:0x00000000;
        break;
    case 0x67:  /* PACKUSWB Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        if(GX==EX) {
            for(int i=0; i<8; ++i)
                GX->ub[i] = (EX->sw[i]<0)?0:((EX->sw[i]>0xff)?0xff:EX->sw[i]);
            GX->q[1] = GX->q[0];
        } else {
            for(int i=0; i<8; ++i)
                GX->ub[i] = (GX->sw[i]<0)?0:((GX->sw[i]>0xff)?0xff:GX->sw[i]);
            for(int i=0; i<8; ++i)
                GX->ub[8+i] = (EX->sw[i]<0)?0:((EX->sw[i]>0xff)?0xff:EX->sw[i]);
        }
        break;
    case 0x68:  /* PUNPCKHBW Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        for(int i=0; i<8; ++i)
            GX->ub[2 * i] = GX->ub[i + 8];
        if(GX==EX)
            for(int i=0; i<8; ++i)
                GX->ub[2 * i + 1] = GX->ub[2 * i];
        else
            for(int i=0; i<8; ++i)
                GX->ub[2 * i + 1] = EX->ub[i + 8];
        break;
    case 0x69:  /* PUNPCKHWD Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        for(int i=0; i<4; ++i)
            GX->uw[2 * i] = GX->uw[i + 4];
        if(GX==EX)
            for(int i=0; i<4; ++i)
                GX->uw[2 * i + 1] = GX->uw[2 * i];
        else
            for(int i=0; i<4; ++i)
                GX->uw[2 * i + 1] = EX->uw[i + 4];
        break;
    case 0x6A:  /* PUNPCKHDQ Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        // no copy needed if GX==EX
        GX->ud[0] = GX->ud[2];
        GX->ud[1] = EX->ud[2];
        GX->ud[2] = GX->ud[3];
        GX->ud[3] = EX->ud[3];
        break;
    case 0x6B:  /* PACKSSDW Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        if(GX==EX) {
            for(int i=0; i<4; ++i)
                GX->sw[i] = (EX->sd[i]<-32768)?-32768:((EX->sd[i]>32767)?32767:EX->sd[i]);
            GX->q[1] = GX->q[0];
        } else {
            for(int i=0; i<4; ++i)
                GX->sw[i] = (GX->sd[i]<-32768)?-32768:((GX->sd[i]>32767)?32767:GX->sd[i]);
            for(int i=0; i<4; ++i)
                GX->sw[4+i] = (EX->sd[i]<-32768)?-32768:((EX->sd[i]>32767)?32767:EX->sd[i]);
        }
        break;
    case 0x6C:  /* PUNPCKLQDQ Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->q[1] = EX->q[0];
        break;
    case 0x6D:  /* PUNPCKHQDQ Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->q[0] = GX->q[1];
        GX->q[1] = EX->q[1];
        break;
    case 0x6E:                      /* MOVD Gx, Ed */
        nextop = F8;
        GETED(0);
        GETGX;
        if(rex.w)
            GX->q[0] = ED->q[0];
        else
            GX->q[0] = ED->dword[0];
        GX->q[1] = 0;
        break;
    case 0x6F:                      /* MOVDQA Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->q[0] = EX->q[0];
        GX->q[1] = EX->q[1];
        break;

    case 0x7E:                      /* MOVD Ed, Gx */
        nextop = F8;
        GETED(0);
        GETGX;
        if(rex.w)
            ED->q[0] = GX->q[0];
        else {
            if(MODREG)
                ED->q[0] = GX->ud[0];
            else
                ED->dword[0] = GX->ud[0];
        }
        break;
    case 0x7F:  /* MOVDQA Ex,Gx */
        nextop = F8;
        GETEX(0);
        GETGX;
        EX->q[0] = GX->q[0];
        EX->q[1] = GX->q[1];
        break;

    case 0xA3:                      /* BT Ew,Gw */
        CHECK_FLAGS(emu);
        nextop = F8;
        GETEW(0);
        GETGW;
        if(rex.w) {
            if(EW->q[0] & (1LL<<(GW->q[0]&63)))
                SET_FLAG(F_CF);
            else
                CLEAR_FLAG(F_CF);
        } else {
            if(EW->word[0] & (1<<(GW->word[0]&15)))
                SET_FLAG(F_CF);
            else
                CLEAR_FLAG(F_CF);
        }
        break;
    case 0xA4:                      /* SHLD Ew,Gw,Ib */
    case 0xA5:                      /* SHLD Ew,Gw,CL */
        nextop = F8;
        GETEW((nextop==0xA4)?1:0);
        GETGW;
        if(opcode==0xA4)
            tmp8u = F8;
        else
            tmp8u = R_CL;
        if(rex.w)
            EW->q[0] = shld64(emu, EW->q[0], GW->q[0], tmp8u);
        else
            EW->word[0] = shld16(emu, EW->word[0], GW->word[0], tmp8u);
        break;

    case 0xAB:                      /* BTS Ew,Gw */
        CHECK_FLAGS(emu);
        nextop = F8;
        GETEW(0);
        GETGW;
        if(rex.w) {
            if(EW->q[0] & (1LL<<(GW->q[0]&63)))
                SET_FLAG(F_CF);
            else {
                EW->q[0] |= (1LL<<(GW->q[0]&63));
                CLEAR_FLAG(F_CF);
            }
        } else {
            if(EW->word[0] & (1<<(GW->word[0]&15)))
                SET_FLAG(F_CF);
            else {
                EW->word[0] |= (1<<(GW->word[0]&15));
                CLEAR_FLAG(F_CF);
            }
        }
        break;
    case 0xAC:                      /* SHRD Ew,Gw,Ib */
    case 0xAD:                      /* SHRD Ew,Gw,CL */
        nextop = F8;
        GETEW((nextop==0xAC)?1:0);
        GETGW;
        if(opcode==0xAC)
            tmp8u = F8;
        else
            tmp8u = R_CL;
        if(rex.w)
            EW->q[0] = shrd64(emu, EW->q[0], GW->q[0], tmp8u);
        else
            EW->word[0] = shrd16(emu, EW->word[0], GW->word[0], tmp8u);
        break;

    case 0xAF:                      /* IMUL Gw,Ew */
        nextop = F8;
        GETEW(0);
        GETGW;
        if(rex.w)
            GW->q[0] = imul64(emu, GW->q[0], EW->q[0]);
        else
            GW->word[0] = imul16(emu, GW->word[0], EW->word[0]);
        break;

    case 0xB3:                      /* BTR Ew,Gw */
        CHECK_FLAGS(emu);
        nextop = F8;
        GETEW(0);
        GETGW;
        if(rex.w) {
            if(EW->q[0] & (1LL<<(GW->q[0]&63))) {
                SET_FLAG(F_CF);
                EW->q[0] ^= (1LL<<(GW->q[0]&63));
            } else
                CLEAR_FLAG(F_CF);
        } else {
            if(EW->word[0] & (1<<(GW->word[0]&15))) {
                SET_FLAG(F_CF);
                EW->word[0] ^= (1<<(GW->word[0]&15));
            } else
                CLEAR_FLAG(F_CF);
        }
        break;

    case 0xBB:                      /* BTC Ew,Gw */
        CHECK_FLAGS(emu);
        nextop = F8;
        GETEW(0);
        GETGW;
        if(EW->word[0] & (1<<(GW->word[0]&15)))
            SET_FLAG(F_CF);
        else
            CLEAR_FLAG(F_CF);
        EW->word[0] ^= (1<<(GW->word[0]&15));
        break;
    case 0xBC:                      /* BSF Ew,Gw */
        CHECK_FLAGS(emu);
        nextop = F8;
        GETEW(0);
        GETGW;
        if(rex.w) {
            tmp64u = EW->q[0];
            if(tmp64u) {
                CLEAR_FLAG(F_ZF);
                tmp8u = 0;
                while(!(tmp64u&(1LL<<tmp8u))) ++tmp8u;
                GW->q[0] = tmp8u;
            } else {
                SET_FLAG(F_ZF);
            }
        } else {
            tmp16u = EW->word[0];
            if(tmp16u) {
                CLEAR_FLAG(F_ZF);
                tmp8u = 0;
                while(!(tmp16u&(1<<tmp8u))) ++tmp8u;
                GW->word[0] = tmp8u;
            } else {
                SET_FLAG(F_ZF);
            }
        }
        break;
    case 0xBD:                      /* BSR Ew,Gw */
        CHECK_FLAGS(emu);
        nextop = F8;
        GETEW(0);
        GETGW;
        if(rex.w) {
            tmp64u = EW->q[0];
            if(tmp64u) {
                CLEAR_FLAG(F_ZF);
                tmp8u = 63;
                while(!(tmp64u&(1LL<<tmp8u))) --tmp8u;
                GW->q[0] = tmp8u;
            } else {
                SET_FLAG(F_ZF);
            }
        } else {
            tmp16u = EW->word[0];
            if(tmp16u) {
                CLEAR_FLAG(F_ZF);
                tmp8u = 15;
                while(!(tmp16u&(1<<tmp8u))) --tmp8u;
                GW->word[0] = tmp8u;
            } else {
                SET_FLAG(F_ZF);
            }
        }
        break;
    case 0xBE:                      /* MOVSX Gw,Eb */
        nextop = F8;
        GETEB(0);
        GETGW;
        if(rex.w)
            GW->sq[0] = EB->sbyte[0];
        else
            GW->sword[0] = EB->sbyte[0];
        break;

    case 0xD6:                      /* MOVQ Ex,Gx */
        nextop = F8;
        GETEX(0);
        GETGX;
        EX->q[0] = GX->q[0];
        if(MODREG)
            EX->q[1] = 0;
        break;

    case 0xEF:                      /* PXOR Gx,Ex */
        nextop = F8;
        GETEX(0);
        GETGX;
        GX->q[0] ^= EX->q[0];
        GX->q[1] ^= EX->q[1];
        break;

    default:
        return 1;
    }
    return 0;
}