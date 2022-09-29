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

