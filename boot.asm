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