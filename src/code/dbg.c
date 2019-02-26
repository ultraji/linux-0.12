inline void check_data32(int value, int pos)
{
__asm__ __volatile__(
	"shl $4, %%ebx\n\t"
	"addl $0xb8000, %%ebx\n\t"
	"movl $0xf0000000, %%eax\n\t"
	"movb $28, %%cl\n"
	"1:\n\t"
	"movl %0,%%edx\n\t"
	"andl %%eax, %%edx\n\t"
	"shr %%cl, %%edx\n\t"
	"add $0x30, %%dx\n\t"
	"cmp $0x3a, %%dx\n\t"
	"jb 2f\n\t"
	"add $0x07, %%dx\n\t"
	"2:\n\t"
	"add $0x0c00, %%dx\n\t"
	"movw %%dx,(%%ebx)\n\t"
	"sub $0x04, %%cl\n\t"
	"shr $0x04, %%eax\n\t"
	"add $0x02, %%ebx\n\t"
	"cmpl $0x0,%%eax\n\t"
	"jnz 1b\n"
	::"m"(value),"b"(pos)
);
}