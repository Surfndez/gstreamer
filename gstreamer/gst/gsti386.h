#ifndef GST_HGUARD_GSTI386_H
#define GST_HGUARD_GSTI386_H

#define GET_SP(target) \
  __asm__("movl %%esp, %0" : "=m"(target) : : "esp", "ebp");

#define SET_SP(source) \
  __asm__("movl %0, %%esp\n" : "=m"(thread->sp));

#define JUMP(target) \
    __asm__("jmp " SYMBOL_NAME_STR(cothread_stub))

#define SETUP_STACK(sp) do ; while(0)

#endif /* GST_HGUARD_GSTI386_H */
