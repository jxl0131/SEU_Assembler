         .data
x:       .word  0
y:       .word  0
z:       .word  0
         .text
_start:  lw $1, x($0)
		 addi $1, $1, 9
		 lw $2, y($0)
		 addi $2, $0, 3
		 lw $3, z($0)
		 addi $3, $0, 5
		 addi $4, $0, 1
		 add $1, $1, $4
		 addi $4, $0, 0
		 beq $1, $4, 4
L1:		 j L4
L2:      addi $4, $0, 1
		 sub $2, $2, $4
		 addi $4, $0, 1
		 add $2, $2, $4
		 add $3, $0, $2
		 j L4
L3:      addi $4, $0, 2
		 div $1, $4
		 mflo $1
		 addi $4, $0, 5
		 add $1, $1, $4
		 add $3, $0, $1
L4:      nop

###boot:包含一些预先写在系统中的可调用的函数
	
done:   j 	0x00      #返回到最开始的指令处
dely:  nop               # no.1e
         beq   $a0,$zero,exit
         ori	 $t0,$a0,0x0000
dlop: nop 
        sw    $t8, 0x0000($t6)   #no.20
        addi  $t0, $t0,-1
        bne	$t0,$zero,dlop
        nop
  exit: nop
         sw    $t8, 0x0000($t6)
        jr 	$31

readio:   nop	  # no.25
              lw	$v0,0x0000($a0)  # ��һ�������洢��$a0,��������ֵ�洢��$v0
              nop
              nop
       	      jr	$31

writeio:   nop     #no.2a
              nop
              sw	$a0, 0x0000($a1)
              nop
              nop
	      jr	$31
