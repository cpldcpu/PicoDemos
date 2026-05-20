 
;--------------------------------------------------------------------------
;
;				- DAWN -
;
;4k-Intro for the Party -V-
;
;Done 25.11.1995 by AZURE/ARTWORK
;	(C) Tim Böscke
;---------------------------------------------------------------------------
;
;Yep! Environment-mapping rulez !! #;-)
;
;--------------------------------------------------------------------------

	section "main",code

maustest	MACRO
		IFEQ	maus
		btst	#6,$bfe001
		beq.w	dein
		ENDC
		ENDM
maus=1
morph.nr=40

rb_z=128
start:
	IFEQ	MAUS
	move.l	a7,a7save
	ENDC
	
	lea	$dff000,a6

	move.l 	#coplist,$80(a6)

        move.l  4.w,a4
        lea     VBX(pc),a5
        jsr     -30(a4)                 ;SuperVisor


        ;code


	        lea     Wa.int(pc),a0
                move    $1c(a6),-10(a0)
                or.b    #$80,-10(a0)
                move    #$7fff,$9a(a6)

                move.l	VBD(pc),a4
                move.l  $6c(a4),-8(a0)
                move.l  a0,$6c(a4)
                move    #$c020,$9a(a6)

	moveq	#-1,d0
	move.l	d0,$44(a6)
	
;--------------------------------------------------------
;blur_init
	lea	blur_tab,a0

	move	#256*64-1,d7
.blilo
	move	d7,d0
	eor	#256*64-1,d0
	and	#%11111100111111,d0
	sub	#$202,d0
	bpl.s	.ne1
	and	#$ff,d0
.ne1
	tst.b	d0
	bpl.s	.ne2
	eor.b	d0,d0
.ne2
	move.w	d0,(a0)+
	dbf	d7,.blilo

;createpolystrukt:

	lea	obj.data,a0

dopolys
	moveq	#0,d0		;points ;cicle
	moveq	#rounseg-1,d7
.lop1
	moveq	#0,d1		;points round
	moveq	#circseg-1,d6
.lop2
	move	d0,d2		;1.p
	move	d0,d5		;2.p
	move	d0,d4		;3.p+y
	
	add	#circseg,d4
	and	#circseg*rounseg-1,d4

	move	d4,d3		;4.p+y

	or	d1,d2
	or	d1,d3
	
	addq	#1,d1
	and	#circseg-1,d1

	or	d1,d5
	or	d1,d4
	
	move	#3,(a0)+	; Vierecke
	addq	#2,a0

	movem.w	d2/d3/d4/d5,(a0)
	addq	#8,a0
	move.w	d2,(a0)+
	
	dbf	d6,.lop2

	add	#circseg,d0
	and	#(circseg*rounseg)-1,d0
	dbf	d7,.lop1
	moveq	#-1,d0
	move.l	d0,(a0)+

;-----------------------------------------------------

	bsr.w	calc_ppic
	bsr.w	tm_createscrtab
	bsr.w	makedacols

	lea	coplist,a4
	
	moveq	#-86,d0
	move	#40*rb_z-1,d7
	lea	mask,a0
	move.l	a0,d1
.lop34
	move.b	d0,(a0)+
	dbf	d7,.lop34

	move	d1,(bp_ml-coplist)(a4)
	swap	d1
	move	d1,(bp_mh-coplist)(a4)

	move	#$820f,$96(a6)
	move	#$7201,2(a4)		;Display on..

		
	lea	txt.dawn(pc),a0
	lea	colors1(pc),a3
	bsr	textout
;	bra.w	voxel


	lea	txt.by(pc),a0
	lea	colors2(pc),a3	
	bsr	textout
	lea	txt.azure(pc),a0
	lea	colors3(pc),a3	
	bsr	textout

	clr.l	wa.count
	lea	colors1(pc),a3
	moveq	#32,d0
	bsr	short_init
blop1
	bsr	handle_screen		
	bsr	clear_screen
	bsr	rb_moveovj
	lea	ro_trans(pc),a0
	cmp	#800,(a0)
	blt.s	.ee
	sub	#20,(a0)
.ee
	move	#15*50,d0		
	cmp	wa.count+2(pc),d0
	bgt.s	blop1
	bsr	fadeout

	move	#50,ro_wi+6
	clr.l	wa.count
	move	#3000,ro_trans
	lea	colors1(pc),a3
	moveq	#33,d0
	bsr	short_init
blop2
	bsr	handle_screen		
	bsr	clear_screen
	bsr	rb_moveovj
	lea	ro_trans(pc),a0
	cmp	#800,(a0)
	blt.s	.ee
	sub	#20,(a0)
.ee
	move	#20*50,d0
	cmp	wa.count+2(pc),d0
	bgt.s	blop2
	bsr	fadeout

voxel
	clr.l	wa.count
blopv3
	bsr	handle_screen		
	bsr	clear_screen
	bsr	voxeldo

	move	#25*50,d0
;	move	#65*50,d0
	cmp	wa.count+2(pc),d0
	bgt.s	blopv3
	bsr	fadeout

	clr	ro_wi+6
	addq.w	#1,ppicswitch
	bsr	calc_ppic

	clr.l	wa.count
	lea	colors5(pc),a3
	moveq	#34,d0
	bsr	short_init
blop3
	bsr	handle_screen		
	bsr	clear_screen
	bsr	rb_moveovj

	move	#15*50,d0
	cmp	wa.count+2(pc),d0
	bgt.s	blop3
	bsr	fadeout

	clr.w	blur
	clr.l	wa.count
	move	#1000,ro_trans
	lea	colors1(pc),a3
	moveq	#35,d0
	bsr	short_init
blop4
	bsr	handle_screen		
	bsr	rb_moveovj
	bsr	blur_it

	move	#20*50,d0
	cmp	wa.count+2(pc),d0
	bgt.s	blop4
	bsr	fadeout

weiwe:
	lea	txt.dawn(pc),a0
	lea	colors4(pc),a3
	bsr	textout

	addq.w	#1,blur	
agend:

	moveq	#0,d0
	move	wa.count+2(pc),d0
	lea	sin1024,a0
	asl	#2,d0
	and	#$3ff,d0
	move	(a0,d0.w*2),d0
	add	#32768,d0
	and	#%1111100000000000,d0
	lea	obj.dots,a0
	lea	(a0,d0.l),a0
	move.l	a0,aktobj

	bsr	handle_screen		
	bsr	text
	bsr	rb_moveovj
	bsr	blur_it
	
	btst	#6,$bfe001
	bne.w	agend
dein:
	IFEQ	MAUS
	move.l	a7save,a7
	ENDC
	
        move    #$7fff,$9a(a6)
        move.l	VBD(pc),a4
        move.l  Wa.irqadr(pc),$6c(a4)

	move.l $4.w,a5
	move.l $9c(a5),a5
	move.l $26(a5),$80(a6)
	clr.w  $88(a6)
	move	#$000f,$96(a6)
	move	#$8020,$96(a6)
        move    Wa.irqreg(pc),$9a(a6)
        moveq	#0,d0
	rts

VBX
        move.l  d1,-(a7)                ;not nec. if all irq is disabled ?!
	dc.w	$4e7a,$1801
        move.l  d1,VBD
        move.l  (a7)+,d1
        rte

VBD:	dc.l	0

	IFEQ	maus
a7save:	dc.l	0
	ENDC
;--------------
;short init
;-------------

short_init:
	mulu	#2048,d0
	add.l	#obj.dots,d0
	move.l	d0,aktobj
	bra	makedacols

;----------------------------------------------------------------------------
;
;Handle Screen
;
;---------------------------------------------------------------------------
handle_screen:
	maustest
	
	move.l	light+4(pc),light
	lea	b_swap(pc),a0
	
	add	#4,(a0)
	cmp	#12,(a0)	;8
	bne.s	.b_noov
	clr	(a0)
.b_noov:	
	move	(a0),d2
	move.l	#40*rb_z,d1
	move.l	d1,d4
	add.l	d1,d4
	Move.l	8+2(a0,d2.w),d0
	
	add.l	d4,d0	;d0=d0+d1*2	(d4=d1*2)
	
	add.l	d1,d4	;d4=d1*3
	
	add.l	d0,d4	;d4=d0+d1*3

	move	#$e0,d3
	moveq	#2,d7
	lea	bplpointer,a1
.lopqx
	move	d3,(a1)+
	addq	#2,d3
	swap	d0
	move	d0,(a1)+
	move	d3,(a1)+
	addq	#2,d3
	swap	d0
	move	d0,(a1)+
	sub.l	d1,d0

	move	d3,(a1)+
	addq	#2,d3
	swap	d4
	move	d4,(a1)+
	move	d3,(a1)+
	addq	#2,d3
	swap	d4
	move	d4,(a1)+
	sub.l	d1,d4
	dbf	d7,.lopqx

	Move.l	2(a0,d2.w),a1
	lea	picbuf,a0

	move.w	#160*128,d0
;--------------------------------------------------------------------------
;64 color 2x1 Chunky to planar ...
;
;Hybrid Blitter/CPU/Display ...
;
;Done 4.9.1995 & 14.9.1995 by Azure/Artwork
;
;91 rasterlines for 160x128 without bufferclearing on a1200/30/28
;---------------------------------------------------------------------------

_chunky2planar:
		move	d0,d1
		lsr	#2,d1

		; IN: a0.l = FastMemBuffer
		; IN: a1.l = Left top of destination
		; IN: d0.l = lenghth of CHunky Buffer (&%111 must be 000)

		lea	-4(a1,d1.w),a2
		lea	(a2,d1.w),a3
		lea	4(a0,d0.w),a4	;a4=end of chunky buffer	

		move.l	#$f0f0f0f0,d6
		move.l	#$33333333,d7
.innerloop:
		move.l	(a0)+,d0
		move.l	(a0)+,d1
		move.l	(a0)+,d2

		move.l	d5,(a2)+	;stream 2
		
		move.l	d0,d4
		and.l	d6,d0		
		eor.l	d0,d4
		lsl.l	#4,d4

		move.l	d2,d5
		and.l	d6,d5		

		eor.l	d5,d2
		move.l	d3,(a3)+	;stream 3
		lsr.l	#4,d5

		or.l	d5,d0
		or.l	d4,d2	
		
		move.l	d1,d4
		and.l	d6,d1		

		eor.l	d1,d4
		lsl.l	#4,d4

		move.l	(a0)+,d3	

		move.l	d3,d5
		and.l	d6,d5		
		eor.l	d5,d3
		lsr.l	#4,d5

		or.l	d5,d1
		or.l	d4,d3		

		and.l	d7,d0
		and.l	d7,d1
		lsl.l	#2,d0
		or.l	d0,d1	
		
		move.l	d1,(a1)+	;Stream 1

		move.l	d2,d4
		and.l	d7,d4		
		eor.l	d4,d2
		lsl.l	#2,d4

		move.l	d3,d5
		and.l	d7,d3	
		eor.l	d3,d5

		lsr.l	#2,d5
		or.l	d2,d5
		or.l	d4,d3		

		cmp.l	a0,a4		
		bge.s	.innerloop
			
.blw
	btst	#14,$02(a6)
	bne.s	.blw

	lea	b_swap,a0
	move	(a0),d2
	Move.l	2(a0,d2.w),a1
	lea	40*rb_z*3(a1),a2

	move.l	a1,$50(a6)
	addq	#2,a1
	move.l	a1,$4c(a6)
	move.l	a2,$54(a6)
	move	#$5555,$70(a6)

	clr	$62(a6)
	clr	$64(a6)
	clr	$66(a6)
	
	move.l	#%00011101111001001111000000000000,$40(a6)

	move	#[128+256]*64+20,$58(a6)
.ee
	tst	wa.count2(pc)
	beq.s	.ee
	clr	wa.count2
	rts
;--------------------------------------------------------------------------
;
;Textout
;
;--------------------------------------------------------------------------

textout:
	move.l	a0,txt
	bsr	makedacols
	clr.l	wa.count
.blop:
	bsr	handle_screen		
	bsr	text
	bsr	blur_it
	move	#150,d0
	cmp.w	wa.count+2(pc),d0
	bgt.s	.blop

fadeout
	clr.l	wa.count	
.blop2:
	bsr	handle_screen		
	bsr	blur_it
	moveq	#70,d0
	cmp.w	wa.count+2(pc),d0
	bgt.s	.blop2

	
	rts
;----------------------------------------------------------------------------
;
;Voxel
;
;----------------------------------------------------------------------------


voxeldo:


;	lea	sin1024,a0
;	move.l	wa.count(pc),d0
;	asl	#3,d0
;	and	#$7fe,d0
;;	move	(a0,d0.w),d1
;	add	#32768,d1
;	lsr	#8,d1
;	lsr	#1,d1
;	move	d1,.ypo	

	move.b	wa.count+3(pc),.ypo+1
	
	lea	tm_scrtab,a0
	lea	voxscape+32768,a1
	lea	voxcols+32768,a2
	lea	shadeta,a4
	
	move	#-80,d1		;yadd
	move	#(128-80)*256,.xpo	;ypos
.lop1
	move	.ypo(pc),d2		;begpointer %ff
	move	.xpo(pc),d0			;begpointer *256

	move.l	(a0)+,a3
	lea	160*127(a3),a3
	moveq	#64,d4
	moveq	#99,d6
.ilop
	move	d0,d3
	move.b	d2,d3
	add	d1,d0
	addq	#1,d2
	move.b	(a1,d3.w),d5
	move	(a2,d3.w),d7
	move.b	d6,d7
	move.b	(a4,d7.w),d7
.ee
	cmp	d5,d4
	beq.s	.ok
	bcc.s	.ok
	move.b	d7,(a3)
	lea	-160(a3),a3
	addq	#1,d4
	bra.s	.ee		
.ok	
	subq	#1,d4
	dbf	d6,.ilop

	addq	#1,d1		
	add	#256,.xpo
	cmp	#(128+80)*256,.xpo
	bne.s	.lop1
	rts
	
.ypo	dc.w	0	
.xpo	dc.w	0
;---------------------------------------------------------------------------
;
;Text
;
;----------------------------------------------------------------------------

text:
.lopb
	lea	tm_scrtab,a0
	move.l	txt(pc),a1
	lea	.rnd(pc),a3
	addq.w	#5,(a3)
	move	(a3),d0
	moveq	#3,d1
	and	d1,d0
	lea	(a0,d0.w*4),a0
	move	(a3),d2
	lsr	#2,d2
	and	d1,d2
	add	#40,d2
	mulu	#160,d2	
	moveq	#38,d7
.lop1
	move.b	(a1)+,d0
	moveq	#3,d6
.lop2
	move	d0,d1
	move.l	(a0)+,a2
	lea	(a2,d2.w),a2
	moveq	#7,d5
.lop3
	moveq	#7,d4
.lop4
	btst	#0,d1
	beq.s	.ee
	move.b	(a2),d3
	addq	#8,d3
	cmp.b	#50,d3
	bcs.s	.ee2
	moveq	#50,d3
.ee2
	move.b	d3,(a2)
.ee
	lea	160(a2),a2
	dbf	d4,.lop4
	lsr	d1
	dbf	d5,.lop3
	dbf	d6,.lop2
	dbf	d7,.lop1
	rts
	
.rnd:	dc.w	0

txt:
	dc.l	0
txt.dawn
		dc.w	0,$0000,$fefe,$8282,$82fe	;Dawn
		dc.w	$7c00,$fcfe,$1212,$12fe
		dc.w	$fc00,$7efe,$80e0,$e080
		dc.w	$fe7e,$00fe,$fe02,$0202
		dc.w	$fefc
txt.by
		dc.w	0,0,$0000,$1038,$1000,$0000	;by
		dc.w	$00fe,$fe92,$92fe,$6c00
		dc.w	$8e9e,$9090,$fe7e,$0000
		dc.w	$0000,$1038,$1000,$0000
txt.azure
		dc.w	0,0,$007e,$7f09,$7f7e,$0071	;Azure
		dc.w	$7949,$4f47,$003f,$7f40
		dc.w	$407f,$3f00,$7f7f,$097f
		dc.w	$7600,$7f7f,$4949,$4100,0,0
		
;----------------------------------------------------------------------------
;
;Blur_it
;
;-----------------------------------------------------------------------------
blur_it:
	lea	picbuf,a0
	lea	blur_tab,a1

	moveq	#0,d0
	move	#160*127/2-1,d7
.lop1
	move.w	(a0),d0
	add	160(a0),d0
	lsr	d0
	move	(a1,d0.w*2),(a0)+
	dbf	d7,.lop1

	moveq	#79,d7
.lop2
	clr.l	(a0)+
	dbf	d7,.lop2
	rts
	
;----------------------------------------------------------------------------
;
;Clear_screen
;
;-----------------------------------------------------------------------------

clear_screen:
	lea	picbuf,a0
	move	#160*128/8-1,d7
.lop1
	clr.l	(a0)+
	clr.l	(a0)+
	dbf	d7,.lop1
	rts

;----------------------------------------------------------------------------
;
;Create Morph
;
;-----------------------------------------------------------------------------

createmorph:
		bsr.s	createobjekt
		lea	aktobj(pc),a0
		add.l	#2048,(a0)
		add	#1500,6(a0)
		move	6(a0),$180(a6)
		cmp	#48000,6(a0)
		bcs.s	createmorph
.sss
		move	objze,d0
		cmp	#4*6,d0
		beq.s	.end
		lea	objspec(pc),a0
		lea	6(a0,d0.w),a1
		move.l	(a1)+,(a0)+
		move	(a1)+,(a0)+
		addq.w	#6,objze
		bsr.s	createobjekt
		lea	aktobj(pc),a0
		add.l	#2048,(a0)
		bra.s	.sss
.end
		rts

objze:		dc.w	0
aktobj:		dc.l	obj.dots
		      ;add.w , mulu
objspec:	dc.w	192,00000,100
objtab:		dc.w	0,0,0
		dc.w	128,30000,100
		dc.w	256+64,40000,0
		dc.w	256,20000,100


;-----------------------------------------------------------------------
;
;Create Objekt
;
;-----------------------------------------------------------------------

circseg=32	;32 Circle Segmemt
rounseg=8
createobjekt:

	move.l	aktobj(pc),a0
	lea	sin1024,a1
	lea	512(a1),a5
	move	objspec+4(pc),d0
	lea	(a5,d0.w),a2

	moveq	#0,d5		;d5=pointer
	moveq	#rounseg-1,d6
.lop2
	move	(a1,d5.w*2),d0	;sin =y
	move	(a2,d5.w*2),d1	;cos =z
	add	#1024/rounseg,d5
	muls	#4*384,d0	;kleiner Radius=192
	muls	#2*384,d1
	swap	d0
	add	#1500,d0	;großer Radius=200
	move	d0,d1

	move.l	a1,a3
	move.l	d1,a4
	move	d5,d2	;Pointer
	lsr	#4,d2
	moveq	#circseg-1,d7	;16 Segmente
.lop1
	move.l	a4,d3
	move	(a3),d0
	adda.w	objspec(pc),a3
	add	#32768,d0
	mulu	objspec+2(pc),d0
	swap	d0
	not.w	d0
	lsr	d0
	move	d0,d4
	muls	d3,d0
	asl.l	d0
	swap	d0
	swap	d3
	move	d3,d1
	muls	d4,d1
	asl.l	d1
	swap	d1
	move	(a1,d2.w*2),d3	;sin
	move	(a5,d2.w*2),d4	;cosin
	add	#1024/circseg,d2
	muls	d0,d3	;x
	muls	d0,d4	;y
	move.l	d3,(a0)+
	move.l	d4,-2(a0)
	move.w	d1,(a0)
	addq	#4,a0
	dbf	d7,.lop1

	dbf	d6,.lop2

;------------------------------------------------------------------------
;
;Calclate poly Normals 
;
;-------------------------------------------------------------------------

norms1:
	
	move.l	aktobj(pc),a0
	lea	obj.data,a1
	lea	obj.flno,a2
.lop1
	tst.l	(a1)+
	bmi.s	.end
	movem.w	(a1)+,d5/d6/d7
	addq	#4,a1
	movem.w	(a0,d5.w*8),d0/d1/d2	;p1
	movem.w (a0,d7.w*8),d3/d4/d5	;p3
	movem.w	(a0,d6.w*8),d6/d7/a3	;p2
	
	sub	d6,d3		;v1x
	sub	d7,d4		;  y
	sub	a3,d5		;  z	

	sub	d6,d0		;v2x
	sub	d7,d1		;  y
	sub	a3,d2		;  z

				; v1 x v2= nx= v1y * v2z  -  v1z * v2y
				;	   ny= v1z * v2x  -  v1x * v2z
				;	   nz= v1x * v2y  -  v1y * v2x
	move	d0,d6
	move	d1,d7
	move	d2,a3

	muls	d4,d2	;v1y * v2z
	muls	d5,d0	
	muls	d3,d1	

	muls	d7,d5
	move	a3,d7
	muls	d7,d3
	muls	d6,d4

	sub.l	d5,d2	;d2=x
	sub.l	d3,d0	;d0=y
	sub.l	d4,d1	;d1=z

	asr.l	#8,d0
	asr.l	#8,d1
	asr.l	#8,d2

	move	d2,(a2)+
	move	d0,(a2)+
	move	d1,(a2)+
	addq	#2,a2
	
	bra.s	.lop1
.end	

;----------------------------------------------------------------------------
;
;Merge normals
;
;----------------------------------------------------------------------------

merge:
	lea	obj.flno,a0
	move.l	aktobj(pc),a2
	adda.l	#(256*4*2)*morph.nr,a2
	lea	lengta,a3

	move	#255,d7
.lop1
	moveq	#0,d0
	moveq	#0,d1
	moveq	#0,d2

	lea	.puts(pc),a1

	moveq	#3,d6
.lop2
	move	d7,d4
	not.b	d4
	move	d4,d5
	add.b	4(a1),d5
	add.b	(a1)+,d4
	and	#%11100000,d5
	and	#%00011111,d4
	or	d5,d4
	
	add	(a0,d4.w*8),d0
	add	2(a0,d4.w*8),d1
	add	4(a0,d4.w*8),d2
	dbf	d6,.lop2

	asr	#4,d0
	asr	#4,d1
	asr	#4,d2
	
	move	d0,d3
	muls	d3,d3
	move	d1,d4
	muls	d4,d4
	add.l	d4,d3
	move	d2,d4
	muls	d4,d4
	add.l	d4,d3
	move	(a3,d3.w*2),d4
	
	muls	d4,d0
	muls	d4,d1
	muls	d4,d2

	asr.l	#8,d0
	asr.l	#8,d1
	asr.l	#8,d2
	movem.w	d0/d1/d2,(a2)
	addq	#8,a2
	
	dbf	d7,.lop1
	rts

.puts:	dc.b	0,31,0,31
	dc.b	0,0,256-32,256-32

calc_ppic:
	lea	ppic,a0
	move	#255,d2
	move	#128,d3
	move	d2,d7
.lop1
	move	d2,d6
.lop2
	move	d7,d0
	move	d6,d1
	sub	d3,d0
	sub	d3,d1
	muls	d0,d0
	muls	d1,d1
	add	d0,d1
	lsr	#4,d1

	cmp	#$fc,d1
	ble.s	.nex
	moveq	#-2,d1
.nex
	not.b	d1

	tst	ppicswitch(pc)
	beq.s	.ppic2
	
	and	#255,d1
	muls	#14500,d1
	swap	d1
	move	d6,d0
	eor	d7,d0
	btst	#5,d0
	bne.s	.ee
	addq	#7,d1
.ee
	bra.s	.drop
.ppic2
	lsr.b	#2,d1
.drop
	move.b	d1,(a0)+
	dbf	d6,.lop2
	dbf	d7,.lop1
	rts

ppicswitch:	dc.w	0

b_swap:	dc.w	0
b_tab:
	dc.l	b_scr1,b_scr2,b_scr3
	dc.l	b_scr1,b_scr2,b_scr3

;--------------------------------------------------------
;
;Makedacols
;
;in=a3 Pointer to cols
;
;-----------------------------------------------------

colors1:	dc.w	65000,60000,35000
colors2:	dc.w	65000,50000,35000
colors3:	dc.w	45000,50000,35000
colors4:	dc.w	60000,60000,60000
colors5:	dc.w	60000,50000,10000

makedacols:

	moveq	#63,d7
.lop1
	move	d7,d0
	eor	#$3f,d0
	move	d0,d3
	mulu	d3,d3
	lsr.l	#4,d3
	mulu	(a3)+,d3
	move.l	d3,d1

	move	d0,d3
	mulu	d3,d3
	mulu	d0,d3
	lsr.l	#2,d3
	lsr.l	#8,d3
	
	mulu	(a3)+,d3
	lsr.l	#8,d3
	move.w	d3,d1

	move	d0,d3
	lsl	#2,d3
	mulu	(a3),d3
	subq.l	#4,a3
	swap	d3
	move.b	d3,d1
		
	movem.l	d0/d1,-(a7)
	bsr.s	.setchcol
	movem.l	(a7)+,d0/d1
	move	d0,d2
	and	#$55,d2
	eor	d2,d0
	asl	#1,d2
	lsr	#1,d0
	or	d2,d0
	or.b	#$40,d0
	bsr.s	.setchcol
	dbf	d7,.lop1
	rts

;------------------- SETCHUNKYCOL -----------------------------------------
;D0.w=Register
;D1.w=Color 00RrGgBb
;d2-d4=scratch
;A6=$dff000
.setchcol:
	move.l	#$000f0f0f,d3
	move.l	d1,d2	
	lsr.l	#4,d1
	and.l	d3,d1		;0fff0000
	lsl	#4,d1		;000ff0f0
	lsl.l	#4,d1		;00ff0f00
	lsl	#4,d1		;00fff000
	lsl.l	#4,d1		;0fff0000
	and.l	d3,d2
	lsl	#4,d2
	lsl.l	#4,d2
	lsl	#4,d2
	lsl.l	#4,d2
	swap	d2
	move	d2,d1	
	
;------------------------------- SETaCOL -----------------------------------
;D0.w=Register		  Scratch
;D1.l=Color   0HHH0LLL    Scratch
;D2=xxx 		  Scratch
;A6=$DFF000               Konst.
;---------------------------------------------------------------------------

.SETaCOL:
	move	d0,d2
	and	#%00011111,d0
	or.b	#$c0,d0
	and	#%11100000,d2
	asl	#8,d2
	or	#$22,d2
	move	d2,$106(a6)
	swap	d1
	move.w	d1,(a6,d0.w*2)	
	swap	d1
	or	#$200,d2
	move	d2,$106(a6)
	move.w	d1,(a6,d0.w*2)

	rts

Wa.irqreg               dc.w    0
Wa.irqadr               dc.l    0
wa.count                dc.l    0

Wa.int:         
                movem.l d0-d7/a0-a6,-(a7)       
		lea	ro_wi(pc),a0
		add.w	#-50,(a0)+
		add.w	#80,(a0)+
		add.w	#600,(a0)
		move	(a0)+,d0
		lea	sin1024,a1
		lsr	#5,d0
		move	(a1,d0.w*2),d0
		muls	(a0),d0
		swap	d0
		ext.l	d0
		add.l	#ppic+256*64+64,d0
		move.l	d0,light+4
                addq.l  #1,wa.count
		addq	#1,wa.count2
		moveq	#4,d0
		moveq	#34,d1
		moveq	#16,d4
		move	#9724,d2
		move	#$a0,d3
		lea	sample,a0
		moveq	#3,d7
.lop1
		move.l	a0,$0(a6,d3.w)
		move	d0,$4(a6,d3.w)
		move	d1,$8(a6,d3.w)
		move	d2,$6(a6,d3.w)
		add	d4,d2
		add	d4,d3
		dbf	d7,.lop1	

                movem.l (a7)+,d0-d7/a0-a6
                move    #$20,$dff09c		
                move    #$20,$dff09c		
		nop
                rte
wa.count2		dc.w	0

	section	"cop",data_c
coplist:

	dc.w $0100,$0000
	dc.w $0102,$0000
	dc.w $0106,$0022
	dc.w $1dc,$20
	dc.w $1fc,3+16384
	dc.w $108,-40,$010a,0
	dc.w $8e,$2c81,$90,$2cc1
	dc.w $92,$38,$94,$a8 ;$b8
	dc.w $96,$20
	dc.w $120,0,$122,0
	
	dc.w	$f8
bp_mh:	dc.w	0,$fa
bp_ml:	dc.w	0
	
bplpointer:
	blk.w	[12*2],0
	dc.l -2

sample:
	DC.B	$00,$30,$59,$75,$7F,$75,$59,$30

	section	"rb_shipdax",bss_c
	cnop	0,8
	ds.l	2
b_scr1:	ds.b	[40*rb_z*6]
b_scr2:	ds.b	[40*rb_z*6]
b_scr3:	ds.b	[40*rb_z*6]
mask:	ds.b	[40*rb_z]
	
	section	"main",code

;»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»
;Plotmapped/CHUNKY © AZURE / ARTWORK & BIZARRE ARTS
;
;Written and Copyright , Tim Boescke 1995
;
;V4.0	26.7.95 cut down .. 24.11.1995
;~~~~~~~~~~~~~~
; 2 adds
; no tab-del !
; Polyplot-Schleife optimiert
; Plottab-Struktur geändert
;«««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««

tm_plot:
	move	(a0)+,d6
	addq	#1,d6
        addq	#2,a0
        lea     koords,a1
        lea	TM_lines1,a2
        lea	tm_imag,a4
.colo1
        move    (a0)+,d0         ;KOORDZEIGER holen
        move.l  (a1,d0.w*8),(a2)+ ;XXYY Koordinate holen
	move.l	(a4,d0.w*4),(a2)+	;IKoords in Tabelle Eintragen
	dbf	d6,.colo1

	subq	#8,a2		;Endadresse in Tab	

	lea	tm_lines1,a0
	lea	(256+64)*4(a0),a4

	move	#-200,a4	;max
	move	#2000,a5	;min

	lea	tm_divtab+65536,a3
.inlo1
	movem.l	(a0)+,d0/d1	;X,Y,A,B start fetchen
	movem.l	(a0),d2/d3	;X,Y,A,B ende  fetchen
	lea	tabl,a1
	swap	d0
	swap	d2
		
        cmp     a4,d2
        blt.s   .nex1
        move    d2,a4
.nex1
        cmp     a5,d2
        bgt.s   .nex2
        move    d2,a5
.nex2   
	cmp	d0,d2
	beq.s	.weiter
	bgt.s	.rok
	lea	(256+64)*4(a1),a1
	exg.l	d0,d2
	exg.l	d1,d3
.rok
	lea	(a1,d0.w*4),a1	;Startadresse
	sub	d0,d2		;d2=ydif
	move	d2,d7
	
	move	d2,d4
	asl	#8,d4		;divisor <<8 für tab

	swap	d0		;X1
	swap	d2		;X2

	sub	d0,d2		;D2=Xdif
	ext.l	d2	
	asl.l	#8,d2
	divs	d7,d2
	ext.l	d2	
	ror.l	#8,d2
	move.l	d2,d5		;d5=xx0000XX

	sub	d1,d3		;d3=Bdif
	move.b	d3,d4

	moveq	#0,d6
	move	(a3,d4.w*2),d6
	ror.l	#8,d6		;d6=bb0000BB
	move	d6,d5		;d5=xx0000BB

	swap	d1	
	swap	d3

	sub	d1,d3		;d3=Adif
	move.b	d3,d4
	move	(a3,d4.w*2),d6	;d6=bb00AAaa

	moveq	#0,d4
	move	d1,d4		;d4=000000aw		
	asl	#8,d4		;d4=0000aw00
	clr	d1		
	swap	d1		;d1=000000bw
	
				;bb00AAaa add d6 store d4
				;xx0000BB add d5 store d1
				;    00XX add d2 store d0
	subq	#1,d7		;length
.inlo2
	move	d0,(a1)+	
	move	d4,d3
	move.b	d1,d3
	move	d3,(a1)+

	add.l	d6,d4
	addx.l	d5,d1
	addx.w	d2,d0
	dbf	d7,.inlo2
.weiter
	cmp.l	a0,a2
	bne.w	.inlo1
	
        lea     tabl,a1
        move	#160,d6

	moveq	#0,d0
        move    a5,d0
        move    a4,d7

	tst	 d0
	bpl.s	.ok
	moveq	#0,d0
.ok	
	cmp	d6,d7
	blt.s	.ok2
	move	d6,d7
.ok2
	sub     d0,d7
        subq    #1,d7
        bmi.w   .end
        
        lea     (a1,d0.w*4),a1
        lea	tm_scrtab,a5
        lea	(a5,d0.w*4),a5
	move.l	(a5)+,a0
        move.l	light(pc),a4
.plo1
        move.w  (a1)+,d0
        move.w  [256+64]*4-2(a1),d1
        sub  	 d0,d1
        subq	#1,d1
        bmi.s   .w2
        move	d0,d2
        muls	#160,d2
        lea     (a0,d2.l),a2
	move	d1,d5
	asl	#8,d5

        move.b  (a1)+,d0        
        move.b  [256+64]*4-1(a1),d2
        sub     d0,d2		;D0=xbeg
        move.b	d2,d5
        move	(a3,d5.w*2),d2	;d2=HHLLx
	ror.l	#8,d2
	move.l	d2,d4

        move.b  (a1)+,d3       
        move.b  [256+64]*4-1(a1),d4
        sub     d3,d4		;d3=ybeg
	asl	#8,d3		;d3=ybeg << 8
        move.b	d4,d5		
        move	(a3,d5.w*2),d4	;d4=HHLLy
        tst	blur(pc)
        beq.s	.plo3
.plo2
        move    d3,d5
        move.b  d0,d5
        move.b  (a4,d5.w),(a2)
	lea	160(a2),a2
        add.l   d4,d3		;D3=xx##YYyy
        addx    d2,d0		;D0=----##XX
        dbf     d1,.plo2

	move.l	(a5)+,a0
        dbf	d7,.plo1
        rts

.plo3
        move    d3,d5
        move.b  d0,d5
        move.b  (a4,d5.w),d6
        cmp.b	(a2),d6
        blt.s	.ee
        move.b	d6,(a2)
.ee
	lea	160(a2),a2
        add.l   d4,d3		;D3=xx##YYyy
        addx    d2,d0		;D0=----##XX
        dbf     d1,.plo3

	move.l	(a5)+,a0
        dbf	d7,.plo1
        rts
.w2     
        addq    #2,a1
	move.l	(a5)+,a0
        dbf     d7,.plo1        
.w3
.end
        rts

blur:	dc.w	1

;--------------------------------------------------------------------------
;
;Table for scrambled Plotting...
;
;--------------------------------------------------------------------------

tm_createscrtab:
	lea	tm_scrtab,a0
	lea	picbuf,a1
	lea	.scramble(pc),a2
	moveq	#0,d0
	move	#160,d7
.lopx
	move	d0,d1
	move	d0,d2
	and	#$0f,d1
	eor	d1,d2
	or	(a2,d1.w*2),d2
	lea	(a1,d2.w),a3
	move.l	a3,(a0)+
	addq	#1,d0	
	dbf	d7,.lopx

;--------------------------------------------------------------------------
;
;Create DIVS-table
;
;In:	xxyy
;
;out:   (yy/xx)*256
;
;--------------------------------------------------------------------------

	lea	tm_divtab+128*256*2,a0
	move	#255,d7
.lop1
	move	d7,d1
	beq.s	.eee
	asl	#8,d1
	move	#255,d6
.lop2
	move	d6,d0
	extb.l	d0
	asl	#8,d0
	move.b	d6,d1
	divs	d7,d0
	move	d0,(a0,d1.w*2)
	dbf	d6,.lop2
.eee
	dbf	d7,.lop1

;---------------------------------------------------------------------------
;Calculate Sin-Tabs..
;
;Taylor-Series....
;
;---------------------------------------------------------------------------

		lea	sin1024,a0
		lea	1024(a0),a1
		moveq	#0,d4
		move	#255,d3

.l1		move.l	d4,d0
		swap	d0
		add.l	#823550,d4	
		move	d0,d2
		move	d0,d1
		mulu	d1,d1
		lsl.l	#4,d1
		swap	d1
		mulu	d1,d0
		divu	#6144,d0
		sub	d0,d2
		mulu	d1,d0
		divu	#20480,d0
		add	d0,d2
		asr	#2,d2
		muls	#63,d2		;Output:-32255 - +32255
		
		move.w	d2,(a0)+	
		move.w	d2,-(a1)	;Other Quadrants...
		neg.w	d2
		move.w	d2,1022(a0)
		move.w	d2,1024(a1)
		dbf	d3,.l1

		lea	-512(a0),a0	;Create Wraparound SinTable
		lea	2048(a0),a1
		move	#511,d7
.lop3
		move.l	(a0)+,d0
		move.l	d0,1024*2(a1)
		move.l	d0,2048*2(a1)

		move.l	d0,3072*2(a1)
		move.l	d0,4096*2(a1)

		move.l	d0,(a1)+
		dbf	d7,.lop3

;---------------------------------------------------------------------------
;
;Calculate Tab. for Vector normalisation..
;
; In:  lenght^2
;
; Out: Multiplier to get lenght of 128
;
;----------------------------------------------------------------------------

	moveq	#1,d1
	moveq	#1,d2
	moveq	#0,d3
	lea	lengta+2,a0
	moveq	#1,d5
	move	#50000,d7
.lopa1
	move.l	d5,d0

	lsl.l	#8,d0
.aga
	cmp.l	d0,d3		
	bge.s	.reddi	
	add.l	d2,d3
	addq.l	#2,d2
	addq.l	#1,d1
	bra.s	.aga
.reddi
	move.l	d1,d4
	asl.l	#4,d4
	moveq.l	#$7f,d0
	swap	d0
	divs	d4,d0
	move	d0,(a0)+

	addq	#1,d5
	dbf	d7,.lopa1
	lea	sin1024,a0
	lea	voxscape+32768,a1
	lea	voxcols+32768,a2
	sub.l	a3,a3
	sub.l	a4,a4
	sub.l	a5,a5
	move	#255,d7
.lopb1
	move	#255,d6
.lopb2
	move	d7,d1
	asl	#8,d1
	move.b	d6,d1

	move	d7,d2
	move	d6,d3
	move	#255-31,d0
	sub	d0,d2
	sub	d0,d3
	muls	d2,d2
	muls	d3,d3
	add.l	d2,d3
	lsr.l	#4,d3
	move	(a0,d3.w*2),d5
	asr	#5,d5

	move	d7,d2
	move	d6,d3
	moveq	#31,d0
	sub	d0,d2
	sub	d0,d3
	muls	d2,d2
	muls	d3,d3
	add.l	d2,d3
	lsr.l	#5,d3
	move	(a0,d3.w*2),d4
	asr	#5,d4
	add	d4,d5
	
	move	d5,a5
	asr	#7,d5
	add	#64,d5
	move.b	d5,(a1,d1.w)
	move	a5,d5
	sub	d0,d5
	asr	#6,d5	
	add	#32,d5
	eor	#$3f,d5
	move.b	d5,(a2,d1.w)
	move	a5,d0
	
	dbf	d6,.lopb2
	dbf	d7,.lopb1

	lea	shadeta,a0
	moveq	#64,d7
.lki
	move	#255,d6
.lkk
	move	d7,d0
	move	d6,d1
	and	#$7f,d1
	lsr	#3,d1
	sub	d1,d0
	bpl.s	.eer
	moveq	#0,d0
.eer
	move.b	d0,(a0)+
	
	dbf	d6,.lkk
	dbf	d7,.lki
	
	bra.w	createmorph

.scramble dc.w	0,4,8,12,1,5,9,13,2,6,10,14,3,7,11,15

	section "GROD",BSS

shadeta:
	ds.b	32768
voxscape:
	ds.b	65536
voxcols:
	ds.b	65536
	
	ds.b	160*64	;Well.. nice Y-clipping.. isn`t it ? ;)

picbuf:
ch_pix=160*128
	ds.b	ch_pix
	ds.b	160*64

lengta:	ds.w	65535

blur_tab:
	ds.w	64*256
	
	cnop	0,8
sin1024:
	ds.w	1024*8

tm_imag:ds.l	1000

ppic:	ds.b	65536

tm_scrtab:
	ds.l	[160]	;Tabelle der X-adressen (wegen des scrambled buffers)
tm_divtab:
	ds.w	[65536]
clipdummy:
	ds.l	64
tabl:
	ds.l	256+64	;Format: Xpos.b,ImaX.b,ImaY.b,0.b
tabr:	
	ds.l	256+64
tm_lines1:
	ds.l	2*20	;Format: X1.w,Y1.w,col.w
tm_lines2:
	ds.l	2*20	;Format: X1.w,Y1.w,col.w

koords:	ds.w	1000*4

Polydep:ds.l	1000
objend: ds.l	1

obj.dots:	ds.w	(256*4)*morph.nr
obj.dono:	ds.w	(256*4)*morph.nr

obj.data:	ds.w	(256*14+2)
obj.flno:	ds.w	(256*4)

	section "main",code
	
;»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»»
;Vector Rotation
;««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««««·

zoom=1000

ROTit:
	lea	ro_wi(pc),a3
	movem.w	(a3)+,d0/d1
	lea	sin1024,a3
	lsr	#5,d0
	lsr	#5,d1
	move	(a3,d0.w*2),d5
	move	(a3,d1.w*2),d6
	swap	d5
	swap	d6
	add	#256,d0
	add	#256,d1
	move	(a3,d0.w*2),d5
	move	(a3,d1.w*2),d6
	move	ro_trans(pc),a3
.lop1
	movem.w	(a0)+,d0/d1/d2/d3

	move	d0,d3
	move	d2,d4
	muls	d5,d0	;cos 1
	muls	d5,d2	;cos 2
	swap	d5
	muls	d5,d3	;sin 1
	muls	d5,d4	;sin 2
	swap	d5
	sub.l	d4,d0
	add.l	d3,d2
	swap	d2
	lsl	d2

	move	d1,d3
	move	d2,d4
	muls	d6,d1	;cos 1
	muls	d6,d2	;cos 2
	swap	d6
	muls	d6,d3	;sin 1
	muls	d6,d4	;sin 2
	swap	d6
	sub.l	d4,d1
	add.l	d3,d2
	swap	d2
	lsl	d2
	
	add	a3,d2
	move	d2,d3
	add	#zoom,d3

	asr.l	#8,d0
	asr.l	#8,d1
	divs	d3,d0
	divs	d3,d1
	
	add	#80,d0	;x
	add	#64,d1

	movem.w	d0/d1/d2,(a1)
	
	addq	#8,a1
	dbf	d7,.lop1

	move.l	aktobj(pc),a1
	adda.l	#(256*4*2)*morph.nr,a1
	lea	tm_imag,a0
	move	#255,d7
.lop2
	movem.w	(a1)+,d0/d1/d2/d3

	move	d0,d3
	move	d2,d4
	muls	d5,d0	;cos 1
	muls	d5,d2	;cos 2
	swap	d5
	muls	d5,d3	;sin 1
	muls	d5,d4	;sin 2
	swap	d5
	
	add.l	d3,d2
	sub.l	d4,d0
	swap	d2
	muls	d6,d1	;cos 1
	asl	d2
	swap	d6
	swap	d0
	muls	d6,d2	;sin 2
	sub.l	d2,d1
	swap	d1
	swap	d6

	moveq	#64,d4
	add	d4,d0
	move	d0,(a0)+
	add	d4,d1
	move	d1,(a0)+
	dbf	d7,.lop2

	rts

RO_trans:dc.w	3000	;Translation...			

;------------------------------------------------------------------------------
;
;Draw the shit ....
;
;-----------------------------------------------------------------------------

		
rb_moveovj:	;(a0=Zeiger auf.obj1)
	lea	koords,a1
	move	#255,d7
	move.l	aktobj(pc),a0
	bsr	ROTit
	bsr	garaud	

;	lea	surf_ad(pc),a0
	move.l	#polydep,surf_ad
.lop1	
	move.l	surf_ad(pc),a0
	cmp.l	polyend(pc),a0
	beq.s	.end

	move.l	(a0)+,d0
	move.l	a0,surf_ad
	lea	obj.data,a1
	lea	(a1,d0.w),a0
	
	bsr	tm_plot	
	bra.s	.lop1
.end
	rts	

light:		dc.l	0,0
surf_ad:	dc.l	0
polyend:	dc.l	0

;---------- GAUROUD-SHADING and QUICK-SORT ----------------
garaud:
	moveq	#0,d2
	lea	obj.data,a1
	lea	polydep,a2
	move.l	a2,a5
	lea	koords,a3
.lop2
	cmp	#-1,(a1,d2.w)
	beq.w	.end
	move	(a1,d2.w),d7

	move.l	4(a1,d2.w),d0
	move	8(a1,d2.w),d1

	move.l	(a3,d0.w*8),d5	;d4=x1   |y1	;Backfacing
	swap	d0
	move.l	(a3,d0.w*8),d4	;d5=x2   |y2
	move.l	(a3,d1.w*8),d6  ;d6=x3   |y3

	sub.l	d4,d5		;d5=x1-x2|y1-y2
	sub.l	d4,d6		;d6=x3-x2|y3-y2

	move	d5,d0		;d0=y1-y2
	move	d6,d1		;d1=y3-y2
	swap	d5		;d5=x1-x2
	swap	d6		;d6=x3-x2

	muls	d6,d0
	muls	d1,d5
	
	move.l	d2,d3
	addq	#4,d2
	sub	d0,d5		;d6=d6-d3
	ble.s	.novisi

	swap	d3
.lop3
	move	(a1,d2.w),d0
	add	4(a3,d0.w*8),d3
	addq	#2,d2
	dbf	d7,.lop3

	addq	#2,d2

	neg	d3
	swap	d3
	move.l	d3,(a2)+
	bra.s	.lop2
.novisi
	move	d7,d5
	addq	#2,d5
	asl	#1,d5
	add	d5,d2
	bra.s	.lop2
.end	
;	lea	polyend(pc),a0
	move.l	a2,polyend

;¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤ QUICK-SORT ¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤
quicksort:

	move.l	a5,a0
	move.l	a2,a1
quick2:
	move	#$fffc,d2
	subq.l	#4,a1
.sort
	move.l	a0,a2
	move.l	a1,a3
	move.l	a1,d0
	sub.l	a0,d0
	lsr.w	#1,d0
	and.w	d2,d0
	move	(a0,d0.w),d0
.lop1	subq	#4,a2 
.lop2	addq	#4,a2
	cmp	(a2),d0
	bgt.s	.lop2
	addq	#4,a3
.lop3	
	subq	#4,a3
	cmp	(a3),d0
	blt.s	.lop3
	cmp.l	a2,a3
	blt.s	.lop4
	move.l	(a2),d1
	move.l	(a3),(a2)
	move.l	d1,(a3)
	addq	#4,a2
	subq	#4,a3
	cmp.l	a2,a3
	bge.s	.lop1
.lop4	cmp.l	a0,a3
	ble.s	.lop5
	movem.l	a1-a2,-(a7)
	move.l	a3,a1
	bsr.s	.sort
	movem.l	(a7)+,a1-a2
.lop5	cmp.l	a2,a1
	ble.s	.lop6
	move.l	a2,a0
	bra.s	.sort
.lop6
	rts
RO_wi:	dc.w	0,0,0,0
	
