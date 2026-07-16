/**
 * @file        ui/fonts_inter.cpp
 *
 * @brief       Embedded Inter font (Latin subset) for styled overlays.
 *
 * Inter, by Rasmus Andersson and the Inter Project Authors
 * (https://github.com/rsms/inter), subset to U+0020-00FF plus common
 * typographic punctuation with fontTools and compressed with Dear ImGui's
 * binary_to_compressed_c. Embedding one open font gives every platform an
 * identical menu rendering.
 *
 * Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter)
 * This Font Software is licensed under the SIL Open Font License, Version 1.1
 * (see the full license text below).
 */
/*
 * Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter)
 *
 * This Font Software is licensed under the SIL Open Font License, Version 1.1.
 * This license is copied below, and is also available with a FAQ at:
 * http://scripts.sil.org/OFL
 *
 * -----------------------------------------------------------
 * SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007
 * -----------------------------------------------------------
 *
 * PREAMBLE
 * The goals of the Open Font License (OFL) are to stimulate worldwide
 * development of collaborative font projects, to support the font creation
 * efforts of academic and linguistic communities, and to provide a free and
 * open framework in which fonts may be shared and improved in partnership
 * with others.
 *
 * The OFL allows the licensed fonts to be used, studied, modified and
 * redistributed freely as long as they are not sold by themselves. The
 * fonts, including any derivative works, can be bundled, embedded,
 * redistributed and/or sold with any software provided that any reserved
 * names are not used by derivative works. The fonts and derivatives,
 * however, cannot be released under any other type of license. The
 * requirement for fonts to remain under this license does not apply
 * to any document created using the fonts or their derivatives.
 *
 * DEFINITIONS
 * "Font Software" refers to the set of files released by the Copyright
 * Holder(s) under this license and clearly marked as such. This may
 * include source files, build scripts and documentation.
 *
 * "Reserved Font Name" refers to any names specified as such after the
 * copyright statement(s).
 *
 * "Original Version" refers to the collection of Font Software components as
 * distributed by the Copyright Holder(s).
 *
 * "Modified Version" refers to any derivative made by adding to, deleting,
 * or substituting -- in part or in whole -- any of the components of the
 * Original Version, by changing formats or by porting the Font Software to a
 * new environment.
 *
 * "Author" refers to any designer, engineer, programmer, technical
 * writer or other person who contributed to the Font Software.
 *
 * PERMISSION AND CONDITIONS
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of the Font Software, to use, study, copy, merge, embed, modify,
 * redistribute, and sell modified and unmodified copies of the Font
 * Software, subject to the following conditions:
 *
 * 1) Neither the Font Software nor any of its individual components,
 * in Original or Modified Versions, may be sold by itself.
 *
 * 2) Original or Modified Versions of the Font Software may be bundled,
 * redistributed and/or sold with any software, provided that each copy
 * contains the above copyright notice and this license. These can be
 * included either as stand-alone text files, human-readable headers or
 * in the appropriate machine-readable metadata fields within text or
 * binary files as long as those fields can be easily viewed by the user.
 *
 * 3) No Modified Version of the Font Software may use the Reserved Font
 * Name(s) unless explicit written permission is granted by the corresponding
 * Copyright Holder. This restriction only applies to the primary font name as
 * presented to the users.
 *
 * 4) The name(s) of the Copyright Holder(s) or the Author(s) of the Font
 * Software shall not be used to promote, endorse or advertise any
 * Modified Version, except to acknowledge the contribution(s) of the
 * Copyright Holder(s) and the Author(s) or with their explicit written
 * permission.
 *
 * 5) The Font Software, modified or unmodified, in part or in whole,
 * must be distributed entirely under this license, and must not be
 * distributed under any other license. The requirement for fonts to
 * remain under this license does not apply to any document created
 * using the Font Software.
 *
 * TERMINATION
 * This license becomes null and void if any of the above conditions are
 * not met.
 *
 * DISCLAIMER
 * THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM
 * OTHER DEALINGS IN THE FONT SOFTWARE.
 */

#include <rex/ui/fonts_inter.h>

namespace rex::ui {

// File: 'InterMenu-Regular.ttf' (16744 bytes)
// Exported using binary_to_compressed_c.exe -base85 "InterMenu-Regular.ttf" kInterRegular
static const char kInterRegular_compressed_data_base85[17355+1] =
    "7])#######xW+ND'/###[),##1xL$#Q6>##TZ;995BEL%AiES7Bf>11uRdL<(jlS;/qao7NqEn/3ExF>'aGI_u*m<-s?^01uZn42)WGo(D:iu5$sEn/aNV=B-b;1E#tEn/w.>>#Hb(*H"
    "MY`=-%U^C-(BRm/#-0%J/q$J(tKPH2DQT`3I/d<Bus-0HYmP]4IKVX-TT$=(M.0:),;iu5UrEn/<_[FHgP+pCZZqV%@s+##)hvhFjTvjl2M&##^/###_gY=BZ:.`siJ+##D9u92>4^=B"
    ".Pq(>9m###q$b30RGFiF]gExX]WGwKoBr--Q/5##]PUV$L&F9#BxC'SCbfC#(k^#$&uQX-D1NT/k5?a3wg-r#NI[MupNsS#6wVo861/h*$#H+2$if1BEGhf1;Zx6XEQR:vro4<%me''#"
    "&s[>/V.k9#1]clNLwTkN/(G9#Ms`=-p(Au-X<&WR-uFT#hMAu-MVoCO37(%Ojh_U#;V_TM;8O&Fb3An'6]###8V5g)6QK.$SUZ>#WU>d3XpdMBKeWfU^Hi+i$M`RnI$V?#(tPMg28i$Y"
    "bQ%YMq5U&$?5>##1I:;$3dP[.S'+&#Ea<71H&B.*ig'u$dqh[,0_Nj0jtK,3>U^:/n3H)4f$;Z>t,mm#TC<)C_u<T9Pr2kjr62O#fXxV;QMW1T6YeMXCxrO]pwT[]d+ws:J,M;?a1Ex]"
    "sa<@B/i2OVa0.3NIvp4J(fgGD0ZWV-JuBs-^?7f3e[qkLP_X:.C0Tv-+n<a%wVWF3NS>c4ju1v#r=,fhQ`oi0$EU:Z5'qqI;k5Ui*pN$Nkl]p)NaaOuIlru>Vatu>tqFeT;F#hNEN)=a"
    "A&K=:4V=pjD*>>#teb4M;l@5/-A###ZAO(ju/@(sMbC3tn?SnLM**$#WMOV-@`6.1c7XI)p8KZ-epq0(Au4K1.[/G2:V]U#NOfkuEkL?#<:%@#'Wftucp@p$[:9l]P$Z4]b[)&4Aw82'"
    "XNiWhQC)##DBb3=QD###2OB)&$h13$7b)`#$4=e#&nw8Y9eskuG0+_SR^e;%=_BK-q1Zd.,'?V#dFAu-gB`bR5L(:#is`=-8],[.sPtA#w-=J-7w<J-n.m%0_AnKu51[Ku'10c.HxV1K"
    "Mqw+%VxrrmkfqDPnrI)*Tu29/[_oT%9G3]-R:M;$Wc7ouJv=P#U'9O#Y`I%cGQa<M(U-6jUE*:FO-3h#@_cLpJoru>Uatu>r7[nQ7IKhuF4Anu%SCkaU&J[_,ve;M<0gr-*S=Q1?dnO("
    "D$nO(VBlD3RImM';7/+rvKX1K(7E+2$>e7@CarA#f3(@-fL_lL9NKYG`R.WtuxSw5COb7e,pQS@t@C5/8c###Vs7fhNl1;H.7WP&f%#^GTtpR*6iVrHflJ'J?K]CO,l+9#Pe%Y-k3Vw9"
    "92+S*7/c%FA@/_JCVN1pF9e6EdV###PNv)4`FYI)*)TF4sCd26albV--+%7PT+T7aDk#)i1X/.QhA7m/cFqjn/<3Z$)A<>.fE9D3VvR$$sq4D#&j98.j6:hLs5<v6PPEc`otIl#c90;$"
    "'xEpueF?\?=:So=lM3%l]*5=ruS_+w$DK<jLgUucMmn6g)Ke'5]Evmih;>]Y-O$2L,WE7L,*T.)*7TRwBL*x9.+M>c4dL4gLY@_</G:C+*THPQ#b,Qo)*CRu6PYXI#J6Jv#d`h[u*;[7["
    "g3B_#Rce8uw;cL@%4$&$QIv$$,'GTH+wM)ug]t@7Ha5UqUC<;N>i>oLK_IIMmvO(.'o-lLCl(B#=**=C@';5A2s@u%rPnUuk[=H=DYFgLqPpN2PYrt^x5&vL*cI.4sXZ*Q25E7['ac5/"
    "AS9dD#.KfLuY88%KKEuuE>^Y#5CO1pdQtrmQiou,0X'k0aBf[#bYSF4Ke4`#EG'<6J#DnuSDcP#$DfR#N*>lumxj9i*jE$N,-/wKf?]V%bK)YYQ:+YYlqgL#UFoR#tj(t/#=(%O]/RUM"
    "13DsHnI]>-$Ca8.L`1;H;u^9V`MVPgx<YR*o2r`W6VM:2;Bu@bj9kB-p(Au-Gd1MO-+Au-MRmUR6UCU#;2[i01I:;$T'F9#4h749^Up;.V-j39<u0H2_0GPl81H4[qh&#5ar/a]phTMg"
    "/,_qMab)rmuLCvMW5)##J[?,vF>+i$sB%%#:1r6(8UW:.1kTD3=XNu7'5pG3u.g,=`/H(JQ+dBJSwN3>$)?A%g5wR:JY@01oxkbB]$'D#k5B3rtx@Z&d&e9M(/LVnO=`2()xc<-Z*WB%"
    "s%:8.8X-AgbmFM#n2W7eL_Q9i&s?aSl$Mq50LKbRv*I.Uh9i=uijQ/Tx&.4#C7_+v?9%#M)R6LM(QHH3SwC.3&>gG0i7WT/.W8f3*f:*>fCg?/?;Rv$#VB+*s81#$XeRs$S2osu4.Bi%"
    "1<RSCP;F1NP>(Ndd#@GRH]gf_S6NeQFUIO4aDUVMZC1[+10h.Fc4%Q<lX7/*2tLhR0+@RD-AXCIo>[XAK.L3([HJVD*%8/#dhjPBf@X_$c8###LH7g)4o0N(D<odu>Q#N0F@x*iMD'F."
    ">$:on@oOMgq'2_]_p*P(-r@$TBj7C&`?s;$&:oluwYXeA#PnD[%mHiu^*:=q?'=9u$gK(BDiH+$V):6TIxT:vvij322(D5/OR$##P<?lfL[.;Hj2ZSf*'.T*tXwx=[V'vHCe,jh7v0;H"
    "+Y=_6[0<$##ZCD3%N?T%&+eY#ubARdkC?l9678h*vq8Su0fe8uv3l_VAwE'$l-$(#b2f;%Z5_S-v7RA-kKOg$P<N1)hAxF4:M.r8?^Zp.9%`v#XAxCEmc^11<TxE+foPB3OEJe*;v@cD"
    "j)1,)7F[t-nw=b@K^Gdcsqf+B$[%]F.;9hbit<PtL*G&:#gI5VELF4ME>+)#<4mlL-@^%#XwH>#4$l]#S6$,M0,i_%H;a`#v+Haj;]322oSt$6Wp%qusLZEjE+er8A:[E5pVA]tlA_=c"
    "ETKi3E+'^79,?c2E&-5JrLKq9i*nCNMA4Z$d#jE-RreQ-;V7R&daJ[#3mbwjZYkv6m9,Z.$[(Uuh@F*$Ef2v6<JG*G>]B:uP0v3#(3vG2F6Bi#(^Q(#*Mc##achR#Bb%^.LH:;$b6ZX$"
    "$>B+4sR>c4t29Z-:eS@#mGvA4nB7g)eT:-.vLc<SfaOU$nhl%/$;g?RY:6*NI8ukCX(gxXf.RF4PsHb%uU(##*Er(vGE2S$`Xt&#_PUV$rZv)4$2f]4#IR+4nGUv-@Tsv6SOr_,oZw9."
    "6I/[#>>#G4I3Kw#Omqc)&Z]+iCF3R;EU:7CuiVGHB$#YZ=c9m>s_x-?H_i$>.x_KOv_3XAINwDGUv?c<ZGR^IL<63'-c<w$Ne(+6gkWS2ZjR<MU$]a/GNm9<8e`B'V[.w#3KG[7)4Auu"
    "jXD*/ht?5/GZ:E#ETAsQ6O(:#d^V:2PP;:2#q7fhEa9$.ekBPO@U%qN'S1:#8$&Y-AWW@9C@J'#5,=kOJj4U$o^Q(#D(Ow7T(gG3u6_T&#(C+*//'J35Hrc)0(C+*KG>c4j@h8.KUYQ'"
    "(dTv-XiWI)l(2E4,>[8/_K#L1lUo'[keQ1YS0KGHO&e#7L;0D55SI?6^tDDZ-TLFi3XZ>CT[-d/,PlriM>kdZ7NTLNn>W9CB.lk#jrZqN'JoI+2GFxA$/&gDsAHT%JW4(?q'8thvn?>`"
    "']?nK,GYE6#L(m&8bV&?7j'$9cE20@d05##Yo.T$$'F9#8Z8gMq<@<.Jl*F3RE:a#[7,i(TL.s$7'UB-.LdxL#_s`#QD7[9X+w(^_'*6LmmB0J-NDuDwGJEPlug9WVIk)-Qn*Tuj0^8:"
    "_]Q4]ap:uPlsbkHQUOFm]*jDMg,+PAS$oM(ee%Q'Xv(D.N+i;HOJf&#e.RqL[w3@$1(3JM_(.&4x76lL<S_:%iP&gL5PQI%ugHJCMdS<L:lKtrT:wXQ.Ub]91$'JGTk`]1q1_M#/T(Q#"
    "D,$.R:=3cH)i'o`BaCm2c)V:v5&S?g2Y9_#5Vs)#*Mc##2;1pL&:>GMD.Gw72cx'JRiaF3H2.6:UjU]$S^`6NU0vo7xqK&Jt6N:L=apm]?PSk90fq8WVu?IPW,ko*j435]f,vxO<Sn]#"
    "wI2[Z5P1kViqONm78ecH/.JcM&osmLEsS%#PEvM8-lbA#<A7T%]I&5WDB],mPAJPPTuCO%hJ6*Q['i5p>*&#G`AE%BtUdWqNK4MZV]kD8r4TK2:ps(WS47&Rk:B#v]dC$%g23U-O'Wm."
    "T(kpu6_M=-O0+r.P`39#6m$T*%$*/Cu.OJiRbMq;n0CG)QjIfhYD[`*9<hc)4o0N(K$,o9f-wS^'?Ji)5c`ku;<`wjM_,]7wbhR'l:,P;ssj2um<%cT4ZuB86c%h$C<H.F8+1GDbY'g:"
    "cYOS.&=*R<m%NT/XWLD3K;T+4$AC8.5e/O&;vsI3TeRs$:$JY-jpb)Y9s@d#Ti@x39a1wIvgs](j$7nEx<6BJu9aUL=HX9[TsV%TllE9EEr=kuW<l>AI6a4L]QS>?5>pQ[OL^x9M@0pt"
    "G2$n>omL)_1#eX^qO?:#n1m3#Ia^Y$UG9I;qScH3)/'J3<)]L(Q@EeHN):A=pFK%Zo#+H^;g<pV$o=iBocM7eFc'EhkXUi^*<i3#J5ZlJ0qe<IT:?5/qa%##V#S+i;XP-Mam:A=3Zxu#"
    "hEmO(fm9'^$9LCW)eHo[h.9584>ZX1&t7fhA@OfL&e)fqNugD=4klR*r'>S@4_bw98+uR*r0$PfK5(@-opWB-v6+r.X=B6v8x:b7.7ZruX_=oL-V*$#%*nO(0Ep;.Hv8+4t+-AXMPB8S"
    "Im&E=u<YLZ?s)@0I`l?^nFW?^+`$@^De:_$2w^dM#j^L;G:glU#Fq@k/8-C#j+'GDode8u[c;suqBWh#VM&JC'C*W-Lfa-kV*1u]Xo4o$D6nv$S51/#BBD].//'J34IFg$XE,ND</Y`%"
    "4:^L(FY8204^$cQMdw-J9LVv]OwQrLlOHWBAQE#f?c69D]<6cY_27[#(=0LO_ng^#&iSlNpiwJN#Y:>$Q[Ij%M)TF@Ur/u]cGS=[pj[,^_W=T.xT)NIO*b,'IV(##>>]crMSdu>)Yqr$"
    "N[K,#]0s;-.=dD-Sm`m$BkA?7hGM#(A[7m8Ylq6TG#(*n]et]93'k.G&aFBQW#Vg1_>_R[],rUQf0<UFb98#9xT5QUbqU'%EkUH-J%rd-J05r)+mtq)P-,6M]83(vw#wG$.C1[-a76x0"
    "<w)+Nded##T4Vf8:2=2Cb2ZSf>h1T*2h](aG4c^#uO/PAp4fWq0VLS.G/IL(6_SNMDJUv-ei?<.]agdOIjA-[`[[b%o7jqL6fi?#51Fc%U;''%OVUKNOBJw@I_Xm#.bp%'JmtKG/*KLG"
    "%>0N(o=Q29-?G)4:r,>9QRH?$@;=h(GJK=9cs,K)*d749Kr'^uGu=(-fVr&ZUTK5uC2jF&EgCfUPa.JhRhff(/M51M<g'h$=)C7/?;Rv$A6A-[Rtm?C1s:U%JU=sU2NMP>C@j1^2[aMG"
    "fH%##;]k*vfkv&M+O2'#qVc8/XkC+*I5^+4uuZW.Ht'U%[`^F*&S'f)El:Z#BEvr,&o*=C.9nw_u#87`.^SI/almkEuKqVBhM&mF+m8gQdt+8C/N731I]5BRY0'Z0eE2W9AP7o/`nTYB"
    "C4kL3xO@5J:`hi0^:)d3?[%CA;kZeMFEq;.1enI-Y(BgLk;hj%J-cIc_l:PHRs:D]*jf3Xd;x[TwZa8:p01+I)=u]=k/*$AuU*E[*C>?cYU_<E2JNIGuV1_H7n`rMh.@#MQpJwu=q)d$"
    "%GqhLXIF:.68xK<?a*P(JsZm/%6K[#:#8p9OA1r.;B#ZQwsD(^t&0NupDtZ%)0>6&N]DS[s^(ZujV*71dU4U$Mg%-#;PUV$4>F$%lL8T%RXTx$CdR@YSGpeHLvm/[BPPNgjrhRutQ_YU"
    "Onpmtxd=WC#)>>#jSov$Md''#Ru4[$'xBB#uO/PAR<^-?cx###D=/5=W(AZ5`Y0i)HIYQ';]d8/Ym9'^<x&ASb&]D,r*sB,^e8X7OW+PA,H_7@xLl=UiR[FVu<.3YxOeP9)T%;OMq*+N"
    "hid##_PC7#j,QSfhxJoNFhqw-7pNZMs,f(#H*8nLUiOg$/THT%.vl=l,5ubp8l+.)qY6Wc/<H5Af;)_Q5LfY:[:NCW$),##pYqr$E&@A-1aM#(-5-<-E=3Y$ud[>#)NKVam^Uc+jrO+N"
    "a$O(4TPl++n*ArZcQ=G3QM:c+lb%duGPV$N0A?H#(:Bu?*I:9ie(hutxq;1B@60@f1Hc###23s79FbA,V;ZH*W`dC#/eL+*ijx`4JI^;.B(r_%Ku8E#c>6]g/deD,,hJ^P'p/M#GtC+/"
    "uR#D40V's6[k<R6aeTIEd:?+i#('Y5Prn0D4PAe?(#Ru>OGVe?W#5J*3YGc4NSJW&v*5#%?]d8/DgN_=9[G]uGTeZ+FB,40G<$ou2%KIIYg8`EjgO@5b3A/Ce2dBQ4F]Y:_Yfwt5KQ$#"
    "R(4GM[4<Z#f,F]F)Yqr$h0#>OJp*+Nded##k#8nLhF-##O?uu#;T[I-EBWl%nS%&4ZH7g)p<6g)k&uIucv8EulS55NTjH^XI1ki&x6YY#wt(##rZQMgHs?D*jc;F3RfaiE<8:Z-g1n'="
    ".4:'YKspIVoRKljjSexa-Ue7TVNDju5xEpu,q@MbI>lOHvY-Po4@+HQI,2N0BEh'#$WH(#wK[,;bGw<&L%39/Xe_a4[SCa4H7#`4E`[T%vjWI)p*gucY)%eJaFmPk^e8,Kd(oi*8vWpD"
    "tBe6g35<P:+d)gs,#j-PS$T1GHVjZ7H.[[Bu<w;qaKK`N1C.wU-v@D>*ntcBAI%F-r?nd-&rQMt&';xCsA,QEJk@EZ<?uu#i1CP/3jR;Z'WYc2]$%##lh5kO/c5kOKj9.Xhof+46*l]#"
    "Kep6*GExkVhrId).,Ri$dx2T/hA[0YTQB>>uNS^b(xj+607MfcoKa'fdKAe&NRKA6U^0GGi:*kXLOqF#BTrO=F'T((FhvCI]`3M(sDQ/Hq5lsdc8UbZplr]&I,i0([HJVD('mA&c8+Y'"
    "jHG$@N7LU#>Q5b@R-q(#8A+.#ZrM[$h#x%#jMOV-iD.&4GXR1MpP/N-(ogwLN[<M*KsHd)Gh.[##VB+*:D%LcIqKxM0%ae0emaD@i-8=Z4cN,I[eIc4=/a6ST@?qsxhSlY3F+r28<^#u"
    "_iS1=<qU('to:($<ZE)[dB[fIB5SY,2@(j:_nIs-CP51M3WfX-v<6g)4o0N(%kH)4.c%T%I'jW#X>KP8-#9iu;-a>#6@Hw`$-NM99KUR#bYuf$2I/a#/vibNCaShuX#4gNF(0i#AnoE@"
    ":csE@airrmLI$##I/TF4P4Y)4/h;E4As<v%J6rhLVVR5)h4:W-K;T+40h'u$[A+m;`#u)Sk^>@feRZZ+*I>REFm`8%)W&8.]4?I4U$':/#YVkXrVIG2JLE$g..'5,.0]FJ',bFioOAS/"
    "CXIhMU05##91ST$5x4<1EXI%#JTX&#xv9D3;l.T%&k*l(jFC8.ivq0(ct_3t&bpc))$UG)[P)%X$2RUWHlO@#xgkI#xHgiu$R,ju#QxLX&%c]+)J8;6u)pUmxx$YcafvpA^*2>PW2>8%"
    "4K&W6L`Iu.#;4I&U_ll8$#H+2?/8(s6VF>#iI(##+=uQNh%$##vOpr6&aTW$ocCm9kV)B4FQhc)JcdN#c^X.5S6d^SW<n#N&`(g5ICK+`a+?i#Ele_JTo%%T_VgrFlPD2=aa$iuT&;xL"
    "IehsLwA1sLP;_0M*P4o$=6WdMkLo8%ecMD3OG:j9>fe,;lY_ilwW&i%Fj@x^r;S&JUq?DJM'*)$=N;qiiVtuEIr^uU]2-1C2cT[J;qc>T7laiF;r2U:v9J-Gt@Wa[*O#(b[^sqFHg9SI"
    "o41B4['F9#<(V$#mOwC#ZH/i)6o0N(:3(B#?7qxOWe]c^H&*A+H'kK$)rf]X$M2,2.kl#u[7JfL^x-tLxijmL1PM&Pid6lLi.2]$>76g)[H[F4R,xh%j9##>N^qd$Nc,4GZSC+><;^aP"
    "^lF)$JRbS33_Nr;+w@WL3#*[]%UU0>e8dvS4=[,*YcqkuIMCOL]xc1lLDsM-bPq%/6JQ;#c5k,4.#.&49]3][a,ZJ(xjWU^,agW$^+x'A*(4H#6hTgc2Jj9+?vTobJsDOHuZ2$K4'UDQ"
    "]Jo+@duCJ=WM;NV[==T#ko@AC/ilpLBC^FrH;VjkU=?]Npch)At9g]@W?TYSE.HL%GsNMg*8###mBM2%b..5SA(N]b[JJv,.t]St[7>##KO)sII9h#-2Mnx4.+7m8hkmtJweXI)GL?gL"
    "dRn#%1M0+*QfEn:Lk*Q)LY(^S^v8OM0H?*IukaBf-]31=bP]EnQKF5>WZHx^$'eYTFqg&SM6CR`_2[qEgw<i_'Yx>D'sm(Ig1Zj'_3^#J+Z81Qa]mW:Er7,Q7TtLGg,)1Gbjfo799arn"
    "F8DN0T_C+*VlAL(C(vM9-IMH*kSWe),17,;I9#B?CmKB@Ol9t(O8T+I15M<6YqtshId(JT-&Yd@H7EQ@E.M(Qi5*2>$x)1C77`QM^,qi'JJ`jkVIQ]Na[cmU6&*puB+Vc#0<#0?q<Ba?"
    "^N9#S)?uu##cN1pk@vxkt#BJ1*=m,4Gu29/w45H=k##9%q@^+4xnn8%5Y*w$DL@8%5-[a$m+7V8T1l;H>.ql8MgUS;VWZXAO#^4WYQeiEK]>>-pktpf-;6qgRcHU37`#'K^W&h+Aqw/="
    ">w4O)seY]AoZc;A-/,s`xAE5DQqRbuWV[U;cP=(u1brE#WhjF/;_LHFOv:/:G$sh3'p>k<5<'##q@D0vvj'j$aC3+0E$(,)`?<v#_(U55ud_SebKP<?2?>+5ND]F#L&<<u=gGF<e:;X-"
    "i>j-65gPN#'a>5Nru<F3#.K;/]Fs)'@QQOM_/:lS]mi>$9<9_-RNi-6G`bFro#urmnZai0Y^#&4PS>c41*.#%-kA9.^fse)=+?D#,;_<9_tDYC.&N)a.IC1I:O34E7TW@.PV$,bOW_/?"
    "G@o,?IKd=KkP4TColhGZ7TvNDd['D3Sq5q`'=FoI[VXDEFqM?pmZT)Pgr=c4SY958wh_a4#IR+4V,jhLXU>hL1I_lSPAuY?6A@e-j^*XLYrdkM>XfN/VdVaNj%WKMcGNxJl]OpCVr8_/"
    "U=<F?Lnl><mm%H<e;hnL__W)#>aVs#7Fh'#E$(,)^cJ_4aSlY#rxw-)IVc(j+wl?[B*&vIW*E9M)#dO#PD6XJl,Jlu24<ouNFR'jV%g'8hLY'8rv%(80+^h(;4nY#P:,i(aBf[#05QeH"
    "#'2@[VrMjM5V;W#F%t6bXJ>F#:43guk*49WlxYPh$),##?X3B-xV3B-Z>G[08BF:.`8?[lngX(MT$Im8?p;KTfRZSJGK<s%*`2*4k_Y)4_94glF$2^ujfHg(Kxs?M$,kuL-dm)M`8xiL"
    "Mud8/=U^:/+EsI3q(e'&#d0i)vM[L(YsHd).a,F%TkXF3D$G3[vUEv-bqkxh%?ZUK%#]sK?2>-f:r'(%`,vKF*J*eSn'VguLfR3+*:O-?RV:*IBC`SJ])*giww5;-[[I*4'K+P(OGg+4"
    "N;-F%$IZ)4@Mte)BiaF3rF+gL#CY#%xS:N<wxI=A1hOGA'/c#BO<L2S6R*#,*Q4J)kgdG#xc^]S^usoH?oOQ#^'d1f515##o%jD%]Am3#8IxnL5lG&#%[EU.;DJ)*K0378)TlS/DKXW-"
    "xF?nEOZ$i;&YeZplV$<.aNqc)1YJ,3386J*tt#YS4nL8*']0P8uW:U#LbJ7fC&7?E'HYIH8mk#&=C@LG#/qME8`GTL*Y6b8ma*e=$E:VZqO-kZv,0;HMx,K,_C,5XYCRQNV_2)7:@JrH"
    "jT3J_1>q1Tha:ru7f&7iYx'j:2.KQMCwu<h`*n&>%^/#@c/Y&oYpfmuLdh:Q*$'s.H?47/7gU?[cY0L>Y]N2W./J?4IG:;$>(F9#^3=&##$SX-*<4:.kJ(.$MOmV-JGVD3*p'.$84h0,"
    "`Ar+94cAWS=2RCk0dUFj+.CB#b4Na8Ub[['NeuF#`te=P$1CfCW4DN#dd-(&K9AwSJc(juhZQC#lmr+68:op$fLZ;#$WH(#qVc8/Zk_a4.W8f3DQaa42+9g1:>`:%#DXI)NW^:%H1;j9"
    "P8;Z>&>ew;&n;R3a,vCP*Z+U^Je$[;MS:m4V6KFO.JVha>8P4HJ%kDID?AJcAj=mL-mKn_[^E=<Qcqi5w.4>RArv0aNUU?;F`v'8d46<hwjPHK$H=tLWR$##R;6$vUH-0%lxefL-I;8."
    "/b4muw_VxkC=.o%cuit(oS=V-,/T5]Y%.O(Qu*DWrsH;s-[T2#iiBdavI^cDCMSk+Q8###Hax9.PH+Z-f4t0#49BMuhUZVu`t+lP]fm>#h[]M'fi=&l$86o%3Q%?QpkpY,0u+@Bd_G)4"
    "`&BuGB]bRX8Il&#RbH##Abq>$ZM-@'A$wV%F03W%gqB4+N/>>#gKW-$Gm'hL66F;&/h'u$oroA?JXe%RhPsi>t-qf(-&hY?jWq<1AY%1$a'6]M2]),)cwAGVPIE)$KhQCu3L:.Q2:+>f"
    ".//>-%r-A-F^&;/EW>SnBPkeMa:Q:v7$JY-V:Tk+ifHiT*c68%jJux=#)P:v6JO#$k_`=-%s@u-h%7fP$%QWM&729N5'jfu^6m3#QOc##<knWtW;bJ-6;cG-h7<#0(mUD3mKO=UlJ&Z0"
    "#)>>#=tcN#ua;^MNRYXtRCSn0d@AX#).,##3PUV$1&vp.(a2J<0Nl-$/?^e$ox>e6MNIv$=9Lw-o7cwLG'2'#CDfnLUed##A<(tMcSp$..bYcMTt[:.ORD20d-=vd#XIP/BJAeHb-vE7"
    "Rw(4+*;bV6w>=,;K51[-,wM-Q_@^-#(=*P$iAP##JLH>#Y5G>#3x9J#Ngi7I@2cM#*:'quJWjjL=T#oLJW+vM&%wLMp5nF#G4DAf$nRxF&V,_JpSj>$Hw'w-vhqXMD/0P#mSdxLxeH29"
    "2qx%Y2qx%YB8X-?,k+/1A#aMB*8###,2jc)4,swZc(s=5M)lS%DDw9c1w@Zum]PfL2^d)M:@2tMqCH>#;V`S%8v,3u<'%SugeFm#g,iDb.mX?#^b[o.%/5##&E0>$GAY]8==Y)42VSfL"
    ":s*8$$dm,$oRiCX$8sSX(eGP2OOtE$/.L'#Srgo.VF3]-NH.a$UbYbu$$'MY$*OuGhBM;V$br@F[H*rLL5G9#pLaH$(F?I3MpKB#^-I>#@PC;$u+vjuDUEMB)%LB#BkP+V*iAlNwTVfu"
    "FS3'I>LG&#0,*Qg@n]PJO[e'&pDW]+cDLg)6M$98IYA'H0r[PJLRGig.-pe4,$ge4><E`#c7n:W-*[.<-'Ri;[bme4*qJI4ws%qrYW*29p-F5BvYTm'jdDbNFK1N(Z(4;fSbEP#xe?KY"
    "R<MS[b0=P#w0k9Y$m=5/&)T4YSO5'#`[YJ$[m'-M8eq*<0hp58`^D.3-jK.*u;Tv-h+39/0)xD*S:qLN.ln2=RPlv6fm(#>h0^tJbEm;/VnI>#1.Ga8Rh':/<_xASp%132(Ot5M1C7V#"
    "3qK^#?1xfL-n@X-mg`$$ZHk*$O>WD##<>]=8gCW-(Tq7@xn(##&,###Z_G)4$8[7@r#D>#Qx$g1B`RDW'Do^f=ddC#pp^#$l>9$d2-,vi9`)g1$fBxO$s9M0f+5^GEH<;Q1[,T%<rWX$"
    "wdMa4b%E%$j_.W$ddA@#3>rbr0ie[u`rul&#([?#]pK7n0t^au_HmxO6WI`KuB8-Z$h=xbuA3,Rf`Axb';G##<knD$UKQ;#YKb&#iuclL>T=j1CXmUA?e#K)EOx^-`<>V/7a=mB9/vG*"
    "5RB_Q1VV=.rn0vgqYePVh48vgoP[PV_/lGdU=<JJU1bDI`Qh)H$gQt-U@b=.RRgrEm7$/*q<C5M9AC1MZoNuL82(lL[LZvZU0t`*[_))5[]XT#m/IL2I5>##g1o5%f]m;#^d0'#pPUV$"
    "hcOjL;90C4_>%U9U*/s$jL-x6fM0k1#.,Q'm?`[,LVGKb_F=p@-<AnLXZqxX,'^Gh=?xT#$x@O#EbEN0(W;bJX+6^@LQt5N,#F.h=HtSDZc[T[T;w7:ivPh#'l<v'JjfhX=J(PH4:RMM"
    "x)USO4L)[%gOq[INQ$vSwtqs9O=[?Iwuv9BQ6%/c%/5##>dJ&$*o^:#07nM9kplS/u+aT%,p:0Ae#Z;T&]:H@l(Hm;BR.1<>RiV6&hP#Ae#mVT&Y(-@lF`k;DFMO;Dd>52HtA.$<bv;#"
    "r8q'#mJc8/G2:W-lU'(Q$Tl,%]_Y)4+PF,MEI.S'o`$i$@lLYGY;IqU^-PF?LMER)6FJpG+&0x8oo-iLcnx#'Be>G<#q<DN>t`pC$9I-)9$MN;.FP^GOH*aGn*<6HKEBLGIIug(vo7?5"
    "[0xlLw$wb#ktVuL8LXl%w4DT`,5vEK-%@RDPk6EI`d-EI=9:4FYP5kbCOX@Mea4+QjlUGZjc1,Z9++N0o%F9#RMOV-h@W$>:q`]-O3vv]g;pqGi0CO#k%('uMbjZNI_sMtb/kKPGR-ig"
    "%CormKB@M9(NLV?Ysu8/g%*9/.IjJ:74ULj%rXI)ls'Y$*ktM0jCXI)#DSC4E@%lL*7Ss$R(R&Oqhv$Zl317+vS>dGjoZ]uo]C/5lh`P8)9fUWgp)-.[Z*3En1*B#>27VLAY)uhdfW;9"
    "XDGS-B^IQ:,OshQfC;1=(`Mh3k#p@Z#6>S;-<H`GIo>:WW856DC<V1;MY5l5oT^#S/9*CR;lrxO8^h=8KDU=)fp+B6_V?>#bMifLxK6sZ>N<.bAl[&4RS+qZ2&+3%P0Bq#SLi&KA-r'#"
    "rXj-$T6Y6#P$(,)c8G7(0HF:.Oi3'o^28H2LvHd)&4rJ#fQT<AZFJJXdxoA^kpS=T`ElfL;$S?5i+n=7?Nn1OCJd<@[H,D%$#Ia._rd,$Mn,D-QgME-g;pW*n;/gLpvId)-Wg*%A,Rrd"
    "2Wkd#4?UU:Gkg_@nc.7Chm(qB)Y38@_?cc[fVheGPe<g5.bRH3U`$x69@MO1]R/R9[j/@B)Z-t$HtFODT$,YuA?uu#wdbg$EZCD34[?\?$uhu=#MQ_$8l5KnL;FD3$Zrnn&]g-+&c]F/1"
    "F)V`WY=msL((UiBaE.;QvX=A+BM1u72Q1H2)hV'4a;x-3hs_0HY.2v?,9KM9Fl?B5`KWe3v_N=K&Oi)=&d`A&V*,##,b+u#1;Mx.SPUV$J$O+5aY]+4oY-t$=[0^=D'Up7.Zm/Y&mY&="
    "&AO7:PdPf4S'vn0tJ`K$D#:F>ZnT$7-)9,&t;q=-YXS79AUV414<4Z6Y*(QCcWOW'Bqm3<V*`Hdx51pL?4%&MF'*$#jjP]4F-:T%kkY)4<E_#$.Njl8Mm`WT#m=GK(1UBu'n8?KiE4eu"
    "&I>uuo?#+#?uKb$sf0'#wdEB-)^Nj04Q5+#*Mc##l9k9#UShJ)lKJAclIYe-nk:_83(IDjq/t%cU+w1q(^KAcH?4F%;S@rmXKmcr$+OJ(CA^'/jwCP8Cmea$eA>HX'1ue)Dcu>#rZv)4"
    "?X$$$(hro.x6:U)]GUv-FZFZ-)>*A'3,`gA]*(3MPMY'8jHACjlg^OLV3&th#]D4u^_.W;7V94;':ZCNM:XB,Lv]1<PPCf=Xdl8.ar^S@Dh_'ucR.Wte/W[83*/4EaY[jDm,1`&P)>>#"
    "m7sW#=h@%#(Y2^MVRKvLnc2aMQbwAMoSMG)<R9AXxQd--vK]5/D<#b#(rv&MSqE^Mi7Rju'[S=#_L.U.S%###&mnU09$(,)kjqxXjY7%Mgj(^u#mwFNDe^o/naM1)]&bkd*PMiLOv>5M"
    "KKfX-WmfXub2hpPvXKo//mK#$FN/=#x7l^%ur29/vBo8%;/'J3`cWF3q3Cx$:o.r2U9+D+k<E-$KGB(>pL,:&OOI`$u^?-3;x$Z0qVFdG9hPe6ixTb$r`''#xbJfLdueqlXRT`N0$3$#"
    "p'Ig=9pGg)R;hM:>JhB#m-iT9fsC99'Hpb%8[?IM`hv-N=o8gLsUXd$`tD)%_RXgLdB#W/8rNn:urNn:1cq,3g2xh2?pDq/=QlY#(h@%#+b*a%[#b1)LnE:.jGpq[[d?j]m;*p%$?7(s"
    "3F`Y#L9(&XUtE/U=45W%9RvZIsYg;:se9q9c*vO:u<lO:Z%<(9>GPEGdjEY9I5LTK7(u9)MB1l:tN@p^`Ns$#x:,dlbJ/J3'.Hv5:]3j4X,&G=_tNB@i3.PAITds.[']I$mgSJ=fKrv$"
    "je4>%0=1sM^^Gs-mSL3MBM8W$f@bT97,5EGp6fERPT+FRMBfER4O2igob_##&,###$pID*@DtM'3Q5a$E:Ok+p=P.h-4h]OVkQg`bNZ<EcHH5Apu#-3QSr]4P&Wk+cr2]-sNsV$LcV^T"
    "qx8;-(1I202B*1#AG4.#fY-@'>:th#fAX)WxD-(#R+^*#-h7-#x?T2#KCY>#m+<D#%U%H#U;UJ#9X,N#1HmV#r=YY#Kw3]#7oSa#RAhf#M*tp#rs3x#xVd$$R=>'$-$o)$q<n4$TR8A$"
    ":G-G$..^I$_j7L$5%cq$Gh:_$RFad$xg>n$QGfp$,1@s$b?a)%vI+Q%pV2<%xDrG%%&CJ%UbsL%NXfX%:gT^%jL/a%C-Vc%&J-g%n5;k%1#Hr%`07w%3Xu'&`2=*&9om,&BnQO&b#X=&"
    "7TPJ&7Z,R&DNwW&?5QZ&+-r_&>I-0'%>fp&amWt&t](G'ZxG;'1ZuF':N5N'AE1Z'Qcn)(-+:n'/2.D(LZa5(=Cm?(#HYf(BtdO(Ud$W(b^ti(FBSx(pMe%)`wM))Am:,)/hd0)wfc3)"
    "WXO6)2?*9)_uP;)?0l>),/kA)[_2D)LSIH).7$K)&A`O)(KET)](k])KLte)PN&i)ZpZo)I]A.*QZF:*ag=^*cQ?O*^qGZ*#HC$+qW'j*Ooun*?Eq7+vt,(+@*j)+a5P++>:X.+K$nTM"
    "4i_&P&;O=d;LFV%%osC=&qoCFeJB>#'G:;$V(1.*)Jus']G?8%Y9w0#r=5^RaLRm/'>7@uVU_f%aEi9.(/5##)=dZ$][:_$u__USBG8,r#%4?6aXLGi%w*.$n+Ll]7wW/$MJ,F.7_.F."
    "D[Ml]'st-$<,-&M1-r)N+X_Q$tW9b$fc]r/fLHK%nB-0%,]P8.m9s)%8x,Z$hm7jL;:#gLnFDs-rqvM0*_CGrJ+9/$8mj;%T9Af.,T]]$I#T,MTqm##v(I0#9)qV-u.Ok+wpKfU&SP8."
    "xgBF$[edumxQX]Y4uT^#2nE=$w;X7/)=2O#iT^sL*%Vp.qf`.qQ5q_&4oI_&[b%.$15eQMd-,I%#HvD-2CT;-7?&,OIJ/D$@_TJDw>nEIx+ffLZI+)#-xD<#P'>wLKT]F-#8T;-JSRFN"
    "9_Nj04K]x$ODcB$r%5B$<@K.$;06MK<UTxX9$7GDjM#;HR0_m/VRK>$%@?c$Wtv,$t5OE$N@`K$BEsd$vq)H$hMsH$C*<H$gBbE$#r8gL,nwP/0xhc;J8#.$beI%#uYlS.x_)<#N7T;-"
    ")6T;-Z[lS.aQ4.#re6#5aFl6#_Di$#mVe8#+)-;#S.AP#TvB'#E^wm/fGx:Q'####+c1p.SV0##ge@k=4LBjC_Baf_RhJ,NI[kfL7K$Mps-+-*o7-)<)'o-$,q>5o;s<,<3X5,E3@_#$"
    "sl-qLIAjC$_n-qLJJ/`$J+%#Mb]d)MVpT(M7`:%MJ30?NBLI&MtZ2xL0Nn7$x?uu#/`Rh%w0AYG6Tr.C#ZVxXNXOfLahGP8Q(v(<i+1AF.pu4JMGWiBvaexFu'q+Ds^n=G6qr+;Wc%:)"
    "FlQ-Hwi%8@<##,;5cu+;Sg7_8b/Hrmd*YxXl.P9SLfRG)#r^.h;cHiTMu)&Y`;88SHm$2h[_$^lf9%##&lL5/t*KG)QlRfLm+r4JCqtiCHkhP0[Bv92$Pe?$umq;$B/`($jQ?(#+c68%"
    "J<%%#RI,MtY6TR)eA;YP1Ei:#LKXA#dBP##`4a@$[)A>-vfNj0*,bxOpX(##-]Lv#5IC^#*w,tL0_CaNI'7f=/=9`a4GQ8/6c#<%V]l)M8xSfLN_;;$1jm&MmJG&#gI3#.SxefLc^/%#"
    "V(4GMPRd>#Wn*K)rR<X(5.x+20C4MK%.Duu?']A,VMQ8/8[#.6clNY5s,RrZ9L)d**e_M:C=Oh#Y4)1#[%-/l=V^C-Ek<n/b$Y6#J.>>#O;#-M'@@mgD5h.U(KBVHq#m+MGqG`Nd/+M^"
    "t6FcV[Ia(WYOA`WuvUuYrNW-Hah?YP-xmo%YJeKYhUH$#H@%%#LL7%#Xqn%#]'+&#b9F&#sj9'#wvK'#%-_'#1Q?(#5^Q(#9ea1#Clj1#8d46#&rm(#NVs)#Ui8*#b7p*#fC,+#vtu+#"
    "$+2,#)=M,#:n@-#>$S-#D6o-#RaX.#Vmk.#xqs1#Cx&2#U$X9#uAU/#,4B2#.F^2#076&Mi&u#`+v/P]S5Ioe9>Oo[N-=`aV28GD19$AX[H](a4Z;YY]&6YctD.DN,]UV$,Puu#,i68%"
    "6INP&@*gi'DBGJ(RA[`*R5%)*njvu,m3io.ltlr-Iai7[t#BJ1+W#,23>r%4P]s.CA%ox4DLKV6DF0;6N3)58[#]i9[m%29n:Q`<n.q(<EH,;?49$5A3$(8@.l0S[>DOcDN+1DEQX)>G"
    ")/cxFWc1MTdU*GV`=IfU+lQS%Ylo#$1:<p%w/sc<9___&S+35&NsHcM((^fLqa@2NUL?##_Sj3NVRH###ts;N,@,gLNqa>Na<a?#^WLANeEN$#bYl##XY<rL6fr/#m_nI-wO1T0c:F&#"
    "0E-(#cV5lL#mGoL<(8qL@Bh*#E75O-PX4?-b_CH-p&wX.H0f-#@fRU./Vs)#BN#<-D]7:._/:/#HGTD=NB:/DOj)dE>JIk4Ra^f1+Od-?=$n+DTZn>H4Y3>5b<E,3<D`PBa;rNCft<D3"
    "[f1A=o+Uc;YY6#6DYgM1@bXMCrXlYH(Qc-O`OpoSu(`@-96`P-&Km-.bbrmL5skRM(i>oL5.AqLHQhSMa+-&NSRxqLts%qLs`4rLZ(8qL(4oiLOa4rL%/KV-G<vt$f48qVa*7;-8]cxF"
    "ch6f?^L_(8m]^'/?u(F.2W,R<Y=x?K1=$qrN;(.$C[_%O7juG-)cEB-u8wx-(/'sL&T=RM$orpLv'a..16[qL=N4RM^[U).:bbOMpED/#]M#<-K.fu-5aErL*P%(#mRh`.N)1/#>)m<-"
    "EgG<-&Rn*.@'BkL?;2/#PXkV.duJ*#;seQ-R`n[Z%Aie$I####S^`xO?_,j#M/>>#2[oi'lw0O+*tj-$vR'^#lLN;7pk/s7@cxA#ZfP9`Gtfx=pM?X(h/0eQsAE-ZCEG_&3P^QskmToI"
    "w_0B#qMUlJdZYDO6ZOe$gF`+`7lPGalOQe$($6Yc@hE>dthQe$F.$JhvIF&#8H+:&^1TY,Y9TT9wjTS%wqV*Ib_t*%>e2.-@Uel/t^v--%GJ=BtI_`*Pnl&######%sufXWV$##";


// File: 'InterMenu-SemiBold.ttf' (16612 bytes)
// Exported using binary_to_compressed_c.exe -base85 "InterMenu-SemiBold.ttf" kInterSemiBold
static const char kInterSemiBold_compressed_data_base85[17270+1] =
    "7])#######H_PAm'/###[),##1xL$#Q6>##TZ;995BEL%AiES7e?'o/uRdL<(jlS;J)m<-vrEn/3ExF>'aGI_u*m<-DA^01uZn42Ip*J));iu5NqEn/aNV=B-b;1E#tEn/K->>#Hb(*H"
    "MY`=-Z>:@-N@Rm/#-0%JbR#Zf[MPH2DQT`3t-d<B2=<DH*kP]4tIVX-TT$=(_;^m(62MY5+tEn/<_[FH4YOC@;ViiLuunlJo6Q<BEU:'02ccLMCSPda0iR/Ga$#Mp9F=2Cv,i?-qmnUC"
    "DO4lM8+vlLQ@WfU@C=GH3p7VZI4c>$G6`'#WP?(#$+fh$ROc##)-bv5$LkA#ldK#$Ou;9/'ISs$gdfS%84XG)&V>Ku4,vR#k23m9.O0'-$#H+2$.d:Q>q/@DTCDNu1*P:vtu4<%(f''#"
    "()o>/V.k9#A]clN_8NmN/(G9#us`=-#)Au-X<&WR-uFT##NAu-RuFDO.oO$Ojh_U#3V_TMv2C&Xd9An'6]###8V5g)6QK.$lcSd26xw_#>ewSuH[ELuZIu6upb,+WY6tP=1;+ru%@e.C"
    "'F_fuCW/a#h@uu#e=(##6Ko9;uMW]+mXHq;^7K,3#DXI)dqh[,.UEj0=ALZ-J29f3n3H)4cS,^XB9PiVj'r5F(Y_,GcrsO]aXT;MZcH.h*$VePXAbLpf.]B[&O]6+/?FPV,CL<E3vG8C"
    "DLruBA*B>#x_N1pELsrm@%$##quL^#nsFU.MDJ)*Ro29/*Trc)UQD.3crIv$A0cJ(g>YJ]%Fbbb,;[sZGk#@+hXOdohh3Lca83e)%EOb)xT`7q$O>V^XaTXMMlVsulp$cr_WsCoY*P:v"
    "8'Pr?\?4__#S`($#*Mc##mi$4vO(x+MCXkPAq6@9&4V###,VfZpro2g(R:M;$WnlM($mC^#fgUZ#DW;876*I(a4^7V#hqo<nX%3muRWurusqAs#VlIfLNq$3#blqpLA%3$#iH2WJb6+7S"
    "0*Il8$B#SIjITMgQ@YLk<.N(M,CC[-w.9kO*Af+MN#59#:cs(R-uFT#cLAu-=PGTOk+gnNfFl9#)I,5NNU#oLfO<^OsAw/$/sCc#P?];MEDcGk_gEw^J`N1pPnsrmX$i8Kf<Ss$b[-TK"
    "SY'-4pBf[#h`A7fOKbc]l$.uWI(hV*?%%T]N*[tE=ffLd&+]l#ZiR:mIlru>Xjtu>#iN4R6I$^oe;G.q_RRg]N;G)M::-##%%-Z$JrbW%R/9,NGX@N#<wXQ#w/sWuDm*TuwtiKt0U<Lu"
    "Bj^wLn=JcVO?D#$bE6X1LA2kuAS.Wt))b5v0$Y6#XsxE%;8;h)*c68%MJ*YY2c8R*=K^w0/g^8R&IGcM-uO5vIC:=%g6kGO,l+9#]D:@-]&kB-04Q11R2MG#C*,##c0ST$_;1929PUV$"
    ")ZS+4.M0+*I5^+4+C0N)7pU*k*1aP&&ZCEH*,gXuZ,gU>8NH&J`c%`tq^>38)(-Z$rYa>.fE9D3Ud%$$sq4D#uHNh#47:hLC;-?$lC0F#K8gEu/wJauEg7>e@jNi9vGI=S6=uQuQikm#"
    "3`r48i5j>$>oWB-j*t^-m3_-68s1g$8kI?lAt?v$w^(EYnR^Y,'F9a#QIxNFi_vNFfCI8%w2gf1m`l]#6Un`%s,f]+=$`N(<[YQ'&Nx7RwL$W$R/s.3[jf8mOdYe2(]iV$Pce8u)YErV"
    ";s1OPO^j@t'xJ<5=*3vA^'*CuV.ra21C,->b^jCuUq_d-mk`?Tx?o?TwrA+*gZv)4a(xY-O^T,*b&CP3)P)m/n0GT%YZdLgie269`QUm&Ph&<eAHJlEK.iM'iFq#m#h@ABv0DDu.lJE#"
    "?kF2K#PC5/A($##RNvLgKW+;H7xk+D*#QDatbni'*QtDYs*-k(R:M;$o[k'4%0Tv-]rnh(SMrnuY7VQ#7l4S#js1muRELgnf66Hox?]V%Z?)YY][b:Z.']M'QlIfL)F'9viN(fqdL@XC"
    "UTmtLfEOJMFGl9#h&krL.8;.Mxu@u-8EH*N*ibl$'HQaNRhL:#J79x9w9qR*hr_`<@[wR*u>lS%u.OJiA&8qr9>(##%xTMgDZ_c)YF]T%D39Z-UND.3>U^:/W([j#`?=mI5KIlu&Im2P"
    "HT6r$:D.rPgjYguGDbdT,GvB8dxk+Dh.MDax00,)q?E^6wui,)u-Wv#/BxF4'Ras-VRWu7,mpkLdC<>#GtqFqegKZnhm[Mo:pNu7w%6wdO'HduCF&`#18h;fMD;>&Ob<9B(F5gLm,E$#"
    "g'=m/mVWI)RiXV-PeP`12Csl&umOP8(e%`jbMRbe*.Mehs7>CuLDFAu.VMiKB]71#.dI)vj$3'MHU`'#m8pV.B(C+*L=sM0i+39/ND#G4:d^O9<9Qv$.v(C&[Lgm_*tlb#kextuU0@8%"
    "75WTM-cmJY6R^7s-ws#O-u;P&CLFHQX.fcOb>B6N[8+?k:^A#Bu$C219'&XM3:ZLb.ZchF7rdO2,p+MCFxL-dlwsb.%H:;$Z.`q5=$(,)Z0fX-e1<_=S>)A4dQCJqG@[.(F>RfLXr?/v"
    "9NfnLS9MhL:L-W.&#_'/jvPT%W`(Z#ho`Iq(7Pj^gOqQ#ad_IqVp^EV:j8f:T2/6Pd`6`3@A&.vB^*@99m>j#m1#qrQMc##%M?C#dFAu-p$clR6UCU#fq`=-I],[.%-nK#_%t[:(J+U&"
    "EHkA#rrA+*[=?lLTd'^#Uk`W#rx'<-xZ5%7IoTS.EBVMg$EDKtbK0CcUe@+2FHf-6qMi*Ms:=JMj9MhLDjxF4mGpcNpeYV-.3<b$U[Z>#Vq0J#Dc;o/9ES*+);wXd9u4.)&TrR#YYbeu"
    "Wb(u-x'?,*u(2'7m;IuF2Ij*$0nYGCi7TLkIDDCt/:c.<@II&[l&?AMG&](#8sMuL*uJ%#XwH>#i6cOFG$>`#NOR@#2:fu76o6E5`7q^uK.`Zu=&S%6,5uc4_x=u#B-/t>GxDsJ3#D?u"
    "PsFY#fL0F6l:Tbu==mpLOH>,M54rP'Y_rM(nCQ29qgv5/7-)Gu8.<D#C>5f*8vGA#^]HCuJ$p&$ki-b4qZ_2D-p39uE?*%;g+d1M;CD5/Wk$##O3$Pfj_2;HC?x:HB18>c6b/g)e[M)4"
    "ZPf]%2Ah8.ZH7g)7Cn8%9%x[-$C<>GaKK3:a'Vi'bq.iTo2d0;EIHon/n@R3m7wY#7ZV=P+N9lL^e$##rEG'v#Ue<$[@O&#WPUV$>?9W$VM#G4=U^:/vBo8%-n-H35Hrc)Z_j=.3:,-3"
    "rQ;Z>@4[s$iL0+*QDVeY1W<uEQe$[AU*&LqTr]xXBoNO=D2W,W';'v(bDb>?hR'f;2SijOCD>_+fXe@@w,IXRq:ixRAgIOTw#ZUO(q7e)6[70:2dh3MJ2Xf-j'C9rJ3i9r#Ykxu,k%rN"
    "d[Z##/o$Y1`rZX1#q7fhAWB?.$2_)L4b]`XWZvfi<Vf%O?'.aNHrjj7T++,Me/c`sm_`f14hKfL&.Ds-Tfc;-bo[R&aW/o88c5g)so*_$O5Du$?]d8/jZ_,)a0<j9Pbg/)iiOjL*?Moe"
    "m]+Ml6Y'[Adxbs/w$8IERHsV]?6fVPd`_JU8_nj+Qk3xbJ5dJ3ND%SG<B1]INN.m&EVL`IoLw7X7GssAXB5T%B8A:9QW-=cG++w:vwi(]K415DtF0m;L;F(A4/KDRk6YY#EtO1pPMSMg"
    "[Clr-JuBs-ei?<.8t@X-RE:a#[7,i(TL.s$EB0W$:Hf#53qjsK>liZuFs6-)(6dTLfx;#g);nqM<kSOOR'Vv/Un*Tuj*=Z8beX+`b<9`NY#U?URiIUWSexb-&jFSEo8%UEL)K=uH/JAu"
    "9LP]SgS-##kt7T$=gO.#rH`*1KJ=l(neX@$8v19/98.T%4_D.3X5Du$<4jOWbCS<q1&/v#v%87J#cUI6()NZJO*t]uu4hM#-NuP#o`=tW>`+Xg0_^CBqNbbX9+P:vx);'f2Y9_#5Vs)#"
    "*Mc##O>3jL$(^fLX?KT$5,m=&],h,*[4qkL8EZ)4BPto7n/AZ5n2XjIU/c<Cb5]R&8n=J(s>R3+71M=LuGkU)g_o._e-4GMB:k^#+oIt[kQ5mhO5;OcOVjafT.=GMf0AqL@mJ%#)X$p-"
    "OK5ND<4158jTD^4;vS=rb=BaVw%W1u:SWBKTQ&lprFr.1jtFKL]M5S*ec$8[jxE8RVD1V=<=Vq9.]<l#vc=ATdxA#v(%h'%pW?T-FCHP.EK'*qV==#-@-3<%=-KfUEHAu-4kB]R5L(:#"
    "o^1.48F.%#0%@s$S_R%#]6E:.4*x9.4-P9Bv@jJ%xSp[#?DoXufT2TULV=1+gSd[-dLG5A_'h+$^TphF#38r%FW.0:j<]Y#mPJi9etq`Ev5#,2:pVv-DhOo9;Bc)4$AC8.NKc/1`[C+*"
    "+vsI3TeRs$R46i&W]v'e9kfX+wBO)Rj*ItJRBTgJV)GwBhVXO'.@d2S74`XLb@ShuhK+P<DwA`IYSBG5Smn&_G7dv@K1SFV3jld#^Yl<V;Bb#U]cA?0Xo'##[nPMg.7m'?'@uM(vR1H-"
    "OBWl%w77<.0@DYPv%5##MqO`gNt#sCO+nuPkZQMgD^iwj&7j`#:8H6N#)>>#.S_P1>5=]#AI5+#)GY##'[fe?4NGJ$/6>##(C*R<.DH;$$9LCWQKM#vx6YY#4fd*N)id##=lG3#pKeum"
    "7Ds]P=0mR*`/Tf::CvQWUuUeNd[Z##O+?X1Jds?9fO9r)'0oFiird?KudI+i'>)FIfD###'M4I)O?Z)4I5^+4*&USe-Z=K;>0^_/h.8]h^UR:vA*FGW)PYD=sqoW_$fDX_&[/e$2w^dM"
    "2qeC#QC1gL6VB@#=I6_u>?[ouH$Q(BmS)rdYtGY,2J:]kE'xnNnX&G&%aO=u>f-W-pDH-dR[FM$87</M?BgI*JjE.37dG,*9AIT%U[w9.]77<.rM[L(,pw/1UH0b:w6j]S_s@uG>tReu"
    "]v,>J($aNI&-rAJYxPi'd^gwA87*P8dN#5DZ]Nx=3G'?$0^8%872*XCkP-R/#<-3sI,R=u_Qx8.S6FIp]BJW-Lj_3Xc5X&%Rd''#%$###lWhsF4*22%N1]-?tiou,*6B39XJuD46Gl/M"
    "qt@d)RQ&o8Yt+IWdL]<q,+wG687J&@5#UYIQ5Na3^5C7[_;@rQg';xCK5fWB>LmKXjtTH%:3d,)G/lY?sJiihB9).Ms`iX-P@aq)o45igC'.#P*7q(kT*fs-UX&,Nb_d##)?x+%4pQ4o"
    "A0Au-Add^RML(:#C5*B/Y^ahuY]1xLS#B%#X$(,)n`TO9=k><.d1NT/FfKP8sEYDO>AndF8vi$Bd;]R&:V+$IU4+PA)N(b9]tfs7S[2rZH5fZIO@6##5T]F-Xv=(.ETNn8c(OWSb>sI3"
    "F(U?$J#Nl&el3$B-c29.*63%t(]i5/+=a]=.2w89MDXM_^&;?RZ[-##[?uu#DlX.#hE9D31-KE4+g^I*r%-98s/(<-6L&8SH[r3(6-8+QMkrEB2UfT$s10Iu=*,##jt7T$0R=c/MPUV$"
    "us`a4]sFs-08m598_i/:&lgF*Vg'u$txe*b]qvH1LG8]Sq,UoGhYh9kQ7-1C4T9V:slA>54j0(X1F)u;['Y50_0]wSr]Vt12-#F5$jhn/_pcnV2(ea$s%9m$co80MnJO4:^v>K)[(9+*"
    "bVeZ8^*,d38:JT%Km@d)uwL5jQ%h@NBB#,[XdhX_/5DNOm`V%@+Nx6Du+s6LLh<#]8Q;M-P5+)UFspjj&)*$J*-j%@qX?gtBX2eHQ<)fqAQeGDt:`$':%1N(fwe+%0ni8.]p*P(k;oO("
    "<qiuP(x0m(p.+n0J*'LZh/I;$PTLSuL1wu%_4wQ&su%&FO_(ZuM4sM0wYmS@B7ff(V7=,M*TtfMK]-lLfd><.[=*p%SuJ;$C3n3N#mxt#<CTndL6$MBku8CJSxMCWHMFuu1^M:MMd`,)"
    "]0QfCZuxfLOF-##e*FE$VMMA.Q$(,)G^G6/`Y0i).`IN:wP4K1n:-v#F4cTL.CG-3iB8`2T]L&63K+PA8`O+DuiFWL1U'rdNQLmZf.vD>6BJ4FV.)4Fv<?lfhit7R-uFT#XLAu--3IYM"
    "d'2'#6jDuL<1ld;suHC#K3h1Bvlc;+@w&7/sCSG)Uc`:AMkg<SG0Yv:$?LCW$),##UYqr$eupi;&/^G3[_u'4PfOQ#AgQF3J)5f*hql4*d`*f2,5@h>_)$mu9vP11;9j-*^b%duwsK*L"
    "-JQH#+-[w@$.u8i5PJpt5^lu<]/[2aL.Yi;Kvg>$mqno;VrcG*[te8.(AH>#.QW#5%sKYG;+cY,^j1G+bj:vH/SM+V&5mUm%tJw5rhQDO0bH;-h97l3h#W.anlmUd/u_O3/H@(lo+BT/"
    "aFcYP?$00)i+TC48la$[$7G#%?]d8/W_D>?hf`Zub6EUe-Gf-)8rkT#3]fG3]0Uu@h79j%r?qs9(?(cV<0In:`ollALkMbR3)P:v(&S#/ek?5/Iov&-h4]>OSP',Nded##sS+oLhF-##"
    "LH:;$D/6c%VD###mS%&4ZH7g)p<6g)4NWDuebFIu5b$GX468dimptu%T7YY#e7r92vDSMgHs?D*WkQX-Y7-v&wHi8.$'lA#q@8g1OJUM#jv@`paH<TZ6#M*M(Om4O3_Kq$/3ZV#bN%wK"
    "(.<uco2R<C6?-R<6g6ong4-Q/>f-W-<HjEcv;pO&mIS:/GnwP/4t`a46?9r.3Zk;-Zf2u$];u(a`kgCL4?_dniHUAMaiG3(&*ncuM_)KAl(0DZv2>Yu1^:M5_.27NY&*jK2^sFMlhT@t"
    "x_9*V?>?%B]hpV<3X7C7[=0A><hEI_j=95V9vx(AsUxu#`_3D3NscGVw,BJ1Wk$##RCRW.p&%XLLrK.=hof+46*l]#&$uWU8`4F*m]WI)TP[L(N29f3/EdJ(XiWI)4lv#on(:?#)A>VD"
    "'JVgJ:f+dQxLBYuY-c>BtTx(Lw+k^Edwk_I@_;BfT?X/ga#Z?#;bxEblaJnG^t#.&pf@/>IKElA`-#eY_%0h'5^,C'>$P(?VRI@#f`_q:$=]Y#'0&##SUPMgc_18.$uF)4h?7f3XN,[T"
    "w#a;%&'*@'knRs$Zn;FR-pl?QZMMugc;phmTu=RA*B$q(t@@VYxZIqCmF+psL:KOS@JAD'Gm+l:j.Af1oPXL33X]ZYwE>Sehh>7N?.>>#7IR-HJ#U-Hcf###PNkA#fv</M%w@X-dhH1M"
    "6huH3[ovC#>(`?#vH-+o$`.)3>U:%t$eqo73RvtuLn*Tu$'uIucpCQW$le`3$f0^#Jp,D-=5.J4fPZ;#(F9D3:LgU/]DsI32pxF4?;;.;bxlG*+]L_%L$:u$^>8+4[xA1(Vr=VZg,ldR"
    "34MMk6ZPY,J1*`@pvCs$*uJlAcn%c5Y;>2r,LX7Z?@x]_:lDvgvSLNEFpjI])FSK+(c]_uE=f^$=OMo$kh1$#?PUV$epq0(#&_e$Zd*l(##A^$/xJh(e5`Iq%Jmf1B*TM'%vE.hDQcYU"
    "/:%vUh1AVQP+u(E$[V%FM,^4Stq9V#A;P>#Btw[u4<*1#XsQfL*u&-#FB@s=t=ED#4*x9./`V^#GRZO&eHii9$#H+2[^etu15_W.PH:;$/U>`/sE9D3qTwF4*#-W-3Tj#noUJx$b$eX-"
    "qFmV$P9p7EFKEfLjb%jlis?hQV[#&ORaDxFZc/@J'?(]gH%aFKjOk7P[7D^k9(#HEp?ZS[L4hM1#)Sv-59q+,OBUT&X0<v6guT#epeT@%ZSS]OEu=J$)GD/UG$3<V/?.IH[m+7Dj-G:v"
    "+]kSQN(4aRpE1lP][Zka;(,xJ=UMBf&El)H*<m5a,3&RGt5fo&Uin@KXx&##i@nqL2aN4qZM:5&2P###8IZV-J#G:.H4B_4nPic)ac6K#FGo3v3u:Zuc,1Lh+?n:-AdMW#t_co[3,>$#"
    "H>wfD_ZxlK,%6Lsh[Ll%E;Rv$06SX-:gUD3,dQnapd<Z#7U6AN1l$W$/L1vTYmHQL8FC($)F'[0G`j,@.WKmNVeZOaeB?SVOUrU3wU?##SG,p2QHI7QP>l9M06JqLrd.k$&rID=E^hPM"
    "`_J@>:e0^#Q.G(.RHCl:$H'E<GCTYWpf)s$nP0[NBqK(*H<93YMS=iI$9k5N0a]UK+cRQ</Cq6:OLbHX^=45vhuVUFpIVlDeZ>KB#uLUPT^UqiAs=i<p274M=I4R<tDFa=q$`J:%4e;-"
    "F^H.%6Y35J;Jf(s?[JSnFi]ut:5>##7:12_WJFm0Mapkr.#+`>D62s^a7Uh&`'M0a:a%d#2>VXP$G-)HPewRQQ@S-Qxsm1>xWJ_KJm@&J?:J9DWA(4EWUXbEWYApUks8B+(Pax]rWr2H"
    "#?ar%CgR*AA6<@fWpJLOWbu9LTs?ZB(jS>BWT^Y#U,lSo#Qqon]a&68?ZTj(2=Ss$5>Vm/m,vf(N.M;$xvwr'7@c.P@$*@#v#`ZBREAB4cb]?gg'?'WZhw'BTBj[X1I$7:kgYUKEOup)"
    "#SOg>8@MNpL,Qmj`wdANlgaHX1p;5v$v?pa2XWOM;@4R<AvO.=I%xu#i+3loNTtulv/^f1(+6K3X5N7'='?A4+WkL)iBwX-jZ_,)/K,^=TjK#$O2Wf%vo3+*3IEpj^7tj=$ZN^[jjFf@"
    "K+VH#V5;Al):;qu/d'=f>oO;+D]CqDj5C81NVoQ:95@CF7%@/OvV7+YLK0(B7)t5DP4Mau=$;X:7t*'vjt(E#l*Oq(Va=v@UWm66fKs`5C[jd4:6YY#e8M.q>wUMg^^9p'9[ww#mr0s-"
    ">;gF4_R(f)sWW&)x6t6Jk<Ms9.R?C#LDj<uCraX?_&LD?6`WPMnmxRR600sHrU`/)2w;f6cw6er2nLI6Z;S;M-:$#YM'Fb7>?Ed<cgm(sGN4W-pFQ-Hjs6B$wBr8..h/9.^fse)J1[s6"
    "3Z*=@#?B&IK>D*%#DOZgxB43I/p]M;b(;w@l?0w@].<A)dQxu/`Mgb>5EsKOP6TZ]XpGM6=T83<>S_aOpKKduW57<$]4=&#O,>>#gCGv$s1Zs-k/<+=mn_a4#IR+4V,jhLT;Uv-A.&J."
    "0Lqw?B*.g;xF@e-oF_XLe(9^#L(Ae-^$lXLsQ.##t`Vs#[Fh'#F$(,)^cJ_4aSlY#qo[h(0/`^#6XYS;Z3OI5ZAD4r2kRPA0C4I#.<_jurQj,SF:8tu.Z$U#=+,##VcEB-7@dZ%]+^h("
    ";4nY#P:,i(aBf[#5hKlDA?aU:prIA8e/J4ogRHC#E5:N#_=D7UQiOmuB])ruNc>_8#8>_8CB<_8TdrQNI16g)JH/i)0Kx+g984Q%+Tq[=?s%F.HPI4o[.LF.%<oO(X$vM(34VT#M%`Pe"
    "FiiusUu9W.Gir8$uKx>-X-B01rZv)4/]NT/:5^F4i-^g1-d0i)$sse)JCn8%0x]fL/g=/*w<#]Qw=@MEsYYI>r.C>Lav$$AaSg;@#D_28LjxtE2kFq$ZbE=W4apS#+&X<5WX;+*&o[-5"
    "#r<4NHn@)*%FMS.]CM)4bGfB.k@S_#^L,9/ocB.*CAfB.3s1u$l&q29:[iA.I(g;B_^&eQ9T3?JsbkjEN`cSd*pE<5Y_D+*$fR-5hwe'e#46RbA,^.qi%Hlaac#*#7xk+DvpsPSw6187"
    "jS1bIApL^#%[EU.;DJ)*K0378BTlS/dSYW-D]&PDIaDm$?\?#<.epq0(k5=YJWh[^&/S^;-IOWu>1;M`W(e(U#Nf-B/mfSc6TNZT%&qhlNq[51bT2_1,Tl+f3f(siu')Q(W*5%2K85O8i"
    "<Kiau4/vBJj8]VL'(EjjG9SM0C0?YYxp_ruHE?8iD`9j:0iw8L[fg4k#/nqAP;uU8#b/R#kPh7IbC7fUHs/J*=Zt`##'8?U>%HsL#ID>4BPUV$[%F9#^3=&##$SX-*<4:.kJ(.$MOmV-"
    "JGVD3*p'.$84h0,LkjK7A]5#e+0^Pgrm&pf&fkA#$k?#5tOgZ3*haj7f,358lWhR#$,wK#VM#v5Ssaj0mi`ou2W^.LWwtu5*wglL?;I/L8,98.Vh$##B%NT/Y<#;/>;gF4av%G=6p7I*"
    "9Ze)*ig'u$pH3Q/F0Cw.J>SHus2DOMB1sb4Tx0;Ms5IBCxL(@R^Nu@6Il^7NxU>=EC2%,HB?sib7#7])]lBnNh5R^C*Yl$Ragqx6g$<pN*v/[DhA<eOcY+T9DrSGK_F0aIbl-EeH->>#"
    "fXGV-*-Da*`S_$'xW'B#+NEV-EQi4%C%ns$-'FR/9)FV-rTV5]Y%.O(A:+SnrtH;s-[T2#w<k8fqs3J_CMSk+Q8###Hax9.PH+Z-aQv0#<j5Nu`UZVu`NulP]fm>#[DlY#fi=&l$86o%"
    "06)BPTh%j09:,@Bd_G)4]h9iBXZUBi8Il&#*>G##Abq>$ZM-@'A$wV%F03W%hwK4+#srf(U+qrQk-m2V=1J7&_ghV?HOI`QiV8/?u-qf(@W22N<W$j>L3v&#Dd?g#PF^,0'G<F30F]`^"
    "1G`t-OG1xL%r'8/D&SL?fs<;NXKXV.mZ$K#.tSV/3w=Sn[v`]+>Luxun<-L,,jqpL?>)'#j$###r(mN#c'>uu7%Gc-)HMR*#4LR*?TSl]?mmr[$GPPT&ScPTjdLR*Jp;fL3-8`jpS,MT"
    "SA(P&:3WD=IgMP^J7cB#lQkA#@r(Q#Nk$E.I,?PJ(OQ`k$]x.iQ8QEXQ(n7n^gOP&pn1j9arQC#hiV#AflV#An1[W8d5ns8m`$mLw[d##ovd8/wi`9MgR](W<s0F%FG[(WQuFkOoMs>$"
    "Ie).-5=c&#S+XCNjV;V$LQWjL5?cG-bIwA-ST0S/E]'B#s:+e#(>$d3<='6/jN>/$o[j)#Mrgo.GKkA#igDsu&FgM#^I$K#n*'Gi$mt10x6KfLhZ-+#2U['N(%wLMg^HF##xBW.#6Kiu"
    "f91w-PlRfLbRpuckjd8/RU>eZY@E`am$&F7.$OZM2$&YM1q`=MrVo]%dl]]+:+9,M*8###(dQJ(KWPJ([sgUu'F:R#T.SoeJKiIhXus5vQ?vA#TE]w0XOww0F^9[0W.]p.R[l:-$ed19"
    "GY2SnVq-##`Wa?#Q,5YuX5YY#@OlCsf'>AX]6Y'A*44]-jQj-$;-)6$$IA&$MBH:l_YSXk5mX+`(5&#Y#ZPP&YFBN(V$vM(H16g)BPXS7$SScM7K<hu/0,>P0pshu5am:H_nc2_)]trZ"
    "&w+:2*8QP/^%3D#=xG>#/+BxXpWfO#+,rP/uP>ku#*mf=1$^-W,R4;?pm.P#vXc=YLbOfLH6s7#Ei7L$QV*30Mqn%#Z$eX-$Tkp'xEI5/kH>K1>Cd4/]rgv@bhWv@OPRF%8LT<jt17Z8"
    "u95T8hO;W#?+nV@LLi.MUuM7#a%SL$3T2I.E$(,)kDe;.Gp2V6H&OKl$)wIL1?W-$r&E>h$3VFrBDJX#[r?ui1VY@0>l%2Kq]gM0O?_-6-8=E4ae_F*`^D.3u>K.*?]d8/ec6<.YqgeV"
    "P._[9[SD[6rs_E#1an>Mi#gk2TX=o9:RlKd:@At/j9f(d<Ifp0ICu,#UOTU#Hk(($W.xfL,o@X-gT`$$T/q+$jqKB##Kx4/DK[(#U])%$:%###GMOV-<7X%$HSK8g.qor/K^m$$f;85#"
    "uaUi5&U;8.@3WJ:=,oV#U&B,'uUQ_##AEL#P?DmL^o-j##7*l$4$:hL2vU4)Frds.RHUD3$,)Y$j$60)Sd[Es0WuYu(Jie(5JPU?$cdS%1/I[#&+/HWdSo<UCc0^umjhWM:sIkuVI:;$"
    "6*lr?V.wrmYC18.9oNY5Iifw-h?7f3twC.3m]WI)EOx^-<oPu@WeA,3YG`A=]K-_#c0U^VLoD$hLIKF%W[]JcLi?iIXXL&4H,_QM7vm)G7]5eu/PHJ2&mSH#1xWw0TbI;@ncSsRN10aI"
    "=,ke$st+E^A0WG>9=Vnf7@b31t:$##$r71vlZOi$dv')0<%T*#oD,W.D@AJ.nm-H3$=Ap.VXmP/(=Yt(ELD8.>b/q`g=Q>#x1Qs-19.?\?&(gG36Obc%GlX=VYs[NI1s4)*$(3)*.7L^u"
    "?&?cdT0BYSB;TF%D-wTGhEA*ZvuLG',thMDg=wi0Qc4<dQ7v[C42KP*nuWn<:xT+7g&3^#4'xZ6]d;B7W.,C7V@cJ<]@#0?Sc)##Khd,$*2s[$d'&'-@j&?&ug'u$L#sd2P6&6V7hW'B"
    "46)?ArEqsAMsl-$A<*jL3o.6M>fipL'^B*BnR=<B8uapL<(u`#N>Y^$/04&#P,>>#tOQR(eV8f3vBo8%6'u,%]lWU%TtQHZE4w%IwRN[@EWs8fsrf4GB)7ML1iRY'wR<M:_mS]O=3SmD"
    ")NNE*XiN,6S;P+H@asHu@9Ti;k3:<AJX.w#uraI33+-l.P<p.$n+UqVIIaEM$Yg0b9s.T%;AbnDw>bnD7:ehbWsW:FXj<tL>f$GS2SxDSFU'3#(<M,#dRJ]$Y$M$##*x9.*Zqd%<_fw#"
    "'Th1Wwwc.l`u.T$pjC9J0[YwgK$g0MAa#eu%>QZ$@fZ(#Sfh-'#]NT/uZv)4dQ1P:3Xmek$oXI)IqiJ:.2?e4iA^.$2S>c4#8WT/]Cn8%',aY5Z<wCkGiPg=o?\?`u*LBTM8jGf=14P2d"
    "6'U(=.2N>A.xQHD6<^e#NAK/9p#2&$T/i&vjWP`#8?I.h>gFh;I9HW88d6t1jOhQM=:`.N.a?NZ=>eF>CYpg<9va-WqX/+%Bsc5=0w>M^elb,4NG88Jgob,4u)eNbfoB+$w-Qv$b<4gL"
    "Cgx9.>A[&4U1nih1#43%LI<p#YJq)K?w_'#d@E-$#[:7#P$(,)A2@*<V_`#5LvHd)#xUJ#X`P)>iC3@e:NpqaXN3qDE4SMKGYB)3Jdob495pJPUIo6B5a9@%#+,##UY?,$=Lu=8$_l8/"
    "4>;.;]nk%nTG6^=2f5g)R%*U%H@$GiEGCI#FGZ>8eKnP;$rd)>CoL&?I3x2WK>ftEncxuB2$KD70r,N189jE4./9q/E(Sj:ooU:D$N69%%OGR:'f2wgcr'/#L@E4:Zh5[-Wh-Yu[j2]b"
    "MZMMgB$k;-9XOv-pqUu7uc6C#w,Ag_Ub6(un)s/QI).I-G[P+#xwI1$&Z7`=jX:K*oPft@S)W.3U2l$6.(]mNZ)5qC4P`2MjZgL;:^N.3$_DnN.w#8L(r4NTxjJ]=Ae#5fG(_m0JuBs-"
    "c?rb*`HZcM]TS3%O5fxO^NE)$`+/R1x5?<8DUF:8SbtK;2P2#>mtO'SqMZ&@sk=:Ax@^c3C^()P7ILK2iWBQC4aRv8J:TQ&pN;ON:HV&#q;^;-0]Pn%#ufi':Ne+4FF+F3>HF:.omx9."
    "8TvF#o^hWBLOl/_^=tf1^kNKH+`ac;AQ8$#sH<D<x_WDWxSOP/V1>_8iG1F%pkto@*c68%?0auuANf>-Y3Gv-Aj5TM:g>oL2A2r$24RA-)wEB-[''$.0NhhL[qQ##.#fP%Sbv;#/93jL"
    "P1](#lPUV$5=t,<K^<=AV9Ss$@6]q.[cdC#WLGb-#xte)0_dxOlgo8%Tq?n:^WAhO;)$lt,lYH2ak2]buk8AXp-G4:*V8(u$Rl)3Y0v=?]k+(?Mkw%R]En%R;5V.NOV@+32bUMg7S/`A"
    "H008BHh0(6FPR+O[[Q:vjD)2p%FQv,N0$Pf`cTMUOb5s.Rmcof3FLB#;Y1v#BBLhLi9t^MuDMj$J<fp$c*m<-Yge6/Toed#kv8kL1_::#WLK`NPr?##'G<F3XP91u6&WW#PAQ&MN#6pL"
    "kh)v#']u+DS%]7Ngv3GNG0QP0;.=`#.Jal$=D4#M$,Jk$/-%%%l3UhLHjd8/?;Rv$0fQmq8[cH3q3Cx$H6d.6Xg^],4/20$w[;&?w$QY%**hb$qc&+4pD?U2RA38M9hPe6G].h$r`''#"
    "CqKfLdueqlXRT`N0$3$#ot-K=L)tY-(2NK1I,*,=:g8G=,pWg5S%Sb%Nha`<5.)i<mxVLN'[X(0O*5Y#F#]%#7Jg03'V#^?S+.^?&fAx=Ae@f=qf?oLgj4D<d`N87PtRS%3j?T.ZADD3"
    ".A2T/GS4W#k+wi'Eco%tI*4]ueE85#$*iC$CNZ(%40t%.s`9XJ'8)9;aCEnLnB+o9#?l-$C=a(9Dc:'Hi,tu9S%ajMT>5R*RYY2`aQs$#r(^=@WP$9.T&M:%3h'u$rK963WAF@?$a?Ab"
    "n?.PAM0h;.N=Y$?vdV/=fKrv$MUvC%NoSn$E`:m8vk7oqZm'B#,71BTDCU^TUq(t8l#;99)GGVDg,1;D5=3_8eWPW8N?XmLJ=9wu`CE1%15>##c]LB#n.MJ$oOMSBFd#qLb2*(v4G,J$"
    "&($50t&_lfqS,E$&?Ok+_ZR]OnYxi9U%[0)lu3+3)$R]OuJPfLrwSfL/6i:#]a($#=Nc##FBw<(B63x'IA$'#<>N)#m$),#GaX.#-C[8#0uL?#C%^E#)b7H#YGhJ#sn$S#@Z2W#tCcY#"
    "M'=]#fa@d#:2Sl#/-Z7$WK'#$22W%$cn1($U:<-$xc`S$;i]A$K/_F$&l8I$iibM$/N9Y$Zb1_$QOqj$g0Bm$=a`o$mIL7%lp(,%A=CR%jn+>%i@jD%B'DG%9e/i%d%sW%SbLZ%/H'^%"
    "^(N`%KE%d%KOah%5HTq%Jm<x%JGZ$&x','&Rd[)&XN<5&XYT>&0U&I&Pi'j&j]`S&DC:V&rV[`&84Uj&4n&m&BBns&Wb0J'HaN9'qH/E'N0Eh'uxPV'L(4e'c,eo'MkK+(]4x1(=Cm?("
    ":4-G(>guJ(HELV(IXki(2E*t(sh`w(]g_$);YK')<a(,)wh9/)TNj1),#)4)ZXO6)J>T:)*+8=)_5I@)RLmC)8sLG);t)L)K':R)>NLZ)n^fa)W(=e)fnYr)kWd,*HIV8*kj^F*#.4M*"
    "2bXU*<*Ax*SGcf*NkBj*;-mv*c9Sx*-E:$+TxW&+7tV)+KV`S%)-?uH0tC+Wg*$oo4nw=-laVx5$&###j.'58Ooal#odPir2il##XR-#m##..NsNQ2tfQ3Q)</^(MXI?##T?*1#;i/E#"
    "C&F9#g(qV-9$Mk+&Dm4Sv%t+DAe<#dAMBGi8Kio]=IG]u:xWGsH;i]4$f9#vj$3%v'U@6/p^D4#D;PwL_p=1#x-A-#Yfd*Npr('#*0v/$JmjjLn7LkLQoHlL0,S(#%;^*#7%^(MC(kfL"
    "5,t204'fDbiF:2^3ls-$K;$;HbH<G;;4d;%17+;.[5#J$wNXAYY>7,MsnS+MBb4kQkL9#,=1m>#upEvmsSr`*tZn-$'r9#v:jnwugtG<-fgG<-LCpV-^)=F.apCa3]fKYmW%P`kn,V.$"
    "$a/&O,GVPMiP:d-V@Vw0bq+;?:8l4ScUJG)D1SRM_JH)MPI#-Ma5$6#^0P6#6-'2#et=6#Yq&g1*2Wd$OQPJ$Bl*a$;a6QMZ:p0#Oiu##(5T*#E'J-#-wo=#iiI%#=.v+#Cag81Z=4&#"
    "+3Z3#EC=&#@S[Q/$1FV?_ex@X&%$?$OIxnLYF,e#^v<$MlA^nL8fC%Mf<V+$,fMuLhZnC%`QH.3iBBY$RQj-$OW9^#QYot#RKdAMi7?J_`:L##X@=gLXE]8%DXxr?OJ,.$[6Hoe12l-$"
    "Oo<`Wk=s9)AA;SI3)#E=.&;PJr-SDX$-1_JVNo-$%c&x'Gwl+#*ZlS.2P%%#*ZlS.,-J-#s6T;-L7T;-C7T;-/7T;-,7T;-87T;-47T;-M^rS%uEu%bV>k(jmI;&=_mfD<2I<a<$3VYY"
    "N$28[H^^rm;Mjoec:,#P$hEPS3)`cVB$*2^*j-DWg:_P/l$_(s6:'d)PiC/1q9hcruiVAXg;j582Jdp%i1=PfRMp-$,.OVm(ADJU*6mxXV.dNksMp-$g)p-$%gp-$.OcfLq$iP/@6CG)"
    "2RSM'XFrlKndVG<omrc<[Bv92J5jC$ba3=$V/`($/R?(#+c68%J<%%#Q@g1twxcS)eA;YP/?i:#g@W`#+BP##3SpkLV6$##vfNj0KV8VZpX(##/i_v#Va`^#R3[0#]8c'&)^OE=mVo+#"
    "$^W>-UGfB.0,*Mp8CcA#x[k&#^tPMgD(k>-D$q-6VC9L54HKe$+jZw'`.;;$/UEF%<S<X(5.x+20C4MK%.Duu?']A,VMQ8/8[#.6clNY5s,RrZ9L)d**e_M:C=Oh#^4)1#[%-/lL*,##"
    "Ek<n/b$Y6#J.>>#O;#-M'@@mgD5h.U(KBVHq#m+MGqG`Nd/+M^t6FcV[Ia(WYOA`WuvUuYrNW-Hah?YP-xmo%YJeKYgBcf(^7)R<hn;%#Xqn%#]'+&#b9F&#sj9'#wvK'#%-_'#1Q?(#"
    "5^Q(#9ea1#Clj1#8d46#&rm(#NVs)#Ui8*#b7p*#fC,+#vtu+#$+2,#)=M,#:n@-#>$S-#D6o-#RaX.#Vmk.#xqs1#Cx&2#U$X9#uAU/#,4B2#.F^2#076&Mi&u#`+v/P]S5Ioe9>Oo["
    "N-=`aV28GD19$AX[H](a4Z;YY]&6YctD.DN,]UV$,Puu#,i68%6INP&@*gi'DBGJ(RA[`*R5%)*njvu,m3io.ltlr-Iai7[t#BJ1+W#,23>r%4P]s.CA%ox4DLKV6DF0;6N3)58[#]i9"
    "[m%29n:Q`<n.q(<EH,;?49$5A3$(8@.l0S[>DOcDN+1DEQX)>G)/cxFWc1MTdU*GV`=IfU+lQS%Ylo#$1:<p%w/sc<9___&f#sx+NsHcM((^fL-#:4N).gfLNwd5N2'IAOD@,gLHL*>N"
    "m<a?#Y?(ANeEN$#^AG##XY<rL6fr/#oqN+.v/RqL]LG&##;9K/5jd(#paFoL<(8qL@Bh*#Mfr=-PX4?-b_CH-p&wX.H0f-#@fRU./Vs)#BN#<-D]7:._/:/#KPp`=V)3)F`AM88Qv;dE"
    "/73A=<b9/DbJ*dEPB3X:Ra^f1dHEK2:xju52e`'/>oDJ1Z8-_J_e=X(Q$x?0]vPq2jmMk+B`M]=K7m-$eMeF@WC]w'5>B]OgZxcQ5mGoLJB]qLVLG&#@_nI-AoSN-xY5<-5Z5<-.F[:M"
    "#Kf>-g_nI-QlUH-d?:@-W.%I-@CsM-K55dMJ9-&8V:o`=N^@K;=N4D<Cmkw0uL^J;=8K21A`l]>[Vf]G)5sfDf+]>-)(2j1Kg6WAS$vG-)cEB-u8wx-(/'sLNZ5.#7Bg;-;mA,MeY`/#"
    "<ufN-[Xr.MSit)#&))t-TpYlL5:SqL*FD/#C#)t-Y,KkLt;T-#,6[qLI-lrLVC$LM5mP.#(e<+.**IqLamH+#81SnLd/lrL(u3r8%o''#-0jL^Yul*%5%UB#$,P:v3Y>>#AK8p&.VWh#"
    ".#nw'nHu9))-lEI?=n(<1<7B#0KViB<UXMCXOgA#9Gk]GF)bYHl:'B#vT7L#o)6PJ3D@X([XC]O2>ti_g@Qe$q-=`a>Ue]crbQe$,<m:dM2RMh+:Re$vZ&.$x>sx+B1?>#Ner*>/`:;$"
    "'`vQNxLF&#i^v8/MXZY#P;PQ')>MMFDiu(/G#####m8AtBC(t/";


const char* GetInterRegularCompressedBase85() {
  return kInterRegular_compressed_data_base85;
}

const char* GetInterSemiBoldCompressedBase85() {
  return kInterSemiBold_compressed_data_base85;
}

}  // namespace rex::ui
