# dynamic-scheduler

Dynamic Instruction Scheduler with customized Tomasulo Algorithm to execute instructions out of order -> Uses L2 Cache to operate memory-reference instructions & customized dispatch queue and super-scalar sizes.

This architecture takes in instructions and executes them based on readiness of source operands that are dependant across instructions. Thus, hazards are prevented
